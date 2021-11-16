/*
 * lilac_draw.c
 * ============
 * 
 * Lilac drawing program.
 * 
 * See the manual in the doc directory for user instructions and the
 * README in this directory for build instructions.
 */

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gamma.h"
#include "pshade.h"
#include "texture.h"
#include "ttable.h"

#include "sophistry.h"

/*
 * Constants
 * =========
 */

/*
 * Error code definitions.
 * 
 * Remember to update lilac_errorString()!
 */
#define ERROR_MISMATCH (1)  /* Image dimensions mismatch */

/* Error codes in this range are Sophistry error codes added to the
 * value ERROR_SPH_MIN */
#define ERROR_SPH_MIN (100)
#define ERROR_SPH_MAX (199)

/*
 * Error location definitions.
 */
#define ERRORLOC_UNKNOWN      (0)
#define ERRORLOC_OUTFILE      (1) 
#define ERRORLOC_MASKFILE     (2)
#define ERRORLOC_PENCILFILE   (3)
#define ERRORLOC_SHADINGFILE  (4)

/*
 * Virtual texture types.
 */
#define VTEX_UNDEF  (0)
#define VTEX_PNG    (1)
#define VTEX_PSHADE (2)

/*
 * The maximum number of characters, including the opening dot and the
 * terminating nul, that may be in a texture file extension.
 */
#define MAX_EXT (16)

/*
 * Type declarations
 * =================
 */

/*
 * Stores an HSL color with floating-point channels.
 * 
 * This can not be used for grayscale values, which have an undefined
 * hue.
 */
typedef struct {
  
  /* The hue, in range [0.0, 360.0) */
  float h;
  
  /* The saturation, in range [0.0, 1.0] */
  float s;
  
  /* The lightness, in range [0.0, 1.0] */
  float l;
  
} HSL;

/*
 * Stores an RGB color with floating-point channels.
 */
typedef struct {
  
  /* Red, in range [0.0, 1.0] */
  float r;
  
  /* Green, in range [0.0, 1.0] */
  float g;
  
  /* Blue, in range [0.0, 1.0] */
  float b;
  
} RGB;

/*
 * Virtual texture structure.
 */
typedef struct {
  
  /*
   * The type of virtual texture.
   * 
   * One of the VTEX_ constants.
   */
  int vtype;
  
  /*
   * The actual texture data depends on which virtual texture type.
   */
  union {
    
    /*
     * Dummy value used for VTEX_UNDEF.
     */
    int dummy;
    
    /*
     * Texture index in texture module, used for PNG textures.
     * 
     * This is the one-indexed textured index so it can be passed
     * directly to the texture module.
     */
    int tidx;
    
    /*
     * Pointer to a nul-terminated shader name string for use with
     * programmable shaders.
     */
    char *pShader;
    
  } v;
  
} VTEX;

/*
 * Local data
 * ==========
 */

/*
 * The executable module name, for use in diagnostic reports.
 * 
 * This is set at the start of the program entrypoint.
 */
const char *pModule = NULL;

/*
 * The virtual texture table.
 * 
 * m_vtx_init indicates whether the virtual texture table has been
 * initialized yet.
 * 
 * m_vtx_count starts at zero and then is incremented for each new
 * virtual texture that is added.  All textures with an index in the
 * array that is less than m_vtx_count must have a type other than
 * VTEX_UNDEF.
 * 
 * Use vtx_init() to clear and initialize the virtual texture table if
 * not already initialized, with a count of zero and with all textures
 * being the "undefined" texture type.  vtx_init() is called
 * automatically at the start of the other vtx functions.
 * 
 * Use vtx_load() to load a texture in the virtual texture table.
 * 
 * Use vtx_query() to query a pixel from a texture, routing the call
 * appropriately to the correct texture handling module depending on the
 * texture type.
 */
static int m_vtx_init = 0;
static int m_vtx_count = 0;
static VTEX m_vtx[TEXTURE_MAXCOUNT];

/*
 * Local functions
 * ===============
 */

/* Function prototypes */
static void vtx_init(void);
static int vtx_load(const char *pstr);
uint32_t vtx_query(
    int       tidx,
    int32_t   x,
    int32_t   y,
    int32_t   width,
    int32_t   height,
    int     * status);

static const char *lilac_errorString(int code);

static float hslval(float a, float b, float hue);
static void rgb2hsl(RGB *pRGB, HSL *pHSL);
static void hsl2rgb(HSL *pHSL, RGB *pRGB);
static uint32_t fade(uint32_t rgb, int rate);
static uint32_t composite(uint32_t over, uint32_t under);
static uint32_t colorize(uint32_t rgb_in, uint32_t rgb_tint);

static int lilac(
    const char * pOutPath,
    const char * pMaskPath,
    const char * pPencilPath,
    const char * pShadingPath,
           int * pError,
           int * pErrLoc);

/*
 * Initialize the virtual texture table, if not already initialized.
 * 
 * If the virtual texture table is already initialized, this does
 * nothing.
 */
static void vtx_init(void) {
  int i = 0;
  
  /* Only proceed if not initialized */
  if (!m_vtx_init) {
    
    /* Initialize virtual texture table */
    memset(m_vtx, 0, sizeof(VTEX) * TEXTURE_MAXCOUNT);
    for(i = 0; i < TEXTURE_MAXCOUNT; i++) {
      m_vtx[i].vtype = VTEX_UNDEF;
      m_vtx[i].v.dummy = 0;
    }
    m_vtx_count = 0;
    m_vtx_init = 1;
  }
}

/*
 * Load a virtual texture from a given command-line parameter value.
 * 
 * vtx_init() is called at the start of this function to automatically
 * initialize the virtual texture table if not yet initialized.
 * 
 * This function will handle reporting errors to stderr.  The return
 * value indicates whether the load was successful or not.
 * 
 * If successful, the texture will be added to the virtual texture
 * table.
 * 
 * Parameters:
 * 
 *   pstr - the parameter to parse
 * 
 * Return:
 * 
 *   non-zero if successful, zero if failure
 */
static int vtx_load(const char *pstr) {
  
  int status = 1;
  int errcode = 0;
  const char *pExt = NULL;
  const char *pc = NULL;
  char *pb = NULL;
  size_t slen = 0;
  
  char ext[MAX_EXT];
  
  /* Initialize buffer */
  memset(ext, 0, MAX_EXT);
  
  /* Initialize virtual texture table if necessary */
  vtx_init();
  
  /* Check parameter */
  if (pstr == NULL) {
    abort();
  }
  
  /* Set pExt to point to the last dot in the string, or set it to NULL
   * if there is no dot in the string */
  pExt = NULL;
  for(pc = pstr; *pc != 0; pc++) {
    if (*pc == '.') {
      pExt = pc;
    }
  }
  
  /* If pExt is NULL, then set the ext buffer to the special value "-";
   * else, check that the extension fits together with the opening dot
   * and copy it into the ext buffer */
  if (pExt == NULL) {
    /* No extension, so copy "-" into buffer */
    strcpy(ext, "-");
    
  } else {
    /* Extension -- first make sure that extension is not too long */
    if (strlen(pExt) < MAX_EXT) {
      /* Extension fits in buffer, so copy it in */
      strcpy(ext, pExt);
      
    } else {
      /* Extension doesn't fit in buffer, so can't be recognized */
      status = 0;
      fprintf(stderr,
        "%s: Texture '%s' doesn't have recognized file extension!\n",
        pModule, pstr);
    }
  }
  
  /* Extensions are case-insensitive, so convert uppercase letters in
   * extension to lowercase letters */
  if (status) {
    for(pb = ext; *pb != 0; pb++) {
      if ((*pb >= 'A') && (*pb <= 'Z')) {
        *pb = *pb + ('a' - 'A');
      }
    }
  }
  
  /* Make sure virtual texture table isn't full */
  if (status && (m_vtx_count >= TEXTURE_MAXCOUNT)) {
    status = 0;
    fprintf(stderr, "%s: Too many textures defined!\n", pModule);
  }
  
  /* Now handle the specific extension ("-" is used if there is no
   * extension) */
  if (status && (strcmp(ext, ".png") == 0)) {
    /* PNG image file, so load with the texture module */
    if (!texture_load(pstr, &errcode)) {
      fprintf(stderr, "%s: Error loading texture '%s'...\n",
                pModule, pstr);
      
      if (errcode == SPH_IMAGE_ERR_IMAGEDIM) {
        fprintf(stderr, "%s: Texture dimensions too large!\n",
                  pModule);
      } else {
        fprintf(stderr, "%s: %s!\n",
                  pModule,
                  sph_image_errorString(errcode));
      }
      status = 0;
    }
    
    /* Add the PNG texture to the virtual texture table */
    if (status) {
      m_vtx[m_vtx_count].vtype = VTEX_PNG;
      m_vtx[m_vtx_count].v.tidx = texture_count();
      m_vtx_count++;
    }
    
  } else if (status && (strcmp(ext, "-") == 0)) {
    /* No file extension, so this is a shader name token to use with a
     * programmable shader -- first check that the shader name uses only
     * ASCII alphanumerics and underscores (empty shader names OK) */
    for(pc = pstr; *pc != 0; pc++) {
      if (((*pc < 'A') || (*pc > 'Z')) &&
            ((*pc < 'a') || (*pc > 'z')) &&
            ((*pc < '0') || (*pc > '9')) &&
            (*pc != '_')) {
        status = 0;
        fprintf(stderr, "%s: Shader name '%s' is invalid!\n",
          pModule, pstr);
        break;
      }
    }
    
    /* Next make a dynamic copy of the shader name */
    if (status) {
      slen = strlen(pstr);
      pb = (char *) malloc(slen + 1);
      if (pb == NULL) {
        abort();
      }
      memset(pb, 0, slen + 1);
      strcpy(pb, pstr);
    }
    
    /* Add the procedural texture to the virtual texture table */
    if (status) {
      m_vtx[m_vtx_count].vtype = VTEX_PSHADE;
      m_vtx[m_vtx_count].v.pShader = pb;
      m_vtx_count++;
      pb = NULL;
    }
    
  } else if (status) {
    /* Unrecognized extension */
    status = 0;
    fprintf(stderr,
      "%s: Texture '%s' doesn't have recognized file extension!\n",
      pModule, pstr);
  }
  
  /* Return status */
  return status;
}

/*
 * Get the ARGB pixel value of a given virtual texture at a given
 * coordinate.
 * 
 * This function will automatically initialize the virtual texture table
 * if necessary with vtx_init().
 * 
 * tidx is the texture index.  It must be in range one up to and
 * including m_vtx_count or a fault occurs.  Note that the indices given
 * to this function are one-indexed!
 * 
 * x and y are the image coordinates.  This function enforces that
 * pixels may only be queries in order left-to-right through scanlines,
 * and scanlines from top to bottom through image.
 * 
 * width and height are the width and height in pixels of the output
 * image that is being rendered.  x and y must both be greater than or
 * equal to zero and less than width and height, respectively.
 * 
 * The return value is packed ARGB value in the same format as Sophistry
 * uses.
 * 
 * If the query is successful, *status will be unchanged by this
 * function.  If the query fails, *status will be set to zero and the
 * return value will be zero.  But note that a return value of zero also
 * can be the result of a successful query!
 * 
 * Failures will be reported to standard error by this function.
 * 
 * Parameters:
 * 
 *   tidx - the virtual texture to query
 * 
 *   x - the X coordinate
 * 
 *   y - the Y coordinate
 * 
 *   width - the width of the output image
 * 
 *   height - the height of the output image
 * 
 *   status - pointer to the status flag
 * 
 * Return:
 * 
 *   the ARGB value of the given virtual texture at the given coordinate
 */
uint32_t vtx_query(
    int       tidx,
    int32_t   x,
    int32_t   y,
    int32_t   width,
    int32_t   height,
    int     * status) {
  
  uint32_t result = 0;
  int errcode = 0;
  
  static int32_t s_last_x = 0;
  static int32_t s_last_y = 0;
  
  /* Initialize virtual texture table if needed */
  vtx_init();
  
  /* Check status parameter, dimensions, and coordinates */
  if (status == NULL) {
    abort();
  }
  if ((width < 1) || (height < 1)) {
    abort();
  }
  if ((x < 0) || (x >= width) ||
      (y < 0) || (y >= height)) {
    abort();
  }
  
  /* Enforce proper scanning order and update statistics */
  if (y > s_last_y) {
    /* We've advanced a scanline, so update to new position */
    s_last_x = x;
    s_last_y = y;
  
  } else if (y == s_last_y) {
    /* Still in same scanline, so next check x */
    if (x > s_last_x) {
      /* We've advanced within scanline, so update x */
      s_last_x = x;
    
    } else if (x != s_last_x) {
      /* We must have gone backwards, which is not allowed */
      abort();
    }
  
  } else {
    /* We must have gone backwards in scan order, which is not
     * allowed */
    abort();
  }
  
  /* Make sure given texture index is in range of table */
  if ((tidx >= 1) && (tidx <= m_vtx_count)) {
    
    /* Dispatch call to appropriate texture module */
    if (m_vtx[tidx - 1].vtype == VTEX_PNG) {
      /* PNG texture, so dispatch to texture module */
      result = texture_pixel(m_vtx[tidx - 1].v.tidx, x, y);
      
    } else if (m_vtx[tidx - 1].vtype == VTEX_PSHADE) {
      /* Procedural texture, so dispatch to programmable shader
       * module */
      result = pshade_pixel(
                m_vtx[tidx - 1].v.pShader,
                x, y, width, height,
                &errcode);
      
      /* Check for error */
      if (errcode != PSHADE_ERR_NONE) {
        *status = 0;
        fprintf(stderr, "%s: Programmable shader error...\n",
                  pModule);
        fprintf(stderr, "%s: %s!\n",
          pModule, pshade_errorString(errcode));
      }
      
    } else {
      /* Shouldn't happen -- unknown virtual texture type or
       * undefined */
      abort();
    }
    
  } else {
    /* Texture index not in range */
    abort();
  }
  
  /* Return result */
  return result;
}

/*
 * Given a Lilac error code, return a string for the error message.
 * 
 * The error message does not have any punctuation at the end of the
 * string nor does it have a line break.  The first letter is
 * capitalized.
 * 
 * If zero is passed (meaning no error), the string is "No error"
 * 
 * If the error code is in one of the module ranges, this function
 * invokes the error string function in the appropriate module to get
 * the error message.
 * 
 * If an unrecognized error code is passed, the string is "Unknown
 * error".
 * 
 * Parameters:
 * 
 *   code - the Lilac error code
 * 
 * Return:
 * 
 *   an error message
 */
static const char *lilac_errorString(int code) {
  
  const char *pResult = "Unknown error";
  
  if ((code >= ERROR_SPH_MIN) && (code <= ERROR_SPH_MAX)) {
    pResult = sph_image_errorString(code - ERROR_SPH_MIN);
  
  } else if (code == ERROR_MISMATCH) {
    pResult =
      "Mask, pencil, and shading files must have same dimensions";
  }
  
  return pResult;
}

/*
 * Auxiliary function for HSL/RGB conversions.
 * 
 * See the conversion functions for further information.
 */
static float hslval(float a, float b, float hue) {
  
  float result = 0.0f;
  
  /* Adapted from The Revolutionary Guide to Bitmapped Graphics,
   * Control-Zed, Wrox Press, 1994, pg. 124 */
  
  while (hue >= 360.0f) {
    hue -= 360.0f;
  }
  while (hue < 0.0f) {
    hue += 360.0f;
  }
  
  if (hue < 60.0f) {
    result = a + (b - a) * hue / 60.0f;
  
  } else if (hue < 180.0f) {
    result = b;
  
  } else if (hue < 240.0f) {
    result = a + (b - a) * (240.0f - hue) / 60.0f;
  
  } else {
    result = a;
  }
  
  return result;
}

/*
 * Convert an RGB color to HSL.
 * 
 * pRGB points to the RGB color.  This structure is first adjusted in
 * the following way.  Any component that is not a finite value is set
 * to zero.  Then, each component is clamped to range [0.0, 1.0].
 * 
 * Note that the RGB channels must be in range [0.0, 1.0] rather than
 * the integer range [0, 255].
 * 
 * Grayscale values may not be provided or a fault occurs.  This is
 * because the hue is undefined in grayscale cases.  The grayscale check
 * is done after the RGB values are adjusted as noted above.
 * 
 * The HSL result is written to pHSL.
 * 
 * Parameters:
 * 
 *   pRGB - pointer to the input RGB, which may be adjusted
 * 
 *   pHSL - pointer to the output HSL
 */
static void rgb2hsl(RGB *pRGB, HSL *pHSL) {
  
  float min = 0.0f;
  float max = 0.0f;
  float D = 0.0f;
  
  /* Check parameters */
  if ((pRGB == NULL) || (pHSL == NULL)) {
    abort();
  }
  
  /* Fix non-finite channels */
  if (!isfinite(pRGB->r)) {
    pRGB->r = 0.0f;
  }
  if (!isfinite(pRGB->g)) {
    pRGB->g = 0.0f;
  }
  if (!isfinite(pRGB->b)) {
    pRGB->b = 0.0f;
  }
  
  /* Clamp channel values */
  if (!(pRGB->r >= 0.0f)) {
    pRGB->r = 0.0f;
  }
  if (!(pRGB->g >= 0.0f)) {
    pRGB->g = 0.0f;
  }
  if (!(pRGB->b >= 0.0f)) {
    pRGB->b = 0.0f;
  }
  
  if (!(pRGB->r <= 1.0f)) {
    pRGB->r = 1.0f;
  }
  if (!(pRGB->g <= 1.0f)) {
    pRGB->g = 1.0f;
  }
  if (!(pRGB->b <= 1.0f)) {
    pRGB->b = 1.0f;
  }

  /* Fault if grayscale */
  if ((pRGB->r == pRGB->g) && (pRGB->r == pRGB->b)) {
    abort();
  }
  
  /* Adapted from The Revolutionary Guide to Bitmapped Graphics,
   * Control-Zed, Wrox Press, 1994, pg. 122-123 */
  
  max = pRGB->r;
  if (pRGB->g > max) {
    max = pRGB->g;
  }
  if (pRGB->b > max) {
    max = pRGB->b;
  }
  
  min = pRGB->r;
  if (pRGB->g < min) {
    min = pRGB->g;
  }
  if (pRGB->b < min) {
    min = pRGB->b;
  }

  pHSL->l = (max + min) / 2.0f;
  assert(max != min);
  D = max - min;
  
  if (pHSL->l <= 0.5f) {
    pHSL->s = D / (max + min);
  } else {
    pHSL->s = D / (2.0f - max - min);
  }
  
  if (pRGB->r == max) {
    pHSL->h = (pRGB->g - pRGB->b) / D;
  
  } else if (pRGB->g == max) {
    pHSL->h = 2.0f + (pRGB->b - pRGB->r) / D;
  
  } else if (pRGB->b == max) {
    pHSL->h = 4.0f + (pRGB->r - pRGB->g) / D;
  
  } else {
    /* Shouldn't happen */
    abort();
  }

  pHSL->h *= 60.0f;
  while(pHSL->h >= 360.0f) {
    pHSL->h -= 360.0f;
  }
  while(pHSL->h < 0.0f) {
    pHSL->h += 360.0f;
  }
}

/*
 * Convert an HSL color to RGB.
 * 
 * pHSL points to the HSL color.  This structure is first adjusted in
 * the following way.  Any component that is not a finite value is set
 * to zero.  Then, the S and L components are clamped to the range
 * [0.0, 1.0].  Finally, the H component is adjusted to the degree range
 * [0.0, 360.0).  The H component adjustment is by successive additions
 * or subtractions, so do not pass a huge positive or negative value for
 * H.
 * 
 * The RGB result is written to pRGB
 * 
 * Parameters:
 * 
 *   pHSL - pointer to the input HSL, which may be adjusted
 * 
 *   pRGB - pointer to the output RGB
 */
static void hsl2rgb(HSL *pHSL, RGB *pRGB) {
  
  float m = 0.0f;
  float n = 0.0f;
  
  /* Check parameters */
  if ((pHSL == NULL) || (pRGB == NULL)) {
    abort();
  }
  
  /* Fix non-finite channels */
  if (!isfinite(pHSL->h)) {
    pHSL->h = 0.0f;
  }
  if (!isfinite(pHSL->s)) {
    pHSL->s = 0.0f;
  }
  if (!isfinite(pHSL->l)) {
    pHSL->l = 0.0f;
  }
  
  /* Clamp S and L values */
  if (!(pHSL->s >= 0.0f)) {
    pHSL->s = 0.0f;
  }
  if (!(pHSL->l >= 0.0f)) {
    pHSL->l = 0.0f;
  }
  
  if (!(pHSL->s <= 1.0f)) {
    pHSL->s = 1.0f;
  }
  if (!(pHSL->l <= 1.0f)) {
    pHSL->l = 1.0f;
  }
  
  /* Adjust H value */
  while (pHSL->h < 0.0f) {
    pHSL->h += 360.0f;
  }
  while (pHSL->h >= 360.0f) {
    pHSL->h -= 360.0f;
  }
  
  /* Adapted from The Revolutionary Guide to Bitmapped Graphics,
   * Control-Zed, Wrox Press, 1994, pg. 124 */
  if (pHSL->l <= 0.5f) {
    n = pHSL->l * (1.0f + pHSL->s);
  } else {
    n = pHSL->l + pHSL->s - pHSL->l * pHSL->s;
  }
  
  m = 2.0f * pHSL->l - n;
  
  if (pHSL->s == 0) {
    pRGB->r = pHSL->l;
    pRGB->g = pHSL->l;
    pRGB->b = pHSL->l;
  
  } else {
    pRGB->r = hslval(m, n, pHSL->h + 120.0f);
    pRGB->g = hslval(m, n, pHSL->h);
    pRGB->b = hslval(m, n, pHSL->h - 120.0f);
  }
    
  /* Assert finite */
  assert(isfinite(pRGB->r));
  assert(isfinite(pRGB->g));
  assert(isfinite(pRGB->b));
    
  /* Clamp ranges */
  if (pRGB->r < 0.0f) {
    pRGB->r = 0.0f;
  }
  if (pRGB->g < 0.0f) {
    pRGB->g = 0.0f;
  }
  if (pRGB->b < 0.0f) {
    pRGB->b = 0.0f;
  }
    
  if (pRGB->r > 1.0f) {
    pRGB->r = 1.0f;
  }
  if (pRGB->g > 1.0f) {
    pRGB->g = 1.0f;
  }
  if (pRGB->b > 1.0f) {
    pRGB->b = 1.0f;
  }
}

/*
 * Apply fading to an RGB value.
 * 
 * rgb is the 32-bit ARGB color to fade.
 * 
 * rate is a value in range [0, 255].  If 255, then rgb is returned
 * as-is.  If zero, then a fully transparent pixel is returned.  If in
 * between, the alpha value in the original RGB value is scaled.
 * 
 * Parameters:
 * 
 *   rgb - the RGB value to fade
 * 
 *   rate - the shading rate
 * 
 * Return:
 * 
 *   the faded value
 */
static uint32_t fade(uint32_t rgb, int rate) {
  
  uint32_t result = 0;
  SPH_ARGB argb;
  
  /* Initialize structure */
  memset(&argb, 0, sizeof(SPH_ARGB));
  
  /* Check parameters */
  if ((rate < 0) || (rate > 255)) {
    abort();
  }
  
  /* Handle cases */
  if (rate >= 255) {
    /* Full shading, so return RGB as-is */
    result = rgb;
    
  } else if (rate < 1) {
    /* No shading, so return fully transparent */
    result = 0;
    
  } else {
    /* Partial shading, so first unpack to ARGB */
    sph_argb_unpack(rgb, &argb);
    
    /* Adjust alpha */
    argb.a = (int) (
                (((int32_t) argb.a) * ((int32_t) rate)) / 255
              );
    
    /* Pack to get result */
    result = sph_argb_pack(&argb);
  }
  
  /* Return result */
  return result;
}

/*
 * Apply alpha compositing.
 * 
 * over is the 32-bit ARGB color that is on top.
 * 
 * under is the 32-bit ARGB color that is underneath.
 * 
 * Gamma table must be initialized before calling.
 * 
 * Parameters:
 * 
 *   over - the over color
 * 
 *   under - the under color
 * 
 * Return:
 * 
 *   the composited result
 */
static uint32_t composite(uint32_t over, uint32_t under) {
  
  SPH_ARGB co;
  SPH_ARGB cu;
  SPH_ARGB cf;
  float ao = 0.0f;
  float au = 0.0f;
  float af = 0.0f;
  float mo = 0.0f;
  float mu = 0.0f;
  
  /* Initialize structures */
  memset(&co, 0, sizeof(SPH_ARGB));
  memset(&cu, 0, sizeof(SPH_ARGB));
  memset(&cf, 0, sizeof(SPH_ARGB));
  
  /* Unpack colors */
  sph_argb_unpack(over, &co);
  sph_argb_unpack(under, &cu);
  
  /* Get floating-point alpha values */
  ao = ((float) co.a) / 255.0f;
  au = ((float) cu.a) / 255.0f;
  
  /* Calculate output alpha */
  af = ao + (au * (1.0f - ao));
  if (af * 255.0f < 1.0f) {
    af = 0.0f;
  }
  
  /* Watch for zero output alpha case */
  if (af != 0.0f) {
    
    /* Non-zero output alpha -- composite each component */
    cf.a = (int) floor(((double) af) * 255.0);
    
    mo = gamma_undo(co.r);
    mu = gamma_undo(cu.r);
    cf.r = gamma_correct(((mo * ao) + (mu * au * (1.0f - ao))) / af);
    
    mo = gamma_undo(co.g);
    mu = gamma_undo(cu.g);
    cf.g = gamma_correct(((mo * ao) + (mu * au * (1.0f - ao))) / af);
    
    mo = gamma_undo(co.b);
    mu = gamma_undo(cu.b);
    cf.b = gamma_correct(((mo * ao) + (mu * au * (1.0f - ao))) / af);
    
  } else {
    /* Zero output alpha, so final is fully transparent */
    cf.a = 0;
    cf.r = 0;
    cf.g = 0;
    cf.b = 0;
  }
  
  /* Pack for result */
  return sph_argb_pack(&cf);
}

/*
 * Apply colorization to the given RGB color.
 * 
 * rgb_in is the 32-bit ARGB color to colorize.
 * 
 * rgb_tint is the 24-bit RGB color to use as a tint.  The eight most
 * significant bits are ignored.
 * 
 * Parameters:
 * 
 *   rgb_in - the input RGB
 * 
 *   rgb_tint - the tint
 * 
 * Return:
 * 
 *   the colorized output
 */
static uint32_t colorize(uint32_t rgb_in, uint32_t rgb_tint) {
  
  SPH_ARGB argb;
  int gray_i = 0;
  float gray = 0.0f;
  RGB rgb;
  HSL hsl;
  
  /* Initialize structures */
  memset(&argb, 0, sizeof(SPH_ARGB));
  memset(&rgb, 0, sizeof(RGB));
  memset(&hsl, 0, sizeof(HSL));
  
  /* Down-convert input to grayscale */
  sph_argb_unpack(rgb_in, &argb);
  sph_argb_downGray(&argb);
  gray_i = argb.r;

  /* Unpack RGB tint */
  sph_argb_unpack(rgb_tint, &argb);
 
  /* Check if tint is grayscale */
  if ((argb.r == argb.g) && (argb.r == argb.b)) {
    /* Grayscale tint, so result is just grayscale input */
    argb.a = 255;
    argb.r = gray_i;
    argb.g = gray_i;
    argb.b = gray_i;
  
  } else {
    /* Not a grayscale tint, next check if input greyscale is pure white
     * or black */
    if (gray_i < 1) {
      /* Input grayscale pure black, so result is black */
      argb.a = 255;
      argb.r = 0;
      argb.g = 0;
      argb.b = 0;
      
    } else if (gray_i > 254) {
      /* Input grayscale pure white, so result is white */
      argb.a = 255;
      argb.r = 255;
      argb.g = 255;
      argb.b = 255;
      
    } else {
      /* General case -- input grayscale not pure white or black and
       * tint is not grayscale -- compute floating-point values */
      gray = ((float) gray_i) / 255.0f;

      rgb.r = ((float) argb.r) / 255.0f;
      rgb.g = ((float) argb.g) / 255.0f;
      rgb.b = ((float) argb.b) / 255.0f;

      /* Convert RGB to HSL */
      rgb2hsl(&rgb, &hsl);

      /* Set lightness to grayscale value */
      hsl.l = gray;
        
      /* Convert adjusted HSL back to RGB */
      hsl2rgb(&hsl, &rgb);
        
      /* Convert floating-point RGB to integer */
      argb.a = 255;
      argb.r = (int) floor(((double) rgb.r) * 255.0);
      argb.g = (int) floor(((double) rgb.g) * 255.0);
      argb.b = (int) floor(((double) rgb.b) * 255.0);
        
      if (argb.r < 0) {
        argb.r = 0;
      }
      if (argb.g < 0) {
        argb.g = 0;
      }
      if (argb.b < 0) {
        argb.b = 0;
      }
        
      if (argb.r > 255) {
        argb.r = 255;
      }
      if (argb.g > 255) {
        argb.g = 255;
      }
      if (argb.b > 255) {
        argb.b = 255;
      }
    }
  }
  
  /* Return packed value */
  return sph_argb_pack(&argb);
}

/*
 * Core program function.
 * 
 * The virtual texture table and shading table module must be
 * initialized before calling this function.
 * 
 * The path parameters specify the paths to the relevant files.
 * 
 * The error parameter is either NULL or it points to an integer to
 * receive an error code upon return.
 * 
 * The errloc parameter is either NULL or it points to an integer to
 * receive an error location upon return.
 * 
 * If the function succeeds, a non-zero value is returned and the error
 * code integer (if provided) is set to zero.
 * 
 * If the function fails, a zero value is returned and the error code
 * integer (if provided) is set to an error code.  This error code can
 * be converted into an error message using lilac_errorString().
 * 
 * The error location is set if the error applies to a specific input
 * file.  If there is no error, or the error is not specific to one
 * particular input file, the error location is set to zero.  See the
 * error location definitions earlier in this module.
 * 
 * Parameters:
 * 
 *   pOutPath - path to the output image file
 * 
 *   pMaskPath - path to the mask input image file
 * 
 *   pPencilPath - path to the pencil input image file
 * 
 *   pShadingPath - path to the shading input image file
 * 
 *   pError - pointer to error code return, or NULL
 * 
 *   pErrLoc - pointer to error location, or NULL
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int lilac(
    const char * pOutPath,
    const char * pMaskPath,
    const char * pPencilPath,
    const char * pShadingPath,
           int * pError,
           int * pErrLoc) {
  
  int dummy = 0;
  int status = 1;
  int errcode = 0;
  
  SPH_IMAGE_WRITER *pWriter = NULL;
  
  SPH_IMAGE_READER *pMaskRead = NULL;
  SPH_IMAGE_READER *pPencilRead = NULL;
  SPH_IMAGE_READER *pShadingRead = NULL;
  
  SPH_ARGB argb;
  SHADEREC srec;
  
  int maskval = 0;
  int pencilval = 0;
  int32_t rgbindex = 0;
  
  uint32_t *pOutScan = NULL;
  uint32_t *pMaskScan = NULL;
  uint32_t *pPencilScan = NULL;
  uint32_t *pShadingScan = NULL;
  
  int32_t width = 0;
  int32_t height = 0;
  
  int32_t x = 0;
  int32_t y = 0;
  
  time_t last_update = (time_t) 0;
  time_t current = (time_t) 0;
  
  /* Initialize structures */
  memset(&argb, 0, sizeof(SPH_ARGB));
  memset(&srec, 0, sizeof(SHADEREC));
  
  /* Check parameters */
  if ((pOutPath == NULL) || (pMaskPath == NULL) ||
      (pPencilPath == NULL) || (pShadingPath == NULL)) {
    abort();
  }
  
  /* Redirect error pointers if NULL */
  if (pError == NULL) {
    pError = &dummy;
  }
  if (pErrLoc == NULL) {
    pErrLoc = &dummy;
  }
  
  /* Reset error information */
  *pError = 0;
  *pErrLoc = ERRORLOC_UNKNOWN;
  
  /* Initialize gamma correction tables for sRGB */
  gamma_sRGB();
  
  /* Open readers on each input file */
  if (status) {
    pMaskRead = sph_image_reader_newFromPath(pMaskPath, &errcode);
    if (pMaskRead == NULL) {
      *pError = errcode + ERROR_SPH_MIN;
      *pErrLoc = ERRORLOC_MASKFILE;
      status = 0;
    }
  }
  
  if (status) {
    pPencilRead = sph_image_reader_newFromPath(pPencilPath, &errcode);
    if (pPencilRead == NULL) {
      *pError = errcode + ERROR_SPH_MIN;
      *pErrLoc = ERRORLOC_PENCILFILE;
      status = 0;
    }
  }
  
  if (status) {
    pShadingRead = sph_image_reader_newFromPath(pShadingPath, &errcode);
    if (pShadingRead == NULL) {
      *pError = errcode + ERROR_SPH_MIN;
      *pErrLoc = ERRORLOC_SHADINGFILE;
      status = 0;
    }
  }
  
  /* Get the width and height of the mask file */
  if (status) {
    width = sph_image_reader_width(pMaskRead);
    height = sph_image_reader_height(pMaskRead);
  }
  
  /* Verify that the pencil and shading files have the same
   * dimensions */
  if (status) {
    if (width != sph_image_reader_width(pPencilRead)) {
      *pError = ERROR_MISMATCH;
      status = 0;
    }
  }
  if (status) {
    if (width != sph_image_reader_width(pShadingRead)) {
      *pError = ERROR_MISMATCH;
      status = 0;
    }
  }
  if (status) {
    if (height != sph_image_reader_height(pPencilRead)) {
      *pError = ERROR_MISMATCH;
      status = 0;
    }
  }
  if (status) {
    if (height != sph_image_reader_height(pShadingRead)) {
      *pError = ERROR_MISMATCH;
      status = 0;
    }
  }
  
  /* Open a writer for the output file with the same image dimensions */
  if (status) {
    pWriter = sph_image_writer_newFromPath(
                pOutPath,
                width,
                height,
                SPH_IMAGE_DOWN_NONE,
                0,
                &errcode);
    if (pWriter == NULL) {
      *pError = errcode + ERROR_SPH_MIN;
      *pErrLoc = ERRORLOC_OUTFILE;
      status = 0;
    }
  }
  
  /* Get the scanline pointer for output */
  if (status) {
    pOutScan = sph_image_writer_ptr(pWriter);
  }
  
  /* Begin with the update timer and current time set to the current
   * time (or -1 if error) */
  if (status) {
    last_update = time(NULL);
    current = last_update;
  }
  
  /* Go through each scanline */
  if (status) {
    for(y = 0; y < height; y++) {

      /* If there hasn't been a timer error, see if we need a status
       * update */
      if ((last_update != (time_t)-1) && (current != (time_t)-1)) {
        /* Get current time */
        current = time(NULL);
        
        /* Only proceed if we successfully read current time */
        if (current != (time_t)-1) {
          /* If current time has changed, then update last_update and
           * print a status report */
          if (last_update != current) {
            last_update = current;
            fprintf(stderr, "%s: Rendering %ld / %ld (%.1f%%)\n",
              pModule, (long) (y + 1), (long) height,
              (((double) (y + 1)) / ((double) height)) * 100.0);
          }
        }
      }

      /* Load each scanline from the input files */
      if (status) {
        pMaskScan = sph_image_reader_read(pMaskRead, &errcode);
        if (pMaskScan == NULL) {
          *pError = errcode + ERROR_SPH_MIN;
          *pErrLoc = ERRORLOC_MASKFILE;
          status = 0;
        }
      }
      
      if (status) {
        pPencilScan = sph_image_reader_read(pPencilRead, &errcode);
        if (pPencilScan == NULL) {
          *pError = errcode + ERROR_SPH_MIN;
          *pErrLoc = ERRORLOC_PENCILFILE;
          status = 0;
        }
      }
      
      if (status) {
        pShadingScan = sph_image_reader_read(pShadingRead, &errcode);
        if (pShadingScan == NULL) {
          *pError = errcode + ERROR_SPH_MIN;
          *pErrLoc = ERRORLOC_SHADINGFILE;
          status = 0;
        }
      }

      /* Go through each pixel */
      if (status) {
        for(x = 0; x < width; x++) {

          /* Unpack the mask file pixel */
          sph_argb_unpack(pMaskScan[x], &argb);
          
          /* Down-convert mask file pixel to grayscale */
          sph_argb_downGray(&argb);
          
          /* If grayscale value 128 or greater, set mask value; else,
           * clear it */
          if (argb.g >= 128) {
            maskval = 1;
          } else {
            maskval = 0;
          }
          
          /* Unpack the pencil file pixel */
          sph_argb_unpack(pPencilScan[x], &argb);
          
          /* Down-convert pencil file pixel to grayscale */
          sph_argb_downGray(&argb);
          
          /* If grayscale value 128 or greater, set pencil value; else,
           * clear it */
          if (argb.g >= 128) {
            pencilval = 1;
          } else {
            pencilval = 0;
          }
      
          /* Unpack the shading file pixel */
          sph_argb_unpack(pShadingScan[x], &argb);
          
          /* Down-convert shading file pixel to RGB and set the alpha
           * channel to zero so we only have the RGB */
          sph_argb_downRGB(&argb);
          argb.a = 0;
          
          /* Pack the down-converted shading pixel with zero alpha to
           * get the RGB index */
          rgbindex = (int32_t) sph_argb_pack(&argb);
          
          /* Check for cases */
          if (maskval) {
            /* Mask file white, so output fully transparent */
            pOutScan[x] = 0;

          } else if (!pencilval) {
            /* Mask file black, pencil file black -- get shade record */
            srec.rgbidx = rgbindex;
            ttable_query(&srec);
            
            /* Begin with the second texture faded by the drawing
             * rate */
            pOutScan[x] = fade(
                            vtx_query(2, x, y, width, height, &status),
                            srec.drate);
            
            /* Get the faded pencil texture over the first texture over
             * white */
            if (status) {
              pOutScan[x] = composite(
                              composite(
                                pOutScan[x],
                                vtx_query(
                                  1, x, y, width, height, &status)),
                              UINT32_C(0xffffffff));
            }
            
            /* Colorize the output (unless disabled) */
            if (status && (srec.rgbtint != UINT32_C(0xffffffff))) {
              pOutScan[x] = colorize(pOutScan[x], srec.rgbtint);
            }
          
          } else {
            /* Mask file black, pencil file white -- get shade record */
            srec.rgbidx = rgbindex;
            ttable_query(&srec);
      
            /* Begin with the requested texture faded by the shading
             * rate */
            pOutScan[x] = fade(
                            vtx_query(
                              srec.tidx, x, y, width, height, &status),
                            srec.srate);
            
            /* Composite over the first texture and then pure white */
            if (status) {
              pOutScan[x] = composite(
                              composite(
                                pOutScan[x],
                                vtx_query(
                                  1, x, y, width, height, &status)),
                              UINT32_C(0xffffffff));
            }
            
            /* Colorize the output (unless disabled) */
            if (status && (srec.rgbtint != UINT32_C(0xffffffff))) {
              pOutScan[x] = colorize(pOutScan[x], srec.rgbtint);
            }
          }
          
          /* Leave loop if error */
          if (!status) {
            break;
          }
        }
      }
      
      /* Write the output scanline */
      if (status) {
        sph_image_writer_write(pWriter);
      }
      
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
  }
  
  /* Close writer object if open */
  sph_image_writer_close(pWriter);
  pWriter = NULL;
  
  /* Close reader objects if open */
  sph_image_reader_close(pMaskRead);
  pMaskRead = NULL;
  
  sph_image_reader_close(pPencilRead);
  pPencilRead = NULL;
  
  sph_image_reader_close(pShadingRead);
  pShadingRead = NULL;
  
  /* Return status */
  return status;
}

/*
 * Program entrypoint
 * ==================
 */

int main(int argc, char *argv[]) {

  int status = 1;
  int i = 0;
  int errcode = 0;
  int errloc = 0;

  /* Get module name */
  if (argc > 0) {
    if (argv != NULL) {
      pModule = argv[0];
    }
  }
  if (pModule == NULL) {
    pModule = "lilac_draw";
  }
 
  /* Check parameters */
  if (argc < 0) {
    abort();
  }
  if ((argc > 0) && (argv == NULL)) {
    abort();
  }
  for(i = 0; i < argc; i++) {
    if (argv[i] == NULL) {
      abort();
    }
  }

  /* In addition to the module name, we must have at least eight
   * additional parameters */
  if (argc < 9) {
    fprintf(stderr, "%s: Not enough parameters!\n", pModule);
    status = 0;
  }
  
  /* The number of textures passed may not exceed the maximum number of
   * textures */
  if (status) {
    if (argc - 7 > TEXTURE_MAXCOUNT) {
      fprintf(stderr, "%s: Too many textures!\n", pModule);
      status = 0;
    }
  }
  
  /* Use parameter index six to initialize the programmable shader
   * module, unless it has the special value "-" */
  if (status) {
    if (strcmp(argv[6], "-") != 0) {
      if (!pshade_load(argv[6], &errcode)) {
        status = 0;
        fprintf(stderr, "%s: Error loading programmable shader...\n",
          pModule);
        fprintf(stderr, "%s: %s!\n",
          pModule, pshade_errorString(errcode));
      }
    }
  }
  
  /* Starting at parameter index seven and proceeding through the
   * remaining parameters, load each path in the virtual texture
   * table */
  if (status) {
    for(i = 7; i < argc; i++) {
      if (!vtx_load(argv[i])) {
        status = 0;
        break;
      }
    }
  }
  
  /* Use parameter index five to initialize the shading table */
  if (status) {
    if (!ttable_parse(argv[5], &errcode, &errloc, m_vtx_count)) {
      fprintf(stderr, "%s: Error reading table file...\n", pModule);
      if (errloc >= 0) {
        fprintf(stderr, "%s: Error on line %d...\n", pModule, errloc);
      }
      fprintf(stderr, "%s: %s!\n", pModule,
              ttable_errorString(errcode));
      status = 0;
    }
  }
  
  /* Begin the core program function */
  if (status) {
    if (!lilac(argv[1], argv[2], argv[3], argv[4], &errcode, &errloc)) {
      
      if (errloc == ERRORLOC_OUTFILE) {
        fprintf(stderr, "%s: Error writing output file...\n", pModule);
      
      } else if (errloc == ERRORLOC_MASKFILE) {
        fprintf(stderr, "%s: Error reading mask file...\n", pModule);
      
      } else if (errloc == ERRORLOC_PENCILFILE) {
        fprintf(stderr, "%s: Error reading pencil file...\n", pModule);
        
      } else if (errloc == ERRORLOC_SHADINGFILE) {
        fprintf(stderr, "%s: Error reading shading file...\n", pModule);
      }
      
      fprintf(stderr, "%s: %s!\n", pModule, lilac_errorString(errcode));
      status = 0;
    }
  }
  
  /* Invert status and return */
  if (status) {
    status = 0;
  } else {
    status = 1;
  }
  return status;
}
