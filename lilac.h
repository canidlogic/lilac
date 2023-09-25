#ifndef LILAC_H_INCLUDED
#define LILAC_H_INCLUDED

/*
 * lilac.h
 * =======
 * 
 * Lilac 2D rendering library.
 * 
 * Requirements
 * ------------
 * 
 * Requires the following external libraries:
 * 
 *   - libpng
 *   - libsophistry
 * 
 * May require the <math.h> library with -lm
 */

#include <stddef.h>
#include <stdint.h>

/*
 * Constants
 * =========
 */

/*
 * The maximum dimensions of the total image size.
 * 
 * The output width and height dimensions may not exceed this constant.
 * The constant value here is the largest power of two such that if an
 * image were requested with maximum dimensions, the disk size of the
 * uncompressed image would be 1 GiB and not require 64-bit file
 * offsets.
 */
#define LILAC_MAX_IMAGE (16384)

/*
 * The minimum and maximum dimensions of a tile.
 * 
 * The maximum tile size requires 64 MiB of memory for the tile buffer,
 * while the minimum tile size requires 16 KiB.  Larger tiles will also
 * result in more intersections in the intersection buffer.
 * 
 * LILAC_MAX_TILE must be small enough that coordinates in range 0 to
 * (LILAC_MAX_TILE - 1) can fit within an unsigned 15-bit integer.
 */
#define LILAC_MIN_TILE (64)
#define LILAC_MAX_TILE (4096)

/*
 * Type declarations
 * =================
 */

typedef struct {
  
  /*
   * Pointer to the pixel data buffer of the locked tile.
   * 
   * Each pixel has the same packed 32-bit integer format that is used
   * in libsophistry.
   * 
   * Pixels are stored in horizontal scanlines, where pixels run from
   * left to right.  Horizontal scanlines are stored in top to bottom
   * order within the tile.
   * 
   * The length of each scanline is determined by the pitch field of
   * this structure.  However, not all pixels within the scanline may
   * actually be part of the image.  The w field of this structure 
   * determines how many pixels at the start of each scanline are
   * actually part of the image.
   * 
   * The number of scanlines is determined by the h field of this
   * structure.
   * 
   * The x and y fields determine where this tile is located within the
   * output image.  x plus w will always be less than or equal to the
   * full image width, and y plus h will always be less than or equal to
   * the full image height.
   * 
   * This pointer remains valid while the tile is locked.  The tile
   * buffer is owned by the Lilac renderer, and the client should not
   * attempt to free it.
   */
  uint32_t *pData;
  
  /*
   * The X coordinate of the upper-left corner of the tile.
   * 
   * This coordinate is within the coordinate system of the output
   * image.  It is always at least zero and always less than the full
   * image width.
   */
  int32_t x;
  
  /*
   * The Y coordinate of the upper-left corner of the tile.
   * 
   * This coordinate is within the coordinate system of the output
   * image.  It is always at least zero and always less than the full
   * image height.
   */
  int32_t y;
  
  /*
   * The pitch of tile scanlines, measured in pixels.
   * 
   * In order to advance from the start of one scanline to the start of
   * the next, advance by this many pixels.
   * 
   * This will always be at least LILAC_MIN_TILE and at most
   * LILAC_MAX_TILE.
   */
  int32_t pitch;
  
  /*
   * The width of the locked tile, in pixels.
   * 
   * This will always be at least one and at most the pitch value.  If
   * this tile is on the rightmost column of tiles, the width might be
   * less than the pitch.  Otherwise, the width will be equivalent to
   * the pitch.
   */
  int32_t w;
  
  /*
   * The height of the locked tile, in pixels.
   * 
   * This will always be at least one and at most LILAC_MAX_TILE.
   */
  int32_t h;
  
} LILAC_LOCK;

/*
 * Function pointer types
 * ======================
 */

/*
 * Callback for handling errors that occur within the Lilac renderer.
 * 
 * This callback function must not return.  Undefined behavior occurs if
 * the function returns.  The function is expected to stop the program
 * on an error.
 * 
 * lnum is the line number in the lilac.c source code that the error was
 * raised.  It is always present.
 * 
 * pDetail is the error message, or NULL if there is no specific error
 * message.  The error message does not include any line break.
 * 
 * Parameters:
 * 
 *   lnum - the Lilac source code line number
 * 
 *   pDetail - the error message, or NULL
 */
typedef void (*lilac_fp_err)(int lnum, const char *pDetail);

/*
 * Callback for handling warnings that occur within the Lilac renderer.
 * 
 * This callback function DOES return, unlike the error handler.
 * Warnings are used for non-fatal errors, such as a file failing to
 * close.
 * 
 * Apart from returning rather than stopping the program, this callback
 * otherwise has the same interface as lilac_fp_err.
 * 
 * Parameters:
 * 
 *   lnum - the Lilac source code line number
 * 
 *   pDetail - the warning message, or NULL
 */
typedef void (*lilac_fp_warn)(int lnum, const char *pDetail);

/*
 * Public functions
 * ================
 */

/*
 * Initialize the Lilac renderer.
 * 
 * The w and h give the full dimensions of the output image in pixels.
 * Both must be greater than zero and at most LILAC_MAX_IMAGE.
 * 
 * tile gives the width and height of rendering tiles.  This must be in
 * range LILAC_MIN_TILE to LILAC_MAX_TILE.  Let M be the maximum of the
 * w and h parameters.  If tile is greater than M, it will automatically
 * be adjusted down to the maximum of M and LILAC_MIN_TILE.
 * 
 * Larger tile values require fewer tiles to cover the image and
 * therefore are faster, but larger tiles require more memory, not only
 * for the tile buffer, but also for the increased number of
 * intersections that need to be stored in the intersection array.
 * 
 * The bgcol parameter is an ARGB color in the same format expected by
 * libsophistry.  When each tile rendering begins, the tile starts out
 * blanked to this color.
 * 
 * The caller may optionally specify error and/or warning callbacks.
 * See the function pointer documentation for the interface.  If not
 * specified, default handlers are used, which report the message, if
 * available.  The default error handler also exits the program on an
 * error return.
 * 
 * This function must be called before any other functions of this
 * module may be used.  This function may only be called once.
 * 
 * Parameters:
 * 
 *   w - the total width of the output image
 * 
 *   h - the total height of the output image
 * 
 *   tile - the tile dimension
 * 
 *   bgcol - the color to initialize each tile to
 * 
 *   fp_err - the error handler callback, or NULL for default
 * 
 *   fp_warn - the warning handler callback, or NULL for default
 */
void lilac_init(
    int32_t       w,
    int32_t       h,
    int32_t       tile,
    uint32_t      bgcol,
    lilac_fp_err  fp_err,
    lilac_fp_warn fp_warn);

/*
 * Get the total width of the output image in pixels.
 * 
 * The value returned from this function is constant for the duration of
 * rendering.  It will always be in range 1 to LILAC_MAX_IMAGE.
 * 
 * Return:
 * 
 *   the total output image width
 */
int32_t lilac_width(void);

/*
 * Get the total height of the output image in pixels.
 * 
 * The value returned from this function is constant for the duration of
 * rendering.  It will always be in range 1 to LILAC_MAX_IMAGE.
 * 
 * Return:
 * 
 *   the total output image height
 */
int32_t lilac_height(void);

/*
 * Get the total number of tiles that cover the whole image.
 * 
 * The value returned from this function is constant for the duration of
 * rendering.  It will always be in range 1 to 65536.  (If the maximum
 * possible image size is used and the minimum possible tile size, there
 * are 65536 tiles total.)
 * 
 * Return:
 * 
 *   the total number of tiles
 */
int32_t lilac_tiles(void);

/*
 * Begin rendering the next tile.
 * 
 * After initializing the renderer with lilac_init(), you must
 * repeatedly render the 2D scene a total number of times given by the
 * function lilac_tiles().  Each scene render computes a different tile
 * of the output image.
 * 
 * At the start of each scene render, call lilac_begin_tile().  Then,
 * use lilac_begin_path() and lilac_end_path() to render each of the
 * paths, along with lilac_color() to choose the fill color.  You can
 * also use lilac_lock() and lilac_unlock() to directly modify the
 * pixels within tiles.  After everything has been rendered, call
 * lilac_end_tile() to finish rendering the current tile.
 * 
 * If you want a seamless image where the tile boundaries are invisible,
 * you must render the exact same scene in each scene rendering
 * operation.  Lilac does not check that the rendering sequence is the
 * same each time.  If it is different, rendering will still work, but
 * there may be visual discontinuities at the edges of tiles.
 * 
 * You may not use lilac_begin_tile() if there's already a scene render
 * in progress.  Nor may you use lilac_begin_tile() if you've already
 * rendered all the tiles.
 * 
 * The tile starts out with all pixels set to the bgcol parameter that
 * was passed to lilac_init().
 */
void lilac_begin_tile(void);

/*
 * Finish rendering a tile.
 * 
 * Each call to lilac_begin_tile() needs to be paired with a call to
 * lilac_end_tile().  This function may only be used if a tile is
 * currently being rendered.  This function can't be used if there is a
 * path definition currently in progress or if the tile is currently
 * locked.
 */
void lilac_end_tile(void);

/*
 * Perform alpha blending.
 * 
 * All color values are ARGB colors in the format expected by
 * libsophistry, which is a non-premultiplied sRGB color system.
 * 
 * over is the color above and under is the color below.  The return
 * value is the blended result.
 * 
 * If the over alpha channel value is fully opaque, then the over value
 * is just returned as-is.  If the over alpha channel value is fully
 * transparent, then the under value is just returned as-is.  Otherwise,
 * both colors are converted into linear ARGB floating-point space,
 * alpha blending is performed, and then the result is converted back
 * into the packed libsophistry format.
 * 
 * This function is used by Lilac when filling paths.  Clients can also
 * use this function for blending when they are using lilac_lock().
 * 
 * To optimize the common case where the same combination of colors is
 * blended many times in a row, this function will cache the results of
 * the most recent computation and return the cached result if the exact
 * same input parameters are repeated.
 * 
 * Parameters:
 * 
 *   over - the color on top
 * 
 *   under - the color below
 * 
 * Return:
 * 
 *   the blended color
 */
uint32_t lilac_blend(uint32_t over, uint32_t under);

/*
 * Set the color that is used for filling paths.
 * 
 * This color is initially fully opaque black.  The provided new color
 * argument must be an ARGB color in the same format expected by
 * libsophistry.  Color settings persist until they are changed by this
 * function.
 * 
 * The color that is used to fill paths is the current color setting at
 * the time that lilac_end_path() is called.  The lilac_blend() function
 * is used to perform alpha compositing.
 * 
 * Parameters:
 * 
 *   col - the color to use for path fills
 */
void lilac_color(uint32_t col);

/*
 * Begin a new path that will be filled.
 * 
 * This function may only be used when tile rendering is in progress,
 * and no other path or lock operation is currently in progress.
 * 
 * Each lilac_begin_path() call must be paired with a call to
 * lilac_end_path().
 * 
 * Paths begin with no intersections defined, which means that no pixels
 * will be filled.
 * 
 * To create a fill shape, use a set of lilac_line() and lilac_dot()
 * calls.  The order of lilac_line() and lilac_dot() calls does not
 * matter.  However, the order of endpoints within each line declaration
 * DOES matter.
 * 
 * Each scanline is intersected with the lines and dots.  Each
 * intersection stores whether the line or dot was moving upwards or
 * downwards when it crossed the scanline.  (Dots always move in
 * clockwise motion around their circular perimeter.)
 * 
 * Starting at negative infinity and proceeding to positive infinity on
 * each scanline, the "fill count" starts out at zero.  Each time it
 * reaches an intersection that moves upwards, the fill count is
 * incremented.  Each time it reaches an intersection that moves
 * downwards, the fill count is decremented.  Pixels that have a fill
 * count of zero are NOT filled, while pixels that have any kind of
 * non-zero fill count ARE filled.  This is the common "non-zero winding
 * rule" that is frequently used in computer graphics.
 * 
 * In order to have well-behaved fills, lilac_line() declarations should
 * form closed paths.
 */
void lilac_begin_path(void);

/*
 * End a new path that will be filled.
 * 
 * This function may only be used when a path operation is currently in
 * progress.  It ends the path and updates the current tile, filling any
 * covered pixels using the color defined by lilac_color().  The
 * lilac_blend() function will be used for alpha blending when the fill
 * color is transparent.
 * 
 * See lilac_begin_path() for further information about defining paths.
 */
void lilac_end_path(void);

/*
 * Add a line to the current path.
 * 
 * This function may only be used when a path operation is currently in
 * progress.  See lilac_begin_path() for further information.
 * 
 * The coordinates are pixel coordinates in the coordinate space of the
 * output image.  They do NOT have to be within the boundaries of the
 * output image.
 * 
 * The order of the endpoints does matter, because line direction is
 * significant for determining which pixels get filled.  See the
 * lilac_begin_path() function for further information.
 * 
 * Note that if you draw a rectangle with lines, the rightmost column of
 * pixels will be missing.  This is because the fill count will return
 * to zero at the start of rendering that column, causing the last
 * column to not be rendered.
 * 
 * Parameters:
 * 
 *   x1 - the X coordinate of the line start
 * 
 *   y1 - the Y coordinate of the line start
 * 
 *   x2 - the X coordinate of the line end
 * 
 *   y2 - the Y coordinate of the line end
 */
void lilac_line(int32_t x1, int32_t y1, int32_t x2, int32_t y2);

/*
 * Add a circular dot to the current path.
 * 
 * This function may only be used when a path operation is currently in
 * progress.  See lilac_begin_path() for further information.
 * 
 * The center of the circle is given by the X and Y coordinates.  These
 * coordinates are pixel coordinates in the coordinate space of the
 * output image.  They do NOT have to be within the boundaries of the
 * output image.
 * 
 * The radius of the circle is given by r, which must be greater than
 * zero.  The radius is measured in output pixel lengths.
 * 
 * The circular path moves in clockwise direction around the perimeter
 * of the dot.  This is significant for determining which pixels get
 * filled.  See the lilac_begin_path() function for further information.
 * 
 * Parameters:
 * 
 *   x - the X coordinate of the dot center
 * 
 *   y - the Y coordinate of the dot center
 * 
 *   r - the radius of the dot
 */
void lilac_dot(int32_t x, int32_t y, int32_t r);

/*
 * Lock the current tile so that its pixels may be directly accessed.
 * 
 * This function may only be used when tile rendering is in progress,
 * and no other path or lock operation is currently in progress.
 * 
 * Each lilac_lock() call must be paired with a call to lilac_unlock().
 * 
 * The given LILAC_LOCK structure will be filled in with all the
 * necessary information to access the tile pixel data directly.  Any
 * paths that have been added to the current tile prior to this lock
 * call will already be rendered within the pixel data.
 * 
 * The LILAC_LOCK structure is not referred to again after this function
 * returns, so the client does not need to preserve it.  Its initial
 * value on entry to this function is irrelevant.
 * 
 * Parameters:
 * 
 *   pLock - the lock structure that will be filled in
 */
void lilac_lock(LILAC_LOCK *pLock);

/*
 * End a raster lock operation.
 * 
 * This function may only be used when a lock operation is currently in
 * progress.  After calling this function, the client should no longer
 * use the pointer that was written into the LILAC_LOCK structure.
 */
void lilac_unlock(void);

/*
 * Compile the full image into a PNG file at the given output path.
 * 
 * The lilac_end_tile() function must have been called the exact number
 * of times given by lilac_tiles() so that each tile has been rendered
 * before you can use this compilation function.
 * 
 * This function will also close down the renderer, so no further calls
 * to this module are possible after using this function.
 * 
 * libsophistry requires the path to end in a case-insensitive match for
 * ".PNG"
 * 
 * Parameters:
 * 
 *   pPath - the path to the PNG file to create
 */
void lilac_compile(const char *pPath);

#endif
