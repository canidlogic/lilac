#ifndef PLUGIN_H_INCLUDED
#define PLUGIN_H_INCLUDED

/*
 * plugin.h
 * ========
 * 
 * Plug-in registry for Lilac.
 */

#include <stddef.h>

/*
 * The plug-in initialization function.
 * 
 * This calls the registration functions of all plug-in modules so that
 * they can register themselves with Lilac core.
 */
void plugin_init(void);

/*
 * The requirement support function.
 * 
 * This checks whether a given requirement is supported by the plug-in
 * modules that are registered with plugin_init().  pReq is the
 * requirement string, which must be present.  pVer is the version
 * string for the requirement, which is optional and may be NULL.
 * 
 * lnum is the line number from the Shastina script, where the
 * declaration was made.  This is used for diagnostic messages.
 * 
 * Parameters:
 * 
 *   pReq - the requirement string
 * 
 *   pVer - the version string, or NULL
 * 
 *   lnum - the script line for the requirement declaration
 * 
 * Return:
 * 
 *   non-zero if requirement is supported, zero if not
 */
int plugin_supports(const char *pReq, const char *pVer, long lnum);

#endif
