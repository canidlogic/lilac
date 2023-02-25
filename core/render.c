/*
 * render.c
 * ========
 * 
 * Implementation of render.h
 * 
 * See the header for further information.
 */

#include "render.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lilac.h"
#include "sophistry.h"

/*
 * Constants
 * ---------
 */

/*
 * Maximum number of render preparation functions that can be
 * registered.
 */
#define MAX_PREP_COUNT (1024)

/*
 * Type declarations
 * -----------------
 */

struct PREP_LINK_TAG;
typedef struct PREP_LINK_TAG PREP_LINK;
struct PREP_LINK_TAG {
  
  /*
   * Pointer to the render preparation function to invoke.
   */
  fp_render_prep fp;
  
  /*
   * Pointer to the next link in the chain, or NULL if this is the last
   * link.
   */
  PREP_LINK *pNext;
};

/*
 * Local data
 * ----------
 */

/*
 * Render mode flag.
 */
static int m_render = 0;

/*
 * In render mode only, these variables track the current offset, X and
 * Y coordinates, and the cached width and height.
 */
static int32_t m_offs = 0;
static int32_t m_x    = 0;
static int32_t m_y    = 0;
static int32_t m_w    = 0;
static int32_t m_h    = 0;

/*
 * Linked list of preparation functions.
 * 
 * If no preparation functions have been registered yet, the count will
 * be zero and both pointers will be NULL.
 * 
 * Otherwise, the count is the total number of links in the chain, and
 * the pointers are non-NULL values that point to the first and last
 * links in the chain, respectively.
 */
static int32_t m_plCount = 0;
static PREP_LINK *m_plFirst = NULL;
static PREP_LINK *m_plLast  = NULL;

/*
 * Local functions
 * ---------------
 */

/* Prototypes */
static void raiseErr(int sourceLine);

/*
 * Stop on an error.
 * 
 * Use __LINE__ for the argument so that the position of the error will
 * be reported.
 * 
 * This function will not return.
 * 
 * Parameters:
 * 
 *   sourceLine - the line number in the source file the error happened
 */
static void raiseErr(int sourceLine) {
  raiseErrGlobal(sourceLine, __FILE__);
}

/*
 * Public function implementations
 * -------------------------------
 * 
 * See the header for specifications.
 */

/*
 * render_prepare function.
 */
void render_prepare(fp_render_prep fp) {
  
  PREP_LINK *pl = NULL;
  
  /* Check state */
  if (m_render) {
    raiseErr(__LINE__);
  }
  
  /* Check parameter */
  if (fp == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Check that we have space for another prep function */
  if (m_plCount >= MAX_PREP_COUNT) {
    fprintf(stderr, "%s: Too many render preparation functions!\n",
            getModule());
    raiseErr(__LINE__);
  }
  
  /* Allocate a new link */
  pl = (PREP_LINK *) calloc(1, sizeof(PREP_LINK));
  if (pl == NULL) {
    fprintf(stderr, "%s: Out of memory!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Write the function into the link and set the next pointer to NULL
   * since this will be the new last link */
  pl->fp = fp;
  pl->pNext = NULL;
  
  /* Integrate the new link into the chain */
  if (m_plCount > 0) {
    /* This is not the first link */
    m_plLast->pNext = pl;
    m_plLast = pl;
    m_plCount++;
    
  } else {
    /* This is the first link */
    m_plCount = 1;
    m_plFirst = pl;
    m_plLast  = pl;
  }
}

/*
 * render_go function.
 */
void render_go(NODE *pNode) {
  
  const PREP_LINK *pl = NULL;
  SPH_IMAGE_WRITER *pw = NULL;
  int errnum = 0;
  uint32_t *pScan = NULL;
  uint32_t *ps = NULL;
  
  /* Check state */
  if (m_render) {
    raiseErr(__LINE__);
  }
  
  /* Check parameter */
  if (pNode == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Run all render preparation functions */
  if (m_plCount > 0) {
    for(pl = m_plFirst; pl != NULL; pl = pl->pNext) {
      (pl->fp)();
    }
  }
  
  /* Enter render mode */
  m_render = 1;
  
  /* Initialize pixel position */
  m_offs = 0;
  m_x    = 0;
  m_y    = 0;
  m_w    = getConfigInt(CFG_DIM_WIDTH);
  m_h    = getConfigInt(CFG_DIM_HEIGHT);
  
  /* Open an image writer for the results */
  pw = sph_image_writer_newFromPath(
        getConfigStr(CFG_OUT_PATH),
        m_w, m_h, SPH_IMAGE_DOWN_NONE, 0, &errnum);
  if (pw == NULL) {
    fprintf(stderr, "%s: Failed to open output image: %s!\n",
            getModule(), sph_image_errorString(errnum));
    raiseErr(__LINE__);
  }
  
  /* Get the scanline pointer */
  pScan = sph_image_writer_ptr(pw);
  
  /* Render each scanline */
  for(m_y = 0; m_y < m_h; m_y++) {
    
    /* Reset the pixel pointer to start of scanline */
    ps = pScan;
    
    /* Render each pixel in the scanline */
    for(m_x = 0; m_x < m_w; m_x++) {
    
      /* Use the root node to render the current pixel */
      *ps = node_invoke(pNode);
      ps++;
      
      /* Increment pixel offset variable */
      m_offs++;
    }
    
    /* Write the finished scanline to output */
    sph_image_writer_write(pw);
  }
  
  /* Close and finish the output file */
  sph_image_writer_close(pw);
  pw = NULL;
  
  /* Leave render mode */
  m_render = 0;
}

/*
 * render_mode function.
 */
int render_mode(void) {
  return m_render;
}

/*
 * render_offset function.
 */
int32_t render_offset(void) {
  if (!m_render) {
    raiseErr(__LINE__);
  }
  return m_offs;
}

/*
 * render_X function.
 */
int32_t render_X(void) {
  if (!m_render) {
    raiseErr(__LINE__);
  }
  return m_x;
}

/*
 * render_Y function.
 */
int32_t render_Y(void) {
  if (!m_render) {
    raiseErr(__LINE__);
  }
  return m_y;
}
