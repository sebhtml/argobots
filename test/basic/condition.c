/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "abt.h"
#include "abttest.h"

#define DEFAULT_NUM_XSTREAMS    2
#define DEFAULT_NUM_THREADS     3

/* Total number of threads (num_xstreams * num_threads) should be equal to
 * or larger than 3. */
int num_xstreams = DEFAULT_NUM_XSTREAMS;
int num_threads = DEFAULT_NUM_THREADS;

#define TCOUNT          10
#define COUNT_LIMIT     15

int g_counter = 0;
int g_num_incthreads = 0;
int g_waiting = 0;

ABT_mutex mutex = ABT_MUTEX_NULL;
ABT_cond cond = ABT_COND_NULL;
ABT_cond broadcast = ABT_COND_NULL;

typedef struct thread_arg {
    int sid;    /* stream ID */
    int tid;    /* thread ID */
} thread_arg_t;

void inc_counter(void *arg)
{
    int i;
    thread_arg_t *t_arg = (thread_arg_t *)arg;
    int es_id = t_arg->sid;
    int my_id = t_arg->tid;

    for (i = 0; i < TCOUNT; i++) {
        ABT_mutex_lock(mutex);
        g_counter++;

        if (g_counter == COUNT_LIMIT) {
            ABT_test_printf(1, "[ES%d:TH%d] inc_counter(): threshold(%d) "
                    "reached\n", es_id, my_id, g_counter);
            ABT_cond_signal(cond);
            ABT_test_printf(1, "[ES%d:TH%d] inc_counter(): sent signal\n",
                    es_id, my_id);
        }

        ABT_mutex_unlock(mutex);

        ABT_thread_yield();
    }

    ABT_mutex_lock(mutex);
    g_num_incthreads++;
    ABT_cond_wait(broadcast, mutex);
    ABT_mutex_unlock(mutex);
}

void watch_counter(void *arg)
{
    thread_arg_t *t_arg = (thread_arg_t *)arg;
    int es_id = t_arg->sid;
    int my_id = t_arg->tid;

    ABT_test_printf(1, "[ES%d:TH%d] watch_count(): starting\n", es_id, my_id);

    ABT_mutex_lock(mutex);
    while (g_counter < COUNT_LIMIT) {
        ABT_test_printf(1, "[ES%d:TH%d] watch_count(): waiting\n",
                es_id, my_id);
        ABT_cond_wait(cond, mutex);
        ABT_test_printf(1, "[ES%d:TH%d] watch_count(): received signal\n",
                es_id, my_id);
        g_waiting = 1;
        g_counter += 100;
    }
    ABT_mutex_unlock(mutex);

    while (g_num_incthreads != (num_xstreams * num_threads - 1)) {
        ABT_thread_yield();
    }
    ABT_mutex_lock(mutex);
    ABT_cond_broadcast(broadcast);
    ABT_test_printf(1, "[ES%d:TH%d] broadcast signal\n", es_id, my_id);
    ABT_mutex_unlock(mutex);
}

int main(int argc, char *argv[])
{
    int i, j;
    int ret, expected;
    if (argc > 1) num_xstreams = atoi(argv[1]);
    assert(num_xstreams >= 0);
    if (argc > 2) num_threads = atoi(argv[2]);
    assert(num_threads >= 0);

    if (num_xstreams * num_threads < 3) {
        fprintf(stderr, "num_xstreams (%d) * num_threads (%d) < 3\n",
                num_xstreams, num_threads);
        exit(EXIT_FAILURE);
    }

    ABT_xstream *xstreams;
    thread_arg_t **args;

    xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);
    args = (thread_arg_t **)malloc(sizeof(thread_arg_t *) * num_xstreams);
    for (i = 0; i < num_xstreams; i++) {
        args[i] = (thread_arg_t *)malloc(sizeof(thread_arg_t) * num_threads);
    }

    /* Initialize */
    ABT_test_init(argc, argv);

    /* Create Execution Streams */
    ret = ABT_xstream_self(&xstreams[0]);
    ABT_TEST_ERROR(ret, "ABT_xstream_self");
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_create(ABT_SCHED_NULL, &xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_create");
    }

    /* Create a mutex */
    ret = ABT_mutex_create(&mutex);
    ABT_TEST_ERROR(ret, "ABT_mutex_create");

    /* Create condition variables */
    ret = ABT_cond_create(&cond);
    ABT_TEST_ERROR(ret, "ABT_cond_create");
    ret = ABT_cond_create(&broadcast);
    ABT_TEST_ERROR(ret, "ABT_cond_create");

    /* Create threads */
    args[0][0].sid = 0;
    args[0][0].tid = 1;
    ret = ABT_thread_create(xstreams[0], watch_counter, (void *)&args[0][0],
            ABT_THREAD_ATTR_NULL, NULL);
    ABT_TEST_ERROR(ret, "ABT_thread_create");
    i = 0;
    for (j = 1; j < num_threads; j++) {
        int tid = i * num_threads + j + 1;
        args[i][j].sid = i;
        args[i][j].tid = tid;
        ret = ABT_thread_create(xstreams[i],
                inc_counter, (void *)&args[i][j], ABT_THREAD_ATTR_NULL,
                NULL);
        ABT_TEST_ERROR(ret, "ABT_thread_create");
    }
    for (i = 1; i < num_xstreams; i++) {
        for (j = 0; j < num_threads; j++) {
            int tid = i * num_threads + j + 1;
            args[i][j].sid = i;
            args[i][j].tid = tid;
            ret = ABT_thread_create(xstreams[i],
                    inc_counter, (void *)&args[i][j], ABT_THREAD_ATTR_NULL,
                    NULL);
            ABT_TEST_ERROR(ret, "ABT_thread_create");
        }
    }

    /* Switch to other user level threads */
    ABT_thread_yield();

    /* Join Execution Streams */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_join(xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_join");
    }

    /* Free the mutex */
    ret = ABT_mutex_free(&mutex);
    ABT_TEST_ERROR(ret, "ABT_mutex_free");

    /* Free the condition variables */
    ret = ABT_cond_free(&cond);
    ABT_TEST_ERROR(ret, "ABT_cond_free");
    ret = ABT_cond_free(&broadcast);
    ABT_TEST_ERROR(ret, "ABT_cond_free");

    /* Free Execution Streams */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_free(&xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_free");
    }

    /* Validation */
    expected = (num_xstreams * num_threads - 1) * TCOUNT + g_waiting * 100;
    if (g_counter != expected) {
        printf("g_counter = %d (expected: %d)\n", g_counter, expected);
    }

    /* Finalize */
    ret = ABT_test_finalize(g_counter != expected);

    for (i = 0; i < num_xstreams; i++) {
        free(args[i]);
    }
    free(args);
    free(xstreams);

    return ret;
}

