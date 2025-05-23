/* Copyright (c) 2009-2018 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "istream.h"
#include "imap-parser.h"
#include "test-common.h"

static void test_imap_parser_crlf(void)
{
	static const char *test_input = "foo\r\nx\ry\n";
	struct istream *input;
	struct imap_parser *parser;
	const struct imap_arg *args;
	unsigned int i;
	enum imap_parser_error parse_error;

	test_begin("imap parser crlf handling");
	input = test_istream_create(test_input);
	parser = imap_parser_create(input, NULL, 1024);

	/* must return -2 until LF is read */
	for (i = 0; test_input[i] != '\n'; i++) {
		test_istream_set_size(input, i+1);
		(void)i_stream_read(input);
		test_assert(imap_parser_read_args(parser, 0, 0, &args) == -2);
	}
	test_istream_set_size(input, i+1);
	(void)i_stream_read(input);
	test_assert(imap_parser_read_args(parser, 0, 0, &args) == 1);
	test_assert(args[0].type == IMAP_ARG_ATOM);
	test_assert(args[1].type == IMAP_ARG_EOL);

	/* CR without LF should fail with error */
	imap_parser_reset(parser);
	i_stream_seek(input, ++i);
	test_istream_set_size(input, ++i);
	(void)i_stream_read(input);
	test_assert(imap_parser_read_args(parser, 0, 0, &args) == -2);
	test_istream_set_size(input, ++i);
	(void)i_stream_read(input);
	test_assert(imap_parser_read_args(parser, 0, 0, &args) == -2);
	test_istream_set_size(input, ++i);
	(void)i_stream_read(input);
	test_assert(imap_parser_read_args(parser, 0, 0, &args) == -1);
	test_assert(strcmp(imap_parser_get_error
		(parser, &parse_error), "CR sent without LF") == 0 &&
		parse_error == IMAP_PARSE_ERROR_BAD_SYNTAX);

	imap_parser_unref(&parser);
	i_stream_destroy(&input);
	test_end();
}

static void test_imap_parser_partial_list(void)
{
	static const char *test_input = "((((foo {1000000}\r\n";
	struct istream *input;
	struct imap_parser *parser;
	const struct imap_arg *args, *sub_list;

	test_begin("imap parser partial list");
	input = test_istream_create(test_input);
	parser = imap_parser_create(input, NULL, 1024);

	(void)i_stream_read(input);
	test_assert(imap_parser_read_args(parser, 0,
		IMAP_PARSE_FLAG_LITERAL_SIZE, &args) == 1);
	for (unsigned int i = 0; i < 4; i++) {
		sub_list = imap_arg_as_list(&args[0]);
		test_assert(IMAP_ARG_IS_EOL(&args[1]));
		args = sub_list;
	}
	test_assert(imap_arg_atom_equals(&args[0], "foo"));
	test_assert(args[1].type == IMAP_ARG_LITERAL_SIZE);
	test_assert(IMAP_ARG_IS_EOL(&args[2]));

	imap_parser_unref(&parser);
	i_stream_destroy(&input);
	test_end();
}

static void test_imap_parser_read_tag_cmd(void)
{
	enum read_type {
		BOTH,
		TAG,
		COMMAND
	};
	struct {
		const char *input;
		const char *tag;
		int ret;
		enum read_type type;
	} tests[] = {
		{ "tag foo", "tag", 1, BOTH },
		{ "tag\r", "tag", 1, BOTH },
		{ "tag\rfoo", "tag", 1, BOTH },
		{ "tag\nfoo", "tag", 1, BOTH },
		{ "tag\r\nfoo", "tag", 1, BOTH },
		{ "\n", NULL, -1, BOTH },
		{ "tag", NULL, 0, BOTH },
		{ "tag\t", NULL, -1, BOTH },
		{ "tag\x01", NULL, -1, BOTH },
		{ "tag\x1f", NULL, -1, BOTH },
		{ "tag\x7f", NULL, -1, BOTH },
		{ "tag\x80", NULL, -1, BOTH },
		{ "tag\xff", NULL, -1, BOTH },
		{ "tag(", NULL, -1, BOTH },
		{ "tag)", NULL, -1, BOTH },
		{ "tag{", NULL, -1, BOTH },
		{ "tag/ ", "tag/", 1, BOTH },
		{ "tag%", NULL, -1, BOTH },
		{ "tag*", NULL, -1, BOTH },
		{ "tag\"", NULL, -1, BOTH },
		{ "tag\\", NULL, -1, BOTH },
		{ "tag] ", "tag]", 1, TAG },
		{ "tag]", NULL, -1, COMMAND},
		{ "tag+", NULL, -1, TAG },
		{ "tag+ ", "tag+", 1, COMMAND },
	};
	struct istream *input;
	struct imap_parser *parser;
	const char *atom;
	int ret;

	test_begin("imap_parser_read_tag and imap_parser_read_command_name");
	for (unsigned int i = 0; i < N_ELEMENTS(tests); i++) {
		if (tests[i].type != COMMAND) {
			input = test_istream_create(tests[i].input);
			test_assert(i_stream_read(input) > 0);
			parser = imap_parser_create(input, NULL, 1024);
			ret = imap_parser_read_tag(parser, &atom);
			test_assert_idx(ret == tests[i].ret, i);
			test_assert_idx(ret <= 0 || strcmp(tests[i].tag, atom) == 0, i);
			imap_parser_unref(&parser);
			i_stream_destroy(&input);
		}

		if (tests[i].type != TAG) {
			input = test_istream_create(tests[i].input);
			test_assert(i_stream_read(input) > 0);
			parser = imap_parser_create(input, NULL, 1024);
			ret = imap_parser_read_command_name(parser, &atom);
			test_assert_idx(ret == tests[i].ret, i);
			test_assert_idx(ret <= 0 || strcmp(tests[i].tag, atom) == 0, i);
			imap_parser_unref(&parser);
			i_stream_destroy(&input);
		}
	}
	test_end();
}

#define TEST_CASE(str, lit, flags, ret) \
	{ (str), sizeof((str)), (lit), (flags), (ret) }

static void test_imap_parser_read_literal(void)
{
	test_begin("imap_parser_read_literal");

	const struct {
		const char *input;
		size_t len;
		uoff_t lit_len;
		enum imap_parser_flags flags;
		int ret;
	} tests[] = {
		TEST_CASE("{0}\n\n", 0, 0, 1),
		TEST_CASE("{0+}\n\n", 0, 0, 1),
		TEST_CASE("{0}\n", 0, 0, -1),
		TEST_CASE("{-0}\n", 0, 0, -1),
		TEST_CASE("{5}\nHello\n", 5, 0, 1),
		TEST_CASE("{5+}\nHello\n\n", 5, 0, 1),
		TEST_CASE("~{5}\nHello\n", 5, IMAP_PARSE_FLAG_LITERAL8, 1),
		TEST_CASE("~{5+}\nHello\n\n", 5, IMAP_PARSE_FLAG_LITERAL8, 1),
		TEST_CASE("{6+}\nhell\0o\n", 6, 0, 1),
		TEST_CASE("~{6+}\nhell\xc3\xa5\n", 6, IMAP_PARSE_FLAG_LITERAL8, 1),
		TEST_CASE("{6+}\nhello", 0, 0, -2),
		TEST_CASE("{6+}\nhello\n", 0, 0, -1),
	};

	for (size_t i = 0; i < N_ELEMENTS(tests); i++) {
		struct istream *input =
			test_istream_create_data(tests[i].input, tests[i].len);
		test_assert(i_stream_read(input) > 0);
		struct imap_parser *parser =
			imap_parser_create(input, NULL, 1024);
		const struct imap_arg *args;
		int ret = imap_parser_read_args(parser, 0, tests[i].flags, &args);
		if (ret != tests[i].ret) {
			if (tests[i].ret < 0)
				test_failed(t_strdup_printf("%zu: Unexpected success %d", i, ret));
			else
				test_failed(t_strdup_printf("%zu: Unexpected failure %d", i, ret));
		} else if (ret >= 0) {
			test_assert_cmp_idx(args[0].str_len, ==, tests[i].lit_len, i);
		}
		imap_parser_unref(&parser);
		i_stream_destroy(&input);
	}
	test_end();
}

int main(void)
{
	static void (*const test_functions[])(void) = {
		test_imap_parser_crlf,
		test_imap_parser_partial_list,
		test_imap_parser_read_tag_cmd,
		test_imap_parser_read_literal,
		NULL
	};
	return test_run(test_functions);
}
