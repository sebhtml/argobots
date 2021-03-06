/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static ABT_thread_id ABTI_thread_get_new_id(void);


/** @defgroup ULT User-level Thread (ULT)
 * This group is for User-level Thread (ULT).
 */

/**
 * @ingroup ULT
 * @brief   Create a new thread and return its handle through newthread.
 *
 * If newthread is NULL, the thread object will be automatically released when
 * this \a unnamed thread completes the execution of thread_func. Otherwise,
 * ABT_thread_free() can be used to explicitly release the thread object.
 *
 * @param[in]  xstream      handle to the associated ES
 * @param[in]  thread_func  function to be executed by a new thread
 * @param[in]  arg          argument for thread_func
 * @param[in]  attr         thread attribute. If it is ABT_THREAD_ATTR_NULL,
 *                          the default attribute is used.
 * @param[out] newthread    handle to a newly created thread
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_create(ABT_xstream xstream,
                      void (*thread_func)(void *), void *arg,
                      ABT_thread_attr attr, ABT_thread *newthread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream;
    ABTI_sched *p_sched;
    ABTI_thread *p_newthread;
    ABT_thread h_newthread;
    ABTI_thread_attr *p_attr;
    size_t stacksize;

    p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    if (p_xstream->state == ABT_XSTREAM_STATE_CREATED) {
        abt_errno = ABTI_xstream_start(p_xstream);
        if (abt_errno != ABT_SUCCESS) {
            HANDLE_ERROR("ABTI_xstream_start");
            goto fn_fail;
        }
    }

    p_newthread = (ABTI_thread *)ABTU_malloc(sizeof(ABTI_thread));
    if (!p_newthread) {
        HANDLE_ERROR("ABTU_malloc");
        if (newthread) *newthread = ABT_THREAD_NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    /* Create a wrapper unit */
    p_sched = p_xstream->p_sched;
    h_newthread = ABTI_thread_get_handle(p_newthread);
    p_newthread->unit = p_sched->u_create_from_thread(h_newthread);

    p_newthread->p_xstream  = p_xstream;
    p_newthread->id         = ABTI_thread_get_new_id();
    p_newthread->p_name     = NULL;
    p_newthread->type       = ABTI_THREAD_TYPE_USER;
    p_newthread->state      = ABT_THREAD_STATE_READY;
    p_newthread->refcount   = (newthread != NULL) ? 1 : 0;
    p_newthread->request    = 0;
    p_newthread->p_req_arg  = NULL;

    /* Set attributes */
    p_attr = ABTI_thread_attr_get_ptr(attr);
    ABTI_thread_set_attr(p_newthread, p_attr);

    /* Create a mutex */
    abt_errno = ABT_mutex_create(&p_newthread->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    /* Create a stack for this thread */
    stacksize = p_newthread->attr.stacksize;
    p_newthread->p_stack = ABTU_malloc(stacksize);
    if (!p_newthread->p_stack) {
        HANDLE_ERROR("ABTU_malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    /* Create a thread context */
    abt_errno = ABTD_thread_context_create(&p_sched->ctx,
            thread_func, arg, stacksize, p_newthread->p_stack,
            &p_newthread->ctx);
    ABTI_CHECK_ERROR(abt_errno);

    /* Add this thread to the scheduler's pool */
    ABTI_sched_push(p_sched, p_newthread->unit);

    /* Return value */
    if (newthread) *newthread = h_newthread;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_create", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Release the thread object associated with thread handle.
 *
 * This routine deallocates memory used for the thread object. If the thread
 * is still running when this routine is called, the deallocation happens
 * after the thread terminates and then this routine returns. If it is
 * successfully processed, thread is set as ABT_THREAD_NULL.
 *
 * @param[in,out] thread  handle to the target thread
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_free(ABT_thread *thread)
{
    int abt_errno = ABT_SUCCESS;
    ABT_thread h_thread = *thread;
    if (h_thread == ABT_THREAD_NULL) goto fn_exit;

    ABTI_thread *p_thread = ABTI_thread_get_ptr(h_thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    if (p_thread == ABTI_local_get_thread()) {
        HANDLE_ERROR("The current thread cannot be freed.");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    if (p_thread->type == ABTI_THREAD_TYPE_MAIN) {
        HANDLE_ERROR("The main thread cannot be freed explicitly.");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    /* Wait until the thread terminates */
    while (p_thread->state != ABT_THREAD_STATE_TERMINATED) {
        ABT_thread_yield();
    }

    if (p_thread->refcount > 0) {
        /* The thread has finished but it is still referenced.
         * Thus it exists in the xstream's deads pool. */
        ABTI_xstream *p_xstream = p_thread->p_xstream;
        ABTI_mutex_waitlock(p_xstream->mutex);
        ABTI_pool_remove(p_xstream->deads, p_thread->unit);
        ABT_mutex_unlock(p_xstream->mutex);
    }

    /* Free the ABTI_thread structure */
    abt_errno = ABTI_thread_free(p_thread);
    ABTI_CHECK_ERROR(abt_errno);

    /* Return value */
    *thread = ABT_THREAD_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_free", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Wait for thread to terminate.
 *
 * The target thread cannot be the same as the calling thread.
 *
 * @param[in] thread  handle to the target thread
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_join(ABT_thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    ABTI_thread *p_caller = ABTI_local_get_thread();

    if (p_thread == p_caller) {
        HANDLE_ERROR("The target thread should be different.");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    if (p_thread->type == ABTI_THREAD_TYPE_MAIN) {
        HANDLE_ERROR("The main thread cannot be joined.");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    while (p_thread->state != ABT_THREAD_STATE_TERMINATED) {
        if (p_thread->p_xstream == p_caller->p_xstream)
            ABT_thread_yield_to(thread);
        else
            ABT_thread_yield();
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_join", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   The calling thread terminates its execution.
 *
 * Since the calling thread terminates, this routine never returns.
 *
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_exit(void)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_thread *p_thread = ABTI_local_get_thread();

    /* Set the exit request */
    ABTD_atomic_fetch_or_uint32(&p_thread->request, ABTI_THREAD_REQ_EXIT);

    /* Switch the context to the scheduler */
    ABT_thread_yield();

    return abt_errno;
}

/**
 * @ingroup ULT
 * @brief   Request the cancelation of the target thread.
 *
 * @param[in] thread  handle to the target thread
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_cancel(ABT_thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    if (p_thread->type == ABTI_THREAD_TYPE_MAIN) {
        HANDLE_ERROR("The main thread cannot be canceled.");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    /* Set the cancel request */
    ABTD_atomic_fetch_or_uint32(&p_thread->request, ABTI_THREAD_REQ_CANCEL);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_cancel", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Return the handle of the calling ULT.
 *
 * \c ABT_thread_self() returns the handle of the calling ULT. Both the primary
 * ULT and secondary ULTs can get their handle through this routine.
 * If tasklets call this routine, \c ABT_THREAD_NULL will be returned to
 * \c thread.
 *
 * @param[out] thread  ULT handle
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_self(ABT_thread *thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_CHECK_INITIALIZED();

    ABTI_thread *p_thread = ABTI_local_get_thread();
    if (p_thread != NULL) {
        ABTI_thread_retain(p_thread);
    }
    *thread = ABTI_thread_get_handle(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_self", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Get the ES associated with the target ULT.
 *
 * \c ABT_thread_get_xstream() returns the ES handle associated with the
 * target ULT to \c xstream. If the target ULT is not associated with any ES,
 * \c ABT_XSTREAM_NULL is returned to \c xstream.
 *
 * @param[in]  thread   handle to the target ULT
 * @param[out] xstream  ES handle
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_get_xstream(ABT_thread thread, ABT_xstream *xstream)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    /* Return value */
    *xstream = ABTI_xstream_get_handle(p_thread->p_xstream);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_get_xstream", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Return the state of thread.
 *
 * @param[in]  thread  handle to the target thread
 * @param[out] state   the thread's state
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_get_state(ABT_thread thread, ABT_thread_state *state)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    /* Return value */
    *state = p_thread->state;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_get_state", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Yield the processor from the current running thread to the
 *          specific thread.
 *
 * This function can be used for users to explicitly schedule the next thread
 * to execute.
 *
 * @param[in] thread  handle to the target thread
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_yield_to(ABT_thread thread)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_thread *p_cur_thread = ABTI_local_get_thread();
    ABTI_thread *p_tar_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_tar_thread);
    DEBUG_PRINT("YIELD_TO: TH%lu -> TH%lu\n",
                p_cur_thread->id, p_tar_thread->id);

    /* If the target thread is the same as the running thread, just keep
     * its execution. */
    if (p_cur_thread == p_tar_thread) goto fn_exit;

    if (p_tar_thread->state == ABT_THREAD_STATE_TERMINATED) {
        HANDLE_ERROR("Cannot yield to the terminated thread");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    /* Both threads must be associated with the same ES. */
    ABTI_xstream *p_xstream = p_cur_thread->p_xstream;
    if (p_xstream != p_tar_thread->p_xstream) {
        HANDLE_ERROR("The target thread's ES is not the same as mine.");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    ABTI_sched *p_sched = p_xstream->p_sched;

    if (p_cur_thread->type == ABTI_THREAD_TYPE_MAIN) {
        /* Remove the target thread from the pool */
        ABTI_sched_remove(p_sched, p_tar_thread->unit);

        abt_errno = ABTI_xstream_schedule_thread(p_tar_thread);
        ABTI_CHECK_ERROR(abt_errno);

        ABTI_local_set_thread(p_cur_thread);

        /* Yield to another thread to execute all threads */
        ABT_thread_yield();
    } else {
        p_cur_thread->state = ABT_THREAD_STATE_READY;

        /* Add the current thread to the pool again */
        ABTI_sched_push(p_sched, p_cur_thread->unit);

        /* Remove the target thread from the pool */
        ABTI_sched_remove(p_sched, p_tar_thread->unit);

        /* Switch the context */
        ABTI_local_set_thread(p_tar_thread);
        p_tar_thread->state = ABT_THREAD_STATE_RUNNING;
        abt_errno = ABTD_thread_context_switch(&p_cur_thread->ctx,
                                               &p_tar_thread->ctx);
        ABTI_CHECK_ERROR(abt_errno);

        ABTI_local_set_thread(p_cur_thread);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_yield_to", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Yield the processor from the current running thread to a next
 *          thread.
 *
 * The next thread is selected by the scheduler of ES. If there is no more
 * thread to, the calling thread resumes its execution immediately.
 *
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_yield(void)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_thread *p_thread = ABTI_local_get_thread();
    ABTI_xstream *p_xstream = ABTI_local_get_xstream();
    assert(p_thread->p_xstream == p_xstream);

    ABTI_sched *p_sched = p_xstream->p_sched;
    if (p_sched->p_get_size(p_sched->pool) < 1) {
        int has_global_task;
        ABTI_global_has_task(&has_global_task);
        if (!has_global_task) goto fn_exit;
    }

    /* Change the state of current running thread */
    p_thread->state = ABT_THREAD_STATE_READY;

    if (p_thread->type == ABTI_THREAD_TYPE_MAIN) {
        /* Currently, the main program thread waits until all threads
         * finish their execution. */

        /* Start the scheduling */
        abt_errno = ABTI_xstream_schedule(p_xstream);
        ABTI_CHECK_ERROR(abt_errno);

        p_thread->state = ABT_THREAD_STATE_RUNNING;
    } else {
        /* Switch to the scheduler */
        abt_errno = ABTD_thread_context_switch(&p_thread->ctx, &p_sched->ctx);
        ABTI_CHECK_ERROR(abt_errno);
    }

    /* Back to the original thread */
    ABTI_local_set_thread(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_yield", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Migrate a thread to a specific ES.
 *
 * The actual migration occurs asynchronously with this function call.
 * In other words, this function may return immediately without the thread
 * being migrated. The migration request will be posted on the thread, such that
 * next time a scheduler picks it up, migration will happen.
 *
 * @param[in] thread   handle to the thread to migrate
 * @param[in] xstream  handle to the ES to migrate the thread to
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_migrate_to(ABT_thread thread, ABT_xstream xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);
    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    /* checking for cases when migration is not allowed */
    if (p_xstream->state == ABT_XSTREAM_STATE_TERMINATED) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        goto fn_fail;
    }
    if (p_thread->type == ABTI_THREAD_TYPE_MAIN) {
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }
    if (p_thread->state == ABT_THREAD_STATE_TERMINATED) {
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_exit;
    }

    /* checking for migration to the same xstream */
    if (p_thread->p_xstream == p_xstream) {
        /* Invoke the callback function */
        if (p_thread->attr.f_cb) {
            p_thread->attr.f_cb(thread, p_thread->attr.p_cb_arg);
        }
        goto fn_exit;
    }

    /* adding request to the thread */
    ABTI_mutex_waitlock(p_thread->mutex);
    ABTI_thread_add_req_arg(p_thread, ABTI_THREAD_REQ_MIGRATE, p_xstream);
    ABT_mutex_unlock(p_thread->mutex);
    ABTD_atomic_fetch_or_uint32(&p_thread->request, ABTI_THREAD_REQ_MIGRATE);

    /* yielding if it is the same thread */
    if (p_thread == ABTI_local_get_thread()) {
        ABT_thread_yield();
    }
    goto fn_exit;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_migrate_to", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Request migration of the thread to an any available ES.
 *
 * ABT_thread_migrate() requests migration of the thread but does not specify
 * the target ES. The target ES will be determined among available ESs by the
 * runtime. Other semantics of this routine are the same as those of
 * \c ABT_thread_migrate_to()
 *
 * @param[in] thread  handle to the thread
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_migrate(ABT_thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABT_xstream xstream;

    ABTI_xstream_pool *p_xstreams = gp_ABTI_global->p_xstreams;

    /* Choose the destination xstream */
    /* FIXME: Currenlty, the target xstream is randomly chosen. We need a
     * better selection strategy. */
    if (ABTI_pool_get_size(p_xstreams->created) > 0) {
        ABTI_xstream *p_xstream;
        abt_errno = ABTI_global_get_created_xstream(&p_xstream);
        ABTI_CHECK_ERROR(abt_errno);
        xstream = ABTI_xstream_get_handle(p_xstream);
    } else {
        ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
        ABT_unit next = ABTI_unit_get_next(p_thread->p_xstream->unit);
        xstream = ABTI_unit_get_xstream(next);
    }

    abt_errno = ABT_thread_migrate_to(thread, xstream);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_migrate", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Set the callback function.
 *
 * \c ABT_thread_set_callback() sets the callback function to be used when
 * the ULT is migrated.
 *
 * @param[in] thread   handle to the target ULT
 * @param[in] cb_func  callback function pointer
 * @param[in] cb_arg   argument for the callback function
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_set_callback(ABT_thread thread,
                            void(*cb_func)(ABT_thread thread, void *cb_arg),
                            void *cb_arg)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    p_thread->attr.f_cb     = cb_func;
    p_thread->attr.p_cb_arg = cb_arg;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_set_callback", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Set the ULT's migratability.
 *
 * \c ABT_thread_set_migratable() sets the secondary ULT's migratability.
 * This routine cannot be used for the primary ULT.
 * If \c flag is \c ABT_TRUE, the target ULT becomes migratable. On the
 * other hand, if \c flag is \c ABT_FALSE, the target ULT becomes unmigratable.
 *
 * @param[in] thread  handle to the target ULT
 * @param[in] flag    migratability flag (<tt>ABT_TRUE</tt>: migratable,
 *                    <tt>ABT_FALSE</tt>: not)
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_set_migratable(ABT_thread thread, ABT_bool flag)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    if (p_thread->type == ABTI_THREAD_TYPE_USER) {
        p_thread->attr.migratable = flag;
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_set_migratable", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Get the ULT's migratability.
 *
 * \c ABT_thread_is_migratable() returns the ULT's migratability through
 * \c flag. If the target ULT is migratable, \c ABT_TRUE is returned to
 * \c flag. Otherwise, \c flag is set to \c ABT_FALSE.
 *
 * @param[in]  thread  handle to the target ULT
 * @param[out] flag    migratability flag (<tt>ABT_TRUE</tt>: migratable,
 *                     <tt>ABT_FALSE</tt>: not)
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_is_migratable(ABT_thread thread, ABT_bool *flag)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    *flag = p_thread->attr.migratable;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_is_migratable", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Compare two ULT handles for equality.
 *
 * \c ABT_thread_equal() compares two ULT handles for equality. If two handles
 * are associated with the same ULT object, \c result will be set to
 * \c ABT_TRUE. Otherwise, \c result will be set to \c ABT_FALSE.
 *
 * @param[in]  thread1  handle to the ULT 1
 * @param[in]  thread2  handle to the ULT 2
 * @param[out] result   comparison result (<tt>ABT_TRUE</tt>: same,
 *                      <tt>ABT_FALSE</tt>: not same)
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_equal(ABT_thread thread1, ABT_thread thread2, ABT_bool *result)
{
    ABTI_thread *p_thread1 = ABTI_thread_get_ptr(thread1);
    ABTI_thread *p_thread2 = ABTI_thread_get_ptr(thread2);
    *result = (p_thread1 == p_thread2) ? ABT_TRUE : ABT_FALSE;
    return ABT_SUCCESS;
}

/**
 * @ingroup ULT
 * @brief   Increment the ULT's reference count.
 *
 * \c ABT_thread_retain() increments the ULT's reference count by one.
 * If the user obtains a ULT handle through \c ABT_thread_create() or
 * \c ABT_thread_self(), those routines perform an implicit retain.
 *
 * @param[in] thread  handle to the ULT
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_retain(ABT_thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    ABTI_thread_retain(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_retain", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Decrement the ULT's reference count.
 *
 * \c ABT_thread_release() decrements the ULT's reference count by one.
 * After the ULT's reference count becomes zero, the ULT object will be freed.
 *
 * @param[in] thread  handle to the ULT
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_release(ABT_thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    ABTI_thread_release(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_release", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Get the ULT's stack size.
 *
 * \c ABT_thread_get_stacksize() returns the stack size of \c thread in bytes.
 *
 * @param[in]  thread     handle to the target thread
 * @param[out] stacksize  stack size in bytes
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_get_stacksize(ABT_thread thread, size_t *stacksize)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    /* Return value */
    *stacksize = p_thread->attr.stacksize;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_get_stacksize", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Set the scheduling priority of thread.
 *
 * The \c ABT_thread_set_prio() set the scheduling priority of the thread
 * \c thread to the value \c prio.
 *
 * @param[in] thread  handle to the target thread
 * @param[in] prio    scheduling priority
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_set_prio(ABT_thread thread, ABT_sched_prio prio)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);

    /* Sanity check */
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);
    ABTI_CHECK_SCHED_PRIO(prio);

    if (prio == p_thread->attr.prio) goto fn_exit;

    ABTI_mutex_waitlock(p_thread->mutex);
    ABTI_xstream *p_xstream = p_thread->p_xstream;
    ABTI_sched *p_sched = p_xstream->p_sched;

    if (p_sched->kind != ABT_SCHED_PRIO) {
        /* Set the priority */
        p_thread->attr.prio = prio;
        ABT_mutex_unlock(p_thread->mutex);
        goto fn_exit;
    }

    /* The thread in READY needs to be moved to the appropriate queue */
    if (p_thread->state == ABT_THREAD_STATE_READY) {
        ABTI_sched_remove(p_sched, p_thread->unit);
    }

    /* Set the priority */
    p_thread->attr.prio = prio;

    if (p_thread->state == ABT_THREAD_STATE_READY) {
        ABTI_sched_push(p_sched, p_thread->unit);
    }
    ABT_mutex_unlock(p_thread->mutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_set_prio", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Get the scheduling priority of ULT.
 *
 * The \c ABT_thread_get_prio() returns the scheduling priority of the ULT
 * \c thread through \c prio.
 *
 * @param[in]  thread  handle to the target ULT
 * @param[out] prio    scheduling priority
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_get_prio(ABT_thread thread, ABT_sched_prio *prio)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    /* Return value */
    *prio = p_thread->attr.prio;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_get_prio", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Set the thread's name.
 *
 * @param[in] thread  handle to the target thread
 * @param[in] name    thread name
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_set_name(ABT_thread thread, const char *name)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    size_t len = strlen(name);
    ABTI_mutex_waitlock(p_thread->mutex);
    if (p_thread->p_name) ABTU_free(p_thread->p_name);
    p_thread->p_name = (char *)ABTU_malloc(len + 1);
    if (!p_thread->p_name) {
        ABT_mutex_unlock(p_thread->mutex);
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }
    ABTU_strcpy(p_thread->p_name, name);
    ABT_mutex_unlock(p_thread->mutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_set_name", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Return the name of ULT and its length.
 *
 * \c ABT_thread_get_name() gets the string name of target ULT and the length
 * of name in bytes. If \c name is NULL, only \c len is returned.
 * If \c name is not NULL, it should have enough space to save \c len bytes of
 * characters. If \c len is NULL, \c len is ignored.
 *
 * @param[in]  thread  handle to the target ULT
 * @param[out] name    ULT name
 * @param[out] len     the length of name in bytes
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_get_name(ABT_thread thread, char *name, size_t *len)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    if (name != NULL) {
        ABTU_strcpy(name, p_thread->p_name);
    }
    if (len != NULL) {
        *len = strlen(p_thread->p_name);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_get_name", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Get the ULT's id
 *
 * \c ABT_thread_get_id() returns the id of \c a thread.
 *
 * @param[in]  thread     handle to the target thread
 * @param[out] thread_id  thread id
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_get_id(ABT_thread thread, ABT_thread_id *thread_id)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    *thread_id = p_thread->id;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_get_id", abt_errno);
    goto fn_exit;
}


/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

ABTI_thread *ABTI_thread_get_ptr(ABT_thread thread)
{
    ABTI_thread *p_thread;
    if (thread == ABT_THREAD_NULL) {
        p_thread = NULL;
    } else {
        p_thread = (ABTI_thread *)thread;
    }
    return p_thread;
}

ABT_thread ABTI_thread_get_handle(ABTI_thread *p_thread)
{
    ABT_thread h_thread;
    if (p_thread == NULL) {
        h_thread = ABT_THREAD_NULL;
    } else {
        h_thread = (ABT_thread)p_thread;
    }
    return h_thread;
}

int ABTI_thread_create_main(ABTI_xstream *p_xstream, ABTI_thread **p_thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_newthread;

    p_newthread = (ABTI_thread *)ABTU_malloc(sizeof(ABTI_thread));
    if (!p_newthread) {
        HANDLE_ERROR("ABTU_malloc");
        *p_thread = NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    /* The main thread does not need to create context and stack.
     * And, it is not added to the scheduler's pool. */
    p_newthread->unit       = ABT_UNIT_NULL;
    p_newthread->p_xstream  = p_xstream;
    p_newthread->id         = ABTI_thread_get_new_id();
    p_newthread->p_name     = NULL;
    p_newthread->type       = ABTI_THREAD_TYPE_MAIN;
    p_newthread->state      = ABT_THREAD_STATE_RUNNING;
    p_newthread->refcount   = 0;
    p_newthread->request    = 0;
    p_newthread->p_req_arg  = NULL;

    /* Set attributes */
    ABTI_thread_set_attr(p_newthread, NULL);
    p_newthread->attr.migratable = ABT_FALSE;

    /* Create a mutex */
    abt_errno = ABT_mutex_create(&p_newthread->mutex);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABT_mutex_create");
        goto fn_fail;
    }

    *p_thread = p_newthread;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_thread_create_main", abt_errno);
    goto fn_exit;
}

int ABTI_thread_free_main(ABTI_thread *p_thread)
{
    int abt_errno = ABT_SUCCESS;

    /* Free the name */
    if (p_thread->p_name) ABTU_free(p_thread->p_name);

    /* Free the mutex */
    abt_errno = ABT_mutex_free(&p_thread->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    ABTU_free(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_thread_free_main", abt_errno);
    goto fn_exit;
}

int ABTI_thread_free(ABTI_thread *p_thread)
{
    int abt_errno = ABT_SUCCESS;

    /* Free the unit */
    if (p_thread->refcount > 0) {
        ABTI_unit_free(&p_thread->unit);
    } else {
        p_thread->p_xstream->p_sched->u_free(&p_thread->unit);
    }

    /* Free the name */
    if (p_thread->p_name) ABTU_free(p_thread->p_name);

    /* Free the mutex */
    abt_errno = ABT_mutex_free(&p_thread->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    /* Free the stack and the context */
    ABTU_free(p_thread->p_stack);
    abt_errno = ABTD_thread_context_free(&p_thread->ctx);
    ABTI_CHECK_ERROR(abt_errno);

    ABTU_free(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_thread_free", abt_errno);
    goto fn_exit;
}

int ABTI_thread_suspend(void)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_thread *p_thread = ABTI_local_get_thread();
    ABTI_xstream *p_xstream = ABTI_local_get_xstream();
    assert(p_thread->p_xstream == p_xstream);

	/* the main thread is not supposed to call thread suspend*/
    if (p_thread->type == ABTI_THREAD_TYPE_MAIN) {
		abt_errno = ABT_ERR_THREAD;
		goto fn_fail;
	}

    /* Change the state of current running thread */
    p_thread->state = ABT_THREAD_STATE_BLOCKED;

    /* Increase the number of blocked threads */
    ABTI_sched *p_sched = p_xstream->p_sched;
    ABTI_sched_inc_num_blocked(p_sched);

    /* Switch to the scheduler */
    abt_errno = ABTD_thread_context_switch(&p_thread->ctx, &p_sched->ctx);
    ABTI_CHECK_ERROR(abt_errno);

    /* Back to the original thread */
    ABTI_local_set_thread(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_suspend", abt_errno);
    goto fn_exit;
}

/**
 * This function relinquishes the processor without changing the state of the thread. Its main purpose
 * is to avoid a race condition with synchronization primitives (future, eventual, barrier).
 */
int ABTI_thread_relinquish(void)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_thread *p_thread = ABTI_local_get_thread();
    ABTI_xstream *p_xstream = ABTI_local_get_xstream();
    assert(p_thread->p_xstream == p_xstream);

	ABTI_sched *p_sched = p_thread->p_xstream->p_sched;

	/* the main thread should not call thread relinquish */
    if (p_thread->type == ABTI_THREAD_TYPE_MAIN) {
		abt_errno = ABT_ERR_THREAD;
		goto fn_fail;
    } else {
        /* Switch to the scheduler */
        abt_errno = ABTD_thread_context_switch(&p_thread->ctx, &p_sched->ctx);
        ABTI_CHECK_ERROR(abt_errno);
    }

    /* Back to the original thread */
    ABTI_local_set_thread(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_relinquish", abt_errno);
    goto fn_exit;
}

int ABTI_thread_set_ready(ABT_thread thread)
{
    int abt_errno = ABT_SUCCESS, flag = 0;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);

    if (p_thread->state == ABT_THREAD_STATE_BLOCKED) flag = 1;

    /* Add the thread to its associated ES */
    abt_errno = ABTI_xstream_add_thread(p_thread);
    ABTI_CHECK_ERROR(abt_errno);

    /* Decrease the number of blocked threads */
	if (flag) {
        ABTI_sched_dec_num_blocked(p_thread->p_xstream->p_sched);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_set_ready", abt_errno);
    goto fn_exit;
}

int ABTI_thread_set_blocked(ABT_thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    /* Change the state of current running thread */
    p_thread->state = ABT_THREAD_STATE_BLOCKED;

    /* Increase the number of blocked threads */
    ABTI_sched *p_sched = p_thread->p_xstream->p_sched;
    ABTI_sched_inc_num_blocked(p_sched);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_set_blocked", abt_errno);
    goto fn_exit;
}

int ABTI_thread_set_attr(ABTI_thread *p_thread, ABTI_thread_attr *p_attr)
{
    ABTI_thread_attr *my_attr = &p_thread->attr;
    if (p_attr) {
        my_attr->stacksize  = p_attr->stacksize;
        my_attr->prio       = p_attr->prio;
        my_attr->migratable = p_attr->migratable;
        my_attr->f_cb       = p_attr->f_cb;
        my_attr->p_cb_arg   = p_attr->p_cb_arg;
    } else {
        my_attr->stacksize  = ABTI_THREAD_DEFAULT_STACKSIZE;
        my_attr->prio       = ABT_SCHED_PRIO_NORMAL;
        my_attr->migratable = ABT_TRUE;
        my_attr->f_cb       = NULL;
        my_attr->p_cb_arg   = NULL;
    }
    return ABT_SUCCESS;
}

int ABTI_thread_print(ABTI_thread *p_thread)
{
    int abt_errno = ABT_SUCCESS;
    if (p_thread == NULL) {
        printf("[NULL THREAD]");
        goto fn_exit;
    }

    printf("[");
    printf("id:%lu ", p_thread->id);
    printf("xstream:%lu ", p_thread->p_xstream->rank);
    printf("name:%s ", p_thread->p_name);
    printf("type:");
    switch (p_thread->type) {
        case ABTI_THREAD_TYPE_MAIN: printf("MAIN "); break;
        case ABTI_THREAD_TYPE_USER: printf("USER "); break;
        default: printf("UNKNOWN "); break;
    }
    printf("state:");
    switch (p_thread->state) {
        case ABT_THREAD_STATE_READY:      printf("READY "); break;
        case ABT_THREAD_STATE_RUNNING:    printf("RUNNING "); break;
        case ABT_THREAD_STATE_BLOCKED:    printf("BLOCKED "); break;
        case ABT_THREAD_STATE_TERMINATED: printf("TERMINATED "); break;
        default: printf("UNKNOWN ");
    }
    printf("attr:");
    ABTI_thread_attr_print(&p_thread->attr);
    printf("refcount:%u ", p_thread->refcount);
    printf("request:%x ", p_thread->request);
    printf("req_arg:%p ", p_thread->p_req_arg);
    printf("stack:%p", p_thread->p_stack);
    printf("]");

  fn_exit:
    return abt_errno;
}

void ABTI_thread_func_wrapper(int func_upper, int func_lower,
                              int arg_upper, int arg_lower)
{
    void (*thread_func)(void *);
    void *p_arg;
    size_t ptr_size, int_size;

    ptr_size = sizeof(void *);
    int_size = sizeof(int);
    if (ptr_size == int_size) {
        thread_func = (void (*)(void *))(uintptr_t)func_lower;
        p_arg = (void *)(uintptr_t)arg_lower;
    } else if (ptr_size == int_size * 2) {
        uintptr_t shift_bits = CHAR_BIT * int_size;
        uintptr_t mask = ((uintptr_t)1 << shift_bits) - 1;
        thread_func = (void (*)(void *))(
                ((uintptr_t)func_upper << shift_bits) |
                ((uintptr_t)func_lower & mask));
        p_arg = (void *)(
                ((uintptr_t)arg_upper << shift_bits) |
                ((uintptr_t)arg_lower & mask));
    } else {
        assert(0);
    }

    thread_func(p_arg);

    /* Now, the ULT has finished its job. Terminate the ULT.
     * We don't need to use the atomic operation here because the ULT will be
     * terminated regardless of other requests. */
    ABTI_thread *p_thread = ABTI_local_get_thread();
    p_thread->request |= ABTI_THREAD_REQ_TERMINATE;
}


ABT_thread *ABTI_thread_current(void)
{
    return (ABT_thread *)ABTI_local_get_thread();
}

void ABTI_thread_add_req_arg(ABTI_thread *p_thread, uint32_t req, void *arg)
{
    ABTI_thread_req_arg *new;
    ABTI_thread_req_arg *p_head = p_thread->p_req_arg;

    /* Overwrite the previous same request if exists */
    while (p_head != NULL) {
        if (p_head->request == req) {
            p_head->p_arg = arg;
            return;
        }
    }

    new = (ABTI_thread_req_arg *)ABTU_malloc(sizeof(ABTI_thread_req_arg));
    assert(new != NULL);

    /* filling the new argument data structure */
    new->request = req;
    new->p_arg = arg;
    new->next = NULL;

    if (p_head == NULL) {
        p_thread->p_req_arg = new;
    } else {
        while (p_head->next != NULL)
            p_head = p_head->next;
        p_head->next = new;
    }
}

void *ABTI_thread_extract_req_arg(ABTI_thread *p_thread, uint32_t req)
{
    void *result = NULL;
    ABTI_thread_req_arg *p_last = NULL, *p_head = p_thread->p_req_arg;

    while (p_head != NULL) {
        if (p_head->request == req) {
            result = p_head->p_arg;
            if (p_last == NULL)
                p_thread->p_req_arg = p_head->next;
            else
                p_last->next = p_head->next;
            ABTU_free(p_head);
            break;
        }
        p_last = p_head;
        p_head = p_head->next;
    }

    return result;
}

void ABTI_thread_retain(ABTI_thread *p_thread)
{
    ABTD_atomic_fetch_add_uint32(&p_thread->refcount, 1);
}

void ABTI_thread_release(ABTI_thread *p_thread)
{
    uint32_t refcount;
    while ((refcount = p_thread->refcount) > 0) {
        if (ABTD_atomic_cas_uint32(&p_thread->refcount, refcount,
            refcount - 1) == refcount) {
            break;
        }
    }
}


/*****************************************************************************/
/* Internal static functions                                                 */
/*****************************************************************************/

static ABT_thread_id ABTI_thread_get_new_id(void)
{
    static uint64_t thread_id = 0;
    return (ABT_thread_id)ABTD_atomic_fetch_add_uint64(&thread_id, 1);
}

