#ifndef TEXTURE_H_INCLUDED
#define TEXTURE_H_INCLUDED

/*
 * texture.h
 * 
 * Texture module of Lilac.
 * 
 * This module only handles PNG type textures.  The virtual texture
 * table, which is the top-level texture structure, is defined in the
 * lilac_draw.c module.
 */

#include <stddef.h>
#include <stdint.h>

/*
 * The maximum number of textures that can be loaded.
 */
#define TEXTURE_MAXCOUNT (1024)

/*
 * The maximum width/height of texture images.
 */
#define TEXTURE_MAXDIM (2048)

/*
 * Load a texture into memory.
 * 
 * pPath is the path of the image file to load as a texture.
 * 
 * pError is either NULL or it points to a variable to receive a
 * Sophistry error code in case of failure.  If the function is
 * successful and pError is not NULL, *pError will be set to zero.
 * 
 * If texture loading fails because too many textures have been loaded,
 * the error code will be SPH_IMAGE_ERR_UNKNOWN.
 * 
 * The texture module imposes a limit of TEXTURE_MAXDIM on each image
 * dimension, which is much stricter than the limit in Sophistry.  If
 * the image dimensions exceed this, the load will fail with the error
 * SPH_IMAGE_ERR_IMAGEDIM.
 * 
 * Textures are stored in memory.  If the program runs out of memory,
 * there will be a fault.
 * 
 * Parameters:
 * 
 *   pPath - the path of the image file to load as the texture
 * 
 *   pError - pointer to the variable to receive the error code, or NULL
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int texture_load(const char *pPath, int *pError);

/*
 * Retrieve the total count of textures that have been successfully
 * loaded into memory.
 * 
 * Valid texture indices are one up to and including this count value.
 * (If the count is zero, no texture indices are valid.)
 * 
 * At most TEXTURE_MAXCOUNT textures can be loaded.  Any additional
 * attempts to load textures will result in a failure to load.
 * 
 * Return:
 * 
 *   the number of textures that have been loaded
 */
int texture_count(void);

/*
 * Get the ARGB pixel value of a given texture at a given coordinate.
 * 
 * tidx is the texture index.  It must be in range one up to and
 * including texture_count() or a fault occurs.
 * 
 * x and y are the image coordinates.  Textures are automatically tiled
 * in both X and Y directions, so all textures have infinite dimensions.
 * The x and y coordinates may be any value zero or greater, regardless
 * of the texture size.
 * 
 * The return value is packed ARGB value in the same format as Sophistry
 * uses.
 * 
 * Parameters:
 * 
 *   tidx - the texture index to query
 * 
 *   x - the X coordinate
 * 
 *   y - the Y coordinate
 * 
 * Return:
 * 
 *   the ARGB value of the given texture at the given coordinate
 */
uint32_t texture_pixel(int tidx, int32_t x, int32_t y);

#endif
