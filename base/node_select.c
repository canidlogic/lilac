/*
 * node_select.c
 * =============
 * 
 * Implementation of node_select.h
 * 
 * See the header for further information.
 */

#include "node_select.h"

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lilac.h"
#include "node.h"
#include "render.h"
#include "vm.h"

/*
 * Constants
 * =========
 */

/*
 * The initial capacity of records in a mapping table.
 * 
 * The mapping table then grows by doubling up to MAX_RECORDS.
 */
#define INIT_RECORDS (16)

/*
 * The maximum number of mapping records in a select node.
 */
#define MAX_RECORDS (16384)

/*
 * Type declarations
 * =================
 */

/*
 * Record format used for mapping records.
 */
typedef struct {
  
  /*
   * The ARGB value that is used as a key to select this record.
   */
  uint32_t key;
  
  /*
   * The node that this record maps to.
   */
  NODE *val;
  
} PAL_REC;

/*
 * Palette mapping structure.
 */
typedef struct {
  
  /*
   * Pointer to the dynamically allocated array of records.
   */
  PAL_REC *pData;
  
  /*
   * The total capacity of the array indicated by pData.
   */
  int32_t cap;
  
  /*
   * The actual number of records currently in use in the array
   * indicated by pData.
   */
  int32_t count;
  
} PAL;

/*
 * Structure used for instance data for select nodes.
 */
typedef struct {
  
  /*
   * The index node.
   */
  NODE *nIndex;
  
  /*
   * The default node, used when none of the defined mappings apply.
   */
  NODE *nDefault;
  
  /*
   * The palette mapping specific ARGB colors to nodes.
   * 
   * When this structure is in the accumulator, the palette is not
   * sorted and new map entries may be added to it.
   * 
   * When this structure is out of the accumulator, it must be sorted
   * and fixed in definition.
   */
  PAL pal;
  
} SELECT_NODE;

/*
 * Local data
 * ==========
 */

/*
 * The accumulator register.
 * 
 * NULL if empty, else it stores a node that is being built.
 */
static SELECT_NODE *m_ax = NULL;

/*
 * Local functions
 * ===============
 */

/* Function prototypes */
static void raiseErr(int sourceLine);
static int cmp_pal_rec(const void *pA, const void *pB);
static int cmp_pal_key(const void *pK, const void *pR);

static void pal_init(PAL *pp);
static void pal_add(PAL *pp, uint32_t key, NODE *val);
static void pal_sort(PAL *pp);
static NODE *pal_query(PAL *pp, uint32_t key);
static int32_t pal_depth(PAL *pp);

static uint32_t node_select(void *pv);
static void op_select_new(void);
static void op_select_map(void);
static void op_select_finish(void);

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
 * Comparison function for sorting a palette array according to color
 * key.
 * 
 * Parameters:
 * 
 *   pA - pointer to first PAL_REC structure
 * 
 *   pB - pointer to second PAL_REC structure
 * 
 * Return:
 * 
 *   greater than, equal to, or less than zero as A is greater than,
 *   equal to, or less than B
 */
static int cmp_pal_rec(const void *pA, const void *pB) {
  
  int result = 0;
  const PAL_REC *par = NULL;
  const PAL_REC *pbr = NULL;
  
  /* Check parameters */
  if ((pA == NULL) || (pB == NULL)) {
    raiseErr(__LINE__);
  }
  
  /* Cast parameters */
  par = (const PAL_REC *) pA;
  pbr = (const PAL_REC *) pB;
  
  /* Compare by color key */
  if (par->key > pbr->key) {
    result = 1;
    
  } else if (par->key < pbr->key) {
    result = -1;
    
  } else if (par->key == pbr->key) {
    result = 0;
    
  } else {
    raiseErr(__LINE__);
  }
  
  /* Return result */
  return result;
}

/*
 * Comparison function for searching a sorted palette for a specific
 * key.
 * 
 * The key is a uint32_t that holds the color key.  The record type is
 * PAL_REC.
 * 
 * Parameters:
 * 
 *   pK - the key to compare
 * 
 *   pR - the record to compare
 * 
 * Return:
 * 
 *   greater than, equal to, or less than zero as key is greater than,
 *   equal to, or less than record's key
 */
static int cmp_pal_key(const void *pK, const void *pR) {
  
  int result = 0;
  const uint32_t * pKey = NULL;
  const PAL_REC  * pRec = NULL;
  
  /* Check parameters */
  if ((pK == NULL) || (pR == NULL)) {
    raiseErr(__LINE__);
  }
  
  /* Cast parameters */
  pKey = (const uint32_t *) pK;
  pRec = (const PAL_REC  *) pR;
  
  /* Compare by color key */
  if (*pKey > pRec->key) {
    result = 1;
    
  } else if (*pKey < pRec->key) {
    result = -1;
    
  } else if (*pKey == pRec->key) {
    result = 0;
    
  } else {
    raiseErr(__LINE__);
  }
  
  /* Return result */
  return result;
}

/*
 * Initialize a palette structure.
 * 
 * The palette starts out empty.
 * 
 * Parameters:
 * 
 *   pp - the uninitialized palette to initialize
 */
static void pal_init(PAL *pp) {
  
  /* Check parameter */
  if (pp == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Initialize memory */
  memset(pp, 0, sizeof(PAL));
  
  /* Start with no elements and initial capacity */
  pp->cap = INIT_RECORDS;
  pp->count = 0;
  
  /* Allocate array of records */
  pp->pData = (PAL_REC *) calloc((size_t) pp->cap, sizeof(PAL_REC));
  if (pp->pData == NULL) {
    fprintf(stderr, "%s: Out of memory!\n", getModule());
    raiseErr(__LINE__);
  }
}

/*
 * Add a mapping to a palette structure.
 * 
 * Parameters:
 * 
 *   pp - the initialized palette to modify
 * 
 *   key - the ARGB color to use as a key
 * 
 *   val - the node to map this color to
 */
static void pal_add(PAL *pp, uint32_t key, NODE *val) {
  
  int32_t nc = 0;
  
  /* Check parameters */
  if ((pp == NULL) || (val == NULL)) {
    raiseErr(__LINE__);
  }
  
  /* Expand capacity if necessary */
  if (pp->count >= pp->cap) {
    /* Check we can expand capacity */
    if (pp->cap >= MAX_RECORDS) {
      fprintf(stderr,
        "%s: Too many mappings in select node! Max: %ld\n",
        getModule(), (long) MAX_RECORDS);
      reportLine();
      raiseErr(__LINE__);
    }
    
    /* New capacity is minimum of double current capacity and maximum
     * capacity */
    nc = pp->cap * 2;
    if (nc > MAX_RECORDS) {
      nc = MAX_RECORDS;
    }
    
    /* Expand dynamic array to new capacity */
    pp->pData = (PAL_REC *) realloc(pp->pData,
                  ((size_t) nc) * sizeof(PAL_REC));
    if (pp->pData == NULL) {
      fprintf(stderr, "%s: Out of memory!\n", getModule());
      raiseErr(__LINE__);
    }
    
    /* Clear new memory added to array */
    memset(&((pp->pData)[pp->cap]), 0,
            ((size_t) (nc - pp->cap)) * sizeof(PAL_REC));
    
    /* Update capacity */
    pp->cap = nc;
  }

  /* We have capacity in the array, so just append the new record */
  ((pp->pData)[pp->count]).key = key;
  ((pp->pData)[pp->count]).val = val;
  (pp->count)++;
}

/*
 * Compact and sort a palette structure.
 * 
 * All records are put in ascending order of numeric ARGB key value.
 * 
 * The size of the dynamic array will be reduced to exactly fit the
 * number of mapped entries actually declared.
 * 
 * This will also detect cases where a single key value was declared
 * more than once and cause an error in those cases.
 * 
 * Parameters:
 * 
 *   pp - the initialized palette to compact and sort
 */
static void pal_sort(PAL *pp) {
  
  int32_t i = 0;
  
  /* Check parameter */
  if (pp == NULL) {
    raiseErr(__LINE__);
  }
  
  /* In the special case of an empty mapping array, reduce capacity to
   * one and just return after that */
  if (pp->count < 1) {
    /* Reduce capacity if it is greater than one */
    if (pp->cap > 1) {
      pp->pData = realloc(pp->pData, sizeof(PAL_REC));
      if (pp->pData == NULL) {
        fprintf(stderr, "%s: Out of memory!\n");
        raiseErr(__LINE__);
      }
      pp->cap = 1;
    }
    
    /* Return */
    return;
  }
  
  /* If we got here, there is at least one element; if capacity is
   * greater than count, reduce capacity to exactly equal count */
  if (pp->cap > pp->count) {
    pp->pData = realloc(pp->pData,
                  ((size_t) pp->count) * sizeof(PAL_REC));
    if (pp->pData == NULL) {
      fprintf(stderr, "%s: Out of memory!\n");
      raiseErr(__LINE__);
    }
    pp->cap = pp->count;
  }
  
  /* If more than one element, sort the array and check for duplicate
   * keys */
  if (pp->count > 1) {
    qsort(pp->pData, (size_t) pp->count, sizeof(PAL_REC), &cmp_pal_rec);
    for(i = 1; i < pp->count; i++) {
      if (((pp->pData)[i]).key == ((pp->pData)[i - 1]).key) {
        fprintf(stderr, "%s: Duplicate ARGB key in mapping: %08x!\n",
                getModule(), ((pp->pData)[i]).key);
        reportLine();
        raiseErr(__LINE__);
      }
    }
  }
}

/*
 * Query a palette structure.
 * 
 * The palette must have been sorted with pal_sort() and nothing further
 * must have been added to it since then, or undefined behavior occurs.
 * 
 * Parameters:
 * 
 *   pp - the sorted palette
 * 
 *   key - the ARGB key to query
 * 
 * Return:
 * 
 *   the node mapped to this key, or NULL if no node is mapped to that
 *   key
 */
static NODE *pal_query(PAL *pp, uint32_t key) {
  
  PAL_REC *pr = NULL;
  NODE *pResult = NULL;
  
  /* Check parameters */
  if (pp == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Binary search for the key */
  pr = (PAL_REC *) bsearch(&key,
        pp->pData, (size_t) pp->count, sizeof(PAL_REC), &cmp_pal_key);
  
  /* Determine result */
  if (pr != NULL) {
    pResult = pr->val;
  } else {
    pResult = NULL;
  }
  
  /* Return result */
  return pResult;
}

/*
 * Determine the maximum depth among all nodes within a palette.
 * 
 * This function must iterate through all nodes, so use it sparingly.
 * 
 * Parameters:
 * 
 *   pp - the sorted palette
 * 
 * Return:
 * 
 *   the maximum depth of any node within the palette, or zero if the
 *   palette is empty
 */
static int32_t pal_depth(PAL *pp) {
  
  int32_t result = 0;
  int32_t d = 0;
  int32_t i = 0;
  
  /* Check parameters */
  if (pp == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Result starts at zero */
  result = 0;
  
  /* Iterate through all nodes to find the maximum */
  for(i = 0; i < pp->count; i++) {
    /* Get depth of current node */
    d = node_depth(((pp->pData)[i]).val);
    
    /* Update statistics */
    if (d > result) {
      result = d;
    }
  }
  
  /* Return result */
  return result;
}

/*
 * Function that implements select nodes.
 * 
 * Parameters:
 * 
 *   pv - pointer to a SELECT_NODE
 * 
 * Return:
 * 
 *   the color output of this node
 */
static uint32_t node_select(void *pv) {
  
  SELECT_NODE *pd = NULL;
  uint32_t col = 0;
  NODE *pn = NULL;
  
  /* Check parameter */
  if (pv == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Cast pointer */
  pd = (SELECT_NODE *) pv;
  
  /* Get the current color from the index node */
  col = node_invoke(pd->nIndex);
  
  /* Attempt to map the color to a palette node */
  pn = pal_query(&(pd->pal), col);
  
  /* If color was not found in palette, use the default node */
  if (pn == NULL) {
    pn = pd->nDefault;
  }
  
  /* Call through to the selected node */
  return node_invoke(pn);
}

/*
 * Function that is invoked to implement the "select_new" script
 * operation.
 * 
 * Script syntax:
 * 
 *   [index:node] [default:node] select_new -
 */
static void op_select_new(void) {
  
  NODE *nIndex = NULL;
  NODE *nDefault = NULL;
  
  /* Get parameters */
  if (vm_type() != VM_TYPE_NODE) {
    fprintf(stderr, "%s: select_new op expects two node arguments!\n",
            getModule());
    reportLine();
    raiseErr(__LINE__);
  }
  
  nDefault = vm_pop_n();
  
  if (vm_type() != VM_TYPE_NODE) {
    fprintf(stderr, "%s: select_new op expects two node arguments!\n",
            getModule());
    reportLine();
    raiseErr(__LINE__);
  }
  
  nIndex = vm_pop_n();
  
  /* Check that accumulator is empty */
  if (m_ax != NULL) {
    fprintf(stderr,
      "%s: select_new must be used when accumulator empty!\n",
      getModule());
    reportLine();
    raiseErr(__LINE__);
  }
  
  /* Create a new instance data structure in the accumulator */
  m_ax = (SELECT_NODE *) calloc(1, sizeof(SELECT_NODE));
  if (m_ax == NULL) {
    fprintf(stderr, "%s: Out of memory!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Initialize the accumulator data structure with an empty mapping */
  m_ax->nIndex   = nIndex;
  m_ax->nDefault = nDefault;
  pal_init(&(m_ax->pal));
}

/*
 * Function that is invoked to implement the "select_map" script
 * operation.
 * 
 * Script syntax:
 * 
 *   [key:color] [value:node] select_map -
 */
static void op_select_map(void) {
  
  uint32_t key = 0;
  NODE *nValue = NULL;
  
  /* Get parameters */
  if (vm_type() != VM_TYPE_NODE) {
    fprintf(stderr, "%s: select_map op expects color and node!\n",
            getModule());
    reportLine();
    raiseErr(__LINE__);
  }
  
  nValue = vm_pop_n();
  
  if (vm_type() != VM_TYPE_COLOR) {
    fprintf(stderr, "%s: select_map op expects color and node!\n",
            getModule());
    reportLine();
    raiseErr(__LINE__);
  }
  
  key = vm_pop_c();
  
  /* Check that accumulator is not empty */
  if (m_ax == NULL) {
    fprintf(stderr,
      "%s: select_map must be used when accumulator not empty!\n",
      getModule());
    reportLine();
    raiseErr(__LINE__);
  }
  
  /* Add the new mapping */
  pal_add(&(m_ax->pal), key, nValue);
}

/*
 * Function that is invoked to implement the "select_finish" script
 * operation.
 * 
 * Script syntax:
 * 
 *   - select_finish [result:node]
 */
static void op_select_finish(void) {
  
  NODE *pn = NULL;
  int32_t d = 0;
  int32_t depth = 0;
  
  /* Check that accumulator is not empty */
  if (m_ax == NULL) {
    fprintf(stderr,
      "%s: select_map must be used when accumulator not empty!\n",
      getModule());
    reportLine();
    raiseErr(__LINE__);
  }
  
  /* Sort the palette array */
  pal_sort(&(m_ax->pal));
  
  /* Compute the depth, which is one greater than the maximum depth of
   * the index node, default node, and any node referenced from the
   * palette */
  depth = 0;
  
  d = node_depth(m_ax->nIndex);
  if (d > depth) {
    depth = d;
  }
  
  d = node_depth(m_ax->nDefault);
  if (d > depth) {
    depth = d;
  }
  
  d = pal_depth(&(m_ax->pal));
  if (d > depth) {
    depth = d;
  }
  
  depth++;

  /* Create the new node */
  pn = node_define(m_ax, &node_select, depth);
  
  /* Clear the accumulator */
  m_ax = NULL;
  
  /* Push the results */
  vm_push_n(pn);
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

void node_select_init(void) {
  /* Register "select_new" "select_map" "select_finish" operations */
  vm_register("select_new", &op_select_new);
  vm_register("select_map", &op_select_map);
  vm_register("select_finish", &op_select_finish);
}
