#ifndef PSHADE_H_INCLUDED
#define PSHADE_H_INCLUDED

/*
 * pshade.h
 * 
 * Programmable shader module of Lilac.
 */

#include <stddef.h>
#include <stdint.h>

/* 
 * Error codes.
 * 
 * Don't forget to update pshade_errorString()!
 */
#define PSHADE_ERR_NONE   (0)   /* No error */
#define PSHADE_ERR_LALLOC (1)   /* Failed to allocate Lua interpreter */
#define PSHADE_ERR_LOADSC (2)   /* Failed to load script */
#define PSHADE_ERR_INITSC (3)   /* Failed to initialize script */
#define PSHADE_ERR_GROWST (4)   /* Failed to grow stack */

/*
 * Given a programmable shader error code, return an error message.
 * 
 * The error message begins with a capital letter but does not have any
 * punctuation or line break at the end.
 * 
 * If zero is passed, "No error" is returned.  If an unknown error code
 * is passed, "Unknown error" is returned.
 * 
 * Parameters:
 * 
 *   code - the error code to look up
 * 
 * Return:
 * 
 *   an error message for the code
 */
const char *pshade_errorString(int code);

/*
 * Load a Lua script into the programmable shader module.
 * 
 * This function may only be used once.  A fault occurs otherwise.
 * 
 * pScriptPath is the path to the Lua script file to load.  perr points
 * to a variable to receive an error code, which is one of the constants
 * PSHADE_ERR_.  Use pshade_errorString() to convert an error code into
 * an error message.  PSHADE_ERR_NONE is returned when successful.
 * 
 * You should eventually call pshade_close() to close down.
 * 
 * Parameters:
 * 
 *   pScriptPath - path to the Lua script to load
 * 
 *   perr - the variable to receive an error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int pshade_load(const char *pScriptPath, int *perr);

/*
 * Close down any Lua interpreter instance that might be open.
 */
void pshade_close(void);

/*
 * Use the programmable shader module to query a specific pixel in a
 * procedurally-generated texture.
 * 
 * This function can be called even if pshade_load() hasn't been called
 * yet, but it will always fail and return an appropriate error message
 * in this case.
 * 
 * pShader is a string that will be passed through to the Lua script to
 * identify the specific procedural texture shader that is requested.
 * It must be a sequence of zero or more ASCII alphanumeric characters
 * and underscores.
 * 
 * x and y are the coordinates of the specific pixel that is being
 * requested.  Requests must be sequenced in left-to-right and then
 * top-to-bottom order, and this is enforced by this function.  It is,
 * however, acceptable to make multiple queries of the same coordinate,
 * and not every pixel coordinate has to be queried.
 * 
 * width and height are the dimensions of the output image.  Both must
 * be greater than zero.  x and y must be greater than or equal to zero
 * and less than their respective dimension.
 * 
 * perr points to a variable to receive an error code, which is one of
 * the constants PSHADE_ERR_.  If successful, the value PSHADE_ERR_NONE
 * will be written here.  You can use pshade_errorString() to convert an
 * error code into an error message.
 * 
 * You MUST check perr to determine whether this function succeeded,
 * since a zero return value can also be a valid return.
 * 
 * Parameters:
 * 
 *   pShader - the name of the programmable shader to invoke
 * 
 *   x - the X coordinate
 * 
 *   y - the Y coordinate
 * 
 *   width - the width of the output image
 * 
 *   height - the height of the output image
 * 
 *   perr - pointer to a variable to receive an error message
 * 
 * Return:
 * 
 *   the generated ARGB pixel value packed in a single integer; zero is
 *   either a valid return or means an error; use *perr to determine
 *   which interpretation should be used
 */
uint32_t pshade_pixel(
    const char *pShader,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    int *perr);

#endif
