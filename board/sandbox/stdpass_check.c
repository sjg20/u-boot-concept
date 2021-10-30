// SPDX-License-Identifier: GPL-2.0+
/*
 * This file compiles all the struct definitions for standard passage, to ensure
 * there are no errors
 *
 * Copyright 2021 Google LLC
 */

#include <common.h>

/*
 * See also doc/develop/std_passage.rst
 *
 * Instructions:
 *
 * 1. Add your header file to U-Boot, or to include/stdpass if it is not used in
 * U-Boot
 *
 * 2. Add a function below to include the header and use the struct. Please put
 * your function in order of tag ID (see bloblist.h)
 *
 * Template follows, see above for example
 */

/* BLOBLISTT_tag here */
/* #include <stdpass/yourfile.h> if not used in U-Boot*/
void check_struct_name(void)
{
	/* __maybe_unused struct struct_name check; */
}
