#ifndef ATOM_H_INCLUDED
#define ATOM_H_INCLUDED

/*
 * atom.h
 * ======
 * 
 * Atom manager of Lilac.
 * 
 * Requirements
 * ------------
 * 
 * Requires the following Lilac modules:
 * 
 *   - diagnostic.c
 *   - util.c
 * 
 * Requires the following external libraries:
 * 
 *   - librfdict
 */

#include <stddef.h>
#include <stdint.h>

/*
 * Public functions
 * ----------------
 */

/*
 * Convert a name identifier string to a numeric atom code.
 * 
 * If the given name identifier has already been assigned a numeric atom
 * code, the assigned atom code is returned.  Otherwise, a new
 * assignment is made the new assignment is returned.
 * 
 * An error will occur if the given name identifier is not in the valid
 * Lilac name identifier format.
 * 
 * Parameters:
 * 
 *   pKey - the name identifier to convert to an atom
 * 
 *   lnum - the Shastina line number for diagnostics
 * 
 * Return:
 * 
 *   the numeric atom code, which is always zero or greater
 */
int32_t atom_get(const char *pKey, long lnum);

/*
 * Retrieve the name identifier associated with a particular numeric
 * atom code.
 * 
 * An error occurs if the given code is not a registered atom code.
 * 
 * Parameters:
 * 
 *   code - the numeric atom code
 * 
 * Return:
 * 
 *   the name identifier associated with this code
 */
const char *atom_str(int32_t code);

#endif
