// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2010-2011 Calxeda, Inc.
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 */

#define LOG_CATEGORY	LOGC_BOOT

#include <ctype.h>
#include <malloc.h>
#include <mapmem.h>
#include "pxe_utils.h"

/** enum token_type - Tokens for the pxe file parser */
enum token_type {
	T_EOL,
	T_STRING,
	T_EOF,
	T_MENU,
	T_TITLE,
	T_TIMEOUT,
	T_LABEL,
	T_KERNEL,
	T_LINUX,
	T_APPEND,
	T_INITRD,
	T_LOCALBOOT,
	T_DEFAULT,
	T_PROMPT,
	T_INCLUDE,
	T_FDT,
	T_FDTDIR,
	T_FDTOVERLAYS,
	T_ONTIMEOUT,
	T_IPAPPEND,
	T_BACKGROUND,
	T_KASLRSEED,
	T_FALLBACK,
	T_SAY,
	T_INVALID
};

/** struct token - token - given by a value and a type */
struct token {
	char *val;
	enum token_type type;
};

/* Keywords recognized */
static const struct token keywords[] = {
	{"menu", T_MENU},
	{"title", T_TITLE},
	{"timeout", T_TIMEOUT},
	{"default", T_DEFAULT},
	{"prompt", T_PROMPT},
	{"label", T_LABEL},
	{"kernel", T_KERNEL},
	{"linux", T_LINUX},
	{"localboot", T_LOCALBOOT},
	{"append", T_APPEND},
	{"initrd", T_INITRD},
	{"include", T_INCLUDE},
	{"devicetree", T_FDT},
	{"fdt", T_FDT},
	{"devicetreedir", T_FDTDIR},
	{"fdtdir", T_FDTDIR},
	{"fdtoverlays", T_FDTOVERLAYS},
	{"devicetree-overlay", T_FDTOVERLAYS},
	{"ontimeout", T_ONTIMEOUT,},
	{"ipappend", T_IPAPPEND,},
	{"background", T_BACKGROUND,},
	{"kaslrseed", T_KASLRSEED,},
	{"fallback", T_FALLBACK,},
	{"say", T_SAY,},
	{NULL, T_INVALID}
};

/**
 * enum lex_state - lexer state
 *
 * Since pxe(linux) files don't have a token to identify the start of a
 * literal, we have to keep track of when we're in a state where a literal is
 * expected vs when we're in a state a keyword is expected.
 */
enum lex_state {
	L_NORMAL = 0,
	L_KEYWORD,
	L_SLITERAL
};

/**
 * label_create() - crate a new PXE label
 *
 * Allocates memory for and initializes a pxe_label. This uses malloc, so the
 * result must be free()'d to reclaim the memory.
 *
 * Returns a pointer to the label, or NULL if out of memory
 */
static struct pxe_label *label_create(void)
{
	struct pxe_label *label;

	label = malloc(sizeof(struct pxe_label));
	if (!label)
		return NULL;
	memset(label, 0, sizeof(struct pxe_label));

	return label;
}

void label_destroy(struct pxe_label *label)
{
	free(label->name);
	free(label->kernel_label);
	free(label->kernel);
	free(label->config);
	free(label->append);
	free(label->initrd);
	free(label->fdt);
	free(label->fdtdir);
	free(label->fdtoverlays);
	free(label);
}

/**
 * get_string() - retrieves a string from *p and stores it as a token in *t.
 *
 * This is used for scanning both string literals and keywords.
 *
 * Characters from *p are copied into t-val until a character equal to
 * delim is found, or a NUL byte is reached. If delim has the special value of
 * ' ', any whitespace character will be used as a delimiter.
 *
 * If lower is unequal to 0, uppercase characters will be converted to
 * lowercase in the result. This is useful to make keywords case
 * insensitive.
 *
 * The location of *p is updated to point to the first character after the end
 * of the token - the ending delimiter.
 *
 * Memory for t->val is allocated using malloc and must be free()'d to reclaim
 * it.
 *
 * @p: Points to a pointer to the current position in the input being processed.
 *	Updated to point at the first character after the current token
 * @t: Pointers to a token to fill in
 * @delim: Delimiter character to look for, either newline or space
 * @lower: true to convert the string to lower case when storing
 * Returns the new value of t->val, on success, NULL if out of memory
 */
static char *get_string(char **p, struct token *t, char delim, int lower)
{
	char *b, *e;
	size_t len, i;

	/*
	 * b and e both start at the beginning of the input stream.
	 *
	 * e is incremented until we find the ending delimiter, or a NUL byte
	 * is reached. Then, we take e - b to find the length of the token.
	 */
	b = *p;
	e = *p;
	while (*e) {
		if ((delim == ' ' && isspace(*e)) || delim == *e)
			break;
		e++;
	}
	len = e - b;

	/*
	 * Allocate memory to hold the string, and copy it in, converting
	 * characters to lowercase if lower is != 0.
	 */
	t->val = malloc(len + 1);
	if (!t->val)
		return NULL;

	for (i = 0; i < len; i++, b++) {
		if (lower)
			t->val[i] = tolower(*b);
		else
			t->val[i] = *b;
	}

	t->val[len] = '\0';

	/* Update *p so the caller knows where to continue scanning */
	*p = e;
	t->type = T_STRING;

	return t->val;
}

/**
 * get_keyword() - Populate a keyword token with a type and value
 *
 * Updates the ->type field based on the keyword string in @val
 * @t: Token to populate
 */
static void get_keyword(struct token *t)
{
	int i;

	for (i = 0; keywords[i].val; i++) {
		if (!strcmp(t->val, keywords[i].val)) {
			t->type = keywords[i].type;
			break;
		}
	}
}

/**
 * get_token() - Get the next token
 *
 * We have to keep track of which state we're in to know if we're looking to get
 * a string literal or a keyword.
 *
 * @p: Points to a pointer to the current position in the input being processed.
 *	Updated to point at the first character after the current token
 */
static void get_token(char **p, struct token *t, enum lex_state state)
{
	char *c = *p;

	t->type = T_INVALID;

	/* eat non EOL whitespace */
	while (isblank(*c))
		c++;

	/*
	 * eat comments. note that string literals can't begin with #, but
	 * can contain a # after their first character.
	 */
	if (*c == '#') {
		while (*c && *c != '\n')
			c++;
	}

	if (*c == '\n') {
		t->type = T_EOL;
		c++;
	} else if (*c == '\0') {
		t->type = T_EOF;
		c++;
	} else if (state == L_SLITERAL) {
		get_string(&c, t, '\n', 0);
	} else if (state == L_KEYWORD) {
		/*
		 * when we expect a keyword, we first get the next string
		 * token delimited by whitespace, and then check if it
		 * matches a keyword in our keyword list. if it does, it's
		 * converted to a keyword token of the appropriate type, and
		 * if not, it remains a string token.
		 */
		get_string(&c, t, ' ', 1);
		get_keyword(t);
	}

	*p = c;
}

/**
 * eol_or_eof() - Find end of line
 *
 * Increment *c until we get to the end of the current line, or EOF
 *
 * @c: Points to a pointer to the current position in the input being processed.
 *	Updated to point at the first character after the current token
 */
static void eol_or_eof(char **c)
{
	while (**c && **c != '\n')
		(*c)++;
}

/*
 * All of these parse_* functions share some common behavior.
 *
 * They finish with *c pointing after the token they parse, and return 1 on
 * success, or < 0 on error.
 */

/*
 * Parse a string literal and store a pointer it at *dst. String literals
 * terminate at the end of the line.
 */
static int parse_sliteral(char **c, char **dst)
{
	struct token t;
	char *s = *c;

	get_token(c, &t, L_SLITERAL);
	if (t.type != T_STRING) {
		printf("Expected string literal: %.*s\n", (int)(*c - s), s);
		return -EINVAL;
	}

	*dst = t.val;

	return 1;
}

/*
 * Parse a base 10 (unsigned) integer and store it at *dst.
 */
static int parse_integer(char **c, int *dst)
{
	struct token t;
	char *s = *c;

	get_token(c, &t, L_SLITERAL);
	if (t.type != T_STRING) {
		printf("Expected string: %.*s\n", (int)(*c - s), s);
		return -EINVAL;
	}

	*dst = simple_strtol(t.val, NULL, 10);
	free(t.val);

	return 1;
}

/*
 * Parse an include statement, and retrieve and parse the file it mentions.
 *
 * base should point to a location where it's safe to store the file, and
 * nest_level should indicate how many nested includes have occurred. For this
 * include, nest_level has already been incremented and doesn't need to be
 * incremented here.
 */
static int handle_include(struct pxe_context *ctx, char **c, unsigned long base,
			  struct pxe_menu *cfg, int nest_level)
{
	char *include_path;
	char *s = *c;
	int err;
	char *buf;
	int ret;

	err = parse_sliteral(c, &include_path);
	if (err < 0) {
		printf("Expected include path: %.*s\n", (int)(*c - s), s);
		return err;
	}

	err = get_pxe_file(ctx, include_path, base);
	if (err < 0) {
		printf("Couldn't retrieve %s\n", include_path);
		return err;
	}

	buf = map_sysmem(base, 0);
	ret = parse_pxefile_top(ctx, buf, base, cfg, nest_level);
	unmap_sysmem(buf);

	return ret;
}

/*
 * Parse lines that begin with 'menu'.
 *
 * base and nest are provided to handle the 'menu include' case.
 *
 * base should point to a location where it's safe to store the included file.
 *
 * nest_level should be 1 when parsing the top level pxe file, 2 when parsing
 * a file it includes, 3 when parsing a file included by that file, and so on.
 */
static int parse_menu(struct pxe_context *ctx, char **c, struct pxe_menu *cfg,
		      unsigned long base, int nest_level)
{
	struct token t;
	char *s = *c;
	int err = 0;

	get_token(c, &t, L_KEYWORD);

	switch (t.type) {
	case T_TITLE:
		err = parse_sliteral(c, &cfg->title);
		break;
	case T_INCLUDE:
		err = handle_include(ctx, c, base, cfg, nest_level + 1);
		break;
	case T_BACKGROUND:
		err = parse_sliteral(c, &cfg->bmp);
		break;
	default:
		printf("Ignoring malformed menu command: %.*s\n",
		       (int)(*c - s), s);
	}
	if (err < 0)
		return err;

	eol_or_eof(c);

	return 1;
}

/*
 * Handles parsing a 'menu line' when we're parsing a label.
 */
static int parse_label_menu(char **c, struct pxe_menu *cfg,
			    struct pxe_label *label)
{
	struct token t;
	char *s;

	s = *c;
	get_token(c, &t, L_KEYWORD);

	switch (t.type) {
	case T_DEFAULT:
		if (!cfg->default_label)
			cfg->default_label = strdup(label->name);

		if (!cfg->default_label)
			return -ENOMEM;

		break;
	case T_LABEL:
		parse_sliteral(c, &label->menu);
		break;
	default:
		printf("Ignoring malformed menu command: %.*s\n",
		       (int)(*c - s), s);
	}

	eol_or_eof(c);

	return 0;
}

/*
 * Handles parsing a 'kernel' label.
 * expecting "filename" or "<fit_filename>#cfg"
 */
static int parse_label_kernel(char **c, struct pxe_label *label)
{
	char *s;
	int err;

	err = parse_sliteral(c, &label->kernel);
	if (err < 0)
		return err;

	/* copy the kernel label to compare with FDT / INITRD when FIT is used */
	label->kernel_label = strdup(label->kernel);
	if (!label->kernel_label)
		return -ENOMEM;

	s = strstr(label->kernel, "#");
	if (!s)
		return 1;

	label->config = strdup(s);
	if (!label->config)
		return -ENOMEM;

	*s = 0;

	return 1;
}

/*
 * Parses a label and adds it to the list of labels for a menu.
 *
 * A label ends when we either get to the end of a file, or
 * get some input we otherwise don't have a handler defined
 * for.
 */
static int parse_label(char **c, struct pxe_menu *cfg)
{
	struct token t;
	int len;
	char *s = *c;
	struct pxe_label *label;
	int err;

	label = label_create();
	if (!label)
		return -ENOMEM;

	err = parse_sliteral(c, &label->name);
	if (err < 0) {
		printf("Expected label name: %.*s\n", (int)(*c - s), s);
		label_destroy(label);
		return -EINVAL;
	}
	list_add_tail(&label->list, &cfg->labels);

	while (1) {
		s = *c;
		get_token(c, &t, L_KEYWORD);

		err = 0;
		switch (t.type) {
		case T_MENU:
			err = parse_label_menu(c, cfg, label);
			break;
		case T_KERNEL:
		case T_LINUX:
			err = parse_label_kernel(c, label);
			break;
		case T_APPEND:
			err = parse_sliteral(c, &label->append);
			if (label->initrd)
				break;
			s = strstr(label->append, "initrd=");
			if (!s)
				break;
			s += 7;
			len = (int)(strchr(s, ' ') - s);
			label->initrd = malloc(len + 1);
			strlcpy(label->initrd, s, len);
			label->initrd[len] = '\0';

			break;
		case T_INITRD:
			if (!label->initrd)
				err = parse_sliteral(c, &label->initrd);
			break;
		case T_FDT:
			if (!label->fdt)
				err = parse_sliteral(c, &label->fdt);
			break;
		case T_FDTDIR:
			if (!label->fdtdir)
				err = parse_sliteral(c, &label->fdtdir);
			break;
		case T_FDTOVERLAYS:
			if (!label->fdtoverlays)
				err = parse_sliteral(c, &label->fdtoverlays);
			break;
		case T_LOCALBOOT:
			label->localboot = 1;
			err = parse_integer(c, &label->localboot_val);
			break;
		case T_IPAPPEND:
			err = parse_integer(c, &label->ipappend);
			break;
		case T_KASLRSEED:
			label->kaslrseed = 1;
			break;
		case T_EOL:
			break;
		case T_SAY: {
			char *p = strchr(s, '\n');

			if (p) {
				printf("%.*s\n", (int)(p - *c) - 1, *c + 1);

				*c = p;
			}
			break;
		}
		default:
			/*
			 * put the token back! we don't want it - it's the end
			 * of a label and whatever token this is, it's
			 * something for the menu level context to handle.
			 */
			*c = s;
			return 1;
		}

		if (err < 0)
			return err;
	}
}

/*
 * This 16 comes from the limit pxelinux imposes on nested includes.
 *
 * There is no reason at all we couldn't do more, but some limit helps prevent
 * infinite (until crash occurs) recursion if a file tries to include itself.
 */
#define MAX_NEST_LEVEL 16

int parse_pxefile_top(struct pxe_context *ctx, char *p, ulong base,
		      struct pxe_menu *cfg, int nest_level)
{
	struct token t;
	char *s, *b, *label_name;
	int err;

	b = p;
	if (nest_level > MAX_NEST_LEVEL) {
		printf("Maximum nesting (%d) exceeded\n", MAX_NEST_LEVEL);
		return -EMLINK;
	}

	while (1) {
		s = p;
		get_token(&p, &t, L_KEYWORD);

		err = 0;
		switch (t.type) {
		case T_MENU:
			cfg->prompt = 1;
			err = parse_menu(ctx, &p, cfg,
					 base + ALIGN(strlen(b) + 1, 4),
					 nest_level);
			break;
		case T_TIMEOUT:
			err = parse_integer(&p, &cfg->timeout);
			break;
		case T_LABEL:
			err = parse_label(&p, cfg);
			break;
		case T_DEFAULT:
		case T_ONTIMEOUT:
			err = parse_sliteral(&p, &label_name);
			if (label_name) {
				if (cfg->default_label)
					free(cfg->default_label);

				cfg->default_label = label_name;
			}
			break;
		case T_FALLBACK:
			err = parse_sliteral(&p, &label_name);
			if (label_name) {
				if (cfg->fallback_label)
					free(cfg->fallback_label);

				cfg->fallback_label = label_name;
			}
			break;
		case T_INCLUDE:
			err = handle_include(ctx, &p,
					     base + ALIGN(strlen(b), 4), cfg,
					     nest_level + 1);
			break;
		case T_PROMPT:
			err = parse_integer(&p, &cfg->prompt);
			// Do not fail if prompt configuration is undefined
			if (err <  0)
				eol_or_eof(&p);
			break;
		case T_EOL:
			break;
		case T_EOF:
			return 1;
		default:
			printf("Ignoring unknown command: %.*s\n",
			       (int)(p - s), s);
			eol_or_eof(&p);
		}

		if (err < 0)
			return err;
	}
}
