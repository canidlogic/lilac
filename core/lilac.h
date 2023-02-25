#ifndef LILAC_H_INCLUDED
#define LILAC_H_INCLUDED

/*
 * lilac.h
 * =======
 * 
 * Public functions exported by the main program module.  These cover
 * diagnostic and configuration functions, as well as a wrapper around
 * the Shastina snparser_read() function that integrates Lilac
 * diagnostics.
 * 
 * See the lilac.c for the main program documentation.
 */

#include <stddef.h>
#include "shastina.h"

/*
 * Diagnostic and error functions
 * ------------------------------
 */

/*
 * Return the name of the current executable module, for use in
 * diagnostic messages.
 * 
 * Return:
 * 
 *   the executable module name
 */
const char *getModule(void);

/*
 * Stop on an error.
 * 
 * Use __LINE__ for the first argument and __FILE__ for the second
 * argument.
 * 
 * This function will not return.
 * 
 * Parameters:
 * 
 *   sourceLine - the line number in the source file the error happened
 * 
 *   sourceFile - the source file in which the error happened
 */
void raiseErrGlobal(int sourceLine, const char *sourceFile);

/*
 * Report a line number if available.
 * 
 * If a valid line number was read on the most recent call to
 * readEntity(), then this function will report the line number to
 * standard error.  If readEntity() has not been called or no valid line
 * number was read, then this function does nothing.
 */
void reportLine(void);

/*
 * Shastina parsing wrapper
 * ------------------------
 */

/*
 * Wrapper around snparser_read() function that detects and reports
 * parsing errors.
 * 
 * If this returns, the entity status is guaranteed not to be negative.
 * 
 * This wrapper updates an state variable that keeps track of the line
 * number of the most recent entity, which is used by reportLine(), so
 * only use this wrapper and don't bypass and use snparser_read()
 * directly.
 * 
 * Parameters:
 * 
 *   pp - the Shastina parser
 * 
 *   pEnt - the Shastina entity
 * 
 *   pSrc - the Shastina source
 */
void readEntity(SNPARSER *pp, SNENTITY *pEnt, SNSOURCE *pSrc);

/*
 * Configuration constants
 * -----------------------
 * 
 * These values must be passed to getConfigInt() or getConfigStr() to
 * get their values.
 */

/*
 * (Integer) Non-zero if platform is Win32, zero if POSIX.
 */
#define CFG_WIN32 (1)

/*
 * (Integer) Non-zero if forward slashes in paths should be translated
 * into backslashes, zero if not.
 */
#define CFG_BACKSLASH (2)

/*
 * (Integer) Width in pixels of output image.
 */
#define CFG_DIM_WIDTH (3)

/*
 * (Integer) Height in pixels of output image.
 */
#define CFG_DIM_HEIGHT (4)

/*
 * (Integer) Number of MiB to use for disk buffer of external images.
 */
#define CFG_EXTERNAL_DISK_MIB (5)

/*
 * (Integer) Number of KiB to use for memory buffer of external images.
 */
#define CFG_EXTERNAL_RAM_KIB (6)

/*
 * (Integer) Maximum invocation depth of graph nodes.
 */
#define CFG_GRAPH_DEPTH (7)

/*
 * (Integer) Maximum height of Shastina interpreter stack.
 */
#define CFG_STACK_HEIGHT (8)

/*
 * (Integer) Maximum number of declared Shastina variables and
 * constants.
 */
#define CFG_NAME_LIMIT (9)

/*
 * (String) Path to output file to render.
 */
#define CFG_OUT_PATH (10)

/*
 * All configuration indices must be less than or equal to this value.
 */
#define CFG_MAX_INDEX (16)

/*
 * Configuration function
 * ----------------------
 */

/*
 * Get a configuration parameter integer value.
 * 
 * cfg is a configuration constant that refers to an integer value.
 * 
 * Parameters:
 * 
 *   cfg - the configuration constant to query
 * 
 * Return:
 * 
 *   the integer value
 */
int32_t getConfigInt(int cfg);

/*
 * Get a configuration parameter string value.
 * 
 * cfg is a configuration constant that refers to a string value.
 * 
 * Parameters:
 * 
 *   cfg - the configuration constant to query
 * 
 * Return:
 * 
 *   the string value
 */
const char *getConfigStr(int cfg);

#endif
