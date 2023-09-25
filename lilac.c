/*
 * lilac.c
 * =======
 * 
 * Implementation of lilac.h
 * 
 * See the header for further information.
 */

#include "lilac.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sophistry.h"

/*
 * Diagnostics
 * ===========
 */

/*
 * Callback functions used for errors and warnings, or NULL to use the
 * default implementations.
 */
static lilac_fp_err m_err = NULL;
static lilac_fp_warn m_warn = NULL;

/*
 * Raise an error.
 * 
 * This function does not return.  It will use a client-provided
 * callback m_err if available, or otherwise a default implementation.
 * 
 * Parameters:
 * 
 *   lnum - the line number in this source file
 * 
 *   pDetail - the error message, or NULL
 */
static void raiseErr(int lnum, const char *pDetail) {
  if (m_err != NULL) {
    m_err(lnum, pDetail);
    
  } else {
    fprintf(stderr, "[Lilac rendering error, line %d] ", lnum);
    if (pDetail != NULL) {
      fprintf(stderr, "%s\n", pDetail);
    } else {
      fprintf(stderr, "Generic error\n");
    }
    exit(EXIT_FAILURE);
  }
}

/*
 * Report a warning.
 * 
 * This function DOES return.  It will use a client-provided callback
 * m_warn if available, or otherwise a default implementation.
 * 
 * Parameters:
 * 
 *   lnum - the line number in this source file
 *   
 *   pDetail - the warning message, or NULL
 */
static void sayWarn(int lnum, const char *pDetail) {
  if (m_warn != NULL) {
    m_warn(lnum, pDetail);
  
  } else {
    fprintf(stderr, "[Lilac rendering warning, line %d] ", lnum);
    if (pDetail != NULL) {
      fprintf(stderr, "%s\n", pDetail);
    } else {
      fprintf(stderr, "Generic warning\n");
    }
  }
}

/*
 * Constants
 * =========
 */

/*
 * The different states of the rendering module.
 * 
 * The integer constants are significant here.  Zero is the starting
 * state, meaning not initialized yet.  Greater than zero means some
 * kind of intialized state.  Less than zero means compiled and closed
 * down.
 * 
 * Within the initialized states, increased integer values generally
 * means more things open.  STATE_INIT means initialized, nothing open.
 * STATE_TILE means just a tile open.  STATE_PATH means a tile and a
 * path open.  STATE_LOCK means a tile and a lock open.
 */
#define STATE_CLOSED (-1)
#define STATE_READY  ( 0)
#define STATE_INIT   ( 1)
#define STATE_TILE   ( 2)
#define STATE_PATH   ( 3)
#define STATE_LOCK   ( 4)

/*
 * The initial and maximum capacities of the intersection buffer.
 */
#define IBUF_INIT_CAP (64)
#define IBUF_MAX_CAP  INT32_C(1048576)

/*
 * When converting linear ARGB values, alpha values that are less than
 * this epsilon value are treated as though they were zero.  This avoids
 * numeric problems when undoing alpha premultiplication.
 */
#define ALPHA_EPSILON (0.0001f)

/*
 * When coordinates are less than this distance from each other, they
 * are assumed to be equal.
 */
#define COORD_EPSILON (0.00001)

/*
 * w values in circle rendering must be at least this far within the
 * boundaries of range [-1.0, 1.0] in order for intersection tests to
 * be done.  Else, intersection is assumed to be too close to horizontal
 * and is ignored.
 */
#define CIRCLE_EPSILON (0.00001)

/*
 * Type declarations
 * =================
 */

/*
 * Fully linearized ARGB value where all RGB channels are linear in
 * normalized range [0.0, 1.0].
 * 
 * Furthermore, this is a premultiplied alpha format, in contrast to the
 * non-premultiplied libsophistry format.
 */
typedef struct {
  float a;
  float r;
  float g;
  float b;
} LINEAR_ARGB;

/*
 * Parsed representation of an intersection record.
 */
typedef struct {
  
  /*
   * The tile X coordinate of the intersection.
   * 
   * Must be in range 0 to (LILAC_MAX_TILE - 1).
   */
  int32_t tx;
  
  /*
   * The tile Y coordinate of the intersection.
   * 
   * Must be in range 0 to (LILAC_MAX_TILE - 1).
   */
  int32_t ty;
  
  /*
   * The fill count adjustment value of the intersection.
   * 
   * Must be either 1 or -1.
   */
  int32_t adj;
  
} IREC;

/*
 * Local data
 * ==========
 */

/*
 * The state of this module.  This is one of the STATE_ constants.
 */
static int m_state = STATE_READY;

/*
 * The alpha blending cache.
 * 
 * The lilac_blend() function caches the most recent computation and
 * re-uses the results if the same computation is requested repeatedly.
 */
static int m_bcache_filled = 0;
static uint32_t m_bcache_over = 0;
static uint32_t m_bcache_under = 0;
static uint32_t m_bcache_result = 0;

/*
 * Handle to the temporary file.
 * 
 * The temporary file stores the full, uncompressed output image.  Each
 * pixel is an unsigned 32-bit integer stored in machine order.  Storage
 * order is by scanlines, with pixels left to right within scanlines and
 * scanlines ordered top to bottom.  The length in pixels of scanlines
 * always equals the width of the image.
 */
static FILE *m_fh = NULL;

/*
 * The width and height of the full image, in pixels.
 */
static int32_t m_w = 0;
static int32_t m_h = 0;

/*
 * The tile width and height.
 */
static int32_t m_dim = 0;

/*
 * The background color that each tile is initialized to.
 */
static uint32_t m_bgcol = 0;

/*
 * The current fill color.
 */
static uint32_t m_col = UINT32_C(0xff000000);

/*
 * The number of tile rows and tile columns that cover the whole output
 * image, and the total number of tiles.
 */
static int32_t m_rows = 0;
static int32_t m_cols = 0;
static int32_t m_tile_count = 0;

/*
 * The number of tiles that have been rendered.
 */
static int32_t m_finished = 0;

/*
 * Current tile information.
 * 
 * m_pos is the current tile index, or -1 if no tile currently loaded.
 * 
 * m_tx and m_ty are the (X,Y) coordinates of the upper-left corner of
 * the currently loaded tile.
 * 
 * m_tw and m_th are the actual width and height of the currently loaded
 * tile.
 */
static int32_t m_pos = -1;
static int32_t m_tx  =  0;
static int32_t m_ty  =  0;
static int32_t m_tw  =  0;
static int32_t m_th  =  0;

/*
 * The tile buffer.
 * 
 * Each pixel has the same packed 32-bit integer format as expected by
 * libsophistry.
 * 
 * The width and height of the tile buffer are both equal to m_dim.
 */
static uint32_t *m_tbuf = NULL;

/*
 * The starting count buffer.
 * 
 * This stores, for each scanline of the tile, the fill count of the
 * first pixel of the scanline.
 * 
 * The length of this array is equal to the tile dimensions.
 */
static int32_t *m_start = NULL;

/*
 * The fill count delta buffer.
 * 
 * During rasterization, this stores, for each tile scanline, the
 * changes in fill count at each pixel.  The length of this array is
 * equal to the tile dimensions.
 */
static int32_t *m_delta = NULL;

/*
 * The intersection buffer.
 * 
 * The cap variable stores the current capacity of the buffer in
 * elements.  The len stores how many elements are actually in use.
 * 
 * Each intersection record is packed within an unsigned 32-bit integer.
 * The least significant bit is clear if this intersection is moving
 * downwards and the fill count should be decremented; or set if this
 * intersection is moving upwards and the fill count should be
 * incremented.  Above that are 15 bits selecting the X coordinate
 * within the tile scanline.  Above that are 15 bits selecting the Y
 * coordinate within the tile.
 * 
 * The records in the buffer are not sorted until a path is about to be
 * filled.
 */
static int32_t m_ibuf_cap = 0;
static int32_t m_ibuf_len = 0;
static uint32_t *m_ibuf = NULL;

/*
 * The gamma-correction table.
 * 
 * This is used to convert between sRGB and linear RGB space during
 * lilac_blend().  m_gamma_init indicates whether the table has been
 * initialized.
 * 
 * The lookup table maps each of the 256 sRGB channel values to a linear
 * channel value in range 0.0f to 1.0f.
 */
static int m_gamma_init = 0;
static float m_gamma[256];

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static int cmpUint32(const void *pA, const void *pB);

static void appendInter(uint32_t p);

static void unpackIRec(uint32_t p, IREC *pr);
static uint32_t packIRec(const IREC *pr);

static void verify_gamma(void);
static void init_gamma(void);
static float gamma_undo(int c);
static int gamma_correct(float v);

static void sRGBtoLinear(uint32_t c, LINEAR_ARGB *pl);
static uint32_t lineartosRGB(LINEAR_ARGB *pl);

/*
 * Comparison callback for standard library qsort() function.
 * 
 * This implementation assumes the parameters are uint32_t values and
 * sorts them in ascending numeric order.
 */
static int cmpUint32(const void *pA, const void *pB) {
  
  const uint32_t *pv1 = NULL;
  const uint32_t *pv2 = NULL;
  
  uint32_t v1 = 0;
  uint32_t v2 = 0;
  
  int result = 0;
  
  /* Check parameters */
  if ((pA == NULL) || (pB == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Get integer values */
  pv1 = (const uint32_t *) pA;
  pv2 = (const uint32_t *) pB;
  
  v1 = *pv1;
  v2 = *pv2;
  
  /* Perform comparison */
  if (v1 > v2) {
    result = 1;
    
  } else if (v1 < v2) {
    result = -1;
    
  } else if (v1 == v2) {
    result = 0;
    
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  /* Return result */
  return result;
}

/*
 * Add a packed intersection record to the intersection buffer.
 * 
 * Parameters:
 * 
 *   p - the packed record to add
 */
static void appendInter(uint32_t p) {
  
  int32_t new_cap = 0;
  
  /* Check state */
  if (m_state < STATE_INIT) {
    raiseErr(__LINE__, "Wrong state");
  }
  
  /* Expand buffer if necessary */
  if (m_ibuf_len >= m_ibuf_cap) {
    /* Check that we can extend capacity */
    if (m_ibuf_cap >= IBUF_MAX_CAP) {
      raiseErr(__LINE__, "Too many intersection records");
    }
    
    /* New capacity is minimum of double current capacity and the
     * maximum capacity */
    new_cap = m_ibuf_cap * 2;
    if (new_cap > IBUF_MAX_CAP) {
      new_cap = IBUF_MAX_CAP;
    }
    
    /* Expand allocation */
    m_ibuf = (uint32_t *) realloc(
                            m_ibuf,
                            ((size_t) new_cap) * sizeof(uint32_t));
    if (m_ibuf == NULL) {
      raiseErr(__LINE__, "Out of memory");
    }
    
    memset(
      &(m_ibuf[m_ibuf_cap]),
      0,
      ((size_t) (new_cap - m_ibuf_cap)) * sizeof(uint32_t));
    
    /* Update capacity */
    m_ibuf_cap = new_cap;
  }
  
  /* Append entry */
  m_ibuf[m_ibuf_len] = p;
  m_ibuf_len++;
}

/*
 * Unpack a packed intersection record into an intersection structure.
 * 
 * Parameters:
 * 
 *   p - the packed intersection record
 * 
 *   pr - the structure to unpack the record into
 */
static void unpackIRec(uint32_t p, IREC *pr) {
  /* Check parameters */
  if (pr == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Reset record and unpack */
  memset(pr, 0, sizeof(IREC));
  
  pr->ty = (int32_t) ((p >> 16) & UINT32_C(0x7fff));
  pr->tx = (int32_t) ((p >>  1) & UINT32_C(0x7fff));
  if ((p & 0x1) == 1) {
    pr->adj = 1;
  } else {
    pr->adj = -1;
  }
}

/*
 * Pack an intersection structure into a packed intersection record.
 * 
 * Parameters:
 * 
 *   pr - the structure to pack
 * 
 * Return:
 * 
 *   the packed intersection record
 */
static uint32_t packIRec(const IREC *pr) {
  
  uint32_t p = 0;
  
  /* Check parameters */
  if (pr == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Check ranges */
  if ((pr->ty < 0) || (pr->ty > 0x7fff)) {
    raiseErr(__LINE__, NULL);
  }
  if ((pr->tx < 0) || (pr->tx > 0x7fff)) {
    raiseErr(__LINE__, NULL);
  }
  if ((pr->adj != -1) && (pr->adj != 1)) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Pack the record */
  p = (((uint32_t) pr->ty) << 16) | (((uint32_t) pr->tx) << 1);
  if (pr->adj == 1) {
    p |= 0x1;
  }
  
  /* Return the packed record */
  return p;
}

/*
 * Verify that the gamma table is initialized to proper values.
 * 
 * An error occurs if the gamma table is not initialized or it contains
 * improper values.
 * 
 * To contain proper values, the first record must be 0.0f and the last
 * record must be 1.0f.  Furthermore, all records must be in strictly
 * ascending order without any duplicates.  All records must be finite.
 */
static void verify_gamma(void) {
  
  int i = 0;
  float v = 0.0f;
  
  /* Make sure initialized */
  if (!m_gamma_init) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Check all values finite */
  for(i = 0; i < 256; i++) {
    if (!isfinite(m_gamma[i])) {
      raiseErr(__LINE__, "Gamma lookup numeric problems");
    }
  }
  
  /* Check boundary values */
  if ((m_gamma[0] != 0.0f) || (m_gamma[255] != 1.0f)) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Check strict ascending order */
  v = 0.0f;
  for(i = 1; i < 256; i++) {
    if (!(m_gamma[i] > v)) {
      raiseErr(__LINE__, "Gamma lookup numeric problems");
    }
    v = m_gamma[i];
  }
}

/*
 * Initialize the gamma-correction table appropriately for sRGB.
 * 
 * The call is ignored if the table is already initialized.
 */
static void init_gamma(void) {
  
  int x = 0;
  double u = 0.0;
  
  /* Only proceed if not yet initialized */
  if (!m_gamma_init) {
  
    /* Initialize and clear */
    m_gamma_init = 1;
    memset(m_gamma, 0, sizeof(float) * 256);
    
    /* Set boundaries */
    m_gamma[0] = 0.0f;
    m_gamma[255] = 1.0f;
    
    /* Set intermediate values according to sRGB */
    for(x = 1; x < 255; x++) {
      
      /* Get floating-point value */
      u = ((double) x) / 255.0;
      
      /* Compute value */
      if (u <= 0.04045) {
        u = u / 12.92;
      
      } else {
        u = pow((u + 0.055) / 1.055 , 2.4);
      }
  
      /* Store computed value */
      m_gamma[x] = (float) u;
    }
    
    /* Verify table */
    verify_gamma();
  }
}

/*
 * Given a gamma-corrected integer component, return a linearized
 * floating-point component.
 * 
 * The gamma table is automatically initialized if necessary.
 * 
 * c is clamped to range [0, 255] before linearization.  The output is
 * in range [0.0f, 1.0f].
 * 
 * Parameters:
 * 
 *   c - the component to undo gamma-correction for
 * 
 * Return:
 * 
 *   the linearized value
 */
static float gamma_undo(int c) {
  /* Make sure gamma table initialized */
  init_gamma();
  
  /* Clamp c */
  if (c < 0) {
    c = 0;
  } else if (c > 255) {
    c = 255;
  }
  
  /* Return value from gamma table */
  return m_gamma[c];
}

/*
 * Given a linear floating-point component, return a gamma-corrected
 * integer component.
 * 
 * The gamma table is automatically initialized if necessary.
 * 
 * v is clamped to range [0.0f, 1.0f] before beginning.  If v is
 * non-finite, it is set to 0.0f.
 * 
 * Parameters:
 * 
 *   v - the linear component to gamma-correct
 * 
 * Return:
 * 
 *   the gamma-corrected value
 */
static int gamma_correct(float v) {
  int result = 0;
  int lbound = 0;
  int hbound = 0;
  int mid = 0;
  float dl = 0.0f;
  float dh = 0.0f;
  
  /* Make sure gamma table initialized */
  init_gamma();
  
  /* Change non-finite values to zero */
  if (!isfinite(v)) {
    v = 0.0f;
  }
  
  /* Handle cases */
  if (v <= 0.0f) {
    /* v is zero or less, so result is zero */
    result = 0;
    
  } else if (v >= 1.0f) {
    /* v is one or greater, so result is 255 */
    result = 255;
    
  } else {
    /* General case -- need to do reverse lookup */
    lbound = 0;
    hbound = 255;
    while(lbound < hbound) {
      
      /* Choose midpoint halfway between but greater than lbound */
      mid = lbound + ((hbound - lbound) / 2);
      if (mid <= lbound) {
        mid = lbound + 1;
      }
      
      /* Compare value to midpoint */
      if (v > m_gamma[mid]) {
        /* v greater than midpoint, so midpoint is new lower bound */
        lbound = mid;
        
      } else if (v < m_gamma[mid]) {
        /* v less than midpoint, so upper bound below midpoint */
        hbound = mid - 1;
        
      } else if (v == m_gamma[mid]) {
        /* Found exact match, so zoom in on it */
        lbound = mid;
        hbound = mid;
        
      } else {
        /* Shouldn't happen */
        raiseErr(__LINE__, NULL);
      }
    }
    
    /* lbound is now greatest value in gamma table that is less than or
     * equal to v -- this shouldn't be the last entry */
    if (lbound >= 255) {
      raiseErr(__LINE__, NULL);
    }
    
    /* Compute distances to lbound and to next higher value */
    dl = v - m_gamma[lbound];
    dh = m_gamma[lbound + 1] - v;
    
    /* If dh is less than dl, then result is one greater than lbound,
     * else result is lbound */
    if (dh < dl) {
      result = lbound + 1;
    } else {
      result = lbound;
    }
  }
  
  /* Return result */
  return result;
}

/*
 * Convert a packed sRGB value in libsophistry format into a linear RGB
 * format in floating-point space
 * 
 * Parameters:
 * 
 *   c - the packed sRGB value
 * 
 *   pl - the structure to receive the linear RGB
 */
static void sRGBtoLinear(uint32_t c, LINEAR_ARGB *pl) {
  
  SPH_ARGB a;
  
  /* Initialize structures */
  memset(&a, 0, sizeof(SPH_ARGB));
  
  /* Check parameters */
  if (pl == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Unpack sRGB color */
  sph_argb_unpack(c, &a);
  
  /* Alpha channel is already linear, so convert directly and clamp to
   * range */
  pl->a = ((float) a.a) / 255.0f;
  if (!(pl->a >= 0.0f)) {
    pl->a = 0.0f;
  
  } else if (!(pl->a <= 1.0f)) {
    pl->a = 1.0f;
  }
  
  /* Convert RGB channels, undoing gamma correction in the process */
  pl->r = gamma_undo(a.r);
  pl->g = gamma_undo(a.g);
  pl->b = gamma_undo(a.b);
  
  /* Multiply each RGB channel by the alpha channel */
  pl->r *= pl->a;
  pl->g *= pl->a;
  pl->b *= pl->a;
}

/*
 * Convert a linear RGB value in floating-point space into a packed sRGB
 * value in libsophistry format.
 * 
 * Each floating-point value is clamped to range [0.0, 1.0], with
 * non-finite values set to 0.0, before performing the conversion.
 * 
 * The input structure is clobbered by this function and has an
 * undefined state upon return.
 * 
 * Parameters:
 * 
 *   pl - the linear RGB value to convert
 * 
 * Return:
 * 
 *   the packed sRGB value
 */
static uint32_t lineartosRGB(LINEAR_ARGB *pl) {
  
  SPH_ARGB a;
  
  /* Initialize structures */
  memset(&a, 0, sizeof(SPH_ARGB));
  
  /* Check parameters */
  if (pl == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Convert non-finite alpha to 0.0f and then clamp to range 0.0f to
   * 1.0f, along with converting values below ALPHA_EPSILON down to
   * zero */
  if (!isfinite(pl->a)) {
    pl->a = 0.0f;
  
  } else if (!(pl->a >= ALPHA_EPSILON)) {
    pl->a = 0.0f;
    
  } else if (!(pl->a <= 1.0f)) {
    pl->a = 1.0f;
  }
  
  /* For an alpha value of zero, set the RGB channels to zero;
   * otherwise, undo alpha premultiplication of the RGB channels by
   * dividing out the alpha value */
  if (pl->a == 0.0f) {
    pl->r = 0.0f;
    pl->g = 0.0f;
    pl->b = 0.0f;
  
  } else {
    pl->r /= pl->a;
    pl->g /= pl->a;
    pl->b /= pl->a;
  }
  
  /* Alpha value is linear in packed format, so convert it directly and
   * clamp to integer range */
  a.a = (int) floor(pl->a * 255.0f);
  if (a.a < 0) {
    a.a = 0;
  } else if (a.a > 255) {
    a.a = 255;
  }
  
  /* Apply gamma correction to RGB channels, which also clamps channel
   * values that are out of range */
  a.r = gamma_correct(pl->r);
  a.g = gamma_correct(pl->g);
  a.b = gamma_correct(pl->b);
  
  /* Return the packed value */
  return sph_argb_pack(&a);
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * lilac_init function.
 */
void lilac_init(
    int32_t       w,
    int32_t       h,
    int32_t       tile,
    uint32_t      bgcol,
    lilac_fp_err  fp_err,
    lilac_fp_warn fp_warn) {
  
  int32_t max_dim = 0;
  uint8_t dummy = 0;
  
  /* Check state */
  if (m_state != STATE_READY) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Set handlers right away */
  m_err = fp_err;
  m_warn = fp_warn;
  
  /* Check parameters */
  if ((w < 1) || (w > LILAC_MAX_IMAGE)) {
    raiseErr(__LINE__, "Output image width out of range");
  }
  if ((h < 1) || (h > LILAC_MAX_IMAGE)) {
    raiseErr(__LINE__, "Output image height out of range");
  }
  if ((tile < LILAC_MIN_TILE) || (tile > LILAC_MAX_TILE)) {
    raiseErr(__LINE__, "Tile dimension out of range");
  }
  
  /* Find the maximum of the output image dimensions */
  max_dim = w;
  if (h > max_dim) {
    max_dim = h;
  }
  
  /* If the maximum of the output image dimensions is less than the tile
   * dimension, set the tile dimension to the maximum of the maximum
   * output dimensions and the minimum tile dimensions */
  if (max_dim < tile) {
    tile = max_dim;
    if (tile < LILAC_MIN_TILE) {
      tile = LILAC_MIN_TILE;
    }
  }
  
  /* Initialize metrics */
  m_w = w;
  m_h = h;
  m_dim = tile;
  
  m_cols = m_w / m_dim;
  if ((m_w % m_dim) > 0) {
    m_cols++;
  }
  
  m_rows = m_h / m_dim;
  if ((m_h % m_dim) > 0) {
    m_rows++;
  }
  
  m_tile_count = m_cols * m_rows;
  
  /* Store background color */
  m_bgcol = bgcol;
  
  /* Allocate tile buffer and start out unloaded with no tiles rendered
   * yet */
  m_tbuf = (uint32_t *) calloc(
              (size_t) (m_dim * m_dim), sizeof(uint32_t));
  if (m_tbuf == NULL) {
    raiseErr(__LINE__, "Out of memory");
  }
  
  m_pos = -1;
  m_finished = 0;
  
  /* Allocate starting buffer */
  m_start = (int32_t *) calloc((size_t) m_dim, sizeof(int32_t));
  if (m_start == NULL) {
    raiseErr(__LINE__, "Out of memory");
  }
  
  /* Allocate delta buffer */
  m_delta = (int32_t *) calloc((size_t) m_dim, sizeof(int32_t));
  if (m_delta == NULL) {
    raiseErr(__LINE__, "Out of memory");
  }
  
  /* Allocate intersection buffer with initial capacity */
  m_ibuf_len = 0;
  m_ibuf_cap = IBUF_INIT_CAP;
  m_ibuf = (uint32_t *) calloc((size_t) IBUF_INIT_CAP,
                          sizeof(uint32_t));
  if (m_ibuf == NULL) {
    raiseErr(__LINE__, "Out of memory");
  }
  
  /* Create temporary file and set size to full image size */
  m_fh = tmpfile();
  if (m_fh == NULL) {
    raiseErr(__LINE__, "Failed to create temporary file");
  }
  
  if (fseek(m_fh, (long) ((m_w * m_h * 4) - 1), SEEK_SET)) {
    raiseErr(__LINE__, "I/O error");
  }
  
  if (fwrite(&dummy, 1, 1, m_fh) != 1) {
    raiseErr(__LINE__, "Failed to set temporary file size");
  }
  
  /* Change state to initialized */
  m_state = STATE_INIT;
}

/*
 * lilac_width function.
 */
int32_t lilac_width(void) {
  if (m_state < STATE_INIT) {
    raiseErr(__LINE__, NULL);
  }
  return m_w;
}

/*
 * lilac_height function.
 */
int32_t lilac_height(void) {
  if (m_state < STATE_INIT) {
    raiseErr(__LINE__, NULL);
  }
  return m_h;
}

/*
 * lilac_tiles function.
 */
int32_t lilac_tiles(void) {
  if (m_state < STATE_INIT) {
    raiseErr(__LINE__, NULL);
  }
  return m_tile_count;
}

/*
 * lilac_begin_tile function.
 */
void lilac_begin_tile(void) {
  
  int32_t tc = 0;
  int32_t i = 0;
  
  /* Check state */
  if (m_state != STATE_INIT) {
    raiseErr(__LINE__, "Wrong state");
  }
  
  /* Check that we haven't already rendered all tiles */
  if (m_finished >= m_tile_count) {
    raiseErr(__LINE__, "Tiles already rendered");
  }
  
  /* Setup tile information */
  m_pos = m_finished;
  
  m_ty = (m_pos / m_cols) * m_dim;
  m_tx = (m_pos % m_cols) * m_dim;
  
  m_tw = m_w - m_tx;
  if (m_tw > m_dim) {
    m_tw = m_dim;
  }
  
  m_th = m_h - m_ty;
  if (m_th > m_dim) {
    m_th = m_dim;
  }
  
  /* Clear the tile buffer to the background color */
  tc = m_dim * m_dim;
  for(i = 0; i < tc; i++) {
    m_tbuf[i] = m_bgcol;
  }
  
  /* Update state */
  m_state = STATE_TILE;
}

/*
 * lilac_end_tile function.
 */
void lilac_end_tile(void) {
  
  int32_t fptr = 0;
  int32_t fpitch = 0;
  int32_t y = 0;
  uint32_t *pp = NULL;
  
  /* Check state */
  if (m_state != STATE_TILE) {
    raiseErr(__LINE__, "Wrong state");
  }
  
  /* Compute the initial file pointer in the temporary file for the
   * start of the pixel data, and also the pitch in bytes of the
   * temporary file */
  fpitch = m_w * ((int32_t) sizeof(uint32_t));
  fptr = (m_ty * fpitch) + (m_tx * ((int32_t) sizeof(uint32_t)));
  
  /* Write tile data out to the temporary file in the proper
   * positions */
  pp = m_tbuf;
  for(y = 0; y < m_th; y++) {
    /* Seek to proper position */
    if (fseek(m_fh, (long) fptr, SEEK_SET)) {
      raiseErr(__LINE__, "I/O error");
    }
    
    /* Write the current scanline out to the file */
    if (fwrite(pp, sizeof(uint32_t), (size_t) m_tw, m_fh) != m_tw) {
      raiseErr(__LINE__, "I/O error");
    }
    
    /* Advance file pointer by the pitch */
    fptr += fpitch;
    
    /* Advance the tile pointer by the tile pitch */
    pp += m_dim;
  }
  
  /* Update finished count and state */
  m_finished++;
  m_pos = -1;
  m_state = STATE_INIT;
}

/*
 * lilac_blend function.
 */
uint32_t lilac_blend(uint32_t over, uint32_t under) {
  
  SPH_ARGB a;
  LINEAR_ARGB co;
  LINEAR_ARGB cu;
  LINEAR_ARGB cr;
  uint32_t result = 0;
  
  /* Check caching */
  if (m_bcache_filled) {
    if ((m_bcache_over == over) && (m_bcache_under == under)) {
      return m_bcache_result;
    }
  }
  
  /* Initialize structures */
  memset(&a, 0, sizeof(SPH_ARGB));
  memset(&co, 0, sizeof(LINEAR_ARGB));
  memset(&cu, 0, sizeof(LINEAR_ARGB));
  memset(&cr, 0, sizeof(LINEAR_ARGB));
  
  /* Unpack the over color */
  sph_argb_unpack(over, &a);
  
  /* Alpha channel of over color determines how to process */
  if (a.a >= 255) {
    /* Over color is fully opaque, so result is just the over color */
    result = over;
    
  } else if (a.a <= 0) {
    /* Over color is fully transparent, so result is just the under
     * color */
    result = under;
    
  } else {
    /* Over color is partially transparent, so we need to do full
     * blending -- start by converting to premultiplied linear RGB
     * space */
    sRGBtoLinear(over, &co);
    sRGBtoLinear(under, &cu);
    
    /* Compute the blended color */
    cr.a = co.a + (cu.a * (1.0f - co.a));
    cr.r = co.r + (cu.r * (1.0f - co.a));
    cr.g = co.g + (cu.g * (1.0f - co.a));
    cr.b = co.b + (cu.b * (1.0f - co.a));
    
    /* Convert result back to packed sRGB */
    result = lineartosRGB(&cr);
  }
  
  /* Update caching */
  m_bcache_filled = 1;
  m_bcache_over   = over;
  m_bcache_under  = under;
  m_bcache_result = result;
  
  /* Return result */
  return result;
}

/*
 * lilac_color function.
 */
void lilac_color(uint32_t col) {
  /* Check state */
  if (m_state < STATE_INIT) {
    raiseErr(__LINE__, "Wrong state");
  }
  
  /* Set color */
  m_col = col;
}

/*
 * lilac_begin_path function.
 */
void lilac_begin_path(void) {
  
  int32_t i = 0;
  
  /* Check state */
  if (m_state != STATE_TILE) {
    raiseErr(__LINE__, "Wrong state");
  }
  
  /* Clear the starting count buffer values to zero */
  for(i = 0; i < m_dim; i++) {
    m_start[i] = 0;
  }
  
  /* Clear the intersection buffer */
  m_ibuf_len = 0;
  
  /* Change state to path mode */
  m_state = STATE_PATH;
}

/*
 * lilac_end_path function.
 */
void lilac_end_path(void) {
  
  int32_t ii = 0;
  uint32_t *pRow = NULL;
  
  int32_t x = 0;
  int32_t y = 0;
  int64_t fc = 0;
  
  IREC ir;
  
  /* Initialize structures */
  memset(&ir, 0, sizeof(IREC));
  
  /* Check state */
  if (m_state != STATE_PATH) {
    raiseErr(__LINE__, "Wrong state");
  }
  
  /* If there is more than one intersection record, sort the
   * intersection records in ascending integer order, which is primarily
   * by tile Y coordinate, secondarily by tile X coordinate, and finally
   * by intersection direction */
  if (m_ibuf_len > 1) {
    qsort(m_ibuf, (size_t) m_ibuf_len, sizeof(uint32_t), &cmpUint32);
  }
  
  /* Start the intersection record offset at zero */
  ii = 0;
  
  /* Start the row pointer at the upper-left corner of the tile */
  pRow = m_tbuf;
  
  /* Rasterize each tile scanline */
  for(y = 0; y < m_th; y++) {
    /* If this is not the first scanline, advance the row pointer by the
     * pitch */
    if (y > 0) {
      pRow += m_dim;
    }
    
    /* Copy the first delta value from the starting array and check that
     * it isn't on verge of overflow */
    m_delta[0] = m_start[y];
    if ((m_delta[0] <= INT32_MIN) ||
        (m_delta[0] >= INT32_MAX)) {
      raiseErr(__LINE__, "Fill count delta overflow");
    }
    
    /* Clear all remaining delta values to zero */
    for(x = 1; x < m_tw; x++) {
      m_delta[x] = 0;
    }
    
    /* Process all intersection records relevant to this scanline */
    for( ; ii < m_ibuf_len; ii++) {
      /* Unpack current intersection record */
      unpackIRec(m_ibuf[ii], &ir);
      
      /* If this intersection record is for a later line, leave loop */
      if (ir.ty > y) {
        break;
      }
      
      /* Check range of tx */
      if ((ir.tx < 0) || (ir.tx >= m_tw)) {
        raiseErr(__LINE__, NULL);
      }
      
      /* Update relevant delta scan value */
      m_delta[ir.tx] += ir.adj;
      
      /* Watch for possible overflow */
      if ((m_delta[ir.tx] <= INT32_MIN) || 
          (m_delta[ir.tx] >= INT32_MAX)) {
        raiseErr(__LINE__, "Fill count delta overflow");
      }
    }
    
    /* Start fill count at zero */
    fc = 0;
    
    /* Draw each scanline pixel */
    for(x = 0; x < m_tw; x++) {
      /* Update fill count according to delta value */
      fc += ((int64_t) m_delta[x]);
      
      /* If fill count is non-zero, composite the current color on top
       * of the existing color in the tile */
      if (fc != 0) {
        pRow[x] = lilac_blend(m_col, pRow[x]);
      }
    }
  }
  
  /* Change state to tile mode */
  m_state = STATE_TILE;
}

/*
 * lilac_line function.
 */
void lilac_line(double x1, double y1, double x2, double y2) {
  
  double min_x = 0.0;
  double max_x = 0.0;
  double min_y = 0.0;
  double max_y = 0.0;
  
  double scan_min = 0.0;
  double scan_max = 0.0;
  
  double scan_begin = 0.0;
  double scan_end = 0.0;
  
  double r_min = 0.0;
  double r_max = 0.0;
  
  int32_t i = 0;
  int32_t j = 0;
  int32_t x = 0;
  int32_t y = 0;
  int32_t score = 0;
  
  double y_s = 0.0;
  double t = 0.0;
  double ix = 0.0;
  
  IREC ir;
  
  /* Initialize structures */
  memset(&ir, 0, sizeof(IREC));
  
  /* Check state */
  if (m_state != STATE_PATH) {
    raiseErr(__LINE__, "Wrong state");
  }
  
  /* Check for finite coordinates */
  if ((!isfinite(x1)) || (!isfinite(y1)) ||
      (!isfinite(x2)) || (!isfinite(y2))) {
    raiseErr(__LINE__, "Non-finite coordinates");
  }
  
  /* If Y endpoints are within epsilon distance of each other, then we
   * have a horizontal line, so skip it and do nothing further */
  if (fabs(y2 - y1) < COORD_EPSILON) {
    return;
  }
  
  /* Determine minimum and maximum X and Y coordinates */
  min_x = x1;
  max_x = x1;
  
  if (x2 < min_x) {
    min_x = x2;
  }
  if (x2 > max_x) {
    max_x = x2;
  }
  
  min_y = y1;
  max_y = y1;
  
  if (y2 < min_y) {
    min_y = y2;
  }
  if (y2 > max_y) {
    max_y = y2;
  }
  
  /* Determine the starting and ending X coordinates of each scanline in
   * this tile; the ending X coordinate is excluded */
  scan_begin = (double) m_tx;
  scan_end   = ((double) (m_tx + m_tw));
  
  /* Determine the minimum and maximum Y scanlines of this tile */
  scan_min = ((double) m_ty) + 0.5;
  scan_max = ((double) (m_ty + m_th - 1)) + 0.5;
  
  /* If minimum X of line is greater than or equal to the excluded X
   * endpoint of the line, then skip line */
  if (min_x >= scan_end) {
    return;
  }
  
  /* If minimum Y of line is greater than the maximum scanline Y, or
   * maximum Y of line is less than minimum scanline Y, then skip
   * line */
  if ((min_y > scan_max) || (max_y < scan_min)) {
    return;
  }
  
  /* Let r_min be the maximum of the minimum Y of the line and the
   * minimum scanline Y */
  r_min = min_y;
  if (scan_min > r_min) {
    r_min = scan_min;
  }
  
  /* Let r_max be the minimum of the maximum Y of the line and the
   * maximum scanline Y */
  r_max = max_y;
  if (scan_max < r_max) {
    r_max = scan_max;
  }
  
  /* Use r_min and r_max to determine the index i of the first tile
   * scanline greater or equal to r_min and the index j of the last tile
   * scanline less or equal to r_max */
  i = (int32_t) ceil(r_min - scan_min);
  j = (int32_t) floor(r_max - scan_min);
  
  /* If j is less than i, then the line is entirely between two
   * scanlines, so skip it */
  if (j < i) {
    return;
  }
  
  /* Clamp i and j to the tile boundaries */
  if (i < 0) {
    i = 0;
  } else if (i >= m_th) {
    i = m_th - 1;
  }
  
  if (j < 0) {
    j = 0;
  } else if (j >= m_th) {
    j = m_th - 1;
  }
  
  /* Determine the score for fill count adjustment based on direction of
   * the line */
  if (y1 < y2) {
    /* Line is going down */
    score = -1;
    
  } else if (y1 > y2) {
    /* Line is going up */
    score = 1;
    
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  /* Iterate through tile scanlines i to j */
  for(y = i; y <= j; y++) {
    
    /* Determine the Y coordinate of this tile scanline */
    y_s = ((double) (y + m_ty)) + 0.5;
    
    /* Determine t coordinate of intersection, and clamp to range 0.0 to
     * 1.0 */
    t = (y_s - y1) / (y2 - y1);
    if (!isfinite(t)) {
      raiseErr(__LINE__, "Numeric problem on line intersection");
    }
    if (!(t >= 0.0)) {
      t = 0.0;
    } else if (!(t <= 1.0)) {
      t = 1.0;
    }
    
    /* Compute X coordinate of scanline intersection */
    ix = ((1.0 - t) * x1) + (t * x2);
    if (!isfinite(ix)) {
      raiseErr(__LINE__, "Numeric problem on line intersection");
    }
    
    /* If intersection X coordinate is greater than or equal to the
     * excluded X endpoint of the scanline, then skip this scanline and
     * move to the next */
    if (ix >= scan_end) {
      continue;
    }
    
    /* If intersection X coordinate is less than the X starting point of
     * the scanline, set it to the X starting point of the scanline */
    if (!(ix >= scan_begin)) {
      ix = scan_begin;
    }
    
    /* Determine the X coordinate in the tile and clamp to tile */
    x = (int32_t) floor(ix - scan_begin);
    if (x < 0) {
      x = 0;
    } else if (x >= m_tw) {
      x = m_tw - 1;
    }
    
    /* Adjust either the starting count or the intersection array, based
     * on X coordinate */
    if (x <= 0) {
      /* X is zero, so we need to adjust the starting count */
      if (score == 1) {
        if (m_start[y] < INT32_MAX) {
          m_start[y] += 1;
        } else {
          raiseErr(__LINE__, "Delta count overflow");
        }
        
      } else if (score == -1) {
        if (m_start[y] > INT32_MIN) {
          m_start[y] -= 1;
        } else {
          raiseErr(__LINE__, "Delta count overflow");
        }
        
      } else {
        raiseErr(__LINE__, NULL);
      }
      
    } else {
      /* X is greater than zero, so encode intersection record and add
       * it to intersection buffer */
      ir.tx = x;
      ir.ty = y;
      ir.adj = score;
      
      appendInter(packIRec(&ir));
    }
  }
}

/*
 * lilac_dot function.
 */
void lilac_dot(double x, double y, double r) {
  
  double min_x = 0.0;
  double max_x = 0.0;
  double min_y = 0.0;
  double max_y = 0.0;
  
  double scan_min = 0.0;
  double scan_max = 0.0;
  
  double r_min = 0.0;
  double r_max = 0.0;
  
  int h = 0;
  int32_t i = 0;
  int32_t j = 0;
  int32_t k = 0;
  int32_t score = 0;
  int32_t xi = 0;
  
  double ys = 0.0;
  double w = 0.0;
  double a = 0.0;
  double b = 0.0;
  
  double x_up = 0.0;
  double x_down = 0.0;
  double x_intr = 0.0;
  
  IREC ir;
  
  /* Initialize structures */
  memset(&ir, 0, sizeof(IREC));
  
  /* Check state */
  if (m_state != STATE_PATH) {
    raiseErr(__LINE__, "Wrong state");
  }
  
  /* Check for finite values */
  if ((!isfinite(x)) || (!isfinite(y)) || (!isfinite(r))) {
    raiseErr(__LINE__, "Non-finite parameters");
  }
  
  /* Check that radius is greater than zero */
  if (!(r > 0.0)) {
    raiseErr(__LINE__, "Radius must be greater than zero");
  }
  
  /* If radius is less than coordinate epsilon, then do nothing */
  if (!(r >= COORD_EPSILON)) {
    return;
  }
  
  /* Determine the minimum and maximum X and Y coordinates of the dot */
  min_x = x - r;
  max_x = x + r;
  
  min_y = y - r;
  max_y = y + r;
  
  if ((!isfinite(min_x)) || (!isfinite(max_x)) ||
      (!isfinite(min_y)) || (!isfinite(max_y))) {
    raiseErr(__LINE__, "Numeric problem with circle bounding");
  }
  
  /* Determine the minimum and maximum scanline Y values in the current
   * tile */
  scan_min = ((double) m_ty) + 0.5;
  scan_max = ((double) (m_ty + m_th - 1)) + 0.5;
  
  /* If maximum Y of circle is less than the minimum scanline, skip this
   * dot; if minimum Y of circle is greater than maximum scanline, skip
   * this dot */
  if ((!(max_y >= scan_min)) || (!(min_y <= scan_max))) {
    return;
  }
  
  /* If minimum X of circle is greater or equal to X coordinate that
   * follows this tile, skip this dot */
  if (!(min_x < ((double) (m_tx + m_tw)))) {
    return;
  }
  
  /* Set r_min to the maximum of the circle minimum Y and the minimum Y
   * scanline; set r_max to the minimum of the circle maximum Y and the
   * maximum Y scanline */
  r_min = min_y;
  if (!(scan_min <= r_min)) {
    r_min = scan_min;
  }
  
  r_max = max_y;
  if (!(scan_max >= r_max)) {
    r_max = scan_max;
  }
  
  /* Determine range of tile scanlines that the circle crosses */
  i = (int32_t) ceil(r_min - scan_min);
  j = (int32_t) floor(r_max - scan_min);
  
  /* If empty range (circle between scanlines), then skip the dot */
  if (j < i) {
    return;
  }
  
  /* Bound i and j */
  if (i < 0) {
    i = 0;
  } else if (i >= m_th) {
    i = m_th - 1;
  }
  
  if (j < 0) {
    j = 0;
  } else if (j >= m_th) {
    j = m_th - 1;
  }
  
  /* Iterate through the relevant scanlines */
  for(k = i; k <= j; k++) {
    
    /* Get the Y coordinate of this scanline */
    ys = ((double) (k + m_ty)) + 0.5;
    
    /* Compute the w value of the circle at this scanline */
    w = (ys - y) / r;
    if (!isfinite(w)) {
      raiseErr(__LINE__, "Numeric problem with circle");
    }
    
    /* Skip this scanline if w not within range */
    if (!((w >= -1.0 + CIRCLE_EPSILON) &&
          (w <=  1.0 - CIRCLE_EPSILON))) {
      continue;
    }
    
    /* Compute base intersection angle and the cosine distance */
    a = asin(w);
    if (!isfinite(a)) {
      raiseErr(__LINE__, "Numeric problem with circle angle");
    }
    
    b = r * cos(a);
    if (!isfinite(b)) {
      raiseErr(__LINE__, "Numeric problem with circle angle");
    }
    
    /* Compute the X coordinates on this scanline where the circle is
     * crossing up and crossing down */
    x_down = x + b;
    x_up   = x - b;
    
    if ((!isfinite(x_down)) && (!isfinite(x_up))) {
      raiseErr(__LINE__, "Numeric problem finding circle intersection");
    }
    
    /* Record both these intersections */
    for(h = 0; h < 2; h++) {
      /* Get the current intersection point and its direction score */
      if (h == 0) {
        x_intr = x_up;
        score  = 1;
        
      } else if (h == 1) {
        x_intr = x_down;
        score  = -1;
        
      } else {
        raiseErr(__LINE__, NULL);
      }
      
      /* If current X is beyond this tile, skip it */
      if (!(x_intr < ((double) (m_tx + m_tw)))) {
        continue;
      }
      
      /* If current X is before this tile, boost it to start of tile */
      if (!(x_intr >= (double) m_tx)) {
        x_intr = (double) m_tx;
      }
      
      /* Convert to integer and bound */
      xi = (int32_t) floor(x_intr);
      if (xi < m_tx) {
        xi = m_tx;
      } else if (xi >= m_tx + m_tw)  {
        xi = m_tx + m_tw - 1;
      }
      
      /* Convert to tile X offset */
      xi = xi - m_tx;
      
      /* Record intersection based on offset */
      if (xi <= 0) {
        /* Intersection at start of line, so adjust starting count */
        if (score == 1) {
          if (m_start[k] < INT32_MAX) {
            m_start[k] += 1;
          } else {
            raiseErr(__LINE__, "Delta overflow");
          }
          
        } else if (score == -1) {
          if (m_start[k] > INT32_MIN) {
            m_start[k] -= 1;
          } else {
            raiseErr(__LINE__, "Delta overflow");
          }
          
        } else {
          raiseErr(__LINE__, NULL);
        }
        
      } else {
        /* X is greater than zero, so encode intersection record and add
         * it to intersection buffer */
        ir.tx = xi;
        ir.ty = k;
        ir.adj = score;
        
        appendInter(packIRec(&ir));
      }
    }
  }
}

/*
 * lilac_lock function.
 */
void lilac_lock(LILAC_LOCK *pLock) {
  /* Check state and parameters */
  if (m_state != STATE_TILE) {
    raiseErr(__LINE__, "Wrong state");
  }
  if (pLock == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Clear the return structure */
  memset(pLock, 0, sizeof(LILAC_LOCK));
  
  /* Fill in return structure */
  pLock->pData = m_tbuf;
  pLock->x     = m_tx;
  pLock->y     = m_ty;
  pLock->pitch = m_dim;
  pLock->w     = m_tw;
  pLock->h     = m_th;
  
  /* Update state */
  m_state = STATE_LOCK;
}

/*
 * lilac_unlock function.
 */
void lilac_unlock(void) {
  /* Check state */
  if (m_state != STATE_LOCK) {
    raiseErr(__LINE__, "Wrong state");
  }
  
  /* Update state */
  m_state = STATE_TILE;
}

/*
 * lilac_compile function.
 */
void lilac_compile(const char *pPath) {
  
  SPH_IMAGE_WRITER *pw = NULL;
  int ecode = 0;
  uint32_t *pScan = NULL;
  int32_t y = 0;
  
  /* Check state and parameters */
  if (m_state != STATE_INIT) {
    raiseErr(__LINE__, "Wrong state");
  }
  if (m_finished != m_tile_count) {
    raiseErr(__LINE__, "Tiles remain to render");
  }
  
  if (pPath == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Release memory buffers and update state */
  free(m_tbuf);
  m_tbuf = NULL;
  
  free(m_start);
  m_start = NULL;
  
  free(m_delta);
  m_delta = NULL;
  
  free(m_ibuf);
  m_ibuf = NULL;
  m_ibuf_cap = 0;
  m_ibuf_len = 0;
  
  m_state = STATE_CLOSED;
  
  /* Allocate image writer */
  pw = sph_image_writer_newFromPath(
          pPath, m_w, m_h, SPH_IMAGE_DOWN_NONE, 0, &ecode);
  if (pw == NULL) {
    raiseErr(__LINE__,
      "Failed to open PNG output (file extension must be .png)");
  }
  
  /* Get the scanline buffer */
  pScan = sph_image_writer_ptr(pw);
  
  /* Rewind to start of temporary file */
  if (fseek(m_fh, 0, SEEK_SET)) {
    raiseErr(__LINE__, "I/O error");
  }
  
  /* Write each of the rendered scanlines from the temporary file out to
   * the PNG file */
  for(y = 0; y < m_h; y++) {
    /* Read a scanline from the temporary file into the scanline
     * buffer */
    if (fread(pScan, sizeof(uint32_t), (size_t) m_w, m_fh) != m_w) {
      raiseErr(__LINE__, "I/O error");
    }
    
    /* Write the scanline buffer out to the PNG file */
    sph_image_writer_write(pw);
  }
  
  /* Release the image writer */
  sph_image_writer_close(pw);
  pw = NULL;
  
  /* Close and release the temporary file */
  if (fclose(m_fh)) {
    sayWarn(__LINE__, "Failed to close temporary file");
  }
  m_fh = NULL;
}
