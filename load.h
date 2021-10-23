#ifndef LOAD_H_INCLUDED
#define LOAD_H_INCLUDED

/*
 * load.h
 * ======
 * 
 * Load script interpreter module of Lilac.
 */

#include <stddef.h>
#include <stdint.h>

/*
 * Error codes.
 * 
 * Negative codes are reserved for Shastina error codes.  Zero means no
 * error.  Use load_errstr() to get an error message.
 * 
 * Remember to update load_errstr() when changing these constants!
 */
#define LOAD_ERR_OK     (0)   /* No error */

/*
 * Metavariable atoms.
 * 
 * These are used to select metavariable values in the load_metavar_
 * functions.
 */
#define LOAD_MV_WIDTH   (1)
#define LOAD_MV_HEIGHT  (2)
#define LOAD_MV_RSCRIPT (3)
#define LOAD_MV_RFUNC   (4)

/*
 * Interpret a load script file.
 * 
 * This function may only be called once.  Calling more than once will
 * result in a fault.
 * 
 * pPath is the path to the load script file to interpret.
 * 
 * perr points to a variable to receive an error code.  This is one of
 * the LOAD_ERR constants, or a Shastina error code if it is negative.
 * You can use load_errstr() to get an error message from this code.
 * 
 * pline points to a variable to receive a line number that the error
 * happened at, or zero if there is no line number information.
 * 
 * If the function succeeds, *perr is set to LOAD_ERR_OK and the *pline
 * is set to zero.
 * 
 * During interpretation, this module will invoke other modules as
 * necessary to load appropriate resources.  After the function returns
 * successfully, the load_metavar_ functions can be used to get header
 * parameter values.
 * 
 * Parameters:
 * 
 *   pPath - the path to the load script file to run
 * 
 *   perr - points to a variable to receive an error code
 * 
 *   pline - points to a variable to receive a line number
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int load_run(const char *pPath, int *perr, long *pline);

/*
 * Get the string value of a metavariable.
 * 
 * This function may only be called after a successful return from the
 * load_run() function or else a fault occurs.
 * 
 * atom must be one of the LOAD_MV constants, and it must further be for
 * a metavariable of the string type.
 * 
 * Parameters:
 * 
 *   atom - the metavariable to query
 * 
 * Return:
 * 
 *   the string value of the metavariable
 */
const char *load_metavar_str(int atom);

/*
 * Get the integer value of a metavariable.
 * 
 * This function may only be called after a successful return from the
 * load_run() function or else a fault occurs.
 * 
 * atom must be one of the LOAD_MV constants, and it must further be for
 * a metavariable of the integer type.
 * 
 * Parameters:
 * 
 *   atom - the metavariable to query
 * 
 * Return:
 * 
 *   the integer value of the metavariable
 */
int32_t load_metavar_int(int atom);

/*
 * Convert an error code received from load_run() into an error message.
 * 
 * The error message will begin with a capital letter but not have any
 * punctuation or line break at the end.
 * 
 * For negative error codes, this function will call through to Shastina
 * to get the appropriate error message.
 * 
 * Unrecognized error codes will return "Unknown error" while a value of
 * zero (LOAD_ERR_OK) will return "No error".
 * 
 * Parameters:
 * 
 *   code - the error code to translate
 * 
 * Return:
 * 
 *   an error message for the code
 */
const char *load_errstr(int code);

#endif
