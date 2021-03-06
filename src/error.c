/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


/** @defgroup ERROR Error
 * This group is for error classes.
 */

/**
 * @ingroup ERROR
 * @brief   Get the string of error code and its length.
 *
 * \c ABT_error_get_str() returns the string of given error code through
 * \c str and its length in bytes via \c len. If \c str is NULL, only \c len
 * is returned. If \c str is not NULL, it should have enough space to save
 * \c len bytes of characters. If \c len is NULL, \c len is ignored.
 *
 * @param[in]  errno  error code
 * @param[out] str    error string
 * @param[out] len    the length of string in bytes
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_error_get_str(int errno, char *str, size_t *len)
{
    static const char *err_str[] = {
        "ABT_SUCCESS",
        "ABT_ERR_MEM",
        "ABT_ERR_INV_XSTREAM",
        "ABT_ERR_INV_THREAD",
        "ABT_ERR_INV_THREAD_ATTR",
        "ABT_ERR_INV_TASK",
        "ABT_ERR_INV_SCHED",
        "ABT_ERR_INV_SCHED_KIND",
        "ABT_ERR_INV_SCHED_PRIO",
        "ABT_ERR_INV_UNIT",
        "ABT_ERR_INV_POOL",
        "ABT_ERR_INV_MUTEX",
        "ABT_ERR_INV_COND",
        "ABT_ERR_INV_EVENTUAL",
        "ABT_ERR_INV_FUTURE",
        "ABT_ERR_XSTREAM",
        "ABT_ERR_THREAD",
        "ABT_ERR_TASK",
        "ABT_ERR_SCHED",
        "ABT_ERR_UNIT",
        "ABT_ERR_POOL",
        "ABT_ERR_MUTEX_LOCKED",
        "ABT_ERR_MUTEX",
        "ABT_ERR_COND",
        "ABT_ERR_EVENTUAL",
        "ABT_ERR_FUTURE",
        "ABT_ERR_UNINITIALIZED",
        "ABT_ERR_OTHER"
    };

    int abt_errno = ABT_SUCCESS;
    if (errno < ABT_SUCCESS || errno > ABT_ERR_OTHER) {
        abt_errno = ABT_ERR_OTHER;
        goto fn_fail;
    }
    if (str) ABTU_strcpy(str, err_str[errno]);
    if (len) *len = strlen(err_str[errno]);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_error_get_str", abt_errno);
    goto fn_exit;
}

