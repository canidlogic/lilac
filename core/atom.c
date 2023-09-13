/*
 * atom.c
 * ======
 * 
 * Implementation of atom.h
 * 
 * See the header for further information.
 */

#include "atom.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "diagnostic.h"
#include "util.h"

#include "rfdict.h"

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
 * Constants
 * ---------
 */

/*
 * The initial and maximum capacity of the atom table.
 */
#define ATOM_INIT_CAP (64)
#define ATOM_MAX_CAP  (16384)

/*
 * Local data
 * ----------
 */

/*
 * The mapping of name identifiers to their numeric atom codes.
 */
static RFDICT *m_map = NULL;

/*
 * Table where the index is the numeric atom code and the value is a
 * pointer to a string holding the Lilac name identifier.
 */
static int32_t m_table_len = 0;
static int32_t m_table_cap = 0;
static char **m_table = NULL;

/*
 * Local functions
 * ---------------
 */

/* Prototypes */
static void capTable(int32_t n);
static int validNameChar(int c);
static int validName(const char *pstr);

/*
 * Wrapper around util_capArray() for the atom table.
 */
static void capTable(int32_t n) {
  m_table = util_capArray(
              n,
              &m_table_len,
              &m_table_cap,
              ATOM_INIT_CAP,
              ATOM_MAX_CAP,
              sizeof(char *),
              m_table);
  if (m_table == NULL) {
    raiseErr(__LINE__, "Atom table capacity exceeded");
  }
}

/*
 * Check whether a given character code is for a character that is valid
 * within a Lilac name identifier.
 * 
 * The only valid characters are ASCII alphanumerics, underscores, and
 * period.
 * 
 * Parameters:
 * 
 *   c - the character to check
 * 
 * Return:
 * 
 *   non-zero if a valid character in a Lilac name identifier; zero
 *   otherwise
 */
static int validNameChar(int c) {
  
  int result = 0;
  
  if ((c >= 'A') && (c <= 'Z')) {
    result = 1;
  
  } else if ((c >= 'a') && (c <= 'z')) {
    result = 1;
    
  } else if ((c >= '0') && (c <= '9')) {
    result = 1;
    
  } else if ((c == '_') || (c == '.')) {
    result = 1;
    
  } else {
    result = 0;
  }
  
  return result;
}

/*
 * Check whether a given string is a valid Lilac name identifier.
 * 
 * The length of the name must be in range 1 to 255.  Each character
 * must pass validNameChar().  Neither the first nor last character may
 * be a period, and a period may not occur immediately after another
 * period.
 * 
 * Parameters:
 * 
 *   pstr - the string to check
 * 
 * Return:
 * 
 *   non-zero if valid, zero if invalid
 */
static int validName(const char *pstr) {
  
  int result = 1;
  int32_t i = 0;
  int c = 0;
  size_t slen = 0;
  
  if (pstr == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  slen = strlen(pstr);
  if ((slen < 1) || (slen > 255)) {
    result = 0;
  }
  
  if (result) {
    for(i = 0; i < slen; i++) {
      c = pstr[i];
      if (!validNameChar(c)) {
        result = 0;
        break;
      }
      if ((i <= 0) || (i >= slen - 1)) {
        if (c == '.') {
          result = 0;
          break;
        }
        
      } else {
        if (c == '.') {
          if (pstr[i - 1] == '.') {
            result = 0;
            break;
          }
        }
      }
    }
  }
  
  return result;
}

/*
 * Public function implementations
 * -------------------------------
 * 
 * See the header for specifications.
 */

/*
 * atom_get function.
 */
int32_t atom_get(const char *pKey, long lnum) {
  
  int32_t result = 0;
  char *pCopy = NULL;
  
  /* Check parameters */
  if (pKey == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  if (!validName(pKey)) {
    raiseErr(__LINE__,
      "Invalid atom identifier '%s' on script line %ld",
      pKey, util_lnum(lnum));
  }
  
  /* Allocate mapping if necessary */
  if (m_map == NULL) {
    m_map = rfdict_alloc(1);
  }
  
  /* Query for the atom */
  result = (int32_t) rfdict_get(m_map, pKey, -1);
  
  /* If we didn't find a key, add it and update the result */
  if (result < 0) {
    /* Determine new key value */
    result = m_table_len;
    
    /* Add mapping */
    if (!rfdict_insert(m_map, pKey, (long) result)) {
      raiseErr(__LINE__, NULL);
    }
    
    /* Make a copy of the string */
    pCopy = (char *) malloc(strlen(pKey) + 1);
    if (pCopy == NULL) {
      raiseErr(__LINE__, "Out of memory");
    }
    strcpy(pCopy, pKey);
    
    /* Add to table */
    capTable(1);
    m_table[m_table_len] = pCopy;
    m_table_len++;
  }
  
  /* Return the atom key */
  return result;
}

/*
 * atom_str function.
 */
const char *atom_str(int32_t code) {
  if ((code < 0) || (code >= m_table_len)) {
    raiseErr(__LINE__, "Invalid atom code");
  }
  return m_table[code];
}
