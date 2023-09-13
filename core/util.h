#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

/*
 * util.h
 * ======
 * 
 * Utility module of Lilac.
 * 
 * Requirements
 * ------------
 * 
 * Requires the following Lilac modules:
 * 
 *   - diagnostic.c
 */

#include <stddef.h>
#include <stdint.h>

/*
 * Public functions
 * ----------------
 */

/*
 * Make room in capacity for a given number of elements in a dynamic
 * array.
 * 
 * n is the number of additional elements beyond current length to make
 * room for.  It must be zero or greater.
 * 
 * Upon successful return, the capacity will be at least n elements
 * higher than the current length.
 * 
 * pLen points to the variable that stores the current length (elements
 * actually in use) of the array, which must be zero or greater.  This
 * value will not be modified by the function.
 * 
 * pCap points to the variable that stores the current capacity of the
 * array.  It must be greater than or equal to the length indicated by
 * the pLen parameter.  This value will be modified if necessary by the
 * function.
 * 
 * init_cap is the initial capacity in elements for the array when the
 * array is not allocated yet.  It must be greater than zero.
 * 
 * max_cap is the maximum capacity in elements for the array.  It must
 * be greater than or equal to init_cap.  If the requested expansion of
 * the array given by n would go over this maximum, the function fails
 * and returns NULL.
 * 
 * max_cap may not be so large that multiplying max_cap and esize
 * results in a value that overflows int32_t.
 * 
 * esize is the size of each element in the array, which must be greater
 * than zero.
 * 
 * pBuf is the currently allocated buffer, which must be NULL if and
 * only if pCap indicates the current array capacity is zero.
 * 
 * The return value is the allocated buffer pointer, which might be
 * different from pBuf if the buffer was expanded.  It will be NULL if
 * the operation failed because the maximum capacity was exceeded.
 * 
 * Parameters:
 * 
 *   n - the number of elements to make room for
 * 
 *   pLen - pointer to the current length variable of the array
 * 
 *   pCap - pointer to the current capacity variable of the array
 * 
 *   init_cap - the initial capacity for the array
 * 
 *   max_cap - the maximum capacity for the array
 * 
 *   esize - the size in bytes of each element in the array
 * 
 *   pBuf - the current buffer pointer, which might be NULL
 * 
 * Return:
 * 
 *   the updated buffer pointer, or NULL if the maximum capacity was
 *   exceeded
 */
void *util_capArray(
          int32_t   n,
    const int32_t * pLen,
          int32_t * pCap,
          int32_t   init_cap,
          int32_t   max_cap,
          size_t    esize,
          void    * pBuf);

/*
 * If the given line number is within valid range, return it as-is.  In
 * all other cases, return -1.
 * 
 * Parameters:
 * 
 *   lnum - the candidate line number
 * 
 * Return:
 * 
 *   the line number as-is if valid, else -1
 */
long util_lnum(long lnum);

#endif
