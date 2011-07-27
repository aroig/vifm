#include <stdlib.h>
#include <string.h>

#include "seatest.h"

#include "../../src/cmds.h"

#define ARRAY_LEN(x) (sizeof(x)/sizeof((x)[0]))

static struct cmd_info cmdi;
static char *arg;

static int exec_cmd(const struct cmd_info* cmd_info);
static int call_cmd(const struct cmd_info* cmd_info);
static int delete_cmd(const struct cmd_info* cmd_info);
static int edit_cmd(const struct cmd_info* cmd_info);
static int filter_cmd(const struct cmd_info* cmd_info);
static int history_cmd(const struct cmd_info* cmd_info);
static int invert_cmd(const struct cmd_info* cmd_info);
static int substitute_cmd(const struct cmd_info* cmd_info);
static int quit_cmd(const struct cmd_info* cmd_info);

static const struct cmd_add commands[] = {
	{ .name = "!",          .abbr = NULL,  .handler = exec_cmd,       .id = -1,    .range = 0,    .cust_sep = 0,
		.emark = 0,           .qmark = 1,    .expand = 0,               .regexp = 0, .min_args = 1, .max_args = 1,       .bg = 1,     },
	{ .name = "call",       .abbr = "cal", .handler = call_cmd,       .id = -1,    .range = 0,    .cust_sep = 0,
		.emark = 0,           .qmark = 1,    .expand = 0,               .regexp = 0, .min_args = 1, .max_args = 1,       .bg = 0,     },
	{ .name = "delete",     .abbr = "d",   .handler = delete_cmd,     .id =  1,    .range = 1,    .cust_sep = 0,
		.emark = 1,           .qmark = 0,    .expand = 0,               .regexp = 0, .min_args = 0, .max_args = 1,       .bg = 0,     },
	{ .name = "edit",       .abbr = "e",   .handler = edit_cmd,       .id = -1,    .range = 1,    .cust_sep = 0,
		.emark = 0,           .qmark = 0,    .expand = 0,               .regexp = 0, .min_args = 0, .max_args = NOT_DEF, .bg = 0,     },
	{ .name = "filter",     .abbr = "fil", .handler = filter_cmd,     .id = -1,    .range = 1,    .cust_sep = 0,
		.emark = 1,           .qmark = 1,    .expand = 0,               .regexp = 1, .min_args = 0, .max_args = NOT_DEF, .bg = 0,     },
	{ .name = "history",    .abbr = "his", .handler = history_cmd,    .id = -1,    .range = 0,    .cust_sep = 0,
		.emark = 0,           .qmark = 0,    .expand = 0,               .regexp = 0, .min_args = 0, .max_args = 0,       .bg = 0,     },
	{ .name = "invert",     .abbr = NULL,  .handler = invert_cmd,     .id = -1,    .range = 0,    .cust_sep = 0,
		.emark = 0,           .qmark = 1,    .expand = 0,               .regexp = 0, .min_args = 0, .max_args = 0,       .bg = 0,     },
	{ .name = "substitute", .abbr = "s",   .handler = substitute_cmd, .id = -1,    .range = 0,    .cust_sep = 1,
		.emark = 0,           .qmark = 0,    .expand = 0,               .regexp = 0, .min_args = 0, .max_args = 3,       .bg = 0,     },
	{ .name = "quit",       .abbr = "q",   .handler = quit_cmd,       .id = -1,    .range = 0,    .cust_sep = 0,
		.emark = 1,           .qmark = 0,    .expand = 0,               .regexp = 0, .min_args = 0, .max_args = 0,       .bg = 0,     },
};

static int
exec_cmd(const struct cmd_info* cmd_info)
{
	cmdi = *cmd_info;
	if(cmdi.argc > 0)
	{
		free(arg);
		arg = strdup(cmdi.argv[0]);
	}
	return 0;
}

static int
call_cmd(const struct cmd_info* cmd_info)
{
	cmdi = *cmd_info;
	return 0;
}

static int
delete_cmd(const struct cmd_info* cmd_info)
{
	cmdi = *cmd_info;
	if(cmdi.argc > 0)
	{
		free(arg);
		arg = strdup(cmdi.argv[0]);
	}
	return 0;
}

static int
edit_cmd(const struct cmd_info* cmd_info)
{
	cmdi = *cmd_info;
	if(cmdi.argc > 0)
	{
		free(arg);
		arg = strdup(cmdi.argv[0]);
	}
	return 0;
}

static int
filter_cmd(const struct cmd_info* cmd_info)
{
	cmdi = *cmd_info;
	if(cmdi.argc > 0)
	{
		free(arg);
		arg = strdup(cmdi.argv[0]);
	}
	return 0;
}

static int
history_cmd(const struct cmd_info* cmd_info)
{
	cmdi = *cmd_info;
	return 0;
}

static int
invert_cmd(const struct cmd_info* cmd_info)
{
	cmdi = *cmd_info;
	return 0;
}

static int
substitute_cmd(const struct cmd_info* cmd_info)
{
	cmdi = *cmd_info;
	if(cmdi.argc > 0)
	{
		free(arg);
		arg = strdup(cmdi.argv[0]);
	}
	return 0;
}

static int
quit_cmd(const struct cmd_info* cmd_info)
{
	cmdi = *cmd_info;
	return 0;
}

static void
setup(void)
{
	add_buildin_commands(commands, ARRAY_LEN(commands));
}

static void
teardown(void)
{
}

static void
test_trimming(void)
{
	assert_int_equal(0, execute_cmd("q"));
	assert_int_equal(0, execute_cmd(" q"));
}

static void
test_range_acceptance(void)
{
	assert_int_equal(0, execute_cmd("%delete"));
	assert_int_equal(CMDS_ERR_NO_RANGE_ALLOWED, execute_cmd("%history"));
}

static void
test_range(void)
{
	cmds_conf.begin = 10;
	cmds_conf.current = 50;
	cmds_conf.end = 100;

	assert_int_equal(0, execute_cmd("%delete!"));
	assert_int_equal(10, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd("$delete!"));
	assert_int_equal(100, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd(".,$delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd(",$delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd("$,.delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd("$,delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd(",delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(50, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd("5,6,7delete!"));
	assert_int_equal(6, cmdi.begin);
	assert_int_equal(7, cmdi.end);
	assert_true(cmdi.emark);
}

static void
test_range_and_spaces(void)
{
	cmds_conf.begin = 10;
	cmds_conf.current = 50;
	cmds_conf.end = 100;

	assert_int_equal(0, execute_cmd("% delete!"));
	assert_int_equal(10, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd("  ,  ,  ,   $  ,   .   delete!"));
	assert_int_equal(50, cmdi.begin);
	assert_int_equal(100, cmdi.end);
	assert_true(cmdi.emark);
}

static void
test_bang_acceptance(void)
{
	assert_int_equal(0, execute_cmd("q"));
	assert_int_equal(CMDS_ERR_NO_BANG_ALLOWED, execute_cmd("history!"));
}

static void
test_bang(void)
{
	assert_int_equal(0, execute_cmd("q"));
	assert_false(cmdi.emark);

	assert_int_equal(0, execute_cmd("q!"));
	assert_true(cmdi.emark);

	assert_int_equal(CMDS_ERR_TRAILING_CHARS, execute_cmd("q !"));
}

static void
test_qmark_acceptance(void)
{
	assert_int_equal(0, execute_cmd("invert?"));
	assert_int_equal(CMDS_ERR_NO_QMARK_ALLOWED, execute_cmd("history?"));
}

static void
test_qmark(void)
{
	assert_int_equal(0, execute_cmd("invert"));
	assert_false(cmdi.qmark);

	assert_int_equal(0, execute_cmd("invert?"));
	assert_true(cmdi.qmark);

	assert_int_equal(CMDS_ERR_TRAILING_CHARS, execute_cmd("invert ?"));
}

static void
test_args(void)
{
	assert_int_equal(CMDS_ERR_TOO_FEW_ARGS, execute_cmd("call"));
	assert_int_equal(CMDS_ERR_TRAILING_CHARS, execute_cmd("call a b"));

	assert_int_equal(0, execute_cmd("call a"));
	assert_int_equal(1, cmdi.argc);

	assert_int_equal(0, execute_cmd("delete"));
	assert_int_equal(0, cmdi.argc);

	assert_int_equal(0, execute_cmd("edit a b c d"));
	assert_int_equal(4, cmdi.argc);
}

static void
test_count(void)
{
	assert_int_equal(0, execute_cmd("delete 10"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("10", arg);

	assert_int_equal(0, execute_cmd("delete10"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("10", arg);
}

static void
test_end_characters(void)
{
	assert_int_equal(0, execute_cmd("call a"));
	assert_int_equal(CMDS_ERR_TRAILING_CHARS, execute_cmd("call, a"));
}

static void
test_custom_separator(void)
{
	assert_int_equal(0, execute_cmd("s"));
	assert_int_equal(0, cmdi.argc);
	assert_int_equal(0, execute_cmd("s/some"));
	assert_int_equal(1, cmdi.argc);
	assert_int_equal(0, execute_cmd("s/some/thing"));
	assert_int_equal(2, cmdi.argc);
	assert_int_equal(0, execute_cmd("s/some/thing/g"));
	assert_int_equal(3, cmdi.argc);
	assert_int_equal(CMDS_ERR_TRAILING_CHARS, execute_cmd("s/some/thing/g/j"));
}

static void
test_custom_separator_and_arg_format(void)
{
	assert_int_equal(0, execute_cmd("s/\"so\"me\"/thing/g"));
	assert_int_equal(3, cmdi.argc);
	assert_string_equal("\"so\"me\"", arg);

	assert_int_equal(0, execute_cmd("s/'some'/thing/g"));
	assert_int_equal(3, cmdi.argc);
	assert_string_equal("'some'", arg);
}

static void
test_regexp_flag(void)
{
	assert_int_equal(0, execute_cmd("e /te|xt/"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("/te|xt/", arg);

	assert_int_equal(0, execute_cmd("filter /te|xt/"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("te|xt", arg);
}

static void
test_backgrounding(void)
{
	assert_int_equal(0, execute_cmd("e &"));
	assert_int_equal(1, cmdi.argc);
	assert_int_equal(0, cmdi.bg);

	assert_int_equal(0, execute_cmd("!vim&"));
	assert_int_equal(1, cmdi.argc);
	assert_int_equal(0, cmdi.bg);

	assert_int_equal(0, execute_cmd("!vim &"));
	assert_int_equal(1, cmdi.argc);
	assert_string_equal("vim", arg);
	assert_int_equal(1, cmdi.bg);
}

static void
test_no_args_after_qmark(void)
{
	assert_int_equal(0, execute_cmd("filter?"));
	assert_int_equal(0, execute_cmd("filter?      "));
	assert_int_equal(CMDS_ERR_TRAILING_CHARS, execute_cmd("filter? some_thing"));
}

static void
test_no_space_before_e_and_q_marks(void)
{
	assert_int_equal(0, execute_cmd("filter?"));
	assert_int_equal(cmdi.argc, 0);
	assert_true(cmdi.qmark);

	assert_int_equal(0, execute_cmd("filter ?"));
	assert_int_equal(cmdi.argc, 1);
	assert_false(cmdi.qmark);
}

static void
test_only_one_mark(void)
{
	assert_int_equal(0, execute_cmd("filter?"));
	assert_int_equal(cmdi.argc, 0);
	assert_true(cmdi.qmark);

	assert_int_equal(0, execute_cmd("filter!"));
	assert_int_equal(cmdi.argc, 0);
	assert_true(cmdi.emark);

	assert_int_equal(0, execute_cmd("filter!!"));
	assert_int_equal(cmdi.argc, 1);
	assert_true(cmdi.emark);

	assert_int_equal(CMDS_ERR_TRAILING_CHARS, execute_cmd("filter?!"));
	assert_int_equal(CMDS_ERR_TRAILING_CHARS, execute_cmd("filter??"));

	assert_int_equal(0, execute_cmd("filter!?"));
	assert_int_equal(cmdi.argc, 1);
	assert_true(cmdi.emark);
	assert_false(cmdi.qmark);
}

void
input_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_trimming);
	run_test(test_range_acceptance);
	run_test(test_range);
	run_test(test_range_and_spaces);
	run_test(test_bang_acceptance);
	run_test(test_bang);
	run_test(test_qmark_acceptance);
	run_test(test_qmark);
	run_test(test_args);
	run_test(test_count);
	run_test(test_end_characters);
	run_test(test_custom_separator);
	run_test(test_custom_separator_and_arg_format);
	run_test(test_regexp_flag);
	run_test(test_backgrounding);
	run_test(test_no_args_after_qmark);
	run_test(test_no_space_before_e_and_q_marks);
	run_test(test_only_one_mark);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */