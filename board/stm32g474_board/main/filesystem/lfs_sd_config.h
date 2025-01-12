/*
 * lfs_sd_config.h
 *
 *  Created on: Sep 4, 2024
 *      Author: Sicris Rey Embay
 */

#ifndef FILESYSTEM_LFS_SD_CONFIG_H_
#define FILESYSTEM_LFS_SD_CONFIG_H_

#include "logger_conf.h"
#include "stdint.h"
#include "stdbool.h"
#include "string.h"
#include "inttypes.h"

#if CONFIG_USE_LFS_SD

// System includes
#ifndef LFS_NO_MALLOC
#include "stdlib.h"
#endif

#ifndef LFS_NO_ASSERT
#include "assert.h"
#endif

#if CONFIG_LFS_LOG_TRACE
#define LFS_YES_TRACE
#endif

#if !defined(LFS_NO_DEBUG) || \
    !defined(LFS_NO_WARN) || \
    !defined(LFS_NO_ERROR) || \
    defined(LFS_YES_TRACE)
#include <stdio.h>
#endif

#include "logger_conf.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lpuart.h"

#define LFS_THREADSAFE      1
#define LFS_NAME_MAX        255

// Logging functions
#ifndef LFS_TRACE
#ifdef LFS_YES_TRACE
#define LFS_TRACE_(fmt, ...) \
    LPUART_printf("  %s:%d:trace: " fmt "%s\r\n", __FILE_NAME__, __LINE__, __VA_ARGS__)
#define LFS_TRACE(...) LFS_TRACE_(__VA_ARGS__, "")
#else
#define LFS_TRACE(...)
#endif
#endif

#ifndef LFS_DEBUG
#ifndef LFS_NO_DEBUG
#define LFS_DEBUG_(fmt, ...) \
    LPUART_printf("  %s:%d:debug: " fmt "%s\r\n", __FILE_NAME__, __LINE__, __VA_ARGS__)
#define LFS_DEBUG(...) LFS_DEBUG_(__VA_ARGS__, "")
#else
#define LFS_DEBUG(...)
#endif
#endif

#ifndef LFS_WARN
#ifndef LFS_NO_WARN
#define LFS_WARN_(fmt, ...) \
    LPUART_printf("  %s:%d:warn: " fmt "%s\r\n", __FILE_NAME__, __LINE__, __VA_ARGS__)
#define LFS_WARN(...) LFS_WARN_(__VA_ARGS__, "")
#else
#define LFS_WARN(...)
#endif
#endif

#ifndef LFS_ERROR
#ifndef LFS_NO_ERROR
#define LFS_ERROR_(fmt, ...) \
    LPUART_printf("  %s:%d:error: " fmt "%s\r\n", __FILE_NAME__, __LINE__, __VA_ARGS__)
#define LFS_ERROR(...) LFS_ERROR_(__VA_ARGS__, "")
#else
#define LFS_ERROR(...)
#endif
#endif

// Runtime assertions
#ifndef LFS_ASSERT
#ifndef LFS_NO_ASSERT
#define LFS_ASSERT(test) configASSERT(test)
#else
#define LFS_ASSERT(test)
#endif
#endif

// Min/max functions for unsigned 32-bit numbers
static inline uint32_t lfs_max(uint32_t a, uint32_t b) {
    return (a > b) ? a : b;
}

static inline uint32_t lfs_min(uint32_t a, uint32_t b) {
    return (a < b) ? a : b;
}

// Align to nearest multiple of a size
static inline uint32_t lfs_aligndown(uint32_t a, uint32_t alignment) {
    return a - (a % alignment);
}

static inline uint32_t lfs_alignup(uint32_t a, uint32_t alignment) {
    return lfs_aligndown(a + alignment-1, alignment);
}

// Find the smallest power of 2 greater than or equal to a
static inline uint32_t lfs_npw2(uint32_t a) {
    return 32 - __builtin_clz(a-1);
}

// Count the number of trailing binary zeros in a
// lfs_ctz(0) may be undefined
static inline uint32_t lfs_ctz(uint32_t a) {
    return __builtin_ctz(a);
}

// Count the number of binary ones in a
static inline uint32_t lfs_popc(uint32_t a) {
    return __builtin_popcount(a);
}

// Find the sequence comparison of a and b, this is the distance
// between a and b ignoring overflow
static inline int lfs_scmp(uint32_t a, uint32_t b) {
    return (int)(unsigned)(a - b);
}

// Convert between 32-bit little-endian and native order
static inline uint32_t lfs_fromle32(uint32_t a) {
    return a;
}

static inline uint32_t lfs_tole32(uint32_t a) {
    return lfs_fromle32(a);
}

// Convert between 32-bit big-endian and native order
static inline uint32_t lfs_frombe32(uint32_t a) {
    return __builtin_bswap32(a);
}

static inline uint32_t lfs_tobe32(uint32_t a) {
    return lfs_frombe32(a);
}

// Calculate CRC-32 with polynomial = 0x04c11db7
uint32_t lfs_crc(uint32_t crc, const void *buffer, size_t size);

// Allocate memory, only used if buffers are not provided to littlefs
//
// littlefs current has no alignment requirements, as it only allocates
// byte-level buffers.
static inline void *lfs_malloc(size_t size) {
    return pvPortMalloc(size);
}

// Deallocate memory, only used if buffers are not provided to littlefs
static inline void lfs_free(void *p) {
    vPortFree(p);
}

#endif /* CONFIG_USE_LFS_SD */
#endif /* FILESYSTEM_LFS_SD_CONFIG_H_ */
