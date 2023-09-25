/*
 * gamma.c
 * 
 * Implementation of gamma.h
 * 
 * See the header for further information.
 */
#include "gamma.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * Gamma table
 * ===========
 */

static int m_gamma_init = 0;
static float m_gamma[256];

/*
 * Local functions
 * ===============
 */

/* Function prototypes */
static void verify(void);

/*
 * Verify that the gamma table is initialized to proper values.
 * 
 * A fault occurs if the gamma table is not initialized or it contains
 * improper values.
 * 
 * To contain proper values, the first record must be 0.0f and the last
 * record must be 1.0f.  Furthermore, all records must be in strictly
 * ascending order without any duplicates.  All records must be finite.
 */
static void verify(void) {
  
  int i = 0;
  float v = 0.0f;
  
  /* Make sure initialized */
  if (!m_gamma_init) {
    abort();
  }
  
  /* Check all values finite */
  for(i = 0; i < 256; i++) {
    if (!isfinite(m_gamma[i])) {
      abort();
    }
  }
  
  /* Check boundary values */
  if ((m_gamma[0] != 0.0f) || (m_gamma[255] != 1.0f)) {
    abort();
  }
  
  /* Check strict ascending order */
  v = 0.0f;
  for(i = 1; i < 256; i++) {
    if (!(m_gamma[i] > v)) {
      abort();
    }
    v = m_gamma[i];
  }
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * gamma_sRGB function.
 */
void gamma_sRGB(void) {
  
  int x = 0;
  double u = 0.0;
  
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
  verify();
}

/*
 * gamma_undo function.
 */
float gamma_undo(int c) {
  
  /* Make sure gamma table initialized */
  if (!m_gamma_init) {
    abort();
  }
  
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
 * gamma_correct function.
 */
int gamma_correct(float v) {
  
  int result = 0;
  int lbound = 0;
  int hbound = 0;
  int mid = 0;
  float dl = 0.0f;
  float dh = 0.0f;
  
  /* Make sure gamma table initialized */
  if (!m_gamma_init) {
    abort();
  }
  
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
        abort();
      }
    }
    
    /* lbound is now greatest value in gamma table that is less than or
     * equal to v -- this shouldn't be the last entry */
    assert(lbound < 255);
    
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
