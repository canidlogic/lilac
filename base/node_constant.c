/*
 * node_constant.c
 * ===============
 * 
 * Implementation of node_constant.h
 * 
 * See the header for further information.
 */

#include "node_constant.h"

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "lilac.h"
#include "node.h"
#include "vm.h"

/*
 * Type declarations
 * =================
 */

/*
 * Structure used for instance data for constant nodes.
 */
typedef struct {
  
  /*
   * The color that this node will always return.
   */
  uint32_t col;
  
} CONSTANT_NODE;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static void raiseErr(int sourceLine);
static uint32_t node_constant(void *pv);
static void op_constant(void);

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
 * Function that implements constant nodes.
 * 
 * Parameters:
 * 
 *   pv - pointer to a CONSTANT_NODE
 * 
 * Return:
 * 
 *   the color output of this node
 */
static uint32_t node_constant(void *pv) {
  /* For this node type, we always return the same color type; if you
   * want the coordinates we are being asked to render, consult with the
   * functions of the render module in Lilac Core */
  
  CONSTANT_NODE *pd = NULL;
  pd = (CONSTANT_NODE *) pv;
  return pd->col;
}

/*
 * Function that is invoked to implement the "constant" script
 * operation.
 * 
 * Takes a single input parameter that is the color that will be
 * returned for every pixel in the new node.  The result is a new node
 * that always returns this color.
 * 
 * Script syntax:
 * 
 *   [c:color] constant [r:node]
 */
static void op_constant(void) {
  
  uint32_t col = 0;
  CONSTANT_NODE *pd = NULL;
  NODE *pn = NULL;
  
  /* Get parameters */
  if (vm_type() != VM_TYPE_COLOR) {
    fprintf(stderr, "%s: constant op expects color argument!\n",
            getModule());
    reportLine();
    raiseErr(__LINE__);
  }
  
  col = vm_pop_c();
  
  /* Create a new instance data structure for the new node */
  pd = (CONSTANT_NODE *) calloc(1, sizeof(CONSTANT_NODE));
  if (pd == NULL) {
    fprintf(stderr, "%s: Out of memory!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Initialize the instance data */
  pd->col = col;
  
  /* Create the new node -- since this node does not reference any other
   * nodes, the depth is set to one; if we reference other nodes, we
   * should get the maximum depth value among all those nodes and then
   * pass one greater than that as the depth value */
  pn = node_define(pd, &node_constant, 1);
  
  /* Push the results */
  vm_push_n(pn);
}

/*
 * Public function implementations
 * ===============================
 */

void node_constant_init(void) {
  /* Register the "constant" operation */
  vm_register("constant", &op_constant);
}
