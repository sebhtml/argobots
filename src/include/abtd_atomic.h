/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTD_ATOMIC_H_INCLUDED
#define ABTD_ATOMIC_H_INCLUDED

#include <stdint.h>

static inline
int32_t ABTD_atomic_cas_int32(int32_t *ptr, int32_t oldv, int32_t newv)
{
    return __sync_val_compare_and_swap(ptr, oldv, newv);
}

static inline
uint32_t ABTD_atomic_cas_uint32(uint32_t *ptr, uint32_t oldv, uint32_t newv)
{
    return __sync_val_compare_and_swap(ptr, oldv, newv);
}

static inline
int64_t ABTD_atomic_cas_int64(int64_t *ptr, int64_t oldv, int64_t newv)
{
    return __sync_val_compare_and_swap(ptr, oldv, newv);
}

static inline
uint64_t ABTD_atomic_cas_uint64(uint64_t *ptr, uint64_t oldv, uint64_t newv)
{
    return __sync_val_compare_and_swap(ptr, oldv, newv);
}

static inline
int32_t ABTD_atomic_fetch_add_int32(int32_t *ptr, int32_t v)
{
    return __sync_fetch_and_add(ptr, v);
}

static inline
uint32_t ABTD_atomic_fetch_add_uint32(uint32_t *ptr, uint32_t v)
{
    return __sync_fetch_and_add(ptr, v);
}

static inline
int64_t ABTD_atomic_fetch_add_int64(int64_t *ptr, int64_t v)
{
    return __sync_fetch_and_add(ptr, v);
}

static inline
uint64_t ABTD_atomic_fetch_add_uint64(uint64_t *ptr, uint64_t v)
{
    return __sync_fetch_and_add(ptr, v);
}

static inline
int32_t ABTD_atomic_fetch_sub_int32(int32_t *ptr, int32_t v)
{
    return __sync_fetch_and_sub(ptr, v);
}

static inline
uint32_t ABTD_atomic_fetch_sub_uint32(uint32_t *ptr, uint32_t v)
{
    return __sync_fetch_and_sub(ptr, v);
}

static inline
int64_t ABTD_atomic_fetch_sub_int64(int64_t *ptr, int64_t v)
{
    return __sync_fetch_and_sub(ptr, v);
}

static inline
uint64_t ABTD_atomic_fetch_sub_uint64(uint64_t *ptr, uint64_t v)
{
    return __sync_fetch_and_sub(ptr, v);
}

static inline
int32_t ABTD_atomic_fetch_and_int32(int32_t *ptr, int32_t v)
{
    return __sync_fetch_and_and(ptr, v);
}

static inline
uint32_t ABTD_atomic_fetch_and_uint32(uint32_t *ptr, uint32_t v)
{
    return __sync_fetch_and_and(ptr, v);
}

static inline
int64_t ABTD_atomic_fetch_and_int64(int64_t *ptr, int64_t v)
{
    return __sync_fetch_and_and(ptr, v);
}

static inline
uint64_t ABTD_atomic_fetch_and_uint64(uint64_t *ptr, uint64_t v)
{
    return __sync_fetch_and_and(ptr, v);
}

static inline
int32_t ABTD_atomic_fetch_or_int32(int32_t *ptr, int32_t v)
{
    return __sync_fetch_and_or(ptr, v);
}

static inline
uint32_t ABTD_atomic_fetch_or_uint32(uint32_t *ptr, uint32_t v)
{
    return __sync_fetch_and_or(ptr, v);
}

static inline
int64_t ABTD_atomic_fetch_or_int64(int64_t *ptr, int64_t v)
{
    return __sync_fetch_and_or(ptr, v);
}

static inline
uint64_t ABTD_atomic_fetch_or_uint64(uint64_t *ptr, uint64_t v)
{
    return __sync_fetch_and_or(ptr, v);
}

static inline
int32_t ABTD_atomic_fetch_xor_int32(int32_t *ptr, int32_t v)
{
    return __sync_fetch_and_xor(ptr, v);
}

static inline
uint32_t ABTD_atomic_fetch_xor_uint32(uint32_t *ptr, uint32_t v)
{
    return __sync_fetch_and_xor(ptr, v);
}

static inline
int64_t ABTD_atomic_fetch_xor_int64(int64_t *ptr, int64_t v)
{
    return __sync_fetch_and_xor(ptr, v);
}

static inline
uint64_t ABTD_atomic_fetch_xor_uint64(uint64_t *ptr, uint64_t v)
{
    return __sync_fetch_and_xor(ptr, v);
}

static inline
int32_t ABTD_atomic_fetch_nand_int32(int32_t *ptr, int32_t v)
{
    return __sync_fetch_and_nand(ptr, v);
}

static inline
uint32_t ABTD_atomic_fetch_nand_uint32(uint32_t *ptr, uint32_t v)
{
    return __sync_fetch_and_nand(ptr, v);
}

static inline
int64_t ABTD_atomic_fetch_nand_int64(int64_t *ptr, int64_t v)
{
    return __sync_fetch_and_nand(ptr, v);
}

static inline
uint64_t ABTD_atomic_fetch_nand_uint64(uint64_t *ptr, uint64_t v)
{
    return __sync_fetch_and_nand(ptr, v);
}

#endif /* ABTD_ATOMIC_H_INCLUDED */
