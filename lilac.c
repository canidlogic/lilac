/*
 * lilac.c
 * 
 * Main program module for lilac.
 * 
 * See readme file for more information about this program.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "sophistry.h"

/*
 * Error code definitions.
 * 
 * Remember to update lilac_errorString()!
 */

/* Error codes in this range are Sophistry error codes added to the
 * value ERROR_SPH_MIN */
#define ERROR_SPH_MIN (100)
#define ERROR_SPH_MAX (199)

/* Function prototypes */
static const char *lilac_errorString(int code);

static int lilac(
    const char * pOutPath,
    const char * pMaskPath,
    const char * pPencilPath,
    const char * pShadingPath,
           int * pError);

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
    pResult = sph_image_errorString(code);
  }
  
  return pResult;
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
 * If the function succeeds, a non-zero value is returned and the error
 * code integer (if provided) is set to zero.
 * 
 * If the function fails, a zero value is returned and the error code
 * integer (if provided) is set to an error code.  This error code can
 * be converted into an error message using lilac_errorString().
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
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int lilac(
    const char * pOutPath,
    const char * pMaskPath,
    const char * pPencilPath,
    const char * pShadingPath,
           int * pError) {
  /* @@TODO: */
  printf("lilac %s %s %s %s\n",
          pOutPath, pMaskPath, pPencilPath, pShadingPath);
  return 1;
}

/*
 * Program entrypoint.
 */
int main(int argc, char *argv[]) {
  
  char *pModule = NULL;
  int status = 1;
  int i = 0;
  int errcode = 0;
  
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
  
  /* Starting at parameter index six and proceeding through the
   * remaining parameters, pass each path to the texture module to load
   * the texture */
  if (status) {
    for(i = 6; i < argc; i++) {
      /* @@TODO: pass path to texture module */
      printf("texture %s\n", argv[i]);
    }
  }
  
  /* Use parameter index five to initialize the shading table */
  if (status) {
    /* @@TODO: pass path to table module */
    printf("table %s\n", argv[5]);
  }
  
  /* Begin the core program function */
  if (status) {
    if (!lilac(argv[1], argv[2], argv[3], argv[4], &errcode)) {
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
