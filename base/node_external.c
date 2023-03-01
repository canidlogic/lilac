/*
 * node_external.c
 * ===============
 * 
 * Implementation of node_external.h
 * 
 * See the header for further information.
 */

#include "node_external.h"

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "istr.h"
#include "lilac.h"
#include "node.h"
#include "render.h"
#include "vm.h"

#include "sophistry.h"

/*
 * Type declarations
 * =================
 */

/*
 * Structure used for instance data for external nodes.
 */
typedef struct {
  
  /*
   * The index of the data for this external node within interleaved
   * pixel data.
   * 
   * Within each interleaved pixel, the pixel for this external node
   * will be the uint32_t with index i, where i=0 is the first.
   */
  int32_t i;
  
} EXTERNAL_NODE;

/*
 * Structure (EXTERNAL_LINK) used for storing a linked list of all the
 * external PNG files that will need to be compiled.
 * 
 * A structure prototype precedes it so that the structure can refer to
 * itself.
 */
struct EXTERNAL_LINK_TAG;
typedef struct EXTERNAL_LINK_TAG EXTERNAL_LINK;
struct EXTERNAL_LINK_TAG {
  
  /*
   * Pointer to the next record in the chain, or NULL if this is the
   * last record.
   */
  EXTERNAL_LINK *pNext;
  
  /*
   * Holds a reference to the string storing the path to the external
   * PNG file.
   */
  ISTR iPath;
};

/*
 * Local data
 * ==========
 */

/*
 * Linked list storing the paths to all the external PNG files that will
 * be interleaved.
 * 
 * m_count is the number of external PNG files that have been scheduled
 * for interleaving.
 * 
 * m_bytes is the total number of bytes required to hold all the
 * interleaved data.  This must remain under the CFG_EXTERNAL_DISK_MIB
 * limit.
 * 
 * m_pFirst and m_pLast point to the first and last linked-list element,
 * or are NULL if the linked list is empty.
 */
static int32_t m_count = 0;
static int64_t m_bytes = 0;
static EXTERNAL_LINK *m_pFirst = NULL;
static EXTERNAL_LINK *m_pLast = NULL;

/*
 * The interleaved pixel buffer on disk.
 * 
 * fp is NULL until initialized.  Then, it stores the file handle to the
 * interleaved data buffer temporary file.  The file pointer has an
 * undefined position.
 */
static FILE *m_fp = NULL;

/*
 * The memory buffer for interleaved pixel data.
 * 
 * m_pBuf is NULL until initialized.
 * 
 * m_offs, when initialized, is the pixel offset of the first
 * interleaved pixel stored in the memory buffer.
 * 
 * m_plen, when initialized, is the number of interleaved pixels that
 * can be stored in the memory buffer.
 */
static uint32_t *m_pBuf = NULL;
static int32_t m_offs = 0;
static int32_t m_plen = 0;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static void raiseErr(int sourceLine);

static int32_t buffer_offs(int32_t offs);

static void prep_external(void);
static uint32_t node_external(void *pv);
static void op_external(void);

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
 * Make sure that the memory buffer includes a specific pixel offset and
 * return the starting offset of that interleaved pixel within the
 * buffer.
 * 
 * If the requested offset is not currently in the buffer, then the
 * buffer is reloaded starting at the given offset.  If you pass the
 * special value of -1, then the buffer will always be reloaded at
 * offset zero and zero will be returned -- this is only used when
 * initializing the buffer for the first time.
 * 
 * Parameters:
 * 
 *   offs - the pixel offset to load
 * 
 * Return:
 * 
 *   the uint32_t offset within the buffer of the first dword belonging
 *   to this interleaved pixel
 */
static int32_t buffer_offs(int32_t offs) {
  
  int reload = 0;
  int32_t max_offs = 0;
  int32_t pcount = 0;
  int32_t po = 0;
  
  /* Check state */
  if ((m_fp == NULL) || (m_pBuf == NULL)) {
    raiseErr(__LINE__);
  }
  
  /* Compute maximum pixel offset */
  max_offs = getConfigInt(CFG_DIM_WIDTH) * getConfigInt(CFG_DIM_HEIGHT);
  max_offs--;
  
  /* Check parameter */
  if ((offs < -1) || (offs > max_offs)) {
    raiseErr(__LINE__);
  }
  
  /* Reload flag starts out zero, unless -1 was given, in which case set
   * the reload flag and change offs to zero */
  if (offs == -1) {
    reload = 1;
    offs = 0;
    
  } else {
    reload = 0;
  }
  
  /* If requested offset is not within the current buffer, then set
   * reload flag */
  if ((offs < m_offs) || (offs >= m_offs + m_plen)) {
    reload = 1;
  }
  
  /* If reload flag is set, then load memory buffer starting at the
   * given offset */
  if (reload) {
    /* Seek to start of interleaved data to read */
    if (fseek(
          m_fp,
          ((size_t) offs) * ((size_t) m_count) * sizeof(uint32_t),
          SEEK_SET)) {
      fprintf(stderr, "%s: I/O error!\n", getModule());
      raiseErr(__LINE__);
    }
    
    /* Compute number of interleaved pixels to read, which is minimum of
     * number of interleaved pixels remaining at offs and the number of
     * interleaved pixels that fit in the memory buffer */
    pcount = max_offs - offs;
    if (m_plen < pcount) {
      pcount = m_plen;
    }
    
    /* Read pixel data into the buffer */
    if (fread(
          m_pBuf,
          ((size_t) m_count) * sizeof(uint32_t),
          (size_t) pcount,
          m_fp) != pcount) {
      fprintf(stderr, "%s: I/O error!\n", getModule());
      raiseErr(__LINE__);
    }
    
    /* Update buffer offset */
    m_offs = offs;
  }
  
  /* Compute pixel offset within the memory buffer */
  po = offs - m_offs;
  
  /* Compute DWORD offset within the memory buffer */
  po *= m_count;
  
  /* Return start of interleaved pixel */
  return po;
}

/*
 * Render preparation function that loads all the interleaved pixel data
 * in the temporary file and starts the memory buffer.
 */
static void prep_external(void) {
  int32_t i = 0;
  int32_t step = 0;
  int32_t x = 0;
  int32_t y = 0;
  int32_t w = 0;
  int32_t h = 0;
  int32_t ipsize = 0;
  int32_t bfsize = 0;
  uint32_t *pScan = NULL;
  int errnum = 0;
  EXTERNAL_LINK *pl = NULL;
  SPH_IMAGE_READER *pr = NULL;
  char dummy = (char) 0;
  
  /* Check state */
  if ((m_fp != NULL) || (m_pBuf != NULL)) {
    raiseErr(__LINE__);
  }
  
  /* Only proceed if at least one external node */
  if (m_count > 0) {
    
    /* Create temporary file for the interleaved data */
    m_fp = tmpfile();
    if (m_fp == NULL) {
      fprintf(stderr, "%s: Failed to create temporary file!\n",
              getModule());
      raiseErr(__LINE__);
    }
    
    /* Set the full length of the temporary file */
    if (fseek(m_fp, (long) (m_bytes - 1), SEEK_SET)) {
      fprintf(stderr, "%s: I/O error!\n", getModule());
      raiseErr(__LINE__);
    }
    
    if (fwrite(&dummy, 1, 1, m_fp) != 1) {
      fprintf(stderr, "%s: Out of disk space!\n", getModule());
      raiseErr(__LINE__);
    }
  
    /* Compute the step, which is how much to add to the file offset
     * after writing each subpixel to get to the next subpixel in the
     * image; this is the total size of an interleaved pixel, less the
     * uint32_t that is written for the subpixel */
    step = (m_count - 1) * ((int32_t) sizeof(uint32_t));
    
    /* Get the width and height of the output image */
    w = getConfigInt(CFG_DIM_WIDTH);
    h = getConfigInt(CFG_DIM_HEIGHT);
    
    /* Decode all the external files into the file */
    pl = m_pFirst;
    for(i = 0; i < m_count; i++) {
      /* Check we have a current link */
      if (pl == NULL) {
        raiseErr(__LINE__);
      }
      
      /* Set file pointer so it is ready to write the first subpixel of
       * this image */
      if (fseek(
            m_fp,
            ((size_t) i) * sizeof(uint32_t),
            SEEK_SET)) {
        fprintf(stderr, "%s: I/O error!\n", getModule());
        raiseErr(__LINE__);
      }
      
      /* Initialize image reader on current PNG file */
      pr = sph_image_reader_newFromPath(
            istr_ptr(&(pl->iPath)), &errnum);
      if (pr == NULL) {
        fprintf(stderr, "%s: Failed to read external image %s: %s!\n",
                getModule(), istr_ptr(&(pl->iPath)),
                sph_image_errorString(errnum));
        reportLine();
        raiseErr(__LINE__);
      }
      
      /* Check that dimensions match */
      if ((w != sph_image_reader_width(pr)) ||
          (h != sph_image_reader_height(pr))) {
        fprintf(stderr, "%s: External image %s has wrong dimensions!\n",
                getModule(), istr_ptr(&(pl->iPath)));
        reportLine();
        raiseErr(__LINE__);
      }
      
      /* Go through each scanline */
      for(y = 0; y < h; y++) {
      
        /* Read the next scanline */
        pScan = sph_image_reader_read(pr, &errnum);
        if (pScan == NULL) {
          fprintf(stderr, "%s: Error reading external image %s: %s!\n",
                  getModule(), istr_ptr(&(pl->iPath)),
                  sph_image_errorString(errnum));
          reportLine();
          raiseErr(__LINE__);
        }
      
        /* Transfer each pixel into the interleaved data file */
        for(x = 0; x < w; x++) {
          
          /* If this is not the very first pixel, advance the file
           * pointer by the step distance */
          if ((x != 0) || (y != 0)) {
            if (fseek(m_fp, (size_t) step, SEEK_CUR)) {
              fprintf(stderr, "%s: I/O error!\n", getModule());
              raiseErr(__LINE__);
            }
          }
          
          /* If this is not the first pixel of the scanline, advance the
           * scanline pointer */
          if (x != 0) {
            pScan++;
          }
          
          /* Write the subpixel into the interleaved data file */
          if (fwrite(pScan, sizeof(uint32_t), 1, m_fp) != 1) {
            fprintf(stderr, "%s: I/O error!\n", getModule());
            raiseErr(__LINE__);
          }
        }
      }
      
      /* Close image reader */
      sph_image_reader_close(pr);
      pr = NULL;
      
      /* Move to next link in chain */
      pl = pl->pNext;
    }
    
    /* All the interleaved data is now prepared, so we move on to
     * setting up the memory buffer; begin by computing the size of an
     * interleaved pixel */
    ipsize = m_count * ((int32_t) sizeof(uint32_t));
    
    /* Compute the requested buffer size */
    bfsize = getConfigInt(CFG_EXTERNAL_RAM_KIB) * 1024;
    
    /* Align buffer so its length is a multiple of the interleaved data
     * size */
    bfsize = bfsize / ipsize;
    if (bfsize < 1) {
      bfsize = 1;
    }
    bfsize = bfsize * ipsize;
    
    /* Allocate the buffer and initialize buffer state variables */
    m_pBuf = (uint32_t *) malloc((size_t) bfsize);
    if (m_pBuf == NULL) {
      fprintf(stderr, "%s: Out of memory!\n", getModule());
      raiseErr(__LINE__);
    }
    
    m_offs = 0;
    m_plen = bfsize / ipsize;
    
    /* Force a reload of the buffer to initialize it at the start */
    buffer_offs(-1);
  }
}

/*
 * Function that implements external nodes.
 * 
 * Parameters:
 * 
 *   pv - pointer to a EXTERNAL_NODE
 * 
 * Return:
 * 
 *   the color output of this node
 */
static uint32_t node_external(void *pv) {
  
  EXTERNAL_NODE *pd = NULL;
  int32_t ro = 0;
  
  /* Check parameter and cast to pointer type */
  if (pv == NULL) {
    raiseErr(__LINE__);
  }
  pd = (EXTERNAL_NODE *) pv;
  
  /* Check state */
  if ((m_fp == NULL) || (m_pBuf == NULL)) {
    raiseErr(__LINE__);
  }
  
  /* Get the pixel offset that is currently being rendered */
  ro = render_offset();
  
  /* Convert that pixel offset into a DWORD offset in the memory
   * buffer */
  ro = buffer_offs(ro);
  
  /* Return the appropriate subpixel from the interleaved pixel */
  return m_pBuf[ro + pd->i];
}

/*
 * Function that is invoked to implement the "external" script
 * operation.
 * 
 * Takes a single input parameter that is the path to the external PNG
 * file.  The result is a new node that returns pixel data from the PNG
 * file.  The external file is not accessed when this node is created;
 * rather, everything will be interleaved during a render preparation
 * function.
 * 
 * Script syntax:
 * 
 *   [path:string] external [r:node]
 */
static void op_external(void) {
  
  ISTR str;
  int64_t ex = 0;
  int64_t lim = 0;
  EXTERNAL_LINK *pl = NULL;
  EXTERNAL_NODE *pd = NULL;
  NODE *pn = NULL;
  
  /* Initialize structures */
  istr_init(&str);
  
  /* Get parameters */
  if (vm_type() != VM_TYPE_STRING) {
    fprintf(stderr, "%s: external op expects string argument!\n",
            getModule());
    reportLine();
    raiseErr(__LINE__);
  }
  
  vm_pop_s(&str);
  
  /* Compute the size in bytes needed for the memory buffer, since at
   * least one full interleaved pixel must fit in the memory buffer */
  ex = ((int64_t) m_count) + 1;
  ex = ex * ((int64_t) sizeof(uint32_t));
  
  /* Compute the limit in bytes for the memory buffer */
  lim = ((int64_t) getConfigInt(CFG_EXTERNAL_RAM_KIB))
        * INT64_C(1024);
  
  /* Check whether adding this node would go over the memory limit */
  if (ex > lim) {
    fprintf(stderr,
    "%s: external node buffer exceeded! Adjust CFG_EXTERNAL_RAM_KIB.\n",
    getModule());
    reportLine();
    raiseErr(__LINE__);
  }
  
  /* Compute the size in bytes of pixel data needed for this node */
  ex = ((int64_t) getConfigInt(CFG_DIM_WIDTH))
        * ((int64_t) getConfigInt(CFG_DIM_HEIGHT))
        * ((int64_t) sizeof(uint32_t));
  
  /* Compute the limit in bytes for all interleaved data */
  lim = ((int64_t) getConfigInt(CFG_EXTERNAL_DISK_MIB))
        * INT64_C(1024) * INT64_C(1024);
  
  /* Check whether adding this node would go over the disk limit */
  if (ex + m_bytes > lim) {
    fprintf(stderr,
    "%s: external node limit exceeded! Adjust CFG_EXTERNAL_DISK_MIB.\n",
    getModule());
    reportLine();
    raiseErr(__LINE__);
  }
  
  /* We have room for this node, so start by creating the new link */
  pl = (EXTERNAL_LINK *) calloc(1, sizeof(EXTERNAL_LINK));
  if (pl == NULL) {
    fprintf(stderr, "%s: Out of memory!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Fill in the new link */
  pl->pNext = NULL;
  istr_copy(&(pl->iPath), &str);
  
  /* Add to linked list */
  if (m_count < 1) {
    m_count = 1;
    m_bytes = ex;
    m_pFirst = pl;
    m_pLast = pl;
    
  } else {
    m_count++;
    m_bytes += ex;
    m_pLast->pNext = pl;
    m_pLast = pl;
  }
  
  /* Create a new instance data structure for the new node */
  pd = (EXTERNAL_NODE *) calloc(1, sizeof(EXTERNAL_NODE));
  if (pd == NULL) {
    fprintf(stderr, "%s: Out of memory!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Initialize the instance data */
  pd->i = m_count - 1;
  
  /* Create the new node -- since this node does not reference any other
   * nodes, the depth is set to one */
  pn = node_define(pd, &node_external, 1);
  
  /* Push the results */
  vm_push_n(pn);
  
  /* Release local structures */
  istr_reset(&str);
}

/*
 * Public function implementations
 * ===============================
 */

void node_external_init(void) {
  /* Register the render preparation function */
  render_prepare(&prep_external);
  
  /* Register the "external" operation */
  vm_register("external", &op_external);
}
