/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "abt.h"
#include "abttest.h"

#define DEFAULT_NUM_THREADS     4
#define DEFAULT_NAME_LEN        16

typedef struct thread_arg {
    int id;
    char name[DEFAULT_NAME_LEN];
} thread_arg_t;

void thread_func(void *arg)
{
    thread_arg_t *t_arg = (thread_arg_t *)arg;
    ABT_test_printf(1, "[TH%d]: My name is %s.\n", t_arg->id, t_arg->name);
}

int main(int argc, char *argv[])
{
    int i, ret;
    int num_threads = DEFAULT_NUM_THREADS;
    if (argc > 1) num_threads = atoi(argv[1]);
    assert(num_threads >= 0);

    ABT_xstream xstream;
    ABT_thread *threads;
    thread_arg_t *args;
    char xstream_name[16];
    size_t name_len;
    char *name;
    threads = (ABT_thread *)malloc(sizeof(ABT_thread *) * num_threads);
    args = (thread_arg_t *)malloc(sizeof(thread_arg_t) * num_threads);

    /* Initialize */
    ABT_test_init(argc, argv);

    /* Get the SELF Execution Stream */
    ret = ABT_xstream_self(&xstream);
    ABT_TEST_ERROR(ret, "ABT_xstream_self");

    /* Set the name for ES */
    sprintf(xstream_name, "SELF-xstream");
    ABT_test_printf(1, "Set the xstream's name as '%s'\n", xstream_name);
    ret = ABT_xstream_set_name(xstream, xstream_name);
    ABT_TEST_ERROR(ret, "ABT_xstream_set_name");

    /* Create threads */
    for (i = 0; i < num_threads; i++) {
        args[i].id = i + 1;
        sprintf(args[i].name, "arogobot-%d", i);
        ret = ABT_thread_create(xstream,
                thread_func, (void *)&args[i], ABT_THREAD_ATTR_NULL,
                &threads[i]);
        ABT_TEST_ERROR(ret, "ABT_thread_create");

        /* Set the thread's name */
        ret = ABT_thread_set_name(threads[i], args[i].name);
        ABT_TEST_ERROR(ret, "ABT_thread_set_name");
    }

    /* Get the name of ES */
    ret = ABT_xstream_get_name(xstream, NULL, &name_len);
    ABT_TEST_ERROR(ret, "ABT_xstream_get_name");
    name = (char *)malloc(sizeof(char) * (name_len + 1));
    ret = ABT_xstream_get_name(xstream, xstream_name, &name_len);
    ABT_TEST_ERROR(ret, "ABT_xstream_get_name");
    ABT_test_printf(1, "Stream's name: %s\n", xstream_name);
    free(name);

    /* Get the threads' names */
    for (i = 0; i < num_threads; i++) {
        char thread_name[16];
        ret = ABT_thread_get_name(threads[i], thread_name, &name_len);
        ABT_TEST_ERROR(ret, "ABT_thread_get_name");
        ABT_test_printf(1, "TH%d's name: %s\n", i + 1, thread_name);
    }

    /* Switch to other user level threads */
    ABT_thread_yield();

    /* Free threads */
    for (i = 0; i < num_threads; i++) {
        ret = ABT_thread_free(&threads[i]);
        ABT_TEST_ERROR(ret, "ABT_thread_free");
    }

    /* Finalize */
    ret = ABT_test_finalize(0);

    free(args);
    free(threads);

    return ret;
}
