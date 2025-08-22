// SPDX-License-Identifier: GPL-2.0+
/*
 * Deals with splitting up text output into separate screenfuls
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY LOGC_CONSOLE

#include <env.h>
#include <errno.h>
#include <malloc.h>
#include <pager.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

const char *pager_post(struct pager *pag, bool use_pager, const char *s)
{
	struct membuf old;
	int ret, len;

	if (!pag || !use_pager || pag->test_bypass || pag->state == PAGERST_BYPASS)
		return s;

	len = strlen(s);
	if (!len)
		return NULL;

	old = pag->mb;
	ret = membuf_put(&pag->mb, s, len);
	if (ret == len) {
		/* all is well */
	} else {
		/*
		 * We couldn't store any of the text, so we'll store none of
		 * it. The pager is now in an non-functional state until it
		 * can eject the overflow text.
		 *
		 * The buffer is presumably empty, since callers are not allowed
		 * to call pager_post() unless all the output from the previous
		 * call was provided via pager_next().
		 */
		pag->overflow = s;
		pag->mb = old;
	}

	return pager_next(pag, true, 0);
}

const char *pager_next(struct pager *pag, bool use_pager, int key)
{
	char *str, *p, *end;
	int ret;

	if (!use_pager || (pag && pag->test_bypass))
		return NULL;

	/* replace the real character we overwrite with nul, if needed */
	if (pag->nulch) {
		*pag->nulch = pag->oldch;
		pag->nulch = NULL;
	}

	/* if we're at the limit, wait */
	switch (pag->state) {
	case PAGERST_OK:
		break;
	case PAGERST_AT_LIMIT:
		pag->state = PAGERST_WAIT_USER;
		return PAGER_PROMPT;
	case PAGERST_WAIT_USER:
		if (key != ' ')
			return PAGER_WAITING;
		pag->state = PAGERST_CLEAR_PROMPT;
		return PAGER_BLANK;
	case PAGERST_CLEAR_PROMPT:
		pag->state = PAGERST_OK;
		break;
	case PAGERST_BYPASS:
		return NULL;
	}

	ret = membuf_getraw(&pag->mb, pag->buf.size - 1, false, &str);
	if (!ret) {
		if (pag->overflow) {
			const char *oflow = pag->overflow;

			pag->overflow = NULL;
			return oflow;
		}
		return NULL;
	}

	/* return lines until we reach the limit */
	for (p = str, end = str + ret; p < end; p++) {
		if (*p == '\n' && ++pag->line_count == pag->page_len - 1) {
			/* remember to display the pager message next time */
			pag->state = PAGERST_AT_LIMIT;
			pag->line_count = 0;

			/* skip the newline, since our prompt has one */
			p++;
			break;
		}
	}

	/* remove the used bytes from the membuf */
	ret = membuf_getraw(&pag->mb, p - str, true, &str);

	/* don't output the newline, since our prompt has one */
	if (pag->state == PAGERST_AT_LIMIT)
		p--;

	/* terminate the string */
	pag->nulch = p;
	pag->oldch = *pag->nulch;
	*pag->nulch = '\0';

	return str;
}

bool pager_set_bypass(struct pager *pag, bool bypass)
{
	bool was_bypassed = false;

	if (!pag)
		return false;

	was_bypassed = pag->state == PAGERST_BYPASS;
	if (bypass)
		pag->state = PAGERST_BYPASS;
	else
		pag->state = PAGERST_OK;

	return was_bypassed;
}

bool pager_set_test_bypass(struct pager *pag, bool bypass)
{
	bool was_bypassed = false;

	if (!pag)
		return false;

	was_bypassed = pag->test_bypass;
	pag->test_bypass = bypass;

	return was_bypassed;
}

void pager_set_page_len(struct pager *pag, int page_len)
{
	if (page_len < 2)
		return;
	pag->page_len = page_len;
	pag->line_count = 0;
}

void pager_reset(struct pager *pag)
{
	pag->line_count = 0;
}

static int on_pager(const char *name, const char *value, enum env_op op,
		    int flags)
{
	struct pager *pag = gd_pager();
	int new_page_len;

	if (!IS_ENABLED(CONFIG_CONSOLE_PAGER) || !pag)
		return 0;

	switch (op) {
	case env_op_create:
	case env_op_overwrite:
		if (value) {
			new_page_len = simple_strtoul(value, NULL, 16);
			pager_set_page_len(pag, new_page_len);
		}
		break;
	case env_op_delete:
		/* Reset to default when deleted */
		pager_set_page_len(pag, CONFIG_CONSOLE_PAGER_LINES);
		break;
	}

	return 0;
}
U_BOOT_ENV_CALLBACK(pager, on_pager);

void pager_uninit(struct pager *pag)
{
	abuf_uninit(&pag->buf);
	free(pag);
}

int pager_init(struct pager **pagp, int page_len, int buf_size)
{
	struct pager *pag;

	pag = malloc(sizeof(struct pager));
	if (!pag)
		return log_msg_ret("pag", -ENOMEM);
	memset(pag, '\0', sizeof(struct pager));
	pag->page_len = page_len;
	if (!abuf_init_size(&pag->buf, buf_size))
		return log_msg_ret("pah", -ENOMEM);

	/*
	 * nul-terminate the buffer, which will come in handy if we need to
	 * return up to the last byte
	 */
	((char *)pag->buf.data)[buf_size - 1] = '\0';
	membuf_init(&pag->mb, pag->buf.data, buf_size);
	*pagp = pag;

	return 0;
}
