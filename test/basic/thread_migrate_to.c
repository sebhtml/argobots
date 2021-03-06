/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "abt.h"
#include "abttest.h"

void thread_func(void *arg)
{
    int i;
    size_t my_id = (size_t)arg;
    ABT_test_printf(1, "[TH%lu]: Hello, world!\n", my_id);

    for (i = 0; i < 2; i++){
        ABT_test_printf(1, "Thread %lu, it %d\n", my_id, i);
        ABT_thread_yield();
    }
}

int main(int argc, char *argv[])
{
    int ret;

    ABT_xstream xstream, myxstream;
    ABT_thread thread;

    /* Initialize */
    ABT_test_init(argc, argv);

    /* Create Execution Streams */
    ret = ABT_xstream_create(ABT_SCHED_NULL, &xstream);
    ABT_TEST_ERROR(ret, "ABT_xstream_create");
    ret = ABT_xstream_self(&myxstream);
    ABT_TEST_ERROR(ret, "ABT_xstream_self");

    /* Create threads */
    size_t tid = 1;
       ret = ABT_thread_create(myxstream,
          thread_func, (void *)tid, ABT_THREAD_ATTR_NULL,
          &thread);
    ABT_TEST_ERROR(ret, "ABT_thread_create");

    ABT_test_printf(1, "[MAIN] Migrating thread\n");

    /* migrating threads from main xstream */
    ABT_thread_migrate_to(thread, xstream);

    /* Switch to other user level threads */
    ABT_thread_yield();

    /* Join Execution Streams */
    ret = ABT_xstream_join(xstream);
    ABT_TEST_ERROR(ret, "ABT_xstream_join");

    /* Free Execution Streams */
    ret = ABT_xstream_free(&xstream);
    ABT_TEST_ERROR(ret, "ABT_xstream_free");

    /* Finalize */
    ret = ABT_test_finalize(0);

    return ret;
}
