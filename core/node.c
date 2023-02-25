/*
 * node.c
 * ======
 * 
 * Implementation of node.h
 * 
 * See the header for further information.
 */

#include "node.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lilac.h"
#include "render.h"

/*
 * Type declarations
 * -----------------
 */

/*
 * Implementation of NODE structure that was prototyped in the header.
 */
struct NODE_TAG {
  
  /*
   * The instance data for this node.
   */
  void *pInstance;
  
  /*
   * Function pointer to the pixel generation function.
   */
  fp_node fp;
  
  /*
   * The graph depth of this node.
   */
  int32_t depth;
};

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
 * node_define function.
 */
NODE *node_define(void *pInstance, fp_node fp, int32_t depth) {
  
  NODE *pNode = NULL;
  
  /* Check parameters */
  if (fp == NULL) {
    raiseErr(__LINE__);
  }
  if (depth < 1) {
    raiseErr(__LINE__);
  }
  
  /* Check that graph depth limit is not exceeded */
  if (depth > getConfigInt(CFG_GRAPH_DEPTH)) {
    fprintf(stderr, "%s: Graph depth exceeded!  Maximum depth: %ld\n",
            getModule(), (long) getConfigInt(CFG_GRAPH_DEPTH));
    reportLine();
    raiseErr(__LINE__);
  }
  
  /* Allocate node and copy information into it */
  pNode = (NODE *) calloc(1, sizeof(NODE));
  if (pNode == NULL) {
    fprintf(stderr, "%s: Out of memory!\n", getModule());
    raiseErr(__LINE__);
  }
  
  pNode->pInstance = pInstance;
  pNode->fp        = fp;
  pNode->depth     = depth;
  
  /* Return the new node */
  return pNode;
}

/*
 * node_invoke function.
 */
uint32_t node_invoke(NODE *pNode) {
  
  /* Check state */
  if (!render_mode()) {
    raiseErr(__LINE__);
  }
  
  /* Check parameters */
  if (pNode == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Call through */
  return (pNode->fp)(pNode->pInstance);
}

/*
 * node_depth function.
 */
int32_t node_depth(NODE *pNode) {
  /* Check parameter */
  if (pNode == NULL) {
    raiseErr(__LINE__);
  }
}
