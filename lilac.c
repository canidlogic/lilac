/*
 * lilac.c
 * 
 * Main program module for lilac.
 * 
 * See readme file for more information about this program.
 */

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sophistry.h"
#include "texture.h"
#include "ttable.h"

/*
 * The Sophistry output encoding quality value.
 * 
 * This is only used with JPEG output, which is not recommended.
 */
#define OUT_QUALITY (90)

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

/* Function prototypes */
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
  
  /* @@FIXME: this would be better if gamma correction were undone
   * before the operation and then re-applied afterwards */
  
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
    
    mo = ((float) co.r) / 255.0f;
    mu = ((float) cu.r) / 255.0f;
    
    cf.r = (int) floor((double) (
                    ((mo * ao) + (mu * au * (1.0f - ao))) / af
                  ) * 255.0);
    
    mo = ((float) co.g) / 255.0f;
    mu = ((float) cu.g) / 255.0f;
    
    cf.g = (int) floor((double) (
                    ((mo * ao) + (mu * au * (1.0f - ao))) / af
                  ) * 255.0);
    
    mo = ((float) co.b) / 255.0f;
    mu = ((float) cu.b) / 255.0f;
    
    cf.b = (int) floor((double) (
                    ((mo * ao) + (mu * au * (1.0f - ao))) / af
                  ) * 255.0);
    
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
 * The texture module and shading table module must be initialized
 * before calling this function.
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
                OUT_QUALITY,
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
 
  /* Go through each scanline */
  if (status) {
    for(y = 0; y < height; y++) {

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
            
            /* Get the second texture over the first texture over
             * white */
            pOutScan[x] = composite(
                            composite(
                              texture_pixel(2, x, y),
                              texture_pixel(1, x, y)),
                            UINT32_C(0xffffffff));
            
            /* Colorize the output */
            pOutScan[x] = colorize(pOutScan[x], srec.rgbtint);
          
          } else {
            /* Mask file black, pencil file white -- get shade record */
            srec.rgbidx = rgbindex;
            ttable_query(&srec);
      
            /* Begin with the requested texture faded by the shading
             * rate */
            pOutScan[x] = fade(
                            texture_pixel(srec.tidx, x, y),
                            srec.srate);
            
            /* Composite over the first texture and then pure white */
            pOutScan[x] = composite(
                            composite(
                              pOutScan[x],
                              texture_pixel(1, x, y)),
                            UINT32_C(0xffffffff));
            
            /* Colorize the output */
            pOutScan[x] = colorize(pOutScan[x], srec.rgbtint);
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
 * Program entrypoint.
 */
int main(int argc, char *argv[]) {

  char *pModule = NULL;
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
    pModule = "lilac";
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
  
  /* In addition to the module name, we must have at least seven
   * additional parameters */
  if (argc < 8) {
    fprintf(stderr, "%s: Not enough parameters!\n", pModule);
    status = 0;
  }
  
  /* The number of textures passed may not exceed the maximum number of
   * textures */
  if (status) {
    if (argc - 6 > TEXTURE_MAXCOUNT) {
      fprintf(stderr, "%s: Too many textures!\n", pModule);
      status = 0;
    }
  }
  
  /* Starting at parameter index six and proceeding through the
   * remaining parameters, pass each path to the texture module to load
   * the texture */
  if (status) {
    for(i = 6; i < argc; i++) {
      if (!texture_load(argv[i], &errcode)) {
        
        fprintf(stderr, "%s: Error loading texture %d...\n",
                  pModule,
                  texture_count() + 1);
        
        if (errcode == SPH_IMAGE_ERR_IMAGEDIM) {
          fprintf(stderr, "%s: Texture dimensions too large!\n",
                    pModule);
        } else {
          fprintf(stderr, "%s: %s!\n",
                    pModule,
                    sph_image_errorString(errcode));
        }
        
        status = 0;
        break;
      }
    }
  }
  
  /* Use parameter index five to initialize the shading table */
  if (status) {
    if (!ttable_parse(argv[5], &errcode, &errloc)) {
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
