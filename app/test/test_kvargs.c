/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2014 6WIND S.A.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <rte_common.h>
#include <rte_kvargs.h>

#include "test.h"

/* incremented in handler, to check it is properly called once per
 * key/value association */
static unsigned count;

/* this handler increment the "count" variable at each call and check
 * that the key is "check" and the value is "value%d" */
static int check_handler(const char *key, const char *value,
	__rte_unused void *opaque)
{
	char buf[16];

	/* we check that the value is "check" */
	if (strcmp(key, "check"))
		return -1;

	/* we check that the value is "value$(count)" */
	snprintf(buf, sizeof(buf), "value%d", count);
	if (strncmp(buf, value, sizeof(buf)))
		return -1;

	count ++;
	return 0;
}

/* test a valid case */
static int test_valid_kvargs(void)
{
	struct rte_kvargs *kvlist;
	const char *args;
	const char *valid_keys_list[] = { "foo", "check", NULL };
	const char **valid_keys;

	/* first test without valid_keys */
	args = "foo=1234,check=value0,check=value1";
	valid_keys = NULL;
	kvlist = rte_kvargs_parse(args, valid_keys);
	if (kvlist == NULL) {
		printf("rte_kvargs_parse() error");
		goto fail;
	}
	/* call check_handler() for all entries with key="check" */
	count = 0;
	if (rte_kvargs_process(kvlist, "check", check_handler, NULL) < 0) {
		printf("rte_kvargs_process() error\n");
		rte_kvargs_free(kvlist);
		goto fail;
	}
	if (count != 2) {
		printf("invalid count value %d after rte_kvargs_process(check)\n",
			count);
		rte_kvargs_free(kvlist);
		goto fail;
	}
	count = 0;
	/* call check_handler() for all entries with key="nonexistent_key" */
	if (rte_kvargs_process(kvlist, "nonexistent_key", check_handler, NULL) < 0) {
		printf("rte_kvargs_process() error\n");
		rte_kvargs_free(kvlist);
		goto fail;
	}
	if (count != 0) {
		printf("invalid count value %d after rte_kvargs_process(nonexistent_key)\n",
			count);
		rte_kvargs_free(kvlist);
		goto fail;
	}
	/* count all entries with key="foo" */
	count = rte_kvargs_count(kvlist, "foo");
	if (count != 1) {
		printf("invalid count value %d after rte_kvargs_count(foo)\n",
			count);
		rte_kvargs_free(kvlist);
		goto fail;
	}
	/* count all entries with key="nonexistent_key" */
	count = rte_kvargs_count(kvlist, "nonexistent_key");
	if (count != 0) {
		printf("invalid count value %d after rte_kvargs_count(nonexistent_key)\n",
			count);
		rte_kvargs_free(kvlist);
		goto fail;
	}
	rte_kvargs_free(kvlist);

	/* second test using valid_keys */
	args = "foo=droids,check=value0,check=value1,check=wrong_value";
	valid_keys = valid_keys_list;
	kvlist = rte_kvargs_parse(args, valid_keys);
	if (kvlist == NULL) {
		printf("rte_kvargs_parse() error");
		goto fail;
	}
	/* call check_handler() on all entries with key="check", it
	 * should fail as the value is not recognized by the handler */
	if (rte_kvargs_process(kvlist, "check", check_handler, NULL) == 0) {
		printf("rte_kvargs_process() is success but should not\n");
		rte_kvargs_free(kvlist);
		goto fail;
	}
	count = rte_kvargs_count(kvlist, "check");
	if (count != 3) {
		printf("invalid count value %d after rte_kvargs_count(check)\n",
			count);
		rte_kvargs_free(kvlist);
		goto fail;
	}
	rte_kvargs_free(kvlist);

	/* third test using list as value */
	args = "foo=[0,1],check=value2";
	valid_keys = valid_keys_list;
	kvlist = rte_kvargs_parse(args, valid_keys);
	if (kvlist == NULL) {
		printf("rte_kvargs_parse() error\n");
		goto fail;
	}
	if (strcmp(kvlist->pairs[0].value, "[0,1]") != 0) {
		printf("wrong value %s", kvlist->pairs[0].value);
		goto fail;
	}
	count = kvlist->count;
	if (count != 2) {
		printf("invalid count value %d\n", count);
		rte_kvargs_free(kvlist);
		goto fail;
	}
	rte_kvargs_free(kvlist);

	/* test using empty elements (it is valid) */
	args = "foo=1,,check=value2,,";
	kvlist = rte_kvargs_parse(args, NULL);
	if (kvlist == NULL) {
		printf("rte_kvargs_parse() error\n");
		goto fail;
	}
	if (rte_kvargs_count(kvlist, "foo") != 1) {
		printf("invalid count value for 'foo'\n");
		goto fail;
	}
	if (rte_kvargs_count(kvlist, "check") != 1) {
		printf("invalid count value for 'check'\n");
		goto fail;
	}
	rte_kvargs_free(kvlist);

	return 0;

 fail:
	printf("while processing <%s>", args);
	if (valid_keys != NULL && *valid_keys != NULL) {
		printf(" using valid_keys=<%s", *valid_keys);
		while (*(++valid_keys) != NULL)
			printf(",%s", *valid_keys);
		printf(">");
	}
	printf("\n");
	return -1;
}

static int
test_basic_token_count(void)
{
	static const struct {
		unsigned int expected;
		const char *input;
	} valid_inputs[] = {
		{ 3, "foo=1,check=1,check=2" },
		{ 3, "foo=1,check,check=2"   },
		{ 2, "foo=1,foo="            },
		{ 2, "foo=1,foo="            },
		{ 2, "foo=1,foo"             },
		{ 2, "foo=1,=2"              },
		{ 2, "foo=1,,foo=2,,"        },
		{ 1, "foo=[1,2"              },
		{ 1, ",="                    },
		{ 1, "foo=["                 },
		{ 0, ""                      },
	};
	struct rte_kvargs *kvlist;
	unsigned int count;
	const char *args;
	unsigned int i;

	for (i = 0; i < RTE_DIM(valid_inputs); i++) {
		args = valid_inputs[i].input;
		kvlist = rte_kvargs_parse(args, NULL);
		if (kvlist == NULL) {
			printf("rte_kvargs_parse() error: %s\n", args);
			return -1;
		}
		count = rte_kvargs_count(kvlist, NULL);
		if (count != valid_inputs[i].expected) {
			printf("invalid count value %u (expected %u): %s\n",
			       count, valid_inputs[i].expected, args);
			rte_kvargs_free(kvlist);
			return -1;
		}
		rte_kvargs_free(kvlist);
	}

	return 0;
}

/* test several error cases */
static int test_invalid_kvargs(void)
{
	struct rte_kvargs *kvlist;
	/* list of argument that should fail */
	const char *args_list[] = {
		"wrong-key=x",     /* key not in valid_keys_list */
		NULL };
	const char **args;
	const char *valid_keys_list[] = { "foo", "check", NULL };
	const char **valid_keys = valid_keys_list;

	for (args = args_list; *args != NULL; args++) {

		kvlist = rte_kvargs_parse(*args, valid_keys);
		if (kvlist != NULL) {
			printf("rte_kvargs_parse() returned 0 (but should not)\n");
			rte_kvargs_free(kvlist);
			goto fail;
		}
	}
	return 0;

 fail:
	printf("while processing <%s>", *args);
	if (valid_keys != NULL && *valid_keys != NULL) {
		printf(" using valid_keys=<%s", *valid_keys);
		while (*(++valid_keys) != NULL)
			printf(",%s", *valid_keys);
		printf(">");
	}
	printf("\n");
	return -1;
}

static struct unit_test_suite kvargs_test_suite  = {
	.suite_name = "Kvargs Unit Test Suite",
	.setup = NULL,
	.teardown = NULL,
	.unit_test_cases = {
		TEST_CASE(test_valid_kvargs),
		TEST_CASE(test_basic_token_count),
		TEST_CASE(test_invalid_kvargs),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_kvargs(void)
{
	return unit_test_suite_runner(&kvargs_test_suite);
}

REGISTER_FAST_TEST(kvargs_autotest, true, true, test_kvargs);
