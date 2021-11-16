/*
 * pshade.c
 * 
 * Implementation of pshade.h
 * 
 * See the header for further information.
 */
#include "pshade.h"

/* @@TODO: */
#include <stdio.h>

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * pshade_errorString function.
 */
const char *pshade_errorString(int code) {
  
  const char *pResult = NULL;
  
  switch (code) {
    case PSHADE_ERR_NONE:
      pResult = "No error";
      break;
    
    default:
      pResult = "Unknown error";
  }
  
  return pResult;
}

/*
 * pshade_load function.
 */
int pshade_load(const char *pScriptPath, int *perr) {
  /* @@TODO: */
  fprintf(stderr, "pshade_load %s\n", pScriptPath);
  return 1;
}

/*
 * pshade_pixel function.
 */
uint32_t pshade_pixel(
    const char *pShader,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    int *perr) {
  /* @@TODO: */
  *perr = 1;
  return 0;
}
