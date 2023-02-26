#ifndef NODE_CONSTANT_H_INCLUDED
#define NODE_CONSTANT_H_INCLUDED

/*
 * node_constant.h
 * ===============
 * 
 * Defines a node that just returns a constant color for every pixel.
 * 
 * This is a good module to study for a simple example of how to define
 * a Lilac plug-in.  It demonstrates how to register an operation with
 * Lilac Core that creates a new type of node.  However, it does not
 * demonstrate render preparation functions.
 * 
 * To integrate this plug-in with Lilac Core, include this header with
 * the plugin.c implementation file in Lilac Plugin, and then invoke the
 * node_constant_init() function in plugin_init() function defined in
 * plugin.c
 */

/*
 * Register this module as a Lilac plug-in.
 */
void node_constant_init(void);

#endif
