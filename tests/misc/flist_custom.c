#include <stic.h>

#include <unistd.h> /* chdir() rmdir() symlink() unlink() */

#include <stddef.h> /* size_t */
#include <stdio.h> /* fclose() fopen() fprintf() remove() */
#include <stdlib.h> /* free() */
#include <string.h> /* memset() */

#include "../../src/cfg/config.h"
#include "../../src/cfg/info.h"
#include "../../src/cfg/info_chars.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/fileview.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/filter.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/filtering.h"
#include "../../src/macros.h"
#include "../../src/registers.h"
#include "../../src/running.h"
#include "../../src/sort.h"

#include "utils.h"

static void column_line_print(const void *data, int column_id, const char buf[],
		size_t offset, AlignType align, const char full_column[]);
static void setup_custom_view(FileView *view, int very);
static int filenames_can_include_newline(void);

static char test_data[PATH_MAX];
static const size_t MAX_WIDTH = 20;
static char buf1[80 + 1];
static char buf2[80 + 1];

SETUP_ONCE()
{
	char cwd[PATH_MAX];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	if(is_path_absolute(TEST_DATA_PATH))
	{
		snprintf(test_data, sizeof(test_data), "%s", TEST_DATA_PATH);
	}
	else
	{
		snprintf(test_data, sizeof(test_data), "%s/%s", cwd, TEST_DATA_PATH);
	}
}

SETUP()
{
	update_string(&cfg.fuse_home, "no");
	update_string(&cfg.slow_fs_list, "");

	/* So that nothing is written into directory history. */
	rwin.list_rows = 0;

	view_setup(&lwin);

	curr_view = &lwin;
	other_view = &lwin;

	snprintf(lwin.curr_dir, sizeof(lwin.curr_dir), "%s/..", test_data);
}

TEARDOWN()
{
	update_string(&cfg.slow_fs_list, NULL);
	update_string(&cfg.fuse_home, NULL);

	view_teardown(&lwin);
}

TEST(empty_list_is_not_accepted)
{
	flist_custom_start(&lwin, "test");
	assert_false(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);
}

TEST(duplicates_are_not_added)
{
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);
	assert_int_equal(1, lwin.list_rows);
}

TEST(parent_dir_is_not_added_to_very_custom_view)
{
	opt_handlers_setup();

	cfg.dot_dirs = DD_NONROOT_PARENT;
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_VERY, 0) == 0);
	assert_int_equal(1, lwin.list_rows);
	cfg.dot_dirs = 0;

	opt_handlers_teardown();
}

TEST(custom_view_replaces_custom_view_fine)
{
	assert_false(flist_custom_active(&lwin));

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);
	assert_int_equal(1, lwin.list_rows);

	assert_true(flist_custom_active(&lwin));

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/b");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);
	assert_int_equal(1, lwin.list_rows);

	assert_true(flist_custom_active(&lwin));
}

TEST(reload_considers_local_filter)
{
	filters_view_reset(&lwin);

	assert_false(flist_custom_active(&lwin));

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/b");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	local_filter_apply(&lwin, "b");

	load_dir_list(&lwin, 1);

	assert_int_equal(1, lwin.list_rows);
	assert_string_equal("b", lwin.dir_entry[0].name);

	filter_dispose(&lwin.manual_filter);
	filter_dispose(&lwin.auto_filter);
}

TEST(reload_does_not_remove_broken_symlinks, IF(not_windows))
{
	char test_file[PATH_MAX];

	assert_non_null(os_realpath(TEST_DATA_PATH "/existing-files/a", test_file));

	assert_success(chdir(SANDBOX_PATH));

	/* symlink() is not available on Windows, but other code is fine. */
#ifndef _WIN32
	assert_success(symlink("/wrong/path", "broken-link"));
#endif

	assert_false(flist_custom_active(&lwin));

	copy_str(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH);
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, test_file);
	flist_custom_add(&lwin, "./broken-link");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_int_equal(2, lwin.list_rows);
	load_dir_list(&lwin, 1);
	assert_int_equal(2, lwin.list_rows);

	assert_success(remove("broken-link"));
}

TEST(symlinks_to_dirs_are_recognized_as_dirs, IF(not_windows))
{
	char test_dir[PATH_MAX];

	assert_non_null(os_realpath(TEST_DATA_PATH "/existing-files", test_dir));

	assert_success(chdir(SANDBOX_PATH));

	/* symlink() is not available on Windows, but other code is fine. */
#ifndef _WIN32
	assert_success(symlink(test_dir, "dir-link"));
#endif
	(void)test_dir;

	assert_false(flist_custom_active(&lwin));

	copy_str(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH);
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "./dir-link");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_int_equal(1, lwin.list_rows);
	assert_true(is_directory_entry(&lwin.dir_entry[0]));

	assert_success(remove("dir-link"));
}

TEST(locally_filtered_files_are_not_lost_on_reload)
{
	filters_view_reset(&lwin);

	assert_false(flist_custom_active(&lwin));

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/b");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	local_filter_apply(&lwin, "b");

	load_dir_list(&lwin, 1);
	assert_int_equal(1, lwin.filtered);

	filter_dispose(&lwin.manual_filter);
	filter_dispose(&lwin.auto_filter);
}

TEST(register_macros_are_expanded_relatively_to_orig_dir)
{
	char *expanded;
	char path[PATH_MAX];

	setup_custom_view(&lwin, 0);

	regs_init();

	assert_success(chdir(TEST_DATA_PATH "/existing-files"));
	assert_success(regs_append('r', "b"));
	expanded = expand_macros("%rr:p", NULL, NULL, 0);
	snprintf(path, sizeof(path), "%s/existing-files/b", test_data);
	assert_string_equal(path, expanded);
	free(expanded);

	regs_reset();
}

TEST(dir_macros_are_expanded_to_orig_dir)
{
	char *expanded;
	char path[PATH_MAX];

	snprintf(path, sizeof(path), "%s/existing-files", test_data);

	setup_custom_view(&lwin, 0);

	expanded = expand_macros("%d", NULL, NULL, 0);
	assert_string_equal(path, expanded);
	free(expanded);

	expanded = expand_macros("%D", NULL, NULL, 0);
	assert_string_equal(path, expanded);
	free(expanded);
}

TEST(files_are_sorted_undecorated)
{
	assert_success(chdir(SANDBOX_PATH));

	cfg.type_decs[FT_DIR][DECORATION_SUFFIX][0] = '/';
	cfg.type_decs[FT_DIR][DECORATION_SUFFIX][1] = '\0';

	lwin.sort[0] = SK_BY_NAME;
	memset(&lwin.sort[1], SK_NONE, sizeof(lwin.sort) - 1);

	assert_success(os_mkdir("foo", 0700));
	assert_success(os_mkdir("foo-", 0700));
	assert_success(os_mkdir("foo0", 0700));

	assert_true(get_cwd(lwin.curr_dir, sizeof(lwin.curr_dir)) == lwin.curr_dir);
	assert_false(flist_custom_active(&lwin));
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "foo");
	flist_custom_add(&lwin, "foo-");
	flist_custom_add(&lwin, "foo0");
	assert_success(flist_custom_finish(&lwin, CV_REGULAR, 0));

	assert_string_equal("foo", lwin.dir_entry[0].name);
	assert_string_equal("foo-", lwin.dir_entry[1].name);
	assert_string_equal("foo0", lwin.dir_entry[2].name);

	assert_success(rmdir("foo"));
	assert_success(rmdir("foo-"));
	assert_success(rmdir("foo0"));
}

TEST(unsorted_custom_view_does_not_change_order_of_files)
{
	opt_handlers_setup();

	assert_false(flist_custom_active(&lwin));
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/b");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_VERY, 0) == 0);

	assert_string_equal("b", lwin.dir_entry[0].name);
	assert_string_equal("a", lwin.dir_entry[1].name);

	opt_handlers_teardown();
}

TEST(sorted_custom_view_after_unsorted)
{
	opt_handlers_setup();

	lwin.sort[0] = SK_BY_NAME;
	memset(&lwin.sort[1], SK_NONE, sizeof(lwin.sort) - 1);

	assert_false(flist_custom_active(&lwin));

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/b");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_VERY, 0) == 0);

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/b");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_string_equal("a", lwin.dir_entry[0].name);
	assert_string_equal("b", lwin.dir_entry[1].name);

	opt_handlers_teardown();
}

TEST(unsorted_view_remains_one_on_vifminfo_reread_on_restart)
{
	FILE *const f = fopen(SANDBOX_PATH "/vifminfo", "w");
	fprintf(f, "%c%d", LINE_TYPE_LWIN_SORT, SK_BY_NAME);
	fclose(f);

	opt_handlers_setup();

	assert_false(flist_custom_active(&lwin));
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/b");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_VERY, 0) == 0);
	assert_true(lwin.custom.type == CV_VERY);
	assert_int_equal(SK_NONE, lwin.sort[0]);

	/* ls-like view blocks view column updates. */
	copy_str(cfg.config_dir, sizeof(cfg.config_dir), SANDBOX_PATH);
	curr_stats.restart_in_progress = 1;
	read_info_file(1);
	curr_stats.restart_in_progress = 0;

	assert_true(lwin.custom.type == CV_VERY);
	assert_int_equal(SK_NONE, lwin.sort[0]);

	opt_handlers_teardown();

	assert_success(remove(SANDBOX_PATH "/vifminfo"));
}

TEST(location_is_saved_on_entering_custom_view)
{
	char cwd[PATH_MAX];
	char *saved_cwd;

	cfg_resize_histories(10);

	curr_stats.ch_pos = 1;

	/* Set test data directory as current directory. */
	saved_cwd = save_cwd();
	assert_success(chdir(TEST_DATA_PATH "/existing-files"));
	assert_true(get_cwd(cwd, sizeof(cwd)) == cwd);
	copy_str(lwin.curr_dir, sizeof(lwin.curr_dir), cwd);

	/* Put specific history entry and make sure it's used. */
	save_view_history(&lwin, lwin.curr_dir, "b", 1);
	load_dir_list(&lwin, 0);
	assert_string_equal(lwin.curr_dir, cwd);
	assert_string_equal("b", lwin.dir_entry[lwin.list_pos].name);

	/* Pick different entry. */
	lwin.list_pos = 2;
	assert_string_equal("c", lwin.dir_entry[lwin.list_pos].name);

	/* Go into custom view (load_stage enables saving history). */
	restore_cwd(saved_cwd);
	curr_stats.load_stage = 2;
	setup_custom_view(&lwin, 0);
	curr_stats.load_stage = 0;

	/* Return to previous directory and check that last location was used. */
	cd_updir(&lwin, 1);
	assert_string_equal("c", lwin.dir_entry[lwin.list_pos].name);

	cfg_resize_histories(0U);

	curr_stats.ch_pos = 0;
}

TEST(parent_link_has_correct_origin_field)
{
	char path[PATH_MAX];

	/* This directory entry is added separately and thus can stray from the
	 * others. */

	cfg.dot_dirs = DD_NONROOT_PARENT;
	setup_custom_view(&lwin, 0);
	cfg.dot_dirs = 0;

	assert_string_equal("..", lwin.dir_entry[0].name);
	snprintf(path, sizeof(path), "%s/existing-files", test_data);
	assert_string_equal(path, lwin.dir_entry[0].origin);
}

TEST(custom_view_does_not_reset_local_state)
{
	int very;

	opt_handlers_setup();
	fview_init();

	columns_set_line_print_func(&column_line_print);

	for(very = 0; very < 2; ++very)
	{
		size_t prefix_len = 0U;
		column_data_t cdt = { .view = &lwin, .prefix_len = &prefix_len };

		lwin.columns = columns_create();

		assert_success(exec_commands("set sort=+iname", &lwin, CIT_COMMAND));
		assert_int_equal(SK_BY_INAME, lwin.sort[0]);

		local_filter_apply(&lwin, "b");

		/* Neither on entering it. */
		assert_success(exec_commands("setl sort=-target", &lwin, CIT_COMMAND));
		assert_int_equal(-SK_BY_TARGET, lwin.sort[0]);
		setup_custom_view(&lwin, very);
		assert_int_equal(very ? SK_NONE : -SK_BY_TARGET, lwin.sort[0]);
		assert_false(filter_is_empty(&lwin.local_filter.filter));

		cdt.line_hi_group = 1;
		columns_format_line(lwin.columns, &cdt, MAX_WIDTH);

		assert_success(exec_commands("setl sort=-type", &lwin, CIT_COMMAND));
		assert_int_equal(-SK_BY_TYPE, lwin.sort[0]);

		curr_stats.load_stage = 1;
		assert_success(change_directory(&lwin, lwin.custom.orig_dir));
		curr_stats.load_stage = 0;

		/* Nor on leaving it. */
		assert_int_equal(very ? -SK_BY_TARGET : -SK_BY_TYPE, lwin.sort[0]);
		assert_false(filter_is_empty(&lwin.local_filter.filter));

		cdt.line_hi_group = 2;
		columns_format_line(lwin.columns, &cdt, MAX_WIDTH);
		if(very)
		{
			/* Need to check that we restored original sorting after very custom
			 * view.  The one set in regular custom view is left untouched. */
			assert_string_equal(buf1, buf2);
		}
		else
		{
			assert_true(strcmp(buf1, buf2) != 0);
		}

		columns_free(lwin.columns);
		lwin.columns = NULL;
	}

	opt_handlers_teardown();
	columns_clear_column_descs();
}

TEST(files_with_newline_in_names, IF(filenames_can_include_newline))
{
	FILE *const f = fopen(SANDBOX_PATH "/list", "w");
	fprintf(f, "%s%c", SANDBOX_PATH "/a\nb", '\0');
	fclose(f);

	assert_success(chdir(SANDBOX_PATH));

	replace_string(&cfg.shell, "/bin/sh");
	stats_update_shell_type(cfg.shell);

	create_file("a\nb");
	assert_non_null(get_cwd(lwin.curr_dir, sizeof(lwin.curr_dir)));
	assert_success(output_to_custom_flist(&lwin, "cat list", 0, 0));
	assert_success(unlink("a\nb"));
	assert_success(unlink("list"));

	stats_update_shell_type("/bin/sh");
	update_string(&cfg.shell, NULL);

	assert_int_equal(1, lwin.list_rows);
	if(lwin.list_rows == 1)
	{
		assert_string_equal("a\nb", lwin.dir_entry[0].name);
	}
}

TEST(can_set_very_cv_twice_in_a_row)
{
	fview_init();
	opt_handlers_setup();

	lwin.columns = columns_create();

	lwin.sort[0] = SK_BY_NAME;
	memset(&lwin.sort[1], SK_NONE, sizeof(lwin.sort) - 1);

	setup_custom_view(&lwin, 1);
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "a");
	assert_true(flist_custom_finish(&lwin, CV_VERY, 0) == 0);

	columns_free(lwin.columns);
	lwin.columns = NULL;

	opt_handlers_teardown();
	columns_clear_column_descs();
}

TEST(renaming_dir_in_cv_adjust_its_children_entries)
{
	char path[PATH_MAX];

	snprintf(lwin.curr_dir, sizeof(lwin.curr_dir), "%s/..", test_data);
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files");
	flist_custom_add(&lwin, TEST_DATA_PATH "/existing-files/a");
	copy_str(lwin.curr_dir, sizeof(lwin.curr_dir), test_data);
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);
	assert_int_equal(2, lwin.list_rows);

	fentry_rename(&lwin, &lwin.dir_entry[0], "existing_files");
	snprintf(path, sizeof(path), "%s/existing_files", test_data);
	assert_string_equal(path, lwin.dir_entry[1].origin);
}

TEST(symlinks_are_not_resolved_in_origins, IF(not_windows))
{
#ifndef _WIN32
	assert_success(symlink(TEST_DATA_PATH "/existing-files",
				SANDBOX_PATH "/link"));
#endif

	assert_success(chdir(SANDBOX_PATH "/link"));
	copy_str(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH "/link");
	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, SANDBOX_PATH "/link"); /* Absolute path. */
	flist_custom_add(&lwin, "a");                  /* Relative path. */
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	assert_true(paths_are_equal(SANDBOX_PATH, lwin.dir_entry[0].origin));
	assert_true(paths_are_equal(SANDBOX_PATH "/link", lwin.dir_entry[1].origin));

	assert_success(remove(SANDBOX_PATH "/link"));
}

TEST(files_are_excluded_from_custom_view)
{
	setup_custom_view(&lwin, 0);

	lwin.dir_entry[0].selected = 1;
	lwin.selected_files = 1;
	flist_custom_exclude(&lwin, 1);

	assert_int_equal(0, lwin.selected_files);
}

static void
column_line_print(const void *data, int column_id, const char buf[],
		size_t offset, AlignType align, const char full_column[])
{
	const column_data_t *const cdt = data;
	strncpy((cdt->line_hi_group == 1 ? buf1 : buf2) + offset, buf, strlen(buf));
}

static void
setup_custom_view(FileView *view, int very)
{
	assert_false(flist_custom_active(view));
	snprintf(view->curr_dir, sizeof(view->curr_dir), "%s/..", test_data);
	flist_custom_start(view, "test");
	flist_custom_add(view, TEST_DATA_PATH "/existing-files/a");
	snprintf(view->curr_dir, sizeof(view->curr_dir), "%s/existing-files",
			test_data);
	assert_true(flist_custom_finish(view, very ? CV_VERY : CV_REGULAR, 0) == 0);
}

static int
filenames_can_include_newline(void)
{
	FILE *const f = fopen(SANDBOX_PATH "/a\nb", "w");
	if(f != NULL)
	{
		fclose(f);
		assert_success(unlink(SANDBOX_PATH "/a\nb"));
		return not_windows();
	}
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
