/* vifm
 * Copyright (C) 2011 xaizek.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "string_array.h"

#include <assert.h> /* assert() */
#include <stdarg.h>
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* FILE SEEK_END SEEK_SET fclose() fprintf() fread()
                      ftell() fseek() */
#include <stdlib.h> /* free() malloc() realloc() */
#include <string.h> /* strcspn() */

#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../compat/reallocarray.h"
#include "file_streams.h"

static char * read_whole_file(const char filepath[], size_t *read);
static char * read_seekable_stream(FILE *const fp, size_t *read);
static size_t get_remaining_stream_size(FILE *const fp);
static char ** text_to_lines(char text[], size_t text_len, int *nlines,
		int null_sep);
static char ** break_into_lines(char text[], size_t text_len, int *nlines,
		int null_sep);

int
add_to_string_array(char ***array, int len, int count, ...)
{
	char **p;
	va_list va;

	p = reallocarray(*array, len + count, sizeof(char *));
	if(p == NULL)
		return len;
	*array = p;

	va_start(va, count);
	while(count-- > 0)
	{
		char *arg = va_arg(va, char *);
		if(arg == NULL)
			p[len] = NULL;
		else if((p[len] = strdup(arg)) == NULL)
			break;
		len++;
	}
	va_end(va);

	return len;
}

int
put_into_string_array(char ***array, int len, char item[])
{
	char **const arr = reallocarray(*array, len + 1, sizeof(char *));
	if(arr != NULL)
	{
		*array = arr;
		arr[len++] = item;
	}
	return len;
}

void
remove_from_string_array(char **array, size_t len, int pos)
{
	free(array[pos]);
	memmove(array + pos, array + pos + 1, sizeof(char *)*((len - 1) - pos));
}

int
is_in_string_array(char *array[], size_t len, const char item[])
{
	const int pos = string_array_pos(array, len, item);
	return pos >= 0;
}

int
is_in_string_array_case(char *array[], size_t len, const char item[])
{
	const int pos = string_array_pos_case(array, len, item);
	return pos >= 0;
}

int
is_in_string_array_os(char *array[], size_t len, const char item[])
{
#ifndef _WIN32
	return is_in_string_array(array, len, item);
#else
	return is_in_string_array_case(array, len, item);
#endif
}

char **
copy_string_array(char **array, size_t len)
{
	char **result = reallocarray(NULL, len, sizeof(char *));
	size_t i;
	for(i = 0U; i < len; ++i)
	{
		result[i] = strdup(array[i]);
	}
	return result;
}

int
string_array_pos(char *array[], size_t len, const char item[])
{
	size_t i = len;
	if(item != NULL)
	{
		for(i = 0U; i < len; ++i)
		{
			if(strcmp(array[i], item) == 0)
			{
				break;
			}
		}
	}
	return (i < len) ? (int)i : -1;
}

int
string_array_pos_case(char *array[], size_t len, const char item[])
{
	size_t i = len;
	if(item != NULL)
	{
		for(i = 0; i < len; i++)
		{
			if(strcasecmp(array[i], item) == 0)
			{
				break;
			}
		}
	}
	return (i < len) ? (int)i : -1;
}

void
free_string_array(char *array[], size_t len)
{
	if(array != NULL)
	{
		free_strings(array, len);
		free(array);
	}
}

void
free_strings(char *array[], size_t len)
{
	size_t i;
	for(i = 0; i < len; i++)
	{
		free(array[i]);
	}
}

char **
read_file_of_lines(const char filepath[], int *nlines)
{
	size_t text_len;
	char *const text = read_whole_file(filepath, &text_len);
	char **list = (text == NULL)
	            ? NULL
	            : text_to_lines(text, text_len, nlines, 0);
	if(list == NULL)
	{
		list = malloc(sizeof(*list));
		*nlines = 0;
	}
	return list;
}

/* Reads file specified by filepath into null terminated string.  Returns
 * string of length *read to be freed by caller on success, otherwise NULL is
 * returned. */
static char *
read_whole_file(const char filepath[], size_t *read)
{
	char *content = NULL;
	FILE *fp;

	*read = 0U;

	if((fp = os_fopen(filepath, "rb")) != NULL)
	{
		content = read_seekable_stream(fp, read);
		fclose(fp);
	}

	return content;
}

char **
read_file_lines(FILE *f, int *nlines)
{
	size_t text_len;
	char *const text = read_seekable_stream(f, &text_len);
	return text_to_lines(text, text_len, nlines, 0);
}

char **
read_stream_lines(FILE *f, int *nlines, int null_sep_heuristic, progress_cb cb,
		const void *arg)
{
	int null;
	size_t text_len;
	char *const text = read_nonseekable_stream(f, &text_len, cb, arg);
	if(text == NULL)
	{
		return NULL;
	}
	null = (null_sep_heuristic && strlen(text) != text_len);
	return text_to_lines(text, text_len, nlines, null);
}

char *
read_nonseekable_stream(FILE *fp, size_t *read, progress_cb cb, const void *arg)
{
	enum { PIECE_LEN = 4096 };
	char *content = malloc(PIECE_LEN + 1);

	if(content != NULL)
	{
		char *last_allocated_block = content;
		size_t len = 0U, piece_len;
		skip_bom(fp);
		while((piece_len = fread(content + len, 1, PIECE_LEN, fp)) != 0U)
		{
			const size_t new_size = len + piece_len + PIECE_LEN + 1U;
			last_allocated_block = realloc(content, new_size);
			if(last_allocated_block == NULL)
			{
				break;
			}

			content = last_allocated_block;
			len += piece_len;

			if(cb != NULL)
			{
				cb(arg);
			}
		}
		content[len] = '\0';

		if(last_allocated_block == NULL)
		{
			free(content);
			content = NULL;
		}
		else
		{
			*read = len;
		}
	}

	if(content == NULL)
	{
		*read = 0U;
	}

	return content;
}

/* Reads content of the fp stream that supports seek operation (points to a
 * file) until end-of-file into null terminated string.  Returns string of
 * length *read to be freed by caller on success, otherwise NULL is returned and
 * *read is set to 0UL. */
static char *
read_seekable_stream(FILE *const fp, size_t *read)
{
	char *content;
	size_t len;

	skip_bom(fp);
	len = get_remaining_stream_size(fp);

	*read = 0UL;

	if((content = malloc(len + 1U)) == NULL)
	{
		return NULL;
	}

	if(fread(content, len, 1U, fp) == 1U)
	{
		content[len] = '\0';
		*read = len;
	}
	else
	{
		free(content);
		content = NULL;
	}

	return content;
}

/* Calculates remaining size of the stream.  Assumes that the fp stream supports
 * seek operation.  Returns the size. */
static size_t
get_remaining_stream_size(FILE *const fp)
{
	size_t remaining_size;
	const long pos = ftell(fp);
	assert(pos >= 0 && "Stream expected to support seek operation.");

	(void)fseek(fp, 0, SEEK_END);
	remaining_size = ftell(fp) - pos;
	(void)fseek(fp, pos, SEEK_SET);

	return remaining_size;
}

/* Converts text of length text_len into an array of strings.  Always frees
 * piece of memory pointed to by the text.  Returns non-NULL on success,
 * otherwise NULL is returned, *nlines is untouched.  For empty file non-NULL
 * will be returned, but *nlines will be zero. */
static char **
text_to_lines(char text[], size_t text_len, int *nlines, int null_sep)
{
	char **list = NULL;

	if(text != NULL)
	{
		list = break_into_lines(text, text_len, nlines, null_sep);
		free(text);
	}
	return list;
}

/* Converts text of length text_len into an array of strings.  Returns non-NULL
 * on success, otherwise NULL is returned, *nlines is untouched.  For empty file
 * non-NULL will be returned, but *nlines will be zero. */
static char **
break_into_lines(char text[], size_t text_len, int *nlines, int null_sep)
{
	const char *const seps = null_sep ? "" : "\n\r";
	const char *const end = text + text_len;
	char **list = NULL;

	*nlines = 0;
	while(text < end)
	{
		const size_t line_len = strcspn(text, seps);

		char *after_line = text + line_len;
		if(after_line[0] == '\n')
		{
			after_line += 1;
		}
		else if(after_line[0] == '\r')
		{
			after_line += (after_line[1] == '\n') ? 2 : 1;
		}
		else if(after_line[0] == '\0')
		{
			do
			{
				after_line++;
			}
			while(after_line < end && after_line[0] == '\0');
		}

		text[line_len] = '\0';
		*nlines = add_to_string_array(&list, *nlines, 1, text);

		text = after_line;
	}

	return list;
}

int
write_file_of_lines(const char filepath[], char *strs[], size_t nstrs)
{
	size_t i;

	FILE *const fp = os_fopen(filepath, "w");
	if(fp == NULL)
	{
		return 1;
	}

	for(i = 0U; i < nstrs; ++i)
	{
		fputs(strs[i], fp);
		putc('\n', fp);
	}

	fclose(fp);
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
