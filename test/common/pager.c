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
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

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
	result = pager_post(pag, true, text);
	ut_assertnonnull(result);
	ut_asserteq_str(text, result);

	/* Should be no more text */
	result = pager_next(pag, true, 0);
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

	ut_assertok(pager_init(&pag, 20, 1024));

	/* Post multiple pieces of text */
	result = pager_post(pag, true, text1);
	ut_assertnonnull(result);
	ut_asserteq_str(text1, result);

	/* Should be no more text after first post */
	result = pager_next(pag, true, 0);
	ut_assertnull(result);

	result = pager_post(pag, true, text2);
	ut_assertnonnull(result);
	ut_asserteq_str(text2, result);

	/* Should be no more text after second post */
	result = pager_next(pag, true, 0);
	ut_assertnull(result);

	result = pager_post(pag, true, text3);
	ut_assertnonnull(result);
	ut_asserteq_str(text3, result);

	/* Should be no more text after third post */
	result = pager_next(pag, true, 0);
	ut_assertnull(result);

	pager_uninit(pag);

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
	result = pager_post(pag, true, "this is 16 chars");
	ut_assertnonnull(result);
	ut_asserteq_str("this is 16 chars", result);
	ut_assertnull(pager_next(pag, true, 0));

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
	result = pager_post(pag, true, "test1");
	ut_assertnonnull(result);

	/* overflow handling should return the text */
	ut_asserteq_str("test1", result);
	ut_assertnull(pager_next(pag, true, 0));

	pager_uninit(pag);

	return 0;
}
COMMON_TEST(pager_test_overflow, 0);

/* Test pager with NULL input */
static int pager_test_null_input(struct unit_test_state *uts)
{
	const char *result;

	/* Test pager_post with NULL pager */
	result = pager_post(NULL, true, "test");
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
	result = pager_post(pag, true, "");
	ut_assertnull(result);

	/* Should be no more text */
	result = pager_next(pag, true, 0);
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
	result = pager_post(pag, true, text);
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
	int i;

	ut_assertok(pager_init(&pag, 20, 1024));

	/* Create a very long line without newlines */
	for (i = 0; i < sizeof(long_line) - 1; i++)
		long_line[i] = 'X';
	long_line[sizeof(long_line) - 1] = '\0';

	/* Post the long line */
	result = pager_post(pag, true, long_line);
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
	result = pager_post(pag, true, multiline_text);
	ut_assertnonnull(result);

	/* Should get first 3 lines (excluding the 3rd newline) */
	ut_asserteq_str("Line 1\nLine 2\nLine 3", result);
	/* line_count is reset to 0 when page limit is reached */
	ut_asserteq(0, pag->line_count);

	/* Next call should return pager prompt */
	result = pager_next(pag, true, 0);
	ut_assertnonnull(result);
	ut_asserteq_str("\n: Press SPACE to continue", result);

	/* Press space to continue */
	result = pager_next(pag, true, ' ');
	ut_assertnonnull(result);
	ut_asserteq_str("\r                         \r", result);

	/* Get remaining lines */
	result = pager_next(pag, true, 0);
	ut_assertnonnull(result);
	ut_asserteq_str("Line 4\nLine 5\n", result);

	/* Should be no more text */
	result = pager_next(pag, true, 0);
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
	result = pager_post(pag, true, "Line 1\nLine 2\n");
	ut_assertnonnull(result);
	ut_asserteq_str("Line 1\nLine 2", result);

	/* Next call should return the prompt */
	result = pager_next(pag, true, 0);
	ut_assertnonnull(result);
	ut_asserteq_str("\n: Press SPACE to continue", result);

	/* Next call without space key should return PAGER_WAITING */
	result = pager_next(pag, true, 0);
	ut_asserteq_ptr(PAGER_WAITING, result);

	/* Another call without space should still return PAGER_WAITING */
	result = pager_next(pag, true, 'x');  /* Wrong key */
	ut_asserteq_ptr(PAGER_WAITING, result);

	/* Pressing space should clear the prompt */
	result = pager_next(pag, true, ' ');
	ut_assertnonnull(result);
	ut_asserteq_str("\r                         \r", result);

	/* Now should return NULL (no more content) */
	result = pager_next(pag, true, 0);
	ut_assertnull(result);

	pager_uninit(pag);

	return 0;
}
COMMON_TEST(pager_test_pager_waiting, 0);

/* Test use_pager parameter - output text directly, while buffer is non-empty */
static int pager_test_use_pager_param(struct unit_test_state *uts)
{
	struct pager *pag;
	const char *buffered_text = "Line 1\nLine 2\nLine 3\nLine 4\nLine 5\n";
	const char *direct_text = "This should be written immediately";
	const char *result;

	/* Init with small page length to ensure paging occurs */
	ut_assertok(pager_init(&pag, 3, 1024));

	/* Post text with use_pager=true - should trigger paging */
	result = pager_post(pag, true, buffered_text);
	ut_assertnonnull(result);
	/* Should get first 2 lines */
	ut_asserteq_str("Line 1\nLine 2", result);

	/* Now call pager_post with use_pager=false while text is still buffered */
	result = pager_post(pag, false, direct_text);
	/* Should get the text immediately, not from buffer */
	ut_asserteq_ptr(direct_text, result);

	/* Call pager_next with use_pager=false - should return NULL */
	result = pager_next(pag, false, 0);
	ut_assertnull(result);

	/* Now continue with use_pager=true to get buffered text */
	result = pager_next(pag, true, 0);
	ut_assertnonnull(result);
	/* Should get the pager prompt */
	ut_asserteq_str("\n: Press SPACE to continue", result);

	/* Press space to continue */
	result = pager_next(pag, true, ' ');
	ut_assertnonnull(result);
	ut_asserteq_str("\r                         \r", result);

	/* Get remaining buffered lines - should be next 2 lines due to page limit */
	result = pager_next(pag, true, 0);
	ut_assertnonnull(result);
	ut_asserteq_str("Line 3\nLine 4", result);

	/* Should get pager prompt again */
	result = pager_next(pag, true, 0);
	ut_assertnonnull(result);
	ut_asserteq_str("\n: Press SPACE to continue", result);

	/* Press space to continue */
	result = pager_next(pag, true, ' ');
	ut_assertnonnull(result);
	ut_asserteq_str("\r                         \r", result);

	/* Get final line */
	result = pager_next(pag, true, 0);
	ut_assertnonnull(result);
	ut_asserteq_str("Line 5\n", result);

	/* Should be no more text */
	result = pager_next(pag, true, 0);
	ut_assertnull(result);

	pager_uninit(pag);

	return 0;
}
COMMON_TEST(pager_test_use_pager_param, 0);

/* Test pager bypass mode */
static int pager_test_bypass_mode(struct unit_test_state *uts)
{
	struct pager *pag;
	const char *text = "This text should be returned directly";
	const char *result;

	/* Init with small page length to ensure paging would normally occur */
	ut_assertok(pager_init(&pag, 2, 1024));

	/* Enable bypass mode */
	pager_set_bypass(pag, true);

	/* Post text - should get original string back directly */
	result = pager_post(pag, true, text);
	ut_asserteq_ptr(text, result); /* Should be same pointer */

	/* pager_next should return NULL in bypass mode */
	result = pager_next(pag, true, 0);
	ut_assertnull(result);

	/* Disable bypass mode */
	pager_set_bypass(pag, false);

	/* Now pager should work normally */
	result = pager_post(pag, true, text);
	ut_assertnonnull(result);
	/* In normal mode, result should be different from original text */
	ut_assert(result != text);

	pager_uninit(pag);

	return 0;
}
COMMON_TEST(pager_test_bypass_mode, 0);

/* Test that single character output via putc goes through pager */
static int pager_test_putc(struct unit_test_state *uts)
{
	struct pager *pag;
	const char *result;

	/* Init pager */
	ut_assertok(pager_init(&pag, 20, 1024));
	pager_set_bypass(pag, true);

	/*
	 * Test that individual characters can be posted via pager API
	 * This verifies that console_putc_pager() routes through the pager
	 * system
	 */
	result = pager_post(pag, true, "A");
	ut_asserteq_ptr("A", result); /* Bypass mode returns original pointer */

	result = pager_post(pag, true, "\n");
	ut_asserteq_ptr("\n", result);

	result = pager_post(pag, true, "B");
	ut_asserteq_ptr("B", result);

	/* Disable bypass to test normal functionality with single chars */
	pager_set_bypass(pag, false);

	result = pager_post(pag, true, "X");
	ut_assertnonnull(result);
	ut_asserteq_str("X", result);

	result = pager_next(pag, true, 0);
	ut_assertnull(result);

	pager_uninit(pag);

	return 0;
}
COMMON_TEST(pager_test_putc, 0);

/* Test writing up to page limit then adding final newline */
static int pager_test_limit_plus_newline(struct unit_test_state *uts)
{
	struct pager *pag;
	const char *result;

	/* Init with page length of 3 lines */
	ut_assertok(pager_init(&pag, 3, 1024));

	/* Write text that reaches exactly the page limit (2 newlines) */
	result = pager_post(pag, true, "Line 1\nLine 2");
	ut_assertnonnull(result);
	ut_asserteq_str("Line 1\nLine 2", result);
	ut_asserteq(1, pag->line_count); /* Should have 1 line counted */

	/* Should be no more text yet - haven't hit limit */
	result = pager_next(pag, true, 0);
	ut_assertnull(result);

	/* Now post a single newline - this should trigger the page limit */
	result = pager_post(pag, true, "\n");
	ut_assertnonnull(result);
	/*
	 * Should get empty string since we hit the limit and the newline is
	 * consumed
	 */
	ut_asserteq_str("", result);

	/* Next call should return the pager prompt since we hit the limit */
	result = pager_next(pag, true, 0);
	ut_assertnonnull(result);
	ut_asserteq_str("\n: Press SPACE to continue", result);

	/* Press space to continue */
	result = pager_next(pag, true, ' ');
	ut_assertnonnull(result);
	ut_asserteq_str("\r                         \r", result);

	/* Should be no more text */
	result = pager_next(pag, true, 0);
	ut_assertnull(result);

	pager_uninit(pag);

	return 0;
}
COMMON_TEST(pager_test_limit_plus_newline, 0);

/* Test console integration - pager prompt appears in console output */
static int pager_test_console(struct unit_test_state *uts)
{
	struct pager *pag, *orig_pag;
	char line[100];
	int avail, ret;

	/* Save original pager */
	orig_pag = gd_pager();

	/* Create our own pager for testing */
	ret = pager_init(&pag, 2, 1024);
	if (ret) {
		gd->pager = orig_pag;
		return CMD_RET_FAILURE;
	}

	/* Set up pager to be one away from limit (1 line already counted) */
	pag->line_count = 1;

	/* Assign our pager to the global data */
	gd->pager = pag;

	/* Trigger paging with a second newline */
	putc('\n');

	/* Check if there's any console output available at all */
	avail = console_record_avail();

	/* Restore original pager first */
	gd->pager = orig_pag;
	pager_uninit(pag);

	/* Now check what we got */
	if (!avail) {
		ut_reportf("No console output was recorded at all");
		return CMD_RET_FAILURE;
	}

	/* Try to read the actual output */
	ret = console_record_readline(line, sizeof(line));
	if (ret < 0) {
		ut_reportf("Failed to read first line, avail was %d", avail);
		return CMD_RET_FAILURE;
	}

	/*
	 * console recording does not see the pager prompt, so we should have
	 * just got a newline
	 */
	ut_asserteq_str("", line);

	ut_assert_console_end();

	return 0;
}
COMMON_TEST(pager_test_console, UTF_CONSOLE);
