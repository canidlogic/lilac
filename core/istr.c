/*
 * istr.c
 * ======
 * 
 * Implementation of istr.h
 * 
 * See the header for further information.
 */

#include "istr.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lilac.h"

/*
 * Constants
 * ---------
 */

/*
 * The maximum length in bytes of a string stored in a string object,
 * not including the terminating nul byte.
 */
#define MAX_DATA_LEN (16383)

/*
 * Data type declarations
 * ----------------------
 */

/*
 * ISTR_DATA structure.
 * 
 * Prototype given in the header.
 */
struct ISTR_DATA_TAG {
  
  /*
   * The reference count of this string object.
   * 
   * Starts out at one.  When it drops to zero, the string object should
   * be released.
   */
  int32_t refcount;
  
  /*
   * The length of this string in characters, not including the
   * terminating nul.
   * 
   * Must be at least one, since empty strings are stored implicitly.
   */
  int32_t slen;
  
  /*
   * The actual nul-terminated string.
   * 
   * The buffer declared here only is able to store a one-character
   * string and the nul termination byte.  Longer strings are extended
   * beyond the end of the structure.
   */
  char s[2];
};

/*
 * Local data
 * ----------
 */

/*
 * Zero-length string (just the nul termination byte) that is used for
 * all implicit empty strings.
 */
static char m_zero = (char) 0;

/*
 * Local functions
 * ---------------
 */

/* Prototypes */
static void raiseErr(int sourceLine);

/*
 * Stop on an error.
 * 
 * Use __LINE__ for the argument so that the position of the error will
 * be reported.
 * 
 * This function will not return.
 * 
 * Parameters:
 * 
 *   sourceLine - the line number in the source file the error happened
 */
static void raiseErr(int sourceLine) {
  raiseErrGlobal(sourceLine, __FILE__);
}

/*
 * Public function implementations
 * -------------------------------
 * 
 * See the header for specifications.
 */

/*
 * istr_init function.
 */
void istr_init(ISTR *ps) {
  /* Check parameters */
  if (ps == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Initialize structure */
  memset(ps, 0, sizeof(ISTR));
  ps->pRef = NULL;
}

/*
 * istr_reset function.
 */
void istr_reset(ISTR *ps) {
  /* Check parameters */
  if (ps == NULL) {
    raiseErr(__LINE__);
  }
  
  /* If currently holding a reference, release it */
  if (ps->pRef != NULL) {
    /* Decrement reference count */
    ((ps->pRef)->refcount)--;
    
    /* If reference count has dropped below one, free the structure */
    if ((ps->pRef)->refcount < 1) {
      free(ps->pRef);
    }
    
    /* Clear the pointer */
    ps->pRef = NULL;
  }
}

/*
 * istr_new function.
 */
void istr_new(ISTR *ps, const char *pstr) {
  
  size_t slen = 0;
  ISTR_DATA *pData = NULL;
  char *pc = NULL;
  
  /* Check parameters */
  if ((ps == NULL) || (pstr == NULL)) {
    raiseErr(__LINE__);
  }
  
  /* Get size of string excluding nul and check range */
  slen = strlen(pstr);
  if (slen > MAX_DATA_LEN) {
    fprintf(stderr, "%s: String object is too large!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Reset the ISTR structure */
  istr_reset(ps);
  
  /* Only proceed if at least one character in string; otherwise, use an
   * implicit empty string */
  if (slen > 0) {
    
    /* Allocate the string object with extra length to hold characters
     * beyond the first character */
    pData = (ISTR_DATA *) calloc(1, sizeof(ISTR_DATA) + slen - 1);
    if (pData == NULL) {
      fprintf(stderr, "%s: Out of memory!\n", getModule());
      raiseErr(__LINE__);
    }
    
    /* Initialize structure and copy string in */
    pData->refcount = 1;
    pData->slen = (int32_t) slen;
    strcpy(pData->s, pstr);
    
    /* If configured to do so, change forward slashes to backslashes in
     * the copied string */
    if (getConfigInt(CFG_BACKSLASH)) {
      for(pc = pData->s; *pc != 0; pc++) {
        if (*pc == '/') {
          *pc = (char) '\\';
        }
      }
    }
    
    /* Store pointer to this new object in ISTR structure */
    ps->pRef = pData;
  }
}

/*
 * istr_copy function.
 */
void istr_copy(ISTR *pDest, ISTR *pSrc) {
  /* Check parameters */
  if ((pDest == NULL) || (pSrc == NULL)) {
    raiseErr(__LINE__);
  }
  
  /* Only proceed if two pointers are to different structures */
  if (pDest != pSrc) {
    /* Release destination */
    istr_reset(pDest);
    
    /* Only proceed if source is not an empty string */
    if (pSrc->pRef != NULL) {
    
      /* Copy source into destination */
      pDest->pRef = pSrc->pRef;
      
      /* Increase reference count, watching for overflow */
      if ((pDest->pRef)->refcount < INT32_MAX) {
        ((pDest->pRef)->refcount)++;
      } else {
        fprintf(stderr, "%s: Reference count overflow!\n", getModule());
        raiseErr(__LINE__);
      }
    }
  }
}

/*
 * istr_concat function.
 */
void istr_concat(ISTR *pResult, ISTR *pA, ISTR *pB) {
  
  int32_t sfull = 0;
  ISTR_DATA *pData = NULL;
  
  /* Check parameters */
  if ((pResult == NULL) || (pA == NULL) || (pB == NULL)) {
    raiseErr(__LINE__);
  }
  
  /* Handle different cases */
  if ((pA->pRef == NULL) && (pB->pRef == NULL)) {
    /* Concatenating two empty strings, so just reset result to empty
     * string */
    istr_reset(pResult);
    
  } else if (pA->pRef == NULL) {
    /* A is empty but B is not, so just copy B into result */
    istr_copy(pResult, pB);
    
  } else if (pB->pRef == NULL) {
    /* B is empty but A is not, so just copy A into result */
    istr_copy(pResult, pA);
    
  } else {
    /* Both A and B are non-empty, so we need to compute the full
     * length of the combined string */
    if ((pA->pRef)->slen <= INT32_MAX - (pB->pRef)->slen) {
      sfull = (pB->pRef)->slen + (pB->pRef)->slen;
      
    } else {
      fprintf(stderr, "%s: String is too large!\n", getModule());
      raiseErr(__LINE__);
    }
    
    /* Check full length of concatenated string is within range */
    if (sfull > MAX_DATA_LEN) {
      fprintf(stderr, "%s: String is too large!\n", getModule());
      raiseErr(__LINE__);
    }
    
    /* Allocate the string object with extra length to hold full
     * concatenated result */
    pData = (ISTR_DATA *) calloc(
              1, sizeof(ISTR_DATA) + ((size_t) sfull) - 1);
    if (pData == NULL) {
      fprintf(stderr, "%s: Out of memory!\n", getModule());
      raiseErr(__LINE__);
    }
    
    /* Initialize structure and create concatenated result */
    pData->refcount = 1;
    pData->slen = sfull;
    
    strcpy(pData->s, (pA->pRef)->s);
    strcat(pData->s, (pB->pRef)->s);
    
    /* Reset result structure and store pointer to this new object */
    istr_reset(pResult);
    pResult->pRef = pData;
  }
}

/*
 * istr_ptr function.
 */
const char *istr_ptr(ISTR *ps) {
  
  const char *pResult = NULL;
  
  /* Check parameter */
  if (ps == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Get the result and return it */
  if (ps->pRef == NULL) {
    pResult = &m_zero;
  } else {
    pResult = (ps->pRef)->s;
  }
  
  return pResult;
}
