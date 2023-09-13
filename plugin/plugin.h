#ifndef PLUGIN_H_INCLUDED
#define PLUGIN_H_INCLUDED

/*
 * plugin.h
 * ========
 * 
 * Plug-in registry for Lilac.
 * 
 * Requirements
 * ------------
 * 
 * Requires the following Lilac modules:
 * 
 *   - atom.c
 */

#include <stddef.h>
#include <stdint.h>

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
 * This checks whether a given requirement is supported by this build of
 * Lilac.
 * 
 * ra is the atom of the requirement that is being checked for.
 * 
 * Parameters:
 * 
 *   ra - the requirement atom
 * 
 * Return:
 * 
 *   non-zero if requirement is supported, zero if not
 */
int plugin_supports(int32_t ra);

#endif
