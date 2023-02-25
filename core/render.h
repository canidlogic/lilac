#ifndef RENDER_H_INCLUDED
#define RENDER_H_INCLUDED

/*
 * render.h
 * ========
 * 
 * Defines functions for rendering nodes into the final image file.
 * 
 * When the render_go() function is called, the module enters "render
 * mode" which means that node objects can request the current pixel
 * position from this module.  Render mode is only active while the
 * render_go() function is running.
 */

#include <stddef.h>
#include "node.h"

/*
 * Type declarations
 * -----------------
 */

/*
 * Function pointer type for render preparation functions.
 */
typedef void (*fp_render_prep)(void);

/*
 * Public functions
 * ----------------
 */

/*
 * Register a render preparation function.
 * 
 * When render_go() is called, each render preparation function will be
 * called before entering render mode.  Render preparation functions are
 * called in the order they are registered.
 * 
 * Render preparation functions are intended for node types that must
 * perform some sort of non-trivial initialization after all nodes of
 * that type have been defined but before the nodes can be used in
 * rendering.
 * 
 * Registering the same function more than once will cause the function
 * to be called more than once.
 * 
 * This function can only be used when not in render mode.
 * 
 * Parameters:
 * 
 *   fp - the function to register
 */
void render_prepare(fp_render_prep fp);

/*
 * Render the final image file.
 * 
 * pNode is the root node that determines the color of each pixel in the
 * output image.  Invoking this node may indirectly invoke other nodes
 * that are connected to it within the graph.
 * 
 * The path to the output image file and its dimensions are determined
 * by configuration variables read from the main lilac module.
 * 
 * The module enters render mode when this function is called and leaves
 * render mode when this function returns.  It is an error to call this
 * function when the module is already in render mode.  (In other words,
 * the function is not re-entrant.)
 * 
 * Parameters:
 * 
 *   pNode - the root node to render
 */
void render_go(NODE *pNode);

/*
 * Check whether this module is currently in render mode.
 * 
 * Return:
 * 
 *   non-zero if in render mode, zero if not in render mode
 */
int render_mode(void);

/*
 * Return the pixel offset of the current pixel that is being rendered.
 * 
 * This function can only be used while in render mode.  The return
 * value is somewhere in the range [0, (w*h)-1] where w and h are the
 * width and height in pixels of the output image.
 * 
 * The following relationship always holds between the pixel offset
 * returned by this function and the X and Y coordinates returned by
 * render_X() and render_Y():
 * 
 *   offset = (y * OUTPUT_WIDTH) + x
 * 
 * The render module will start the pixel offset at zero and increment
 * the pixel offset until it reaches its maximum value, invoking the
 * root node exactly once for each pixel offset.  Nodes may depend on
 * this strict linear order.
 * 
 * Return:
 * 
 *   the current pixel offset that is being rendered
 */
int32_t render_offset(void);

/*
 * Return the X coordinate of the current pixel that is being rendered.
 * 
 * This function can only be used in render mode.  The return value is
 * somewhere in the range [0, w-1] where w is the width in pixels of the
 * output image.
 * 
 * The render module will start at coordinates (0,0) and proceed to the
 * coordinates (w-1,h-1) where w and h are the width and height of the
 * output image.  Pixels are iterated by incrementing their X
 * coordinates until the X coordinate overflows, at which point the Y
 * coordinate is incremented and the X coordinate resets to zero.  The
 * render module will invoke the root node exactly once for each pixel
 * coordinate pair.  Nodes may depend on this strict linear order.
 * 
 * Return:
 * 
 *   the current pixel X coordinate that is being rendered
 */
int32_t render_X(void);

/*
 * Return the Y coordinate of the current pixel that is being rendered.
 * 
 * This function can only be used in render mode.  The return value is
 * somewhere in the range [0, h-1] where h is the height in pixels of
 * the output image.
 * 
 * The render module will start at coordinates (0,0) and proceed to the
 * coordinates (w-1,h-1) where w and h are the width and height of the
 * output image.  Pixels are iterated by incrementing their X
 * coordinates until the X coordinate overflows, at which point the Y
 * coordinate is incremented and the X coordinate resets to zero.  The
 * render module will invoke the root node exactly once for each pixel
 * coordinate pair.  Nodes may depend on this strict linear order.
 * 
 * Return:
 * 
 *   the current pixel Y coordinate that is being rendered
 */
int32_t render_Y(void);

#endif
