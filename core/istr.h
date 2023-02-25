#ifndef ISTR_H_INCLUDED
#define ISTR_H_INCLUDED

/*
 * istr.h
 * ======
 * 
 * Reference-counted string type that is used for strings in the
 * Shastina interpreter.
 */

#include <stddef.h>

/*
 * Type declarations
 * -----------------
 */

/*
 * Structure prototype for ISTR_DATA.
 * 
 * Definition given in implementation file.
 */
struct ISTR_DATA_TAG;
typedef struct ISTR_DATA_TAG ISTR_DATA;

/*
 * The ISTR structure.
 * 
 * This structure either holds a reference to a string object or it
 * represents an empty string with zero length.
 * 
 * To avoid memory problems, initialize this structure with istr_init()
 * when you first declare it, and use istr_reset() on the structure
 * before it goes out of scope.
 * 
 * Do not access the internal structure.  Use the istr functions to
 * interact with it instead.
 */
typedef struct {
  
  /*
   * Reference to a string object, or NULL if this structure is storing
   * an empty string.
   */
  ISTR_DATA *pRef;
  
} ISTR;

/*
 * Public functions
 * ----------------
 */

/*
 * Initialize a new ISTR structure.
 * 
 * You must use this function on an ISTR structure before you can use
 * any of the other functions.  However, do not use istr_init() on an
 * ISTR structure that is already initialized or a memory leak may
 * occur!
 * 
 * Make sure that istr_reset() is called on the structure before it goes
 * out of scope or a memory leak may occur!
 * 
 * The initial state of the structure represents an empty string with no
 * characters.
 * 
 * Parameters:
 * 
 *   ps - the ISTR structure to initialize
 */
void istr_init(ISTR *ps);

/*
 * Reset an ISTR structure back to its initial state.
 * 
 * The structure must have first been initialized with istr_init().  If
 * the structure holds any string reference, the reference is released
 * and the structure returns to its initial state or referring
 * implicitly to an empty string with no characters.
 * 
 * Releasing a reference will free the string object if its reference
 * count drops to zero.
 * 
 * Use this function on ISTR structures before they go out of scope to
 * avoid memory leaks!
 * 
 * Parameters:
 * 
 *   ps - the ISTR structure to reset
 */
void istr_reset(ISTR *ps);

/*
 * Create a new string object and load a reference to the new object in
 * the given ISTR structure.
 * 
 * ps must have been initialized with istr_init().  istr_reset() is
 * automatically called on it by this function to release whatever
 * reference it is currently holding.
 * 
 * pstr is the string to copy into the new string object.  A copy is
 * made of this string so further changes to the indicated string have
 * no effect.  If pstr is an empty string, no string object will be
 * created and instead the ISTR structure will just hold an implicit
 * reference to an empty string.
 * 
 * If the configuration variable CFG_BACKSLASH is non-zero, then forward
 * slashes will be converted to backslashes in the new string object.
 * 
 * There is an implementation limit to the maximum size of a string that
 * can be stored in the string object.
 * 
 * Parameters:
 * 
 *   ps - the ISTR structure that will refer to the new object
 * 
 *   pstr - the string to copy into the new string object
 */
void istr_new(ISTR *ps, const char *pstr);

/*
 * Copy a reference from one ISTR object to another.
 * 
 * If pDest and pSrc point to the same structure, this function does
 * nothing.
 * 
 * Otherwise, istr_reset() is first called on pDest to release any
 * current reference.  Then, if pSrc has a string reference, the string
 * reference is copied into pDest and the reference count of the string
 * object is increased.
 * 
 * Parameters:
 * 
 *   pDest - the ISTR structure to copy into
 * 
 *   pSrc - the ISTR structure to copy from
 */
void istr_copy(ISTR *pDest, ISTR *pSrc);

/*
 * Concatenate two strings together into a new string.
 * 
 * pA and pB hold references to the strings to concatenate.  The pResult
 * structure will be istr_reset() and then a reference to the
 * concatenated string will be written into it.
 * 
 * It is allowed for all three pointers to refer to the same structure.
 * 
 * If pA is an empty string, then the call is implemented as istr_copy()
 * from pB into pResult.  If pB is an empty string, then the call is
 * implemented as istr_copy() from pA into pResult.  If both pA and pB
 * are empty strings, the call is implemented as istr_reset() on
 * pResult.
 * 
 * An error occurs if concatenation would cause the new string length to
 * exceed an implementation maximum string length.
 * 
 * Parameters:
 * 
 *   pResult - the ISTR structure to hold the concatenated results
 * 
 *   pA - the first ISTR to concatenate
 * 
 *   pB - the second ISTR to concatenate
 */
void istr_concat(ISTR *pResult, ISTR *pA, ISTR *pB);

/*
 * Get a pointer to the string currently referenced by an ISTR.
 * 
 * This works even if the given ISTR is in a reset state with no string
 * reference.  In this case, a pointer to a string consisting only of a
 * nul termination byte will be returned.
 * 
 * The pointer remains valid so long as at least one ISTR structure
 * holds a reference to the string object.  String objects are
 * immutable.
 * 
 * Parameters:
 * 
 *   ps - the ISTR structure
 * 
 * Return:
 * 
 *   pointer to a nul-terminated string
 */
const char *istr_ptr(ISTR *ps);

#endif
