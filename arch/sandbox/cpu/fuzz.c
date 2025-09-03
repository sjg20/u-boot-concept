// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2022 Google, Inc.
 * Written by Andrew Scull <ascull@google.com>
 */

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <os.h>
#include <asm/fuzzing_engine.h>
#include <asm/u-boot-sandbox.h>

static void *fuzzer_thread(void *ptr)
{
	char cmd[64];
	char *argv[5] = {"./u-boot", "-T", "-c", cmd, NULL};
	const char *fuzz_test;

	/* Find which test to run from an environment variable. */
	fuzz_test = getenv("UBOOT_SB_FUZZ_TEST");
	if (!fuzz_test)
		os_abort();

	snprintf(cmd, sizeof(cmd), "fuzz %s", fuzz_test);

	sandbox_main(4, argv);
	os_abort();
	return NULL;
}

static bool fuzzer_initialized;
static pthread_mutex_t fuzzer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t fuzzer_cond = PTHREAD_COND_INITIALIZER;
static const uint8_t *fuzzer_data;
static size_t fuzzer_size;

int sandbox_fuzzing_engine_get_input(const uint8_t **data, size_t *size)
{
	if (!fuzzer_initialized)
		return -ENOSYS;

	/* Tell the main thread we need new inputs then wait for them. */
	pthread_mutex_lock(&fuzzer_mutex);
	pthread_cond_signal(&fuzzer_cond);
	pthread_cond_wait(&fuzzer_cond, &fuzzer_mutex);
	*data = fuzzer_data;
	*size = fuzzer_size;
	pthread_mutex_unlock(&fuzzer_mutex);
	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	static pthread_t tid;

	pthread_mutex_lock(&fuzzer_mutex);

	/* Initialize the sandbox on another thread. */
	if (!fuzzer_initialized) {
		fuzzer_initialized = true;
		if (pthread_create(&tid, NULL, fuzzer_thread, NULL))
			os_abort();
		pthread_cond_wait(&fuzzer_cond, &fuzzer_mutex);
	}

	/* Hand over the input. */
	fuzzer_data = data;
	fuzzer_size = size;
	pthread_cond_signal(&fuzzer_cond);

	/* Wait for the inputs to be finished with. */
	pthread_cond_wait(&fuzzer_cond, &fuzzer_mutex);
	pthread_mutex_unlock(&fuzzer_mutex);

	return 0;
}
