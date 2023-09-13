/*
 * util.c
 * ======
 * 
 * Implementation of util.h
 * 
 * See the header for further information.
 */

#include "util.h"

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "diagnostic.h"

/*
 * Diagnostics
 * -----------
 */

static void raiseErr(int lnum, const char *pDetail, ...) {
  va_list ap;
  va_start(ap, pDetail);
  diagnostic_global(1, __FILE__, lnum, pDetail, ap);
  va_end(ap);
}

static void sayWarn(int lnum, const char *pDetail, ...) {
  va_list ap;
  va_start(ap, pDetail);
  diagnostic_global(0, __FILE__, lnum, pDetail, ap);
  va_end(ap);
}

/*
 * Public function implementations
 * -------------------------------
 * 
 * See the header for specifications.
 */

/*
 * util_capArray function.
 */
void *util_capArray(
          int32_t   n,
    const int32_t * pLen,
          int32_t * pCap,
          int32_t   init_cap,
          int32_t   max_cap,
          size_t    esize,
          void    * pBuf) {
  
  int32_t target = 0;
  int32_t new_cap = 0;
  char *pc = NULL;
  
  /* Check parameters */
  if ((pLen == NULL) || (pCap == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  if (n < 0) {
    raiseErr(__LINE__, NULL);
  }
  if ((*pLen < 0) || (*pCap < *pLen)) {
    raiseErr(__LINE__, NULL);
  }
  if ((init_cap < 1) || (max_cap < init_cap)) {
    raiseErr(__LINE__, NULL);
  }
  if (esize < 1) {
    raiseErr(__LINE__, NULL);
  }
  if (*pCap > 0) {
    if (pBuf == NULL) {
      raiseErr(__LINE__, NULL);
    }
  } else {
    if (pBuf != NULL) {
      raiseErr(__LINE__, NULL);
    }
  }
  
  if (esize > INT32_MAX) {
    raiseErr(__LINE__, NULL);
  }
  if (max_cap > INT32_MAX / ((int32_t) esize)) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Make initial allocation if necessary -- we do this even if the
   * requested n is zero because we can only use a NULL return for an
   * error indicator */
  if (*pCap < 1) {
    pBuf = calloc((size_t) init_cap, esize);
    if (pBuf == NULL) {
      raiseErr(__LINE__, "Out of memory");
    }
    
    *pCap = init_cap;
    *pLen = 0;
  }
  
  /* Only proceed if n is non-zero */
  if (n > 0) {
    
    /* Compute target length */
    if (n <= INT32_MAX - *pLen) {
      target = *pLen + n;
    } else {
      return NULL;
    }
    
    /* Only proceed if target length exceeds current capacity */
    if (target > *pCap) {
      /* Check that target within maximum capacity */
      if (target > max_cap) {
        return NULL;
      }
      
      /* Compute new capacity by doubling current capacity until greater
       * than or equal to target length, and then limiting to maximum
       * capacity */
      new_cap = *pCap;
      while (new_cap < target) {
        new_cap *= 2;
      }
      if (new_cap > max_cap) {
        new_cap = max_cap;
      }
      
      /* Expand capacity */
      pBuf = realloc(pBuf, ((size_t) new_cap) * esize);
      if (pBuf == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      pc = (char *) pBuf;
      
      memset(
        &(pc[*pCap * ((int32_t) esize)]),
        0,
        ((size_t) (new_cap - *pCap)) * esize);
      
      *pCap = new_cap;
    }
  }
  
  /* Return updated buffer pointer */
  return pBuf;
}

/*
 * util_lnum function.
 */
long util_lnum(long lnum) {
  if ((lnum < 1) || (lnum >= LONG_MAX)) {
    lnum = -1;
  }
  return lnum;
}
