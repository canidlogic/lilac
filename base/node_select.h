#ifndef NODE_SELECT_H_INCLUDED
#define NODE_SELECT_H_INCLUDED

/*
 * node_select.h
 * =============
 * 
 * Defines a node that uses the input of an "index node" to select a
 * node from a table of "palette nodes."
 * 
 * Specifically, for each pixel, the ARGB color retrieved from the index
 * node is used to query a mapping of ARGB values to palette nodes that
 * was established when the node was defined.  The selected palette node
 * is then used to provide the color of the current pixel.
 * 
 * To define new select nodes, an accumulator is used.  The following
 * operation begins a new definition in the accumulator:
 * 
 *     [index:node] [default:node] select_new -
 * 
 * The accumulator must be empty when this operation is invoked.  The
 * [index] parameter is the node to use as the index node.  The
 * [default] parameter is the node to use when none of the mappings
 * apply to the color read from the index node.
 * 
 * Once the accumulator has a definition within it, the following
 * operation is used to add mappings:
 * 
 *     [key:color] [value:node] select_map -
 * 
 * The [key] parameter is the ARGB color from the index node that will
 * be mapped to the [value], which is the palette node for this color.
 * Only exact matches of the [key] will use this mapping.  An error
 * occurs if the same key is defined more than once, or if the total
 * number of mappings exceeds an implementation limit.
 * 
 * Finally, when all mappings have been defined, the following operation
 * finishes the definition in the accumulator and pushes it onto the
 * interpreter stack.  The accumulator register is then cleared:
 * 
 *     - select_finish [result:node]
 * 
 * To integrate this plug-in with Lilac Core, include this header with
 * the plugin.c implementation file in Lilac Plugin, and then invoke the
 * node_select_init() function in plugin_init() function defined in
 * plugin.c
 */

/*
 * Register this module as a Lilac plug-in.
 */
void node_select_init(void);

#endif
