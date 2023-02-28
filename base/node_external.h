#ifndef NODE_EXTERNAL_H_INCLUDED
#define NODE_EXTERNAL_H_INCLUDED

/*
 * node_external.h
 * ===============
 * 
 * Defines a node that provides pixel data read from external PNG image
 * files.
 * 
 * Each external PNG image must have dimensions exactly matching the
 * output dimensions configured in the rendering script.
 * 
 * The external PNG images are not actually accessed until rendering
 * begins.  This module registers a render preparation function that
 * goes through each of the external PNG images that have been
 * referenced one by one and compiles all their pixel data into a single
 * temporary file that interleaves data such that it can all be
 * sequentially accessed during rendering.
 * 
 * Only one external PNG image is open at a time, so there is no limit
 * on the total number of external PNG images.  However, the total size
 * of the interleaved pixel data in the temporary file must be within
 * the size limit established by the CFG_EXTERNAL_DISK_MIB configuration
 * variable.
 * 
 * During rendering, a memory buffer of size established by the
 * CFG_EXTERNAL_RAM_KIB will be used to read through the interleaved
 * pixel data.  CFG_EXTERNAL_RAM_KIB must be large enough to at least
 * hold all the interleaved data for a single pixel.
 * 
 * To integrate this plug-in with Lilac Core, include this header with
 * the plugin.c implementation file in Lilac Plugin, and then invoke the
 * node_external_init() function in plugin_init() function defined in
 * plugin.c
 */

/*
 * Register this module as a Lilac plug-in.
 */
void node_external_init(void);

#endif
