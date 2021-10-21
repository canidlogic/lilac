/*
 * texture.c
 * 
 * Implementation of texture.h
 * 
 * See the header for further information.
 */

#include "texture.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "sophistry.h"

/*
 * Structure definitions
 * =====================
 */

typedef struct {
  
  /*
   * Pointer to the dynamically-allocated pixel data.
   * 
   * This stores scanlines in top-to-bottom order, with pixels in the
   * scanlines stored in left-to-right order.  There is no padding at
   * the end of scanlines.
   */
  uint32_t *pData;
  
  /*
   * The width of the texture in pixels.
   */
  int32_t width;
  
  /*
   * The height of the texture in pixels.
   */
  int32_t height;
  
} TEXTURE;

/*
 * Texture map
 * ===========
 */

/* 
 * The number of textures that have been loaded.
 */
static int m_texture_count = 0;

/*
 * The texture table.
 */
static TEXTURE m_texture[TEXTURE_MAXCOUNT];

/*
 * Local functions
 * ===============
 */

/* Function prototypes */
static void initTable(void);

/*
 * Initialize the texture table if no textures have been loaded yet.
 */
static void initTable(void) {
  if (m_texture_count < 1) {
    memset(m_texture, 0, sizeof(TEXTURE) * TEXTURE_MAXCOUNT);
  }
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * texture_load function.
 */
int texture_load(const char *pPath, int *pError) {
  
  int dummy = 0;
  int status = 1;
  
  SPH_IMAGE_READER *pr = NULL;
  TEXTURE *pt = NULL;
  
  int32_t w = 0;
  int32_t h = 0;
  int32_t y = 0;
  
  uint32_t *pScan = NULL;
  
  /* Initialize texture table if necessary */
  initTable();
  
  /* Check parameters */
  if (pPath == NULL) {
    abort();
  }
  
  /* If no error pointer provided, set to dummy */
  if (pError == NULL) {
    pError = &dummy;
  }
  
  /* Clear error */
  *pError = SPH_IMAGE_ERR_NONE;
  
  /* Fail with unknown error if too many textures loaded */
  if (m_texture_count >= TEXTURE_MAXCOUNT) {
    *pError = SPH_IMAGE_ERR_UNKNOWN;
    status = 0;
  }
  
  /* Open the image file */
  if (status) {
    pr = sph_image_reader_newFromPath(pPath, pError);
    if (pr == NULL) {
      status = 0;
    }
  }
  
  /* Get dimensions */
  if (status) {
    w = sph_image_reader_width(pr);
    h = sph_image_reader_height(pr);
  }
  
  /* Fail if dimensions out of range */
  if (status) {
    if ((w < 1) || (w > TEXTURE_MAXDIM) ||
        (h < 1) || (h > TEXTURE_MAXDIM)) {
      *pError = SPH_IMAGE_ERR_IMAGEDIM;
      status = 0;
    }
  }
  
  /* Increment texture count and get pointer to texture structure */
  if (status) {
    m_texture_count++;
    pt = &(m_texture[m_texture_count - 1]);
    pt->pData = NULL;
  }
  
  /* Copy dimensions into texture */
  if (status) {
    pt->width = w;
    pt->height = h;
  }
  
  /* Allocate buffer for image data */
  if (status) {
    /* We assume size_t is at least 32-bit to avoid overflow */
    assert(sizeof(size_t) >= 4);
    pt->pData = (uint32_t *) malloc(
                  (size_t) (w * h) * sizeof(uint32_t));
    if (pt->pData == NULL) {
      abort();
    }
    memset(pt->pData, 0, (size_t) (w * h) * sizeof(uint32_t));
  }
  
  /* Read each scanline into buffer */
  if (status) {
    for(y = 0; y < h; y++) {
      
      /* Read another scanline */
      pScan = sph_image_reader_read(pr, pError);
      if (pScan == NULL) {
        status = 0;
      }
      
      /* Leave loop if error */
      if (!status) {
        break;
      }
      
      /* Copy scanline into memory buffer */
      memcpy(
          pt->pData + (w * y),
          pScan,
          (size_t) (w) * sizeof(uint32_t));
    }
  }
  
  /* If there was an error but the texture was allocated, free it */
  if ((!status) && (pt != NULL)) {
    if (pt->pData != NULL) {
      free(pt->pData);
    }
    memset(pt, 0, sizeof(TEXTURE));
    m_texture_count--;
    pt = NULL;
  }
  
  /* Close image reader if open */
  sph_image_reader_close(pr);
  pr = NULL;
  
  /* Return status */
  return status;
}

/*
 * texture_count function.
 */
int texture_count(void) {
  return m_texture_count;
}

/*
 * texture_pixel function.
 */
uint32_t texture_pixel(int tidx, int32_t x, int32_t y) {
  
  TEXTURE *pt = NULL;
  
  /* Check parameters */
  if ((tidx < 1) || (tidx > m_texture_count) || (x < 0) || (y < 0)) {
    abort();
  }
  
  /* Get pointer to texture */
  pt = &(m_texture[tidx - 1]);
  
  /* Adjust X and Y to be in range of texture (apply infinite tiling) */
  if (x >= pt->width) {
    if (pt->width > 1) {
      x = x % (pt->width);
    } else {
      x = 0;
    }
  }
  
  if (y >= pt->height) {
    if (pt->height > 1) {
      y = y % (pt->height);
    } else {
      y = 0;
    }
  }
  
  /* Return relevant pixel */
  return (pt->pData)[x + (y * pt->width)];
}
