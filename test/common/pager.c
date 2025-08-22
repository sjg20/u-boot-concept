// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 *
 * Test for pager functionality
 */

#include <errno.h>
#include <malloc.h>
#include <pager.h>
#include <string.h>
#include <test/common.h>
#include <test/test.h>
#include <test/ut.h>

/* Test basic pager init and cleanup */
static int pager_test_basic_init(struct unit_test_state *uts)
{
	struct pager *pag;

	/* Test successful init */
	ut_assertok(pager_init(&pag, 20, 1024));
	ut_assertnonnull(pag);
	ut_asserteq(20, pag->page_len);
	ut_asserteq(0, pag->line_count);
	ut_assertnull(pag->overflow);
	ut_assertnull(pag->nulch);

	/* Clean up */
	pager_uninit(pag);

	/* Test init with different parameters */
	ut_assertok(pager_init(&pag, 10, 2048));
	ut_assertnonnull(pag);
	ut_asserteq(10, pag->page_len);

	pager_uninit(pag);

	return 0;
}
COMMON_TEST(pager_test_basic_init, 0);

/* Test pager with simple text */
static int pager_test_simple_text(struct unit_test_state *uts)
{
	struct pager *pag;
	const char *text = "Hello, World!";
	const char *result;

	ut_assertok(pager_init(&pag, 20, 1024));

	/* Post some text and get it back */
	result = pager_post(pag, text);
	ut_assertnonnull(result);
	ut_asserteq_str(text, result);

	/* Should be no more text */
	result = pager_next(pag, 0);
	ut_assertnull(result);

	pager_uninit(pag);

	return 0;
}
COMMON_TEST(pager_test_simple_text, 0);

/* Test pager with multiple lines */
static int pager_test_multiline(struct unit_test_state *uts)
{
	struct pager *pag;
	const char *text1 = "Line 1\n";
	const char *text2 = "Line 2\n";
	const char *text3 = "Line 3\n";
	const char *result;
	ulong start;

	start = ut_check_free();

	ut_assertok(pager_init(&pag, 20, 1024));

	/* Post multiple pieces of text */
	result = pager_post(pag, text1);
	ut_assertnonnull(result);
	ut_asserteq_str(text1, result);

	/* Should be no more text after first post */
	result = pager_next(pag, 0);
	ut_assertnull(result);

	result = pager_post(pag, text2);
	ut_assertnonnull(result);
	ut_asserteq_str(text2, result);

	/* Should be no more text after second post */
	result = pager_next(pag, 0);
	ut_assertnull(result);

	result = pager_post(pag, text3);
	ut_assertnonnull(result);
	ut_asserteq_str(text3, result);

	/* Should be no more text after third post */
	result = pager_next(pag, 0);
	ut_assertnull(result);

	pager_uninit(pag);

	/* Check for memory leaks */
	ut_assertok(ut_check_delta(start));

	return 0;
}
COMMON_TEST(pager_test_multiline, 0);

/* Test pager with large text that fills the buffer */
static int pager_test_large_text(struct unit_test_state *uts)
{
	struct pager *pag;
	const char *result;

	ut_assertok(pager_init(&pag, 20, 16)); /* Small buffer */

	/* Post large text - should fit in buffer */
	result = pager_post(pag, "this is 16 chars");
	ut_assertnonnull(result);
	ut_asserteq_str("this is 16 chars", result);
	ut_assertnull(pager_next(pag, 0));

	pager_uninit(pag);

	return 0;
}
COMMON_TEST(pager_test_large_text, 0);

/* Test pager overflow handling */
static int pager_test_overflow(struct unit_test_state *uts)
{
	struct pager *pag;
	const char *result;

	ut_assertok(pager_init(&pag, 20, 4)); /* Small buffer */

	/* send some text which is too long for the buffer */
	result = pager_post(pag, "test1");
	ut_assertnonnull(result);

	/* overflow handling should return the text */
	ut_asserteq_str("test1", result);
	ut_assertnull(pager_next(pag, 0));

	pager_uninit(pag);

	return 0;
}
COMMON_TEST(pager_test_overflow, 0);

/* Test pager with NULL input */
static int pager_test_null_input(struct unit_test_state *uts)
{
	const char *result;

	/* Test pager_post with NULL pager */
	result = pager_post(NULL, "test");
	ut_asserteq_str("test", result);

	return 0;
}
COMMON_TEST(pager_test_null_input, 0);

/* Test pager with empty strings */
static int pager_test_empty_strings(struct unit_test_state *uts)
{
	struct pager *pag;
	const char *result;

	ut_assertok(pager_init(&pag, 20, 1024));

	/* Post empty string */
	result = pager_post(pag, "");
	ut_assertnull(result);

	/* Should be no more text */
	result = pager_next(pag, 0);
	ut_assertnull(result);

	pager_uninit(pag);

	return 0;
}
COMMON_TEST(pager_test_empty_strings, 0);

/* Test pager buffer management */
static int pager_test_buffer_management(struct unit_test_state *uts)
{
	struct pager *pag;
	const char *text = "Test buffer management";
	const char *result;

	ut_assertok(pager_init(&pag, 20, 1024));

	/* Verify buffer is properly inited */
	ut_assertnonnull(pag->buf.data);
	ut_asserteq(1024, pag->buf.size);

	/* Post text and verify buffer state */
	result = pager_post(pag, text);
	ut_assertnonnull(result);

	/* Verify the buffer contains our text */
	ut_asserteq_str(text, result);

	pager_uninit(pag);

	return 0;
}
COMMON_TEST(pager_test_buffer_management, 0);

/* Test pager with very long single line */
static int pager_test_long_single_line(struct unit_test_state *uts)
{
	struct pager *pag;
	char long_line[1000];
	const char *result;

	ut_assertok(pager_init(&pag, 20, 1024));

	/* Create a very long line without newlines */
	memset(long_line, 'X', sizeof(long_line) - 1);
	long_line[sizeof(long_line) - 1] = '\0';

	/* Post the long line */
	result = pager_post(pag, long_line);
	ut_assertnonnull(result);

	/* Should get our text back */
	ut_asserteq_str(long_line, result);

	pager_uninit(pag);

	return 0;
}
COMMON_TEST(pager_test_long_single_line, 0);

/* Test pager line counting and page breaks */
static int pager_test_line_counting(struct unit_test_state *uts)
{
	struct pager *pag;
	const char *multiline_text = "Line 1\nLine 2\nLine 3\nLine 4\nLine 5\n";
	const char *result;

	/* Init with page length of 4 lines */
	ut_assertok(pager_init(&pag, 4, 1024));

	/* Post multiline text */
	result = pager_post(pag, multiline_text);
	ut_assertnonnull(result);

	/* Should get first 3 lines (excluding the 3rd newline) */
	ut_asserteq_str("Line 1\nLine 2\nLine 3", result);
	/* line_count is reset to 0 when page limit is reached */
	ut_asserteq(0, pag->line_count);

	/* Next call should return pager prompt */
	result = pager_next(pag, 0);
	ut_assertnonnull(result);
	ut_asserteq_str(PAGER_PROMPT, result);

	/* Press space to continue */
	result = pager_next(pag, ' ');
	ut_assertnonnull(result);
	ut_asserteq_str(PAGER_BLANK, result);

	/* Get remaining lines */
	result = pager_next(pag, 0);
	ut_assertnonnull(result);
	ut_asserteq_str("Line 4\nLine 5\n", result);

	/* Should be no more text */
	result = pager_next(pag, 0);
	ut_assertnull(result);

	pager_uninit(pag);

	return 0;
}
COMMON_TEST(pager_test_line_counting, 0);

/* Test that PAGER_WAITING is returned when pager waits for user input */
static int pager_test_pager_waiting(struct unit_test_state *uts)
{
	struct pager *pag;
	const char *result;

	/* Create pager with small page size to trigger waiting quickly */
	ut_assertok(pager_init(&pag, 3, 1024));

	/* Post text that fills exactly the page limit */
	result = pager_post(pag, "Line 1\nLine 2\n");
	ut_assertnonnull(result);
	ut_asserteq_str("Line 1\nLine 2", result);

	/* Next call should return the prompt */
	result = pager_next(pag, 0);
	ut_assertnonnull(result);
	ut_asserteq_str(PAGER_PROMPT, result);

	/* Next call without space key should return PAGER_WAITING */
	result = pager_next(pag, 0);
	ut_asserteq_ptr(PAGER_WAITING, result);

	/* Another call without space should still return PAGER_WAITING */
	result = pager_next(pag, 'x');  /* Wrong key */
	ut_asserteq_ptr(PAGER_WAITING, result);

	/* Pressing space should clear the prompt */
	result = pager_next(pag, ' ');
	ut_assertnonnull(result);
	ut_asserteq_str("\r                         \r", result);

	/* Now should return NULL (no more content) */
	result = pager_next(pag, 0);
	ut_assertnull(result);

	pager_uninit(pag);

	return 0;
}
COMMON_TEST(pager_test_pager_waiting, 0);
