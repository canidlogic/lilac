/*
 * vm.c
 * ====
 * 
 * Implementation of vm.h
 * 
 * See the header for further information.
 */

#include "vm.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lilac.h"
#include "rfdict.h"

/*
 * Constants
 * ---------
 */

/*
 * The maximum number of operations that may be registered.
 */
#define MAX_OPERATION_COUNT (16384)

/*
 * The maximum amount of Shastina grouping.
 */
#define MAX_GROUP_DEPTH (16384)

/*
 * The initial capacity in variants for VBLOCK structures.
 * 
 * The actual initial capacity is the minimum of this constant and the
 * maximum capacity used to initialize the structure.
 */
#define VBLOCK_INITIAL (16)

/*
 * The initial capacity in integers for IBLOCK structures.
 * 
 * The actual initial capacity is the minimum of this constant and the
 * maximum capacity used to initialize the structure.
 */
#define IBLOCK_INITIAL (16)

/*
 * The initial capacity in pointers for OBLOCK structures.
 * 
 * The actual initial capacity is the minimum of this constant the
 * maximum capacity used to initialize the structure.
 */
#define OBLOCK_INITIAL (16)

/*
 * Type declarations
 * -----------------
 */

/*
 * Data structure that can hold any Lilac interpreter data type.
 * 
 * Use the variant functions to manipulate.
 */
typedef struct {
  
  /*
   * One of the VM_TYPE constants indicating what type of data is stored
   * in this variant.
   * 
   * VM_TYPE_UNDEF is used for variant structures that are not storing
   * any data.
   */
  int vtype;
  
  /*
   * The value is in a union val that is specific to vtype.
   */
  union {
    
    /*
     * An ignored dummy value used for VM_TYPE_UNDEF.
     */
    int dummy;
    
    /*
     * Finite, floating-point value for VM_TYPE_FLOAT.
     */
    double f;
    
    /*
     * Packed ARGB color in Sophistry format for VM_TYPE_COLOR.
     */
    uint32_t col;
    
    /*
     * String object reference for VM_TYPE_STRING.
     */
    ISTR str;
    
    /*
     * Node object for VM_TYPE_NODE.
     */
    NODE *pn;
    
  } val;
  
} VARIANT;

/*
 * A growable array of variant structures.
 * 
 * Use the vblock functions to manipulate.
 */
typedef struct {
  
  /*
   * Pointer to the dynamically allocated array of variants.
   */
  VARIANT *pBank;
  
  /*
   * The current capacity of the array pointed to by pBank.
   * 
   * This must be greater than zero and less than or equal to vmax.
   */
  int32_t vcap;
  
  /*
   * The maximum capacity that the array may grow to.
   * 
   * This must be greater than zero.
   */
  int32_t vmax;
  
  /*
   * The current size of the variant array.
   * 
   * This must be greater than or equal to zero, and less than or equal
   * to vcap.
   */
  int32_t vcount;
  
} VBLOCK;

/*
 * A growable array of integers.
 * 
 * Use the iblock functions to manipulate.
 */
typedef struct {
  
  /*
   * Pointer to the dynamically allocated array of integers.
   */
  uint32_t *pBank;
  
  /*
   * The current capacity of the array pointed to by pBank.
   * 
   * This must be greater than zero and less than or equal to imax.
   */
  int32_t icap;
  
  /*
   * The maximum capacity that the array may grow to.
   * 
   * This must be greater than zero.
   */
  int32_t imax;
  
  /*
   * The current size of the integer array.
   * 
   * This must be greater than or equal to zero, and less than or equal
   * to icap.
   */
  int32_t icount;
  
} IBLOCK;

/*
 * A growable array of operation pointers.
 * 
 * Use the oblock functions to manipulate.
 */
typedef struct {
  
  /*
   * Pointer to the dynamically allocated array of operation pointers.
   */
  fp_vm_op *pBank;
  
  /*
   * The current capacity of the array pointed to by pBank.
   * 
   * This must be greater than zero and less than or equal to omax.
   */
  int32_t ocap;
  
  /*
   * The maximum capacity that the array may grow to.
   * 
   * This must be greater than zero.
   */
  int32_t omax;
  
  /*
   * The current size of the pointer array.
   * 
   * This must be greater than or equal to zero, and less than or equal
   * to ocap.
   */
  int32_t ocount;
  
} OBLOCK;

/*
 * Local data
 * ----------
 */

/*
 * Flag indicating whether vm_run has been called yet.
 */
static int m_called = 0;

/*
 * Flag indicating whether this module is in running mode.
 */
static int m_run = 0;

/*
 * The table of operations.
 * 
 * Pointers are NULL until initialized.
 * 
 * When initialized, m_ops_dict maps operation names to indices in m_ops
 * and m_ops stores pointers to the operation functions.
 */
static RFDICT *m_ops_dict = NULL;
static OBLOCK *m_ops      = NULL;

/*
 * The grouping stack.
 * 
 * NULL until initialized.
 * 
 * When the grouping stack is empty, it means no Shastina group is
 * active.  Each time a Shastina group starts, the current interpreter
 * stack height is pushed on top of the grouping stack.  Each time a
 * Shastina group ends, a check is made that the current interpreter
 * stack height is exactly one greater than the value on top of the
 * grouping stack and then the grouping stack is popped.
 * 
 * If this stack is not empty, then the integer on top of the stack
 * indicates how many interpreter stack entries should be hidden by
 * grouping.
 */
static IBLOCK *m_gs = NULL;

/*
 * The interpreter stack.
 * 
 * NULL until initialized.
 * 
 * m_gs determines how many elements are currently hidden due to
 * grouping.
 */
static VBLOCK *m_st = NULL;

/*
 * The interpreter namespace.
 * 
 * Pointers are NULL until initialized.
 * 
 * m_ns_dict maps variable and constant names to indices in m_ns.
 * 
 * m_ns_flag is a bitmask where there is one bit for each element in the
 * m_ns array.  Given an element i in m_ns, the corresponding bit in
 * m_ns_flag is determined by the following:
 * 
 *   offset = i / 32
 *   bit    = 1 << (i % 32)
 * 
 * Bits that are set indicate that the corresponding element in m_ns is
 * a constant, while bits that are clear indicate that the corresponding
 * element in m_ns is a variable.
 * 
 * m_ns stores that variant values of each variable and constant.
 */
static RFDICT *m_ns_dict = NULL;
static IBLOCK *m_ns_flag = NULL;
static VBLOCK *m_ns      = NULL;

/*
 * Local functions
 * ---------------
 */

/* Prototypes */
static void raiseErr(int sourceLine);
static int validName(const char *pstr);

static void variant_init(VARIANT *pv);
static void variant_reset(VARIANT *pv);
static void variant_copy(VARIANT *pDest, VARIANT *pSrc);
static int variant_type(VARIANT *pv);

static double variant_getFloat(VARIANT *pv);
static uint32_t variant_getColor(VARIANT *pv);
static void variant_getString(VARIANT *pv, ISTR *ps);
static NODE *variant_getNode(VARIANT *pv);

static void variant_setFloat(VARIANT *pv, double f);
static void variant_setColor(VARIANT *pv, uint32_t col);
static void variant_setString(VARIANT *pv, ISTR *ps);
static void variant_setNode(VARIANT *pv, NODE *pn);

static VBLOCK *vblock_alloc(int32_t max_cap);
static int32_t vblock_count(VBLOCK *pb);
static int32_t vblock_append(VBLOCK *pb);
static void vblock_reduce(VBLOCK *pb);
static void vblock_set(VBLOCK *pb, int32_t i, VARIANT *pv);
static void vblock_get(VARIANT *pv, VBLOCK *pb, int32_t i);

static IBLOCK *iblock_alloc(int32_t max_cap);
static int32_t iblock_count(IBLOCK *pb);
static int32_t iblock_append(IBLOCK *pb);
static void iblock_reduce(IBLOCK *pb);
static void iblock_set(IBLOCK *pb, int32_t i, uint32_t val);
static uint32_t iblock_get(IBLOCK *pb, int32_t i);

static OBLOCK *oblock_alloc(int32_t max_cap);
static int32_t oblock_append(OBLOCK *pb, fp_vm_op fp);
static void oblock_invoke(OBLOCK *pb, int32_t i);

static void handleString(SNENTITY *pEnt);
static void handleNumeric(SNENTITY *pEnt);
static void handleDeclare(SNENTITY *pEnt);
static void handleAssign(SNENTITY *pEnt);
static void handleGet(SNENTITY *pEnt);
static void handlBeginGroup(void);
static void handleEndGroup(void);
static void handleArray(SNENTITY *pEnt);
static void handleOperation(SNENTITY *pEnt);

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
 * Check whether the given string contains a valid Lilac script
 * operation, variable, or constant name.
 * 
 * Valid names have a length in range [1, 31], the first character is a
 * US-ASCII alphabetic letter, and all other characters are US-ASCII
 * alphanumerics or underscores.
 * 
 * Parameters:
 * 
 *   pstr - the string to check
 * 
 * Return:
 * 
 *   non-zero if valid name, zero if not
 */
static int validName(const char *pstr) {
  
  size_t slen = 0;
  
  /* Check parameter */
  if (pstr == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Check length */
  slen = strlen(pstr);
  if ((slen < 1) || (slen > 31)) {
    return 0;
  }
  
  /* Check first character */
  if (!(
        ((*pstr >= 'A') && (*pstr <= 'Z')) ||
        ((*pstr >= 'a') && (*pstr <= 'z'))
      )) {
    return 0;
  }
  
  /* Check remaining characters */
  for(pstr++; *pstr != 0; pstr++) {
    if (!(
          ((*pstr >= 'A') && (*pstr <= 'Z')) ||
          ((*pstr >= 'a') && (*pstr <= 'z')) ||
          ((*pstr >= '0') && (*pstr <= '9')) ||
          (*pstr == '_')
        )) {
      return 0;
    }
  }
  
  /* If we got here, name is OK */
  return 1;
}

/*
 * Initialize a variant structure to UNDEF.
 * 
 * Variants must be initialized before any of the other functions can be
 * used.
 * 
 * Do not use on a variant that is already initialized or a memory leak
 * may occur.
 * 
 * To avoid memory leaks, variants should be reset with variant_reset()
 * before they go out of scope.
 * 
 * Parameters:
 * 
 *   pv - the variant to initialize
 */
static void variant_init(VARIANT *pv) {
  if (pv == NULL) {
    raiseErr(__LINE__);
  }
  memset(pv, 0, sizeof(VARIANT));
  pv->vtype = VM_TYPE_UNDEF;
  (pv->val).dummy = 0;
}

/*
 * Reset a variant structure back to UNDEF.
 * 
 * The given variant must be initialized.  You must use this function on
 * variants before they go out of scope to avoid memory leaks.
 * 
 * Parameters:
 * 
 *   pv - the variant to reset
 */
static void variant_reset(VARIANT *pv) {
  if (pv == NULL) {
    raiseErr(__LINE__);
  }
  if (pv->vtype == VM_TYPE_STRING) {
    istr_reset(&((pv->val).str));
  }
  pv->vtype = VM_TYPE_UNDEF;
  (pv->val).dummy = 0;
}

/*
 * Copy the contents of one variant to another variant.
 * 
 * If both pointers point to the same variant structure, this function
 * does nothing.
 * 
 * Both variant structures must be initialized.
 * 
 * Parameters:
 * 
 *   pDest - the variant to copy into
 * 
 *   pSrc - the variant to copy from
 */
static void variant_copy(VARIANT *pDest, VARIANT *pSrc) {
  /* Check parameters */
  if ((pDest == NULL) || (pSrc == NULL)) {
    raiseErr(__LINE__);
  }
  
  /* Only proceed if pointers are to different structures */
  if (pDest != pSrc) {
    /* Reset the destination */
    variant_reset(pDest);
    
    /* Handle the different source types */
    if (pSrc->vtype == VM_TYPE_STRING) {
      /* For a string, use the istr copy to make sure reference counts
       * are appropriately updated */
      pDest->vtype = VM_TYPE_STRING;
      istr_copy(&((pDest->val).str), &((pSrc->val).str));
    
    } else {
      /* For all non-string types, we can just do a raw memory copy */
      memcpy(pDest, pSrc, sizeof(VARIANT));
    }
  }
}

/*
 * Return the type stored in a variant.
 * 
 * The given variant must be initialized.
 * 
 * Parameters:
 * 
 *   pv - the variant to query
 * 
 * Return:
 * 
 *   a VM_TYPE constant
 */
static int variant_type(VARIANT *pv) {
  if (pv == NULL) {
    raiseErr(__LINE__);
  }
  return pv->vtype;
}

/*
 * Retrieve a floating-point value from a variant.
 * 
 * The given variant must be initialized and it must be storing a value
 * of that type.
 * 
 * Parameters:
 * 
 *   pv - the variant to query
 * 
 * Return:
 * 
 *   the floating-point value
 */
static double variant_getFloat(VARIANT *pv) {
  if (pv == NULL) {
    raiseErr(__LINE__);
  }
  if (pv->vtype != VM_TYPE_FLOAT) {
    raiseErr(__LINE__);
  }
  return (pv->val).f;
}

/*
 * Retrieve a color value from a variant.
 * 
 * The given variant must be initialized and it must be storing a value
 * of that type.
 * 
 * Parameters:
 * 
 *   pv - the variant to query
 * 
 * Return:
 * 
 *   the color value
 */
static uint32_t variant_getColor(VARIANT *pv) {
  if (pv == NULL) {
    raiseErr(__LINE__);
  }
  if (pv->vtype != VM_TYPE_COLOR) {
    raiseErr(__LINE__);
  }
  return (pv->val).col;
}

/*
 * Retrieve a string value from a variant.
 * 
 * The given variant must be initialized and it must be storing a value
 * of that type.
 * 
 * The indicated ps structure must be initialized.  A reference to the
 * string value will be copied into it.
 * 
 * Parameters:
 * 
 *   pv - the variant to query
 * 
 *   ps - the ISTR structure to copy the string into
 */
static void variant_getString(VARIANT *pv, ISTR *ps) {
  if ((pv == NULL) || (ps == NULL)) {
    raiseErr(__LINE__);
  }
  if (pv->vtype != VM_TYPE_STRING) {
    raiseErr(__LINE__);
  }
  istr_copy(ps, &((pv->val).str));
}

/*
 * Retrieve a node value from a variant.
 * 
 * The given variant must be initialized and it must be storing a value
 * of that type.
 * 
 * Parameters:
 * 
 *   pv - the variant to query
 * 
 * Return:
 * 
 *   the node value
 */
static NODE *variant_getNode(VARIANT *pv) {
  if (pv == NULL) {
    raiseErr(__LINE__);
  }
  if (pv->vtype != VM_TYPE_NODE) {
    raiseErr(__LINE__);
  }
  return (pv->val).pn;
}

/*
 * Put a floating-point value into a variant.
 * 
 * The given variant must be initialized.  This function will call
 * variant_reset() to clear the variant and then store the new value.
 * 
 * The given value must be finite.
 * 
 * Parameters:
 * 
 *   pv - the variant to set
 * 
 *   f - the floating-point value
 */
static void variant_setFloat(VARIANT *pv, double f) {
  if (pv == NULL) {
    raiseErr(__LINE__);
  }
  if (!isfinite(f)) {
    raiseErr(__LINE__);
  }
  variant_reset(pv);
  pv->vtype = VM_TYPE_FLOAT;
  (pv->val).f = f;
}

/*
 * Put a color value into a variant.
 * 
 * The given variant must be initialized.  This function will call
 * variant_reset() to clear the variant and then store the new value.
 * 
 * Parameters:
 * 
 *   pv - the variant to set
 * 
 *   col - the ARGB value
 */
static void variant_setColor(VARIANT *pv, uint32_t col) {
  if (pv == NULL) {
    raiseErr(__LINE__);
  }
  variant_reset(pv);
  pv->vtype = VM_TYPE_COLOR;
  (pv->val).col = col;
}

/*
 * Put a string value into a variant.
 * 
 * The given variant must be initialized.  This function will call
 * variant_reset() to clear the variant and then store the new value.
 * 
 * The given ISTR must be initialized.  Its current reference will be
 * copied into an ISTR stored in the variant.
 * 
 * Parameters:
 * 
 *   pv - the variant to set
 * 
 *   ps - the ISTR to copy
 */
static void variant_setString(VARIANT *pv, ISTR *ps) {
  if ((pv == NULL) || (ps == NULL)) {
    raiseErr(__LINE__);
  }
  variant_reset(pv);
  pv->vtype = VM_TYPE_STRING;
  istr_init(&((pv->val).str));
  istr_copy(&((pv->val).str), ps);
}

/*
 * Put a node value into a variant.
 * 
 * The given variant must be initialized.  This function will call
 * variant_reset() to clear the variant and then store the new value.
 * 
 * Parameters:
 * 
 *   pv - the variant to set
 * 
 *   pn - the node value
 */
static void variant_setNode(VARIANT *pv, NODE *pn) {
  if ((pv == NULL) || (pn == NULL)) {
    raiseErr(__LINE__);
  }
  variant_reset(pv);
  pv->vtype = VM_TYPE_NODE;
  (pv->val).pn = pn;
}

/*
 * Allocate a new VBLOCK array.
 * 
 * max_cap is the maximum capacity of the array in variants.  It must be
 * greater than zero and less than or equal to (INT32_MAX / 2).
 * 
 * The initial capacity of the array will be the minimum of max_cap and
 * VBLOCK_INITIAL.  When the array runs out of capacity, it is grown by
 * doubling, to a maximum of max_cap.
 * 
 * Parameters:
 * 
 *   max_cap - the maximum capacity of the VBLOCK array
 * 
 * Return:
 * 
 *   a new VBLOCK array
 */
static VBLOCK *vblock_alloc(int32_t max_cap) {
  
  VBLOCK *pv = NULL;
  int32_t i = 0;
  
  /* Check parameter */
  if ((max_cap < 1) || (max_cap > INT32_MAX / 2)) {
    raiseErr(__LINE__);
  }
  
  /* Allocate structure */
  pv = (VBLOCK *) calloc(1, sizeof(VBLOCK));
  if (pv == NULL) {
    fprintf(stderr, "%s: Out of memory!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Initialize vmax to the given max_cap and vcount to zero since the
   * array starts empty */
  pv->vmax = max_cap;
  pv->vcount = 0;
  
  /* The initial capacity is the minimum of max_cap and
   * VBLOCK_INITIAL */
  pv->vcap = VBLOCK_INITIAL;
  if (max_cap < pv->vcap) {
    pv->vcap = max_cap;
  }
  
  /* Allocate the initial array */
  pv->pBank = (VARIANT *) calloc((size_t) pv->vcap, sizeof(VARIANT));
  if (pv->pBank == NULL) {
    fprintf(stderr, "%s: Out of memory!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Initialize all variants */
  for(i = 0; i < pv->vcap; i++) {
    variant_init(&((pv->pBank)[i]));
  }
  
  /* Return the new object */
  return pv;
}

/*
 * Return how long a variant array currently is.
 * 
 * This is the actual number of array elements in use, not the capacity.
 * 
 * Parameters:
 * 
 *   pb - the block to query
 * 
 * Return:
 * 
 *   the number of elements currently in use
 */
static int32_t vblock_count(VBLOCK *pb) {
  
  /* Check parameter */
  if (pb == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Return count */
  return pb->vcount;
}

/*
 * Extend the length of a variant array by one.
 * 
 * The new variant is initialized to UNDEF type.
 * 
 * Parameters:
 * 
 *   pb - the block to extend
 * 
 * Return:
 * 
 *   the array index of the new variant element, or -1 if array is full
 *   to maximum capacity and can't be extended
 */
static int32_t vblock_append(VBLOCK *pb) {
  int32_t oc = 0;
  int32_t nc = 0;
  int32_t i  = 0;
  
  /* Check parameter */
  if (pb == NULL) {
    raiseErr(__LINE__);
  }
  
  /* If we have filled capacity, we need to extend it */
  if (pb->vcount >= pb->vcap) {
    /* If we have reached maximum capacity, error */
    if (pb->vcap >= pb->vmax) {
      return -1;
    }
    
    /* Store the original capacity before extension */
    oc = pb->vcap;
    
    /* New capacity is minimum of double current capacity and the
     * maximum capacity */
    nc = pb->vcap * 2;
    if (nc > pb->vmax) {
      nc = pb->vmax;
    }
    
    /* Expand the array allocation */
    pb->pBank = (VARIANT *) realloc(
                              pb->pBank,
                              ((size_t) nc) * sizeof(VARIANT));
    if (pb->pBank == NULL) {
      fprintf(stderr, "%s: Out of memory!\n", getModule());
      raiseErr(__LINE__);
    }
    
    /* Initialize all the new variants */
    for(i = oc; i < nc; i++) {
      variant_init(&((pb->pBank)[i]));
    }
    
    /* Update capacity */
    pb->vcap = nc;
  }
  
  /* We now have capacity, so we just need to increase count and return
   * the new index */
  (pb->vcount)++;
  return (pb->vcount - 1);
}

/*
 * Reduce the length of a variant array by one.
 * 
 * The dropped variant is reset to UNDEF.  An error occurs if this
 * function is called when the array is already empty.
 * 
 * Parameters:
 * 
 *   pb - the block to reduce
 */
static void vblock_reduce(VBLOCK *pb) {
  /* Check parameters */
  if (pb == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Check that array is not empty */
  if (pb->vcount < 1) {
    raiseErr(__LINE__);
  }
  
  /* Clear last entry and reduce array count */
  variant_reset(&((pb->pBank)[pb->vcount - 1]));
  (pb->vcount)--;
}

/*
 * Set a particular variant within a variant array.
 * 
 * pb is the variant array and i is the index of the element.  i must be
 * greater than or equal to zero and less than the count returned by
 * vblock_count().
 * 
 * pv is an initialized variant structure that holds the value to set.
 * The value of this variant is copied to the indicated array element.
 * 
 * Parameters:
 * 
 *   pb - the block to change
 * 
 *   i - the array element index
 * 
 *   pv - the value to copy to the array element
 */
static void vblock_set(VBLOCK *pb, int32_t i, VARIANT *pv) {
  /* Check parameters */
  if ((pb == NULL) || (pv == NULL)) {
    raiseErr(__LINE__);
  }
  if ((i < 0) || (i >= pb->vcount)) {
    raiseErr(__LINE__);
  }
  
  /* Perform copy operation */
  variant_copy(&((pb->pBank)[i]), pv);
}

/*
 * Get a particular variant within a variant array.
 * 
 * pb is the variant array and i is the index of the element.  i must be
 * greater than or equal to zero and less than the count returned by
 * vblock_count().
 * 
 * pv is an initialized variant structure.  The current value of the
 * requested array element is copied into this variant.
 * 
 * Parameters:
 * 
 *   pv - the variant to copy the array element into
 * 
 *   pb - the block to query
 * 
 *   i - the array element index
 */
static void vblock_get(VARIANT *pv, VBLOCK *pb, int32_t i) {
  /* Check parameters */
  if ((pb == NULL) || (pv == NULL)) {
    raiseErr(__LINE__);
  }
  if ((i < 0) || (i >= pb->vcount)) {
    raiseErr(__LINE__);
  }
  
  /* Perform copy operation */
  variant_copy(pv, &((pb->pBank)[i]));
}

/*
 * Allocate a new IBLOCK array.
 * 
 * max_cap is the maximum capacity of the array in integers.  It must be
 * greater than zero and less than or equal to (INT32_MAX / 2).
 * 
 * The initial capacity of the array will be the minimum of max_cap and
 * IBLOCK_INITIAL.  When the array runs out of capacity, it is grown by
 * doubling, to a maximum of max_cap.
 * 
 * Parameters:
 * 
 *   max_cap - the maximum capacity of the IBLOCK array
 * 
 * Return:
 * 
 *   a new IBLOCK array
 */
static IBLOCK *iblock_alloc(int32_t max_cap) {
  
  IBLOCK *pv = NULL;
  
  /* Check parameter */
  if ((max_cap < 1) || (max_cap > INT32_MAX / 2)) {
    raiseErr(__LINE__);
  }
  
  /* Allocate structure */
  pv = (IBLOCK *) calloc(1, sizeof(IBLOCK));
  if (pv == NULL) {
    fprintf(stderr, "%s: Out of memory!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Initialize imax to the given max_cap and icount to zero since the
   * array starts empty */
  pv->imax = max_cap;
  pv->icount = 0;
  
  /* The initial capacity is the minimum of max_cap and
   * IBLOCK_INITIAL */
  pv->icap = IBLOCK_INITIAL;
  if (max_cap < pv->icap) {
    pv->icap = max_cap;
  }
  
  /* Allocate the initial array */
  pv->pBank = (uint32_t *) calloc((size_t) pv->icap, sizeof(uint32_t));
  if (pv->pBank == NULL) {
    fprintf(stderr, "%s: Out of memory!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Return the new object */
  return pv;
}

/*
 * Return how long an integer array currently is.
 * 
 * This is the actual number of array elements in use, not the capacity.
 * 
 * Parameters:
 * 
 *   pb - the block to query
 * 
 * Return:
 * 
 *   the number of elements currently in use
 */
static int32_t iblock_count(IBLOCK *pb) {
  
  /* Check parameter */
  if (pb == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Return count */
  return pb->icount;
}

/*
 * Extend the length of an integer array by one.
 * 
 * The new integer is initialized to zero.
 * 
 * Parameters:
 * 
 *   pb - the block to extend
 * 
 * Return:
 * 
 *   the array index of the new integer element, or -1 if array is full
 *   to maximum capacity and can't be extended
 */
static int32_t iblock_append(IBLOCK *pb) {
  
  int32_t oc = 0;
  int32_t nc = 0;
  
  /* Check parameter */
  if (pb == NULL) {
    raiseErr(__LINE__);
  }
  
  /* If we have filled capacity, we need to extend it */
  if (pb->icount >= pb->icap) {
    /* If we have reached maximum capacity, error */
    if (pb->icap >= pb->imax) {
      return -1;
    }
    
    /* Store the original capacity before extension */
    oc = pb->icap;
    
    /* New capacity is minimum of double current capacity and the
     * maximum capacity */
    nc = pb->icap * 2;
    if (nc > pb->imax) {
      nc = pb->imax;
    }
    
    /* Expand the array allocation */
    pb->pBank = (uint32_t *) realloc(
                              pb->pBank,
                              ((size_t) nc) * sizeof(uint32_t));
    if (pb->pBank == NULL) {
      fprintf(stderr, "%s: Out of memory!\n", getModule());
      raiseErr(__LINE__);
    }
    
    /* Initialize all the new integers */
    memset(
      &((pb->pBank)[oc]), 0, ((size_t) (nc - oc)) * sizeof(uint32_t));
    
    /* Update capacity */
    pb->icap = nc;
  }
  
  /* We now have capacity, so we just need to increase count and return
   * the new index */
  (pb->icount)++;
  return (pb->icount - 1);
}

/*
 * Reduce the length of an integer array by one.
 * 
 * An error occurs if this function is called when the array is already
 * empty.
 * 
 * Parameters:
 * 
 *   pb - the block to reduce
 */
static void iblock_reduce(IBLOCK *pb) {
  /* Check parameters */
  if (pb == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Check that array is not empty */
  if (pb->icount < 1) {
    raiseErr(__LINE__);
  }
  
  /* Clear last entry and reduce array count */
  memset(&((pb->pBank)[pb->icount - 1]), 0, sizeof(uint32_t));
  (pb->icount)--;
}

/*
 * Set a particular integer within an integer array.
 * 
 * pb is the integer array and i is the index of the element.  i must be
 * greater than or equal to zero and less than the count returned by
 * iblock_count().  val is the value to set.
 * 
 * Parameters:
 * 
 *   pb - the block to change
 * 
 *   i - the array element index
 * 
 *   val - the integer value to set
 */
static void iblock_set(IBLOCK *pb, int32_t i, uint32_t val) {
  /* Check parameters */
  if (pb == NULL) {
    raiseErr(__LINE__);
  }
  if ((i < 0) || (i >= pb->icount)) {
    raiseErr(__LINE__);
  }
  
  /* Perform copy operation */
  (pb->pBank)[i] = val;
}

/*
 * Get a particular integer within an integer array.
 * 
 * pb is the variant array and i is the index of the element.  i must be
 * greater than or equal to zero and less than the count returned by
 * vblock_count().
 * 
 * Parameters:
 * 
 *   pb - the block to query
 * 
 *   i - the array element index
 * 
 * Return:
 * 
 *   the requested integer
 */
static uint32_t iblock_get(IBLOCK *pb, int32_t i) {
  /* Check parameters */
  if (pb == NULL) {
    raiseErr(__LINE__);
  }
  if ((i < 0) || (i >= pb->icount)) {
    raiseErr(__LINE__);
  }
  
  /* Return requested integer */
  return (pb->pBank)[i];
}

/*
 * Allocate a new OBLOCK array.
 * 
 * max_cap is the maximum capacity of the array in integers.  It must be
 * greater than zero and less than or equal to (INT32_MAX / 2).
 * 
 * The initial capacity of the array will be the minimum of max_cap and
 * OBLOCK_INITIAL.  When the array runs out of capacity, it is grown by
 * doubling, to a maximum of max_cap.
 * 
 * Parameters:
 * 
 *   max_cap - the maximum capacity of the OBLOCK array
 * 
 * Return:
 * 
 *   a new OBLOCK array
 */
static OBLOCK *oblock_alloc(int32_t max_cap) {
  
  OBLOCK *pv = NULL;
  
  /* Check parameter */
  if ((max_cap < 1) || (max_cap > INT32_MAX / 2)) {
    raiseErr(__LINE__);
  }
  
  /* Allocate structure */
  pv = (OBLOCK *) calloc(1, sizeof(OBLOCK));
  if (pv == NULL) {
    fprintf(stderr, "%s: Out of memory!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Initialize omax to the given max_cap and ocount to zero since the
   * array starts empty */
  pv->omax = max_cap;
  pv->ocount = 0;
  
  /* The initial capacity is the minimum of max_cap and
   * OBLOCK_INITIAL */
  pv->ocap = OBLOCK_INITIAL;
  if (max_cap < pv->ocap) {
    pv->ocap = max_cap;
  }
  
  /* Allocate the initial array */
  pv->pBank = (fp_vm_op *) calloc((size_t) pv->ocap, sizeof(fp_vm_op));
  if (pv->pBank == NULL) {
    fprintf(stderr, "%s: Out of memory!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Return the new object */
  return pv;
}

/*
 * Append a pointer to a pointer array.
 * 
 * Parameters:
 * 
 *   pb - the block to extend
 * 
 *   fp - the function pointer to append
 * 
 * Return:
 * 
 *   the array index of the new pointer element, or -1 if array is full
 *   to maximum capacity and can't be extended
 */
static int32_t oblock_append(OBLOCK *pb, fp_vm_op fp) {
  
  int32_t oc = 0;
  int32_t nc = 0;
  
  /* Check parameter */
  if ((pb == NULL) || (fp == NULL)) {
    raiseErr(__LINE__);
  }
  
  /* If we have filled capacity, we need to extend it */
  if (pb->ocount >= pb->ocap) {
    /* If we have reached maximum capacity, error */
    if (pb->ocap >= pb->omax) {
      return -1;
    }
    
    /* Store the original capacity before extension */
    oc = pb->ocap;
    
    /* New capacity is minimum of double current capacity and the
     * maximum capacity */
    nc = pb->ocap * 2;
    if (nc > pb->omax) {
      nc = pb->omax;
    }
    
    /* Expand the array allocation */
    pb->pBank = (fp_vm_op *) realloc(
                              pb->pBank,
                              ((size_t) nc) * sizeof(fp_vm_op));
    if (pb->pBank == NULL) {
      fprintf(stderr, "%s: Out of memory!\n", getModule());
      raiseErr(__LINE__);
    }
    
    /* Initialize all the new pointers */
    memset(
      &((pb->pBank)[oc]), 0, ((size_t) (nc - oc)) * sizeof(fp_vm_op));
    
    /* Update capacity */
    pb->ocap = nc;
  }
  
  /* We now have capacity, so append new element, increase count, and
   * return the new index */
  (pb->pBank)[pb->ocount] = fp;
  (pb->ocount)++;
  return (pb->ocount - 1);
}

/*
 * Invoke a particular function within an pointer array.
 * 
 * pb is the variant array and i is the index of the element to invoke.
 * i must be greater than or equal to zero and less than the count
 * returned by vblock_count().
 * 
 * Parameters:
 * 
 *   pb - the block to query
 * 
 *   i - the array element index
 */
static void oblock_invoke(OBLOCK *pb, int32_t i) {
  
  fp_vm_op fp = NULL;
  
  /* Check parameters */
  if (pb == NULL) {
    raiseErr(__LINE__);
  }
  if ((i < 0) || (i >= pb->ocount)) {
    raiseErr(__LINE__);
  }
  
  /* Invoke the requested function */
  fp = (pb->pBank)[i];
  return fp();
}

/*
 * Shastina entity handler for string.
 * 
 * Parameters:
 * 
 *   pEnt - the entity
 */
static void handleString(SNENTITY *pEnt) {
  
  char *pc = NULL;
  ISTR str;
  uint32_t col = 0;
  int i = 0;
  int c = 0;
  
  /* Initialize structures */
  istr_init(&str);
  
  /* Check parameter */
  if (pEnt == NULL) {
    raiseErr(__LINE__);
  }
  if (pEnt->status != SNENTITY_STRING) {
    raiseErr(__LINE__);
  }
  
  /* Check that no string prefix */
  if (strlen(pEnt->pKey) > 0) {
    fprintf(stderr, "%s: String literals may not have prefixes!\n",
            getModule());
    raiseErr(__LINE__);
  }
  
  /* Different handling depending on string type */
  if (pEnt->str_type == SNSTRING_QUOTED) {
  
    /* Double-quoted string literal -- check string literal */
    for(pc = pEnt->pValue; *pc != 0; pc++) {
      if ((*pc < 0x20) || (*pc > 0x7e) ||
          (*pc == '"') || (*pc == '\\')) {
        fprintf(stderr, "%s: Illegal characters in string literal!\n",
                getModule());
        raiseErr(__LINE__);
      }
    }
    
    /* Load into string, which will also handle backslash substitution
     * if necessary */
    istr_new(&str, pEnt->pValue);
    
    /* Push onto stack */
    vm_push_s(&str);
  
  } else if (pEnt->str_type == SNSTRING_CURLY) {
    /* Curly string entity -- check length */
    if (strlen(pEnt->pValue) != 8) {
      fprintf(stderr, "%s: Invalid color literal: %s\n",
              getModule(), pEnt->pValue);
      raiseErr(__LINE__);
    }
    
    /* Parse color value */
    for(i = 0; i < 8; i++) {
      /* Get current character */
      c = (pEnt->pValue)[i];
      
      /* Convert to digit */
      if ((c >= '0') && (c <= '9')) {
        c = c - '0';
      
      } else if ((c >= 'A') && (c <= 'F')) {
        c = c - 'A' + 10;
        
      } else if ((c >= 'a') && (c <= 'f')) {
        c = c - 'a' + 10;
        
      } else {
        fprintf(stderr, "%s: Invalid color literal: %s\n",
                getModule(), pEnt->pValue);
        raiseErr(__LINE__);
      }
      
      /* Add in this digit */
      col = (col << 4) + ((uint32_t) c);
    }
    
    /* Push color value onto stack */
    vm_push_c(col);
  
  } else {
    raiseErr(__LINE__);
  }
    
  /* Reset local structures */
  istr_reset(&str);
}

/*
 * Shastina entity handler for numeric.
 * 
 * Parameters:
 * 
 *   pEnt - the entity
 */
static void handleNumeric(SNENTITY *pEnt) {
  
  char *endptr = NULL;
  double f = 0.0;
  
  /* Check parameter */
  if (pEnt == NULL) {
    raiseErr(__LINE__);
  }
  if (pEnt->status != SNENTITY_NUMERIC) {
    raiseErr(__LINE__);
  }
  
  /* Attempt to parse as double */
  errno = 0;
  f = strtod(pEnt->pKey, &endptr);
  if (errno != 0) {
    fprintf(stderr, "%s: Failed to parse floating-point literal: %s\n",
            getModule(), pEnt->pKey);
    raiseErr(__LINE__);
  }
  if (endptr != NULL) {
    if (*endptr != 0) {
      fprintf(stderr,
              "%s: Failed to parse floating-point literal: %s\n",
              getModule(), pEnt->pKey);
      raiseErr(__LINE__);
    }
  }
  if (!isfinite(f)) {
    fprintf(stderr, "%s: Floating-point literal must be finite: %s\n",
            getModule(), pEnt->pKey);
    raiseErr(__LINE__);
  }
  
  /* Push onto stack */
  vm_push_f(f);
}

/*
 * Shastina entity handler for variable and constant declarations.
 * 
 * Parameters:
 * 
 *   pEnt - the entity
 */
static void handleDeclare(SNENTITY *pEnt) {
  
  int32_t i = 0;
  int32_t j = 0;
  int32_t offs = 0;
  uint32_t flag = 0;
  uint32_t cv = 0;
  VARIANT v;
  
  /* Initialize structures */
  variant_init(&v);
  
  /* Check parameter */
  if (pEnt == NULL) {
    raiseErr(__LINE__);
  }
  if ((pEnt->status != SNENTITY_VARIABLE) &&
      (pEnt->status != SNENTITY_CONSTANT)) {
    raiseErr(__LINE__);
  }
  
  /* Check that the variable or constant name is valid */
  if (!validName(pEnt->pKey)) {
    fprintf(stderr,
      "%s: Invalid variable/constant name: %s\n",
      getModule(), pEnt->pKey);
    raiseErr(__LINE__);
  }
  
  /* Check that something is visible on top of the interpreter stack */
  if (vm_type() == VM_TYPE_UNDEF) {
    fprintf(stderr,
      "%s: Variable/constant declarations require an initial value!\n",
      getModule());
    raiseErr(__LINE__);
  }
  
  /* Add another entry to the namespace array */
  i = vblock_append(m_ns);
  if (i < 0) {
    fprintf(stderr,
      "%s: Too many variables/constants!  Increase name-limit.\n",
      getModule());
    raiseErr(__LINE__);
  }
  
  /* Add another block of 32 flags if necessary and initialize all to
   * zero */
  if ((i % 32) == 0) {
    j = iblock_append(m_ns_flag);
    if (j < 0) {
      raiseErr(__LINE__);
    }
    iblock_set(m_ns_flag, j, 0);
  }
  
  /* If this declaration is for a constant, set the flag */
  if (pEnt->status == SNENTITY_CONSTANT) {
    /* Compute offset and flag bit */
    offs = i / 32;
    flag = ((uint32_t) 1) << (i % 32);
    
    /* Get current value, set the flag, and update */
    cv = iblock_get(m_ns_flag, offs);
    cv |= ((uint32_t) flag);
    iblock_set(m_ns_flag, offs, cv);
  }
  
  /* Map the name to the index */
  if (!rfdict_insert(m_ns_dict, pEnt->pKey, (long) i)) {
    fprintf(stderr, "%s: Redefinition of variable/constant name: %s\n",
        getModule(), pEnt->pKey);
    raiseErr(__LINE__);
  }
  
  /* Get the value on top of the stack and then pop it */
  vblock_get(&v, m_st, vblock_count(m_st) - 1);
  vm_pop();
  
  /* Set the variable or constant value to the value we just got */
  vblock_set(m_ns, i, &v);
  
  /* Reset local structures */
  variant_reset(&v);
}

/*
 * Shastina entity handler for variable assignment.
 * 
 * Parameters:
 * 
 *   pEnt - the entity
 */
static void handleAssign(SNENTITY *pEnt) {
  
  int32_t i = 0;
  int32_t offs = 0;
  uint32_t flag = 0;
  uint32_t cv = 0;
  VARIANT v;
  
  /* Initialize structures */
  variant_init(&v);
  
  /* Check state */
  if (!m_run) {
    raiseErr(__LINE__);
  }
  
  /* Check parameter */
  if (pEnt == NULL) {
    raiseErr(__LINE__);
  }
  if (pEnt->status != SNENTITY_ASSIGN) {
    raiseErr(__LINE__);
  }
  
  /* Check that the variable or constant name is valid */
  if (!validName(pEnt->pKey)) {
    fprintf(stderr,
      "%s: Invalid variable name: %s\n",
      getModule(), pEnt->pKey);
    raiseErr(__LINE__);
  }
  
  /* Check that something is visible on top of the interpreter stack */
  if (vm_type() == VM_TYPE_UNDEF) {
    fprintf(stderr,
      "%s: Variable assignment requires a stack value!\n",
      getModule());
    raiseErr(__LINE__);
  }
  
  /* Look up the variable/constant index */
  i = (int32_t) rfdict_get(m_ns_dict, pEnt->pKey, -1);
  if (i < 0) {
    fprintf(stderr,
      "%s: Variable %s is not declared!\n",
      getModule(), pEnt->pKey);
    raiseErr(__LINE__);
  }
  
  /* Compute the offset and flag bit for i */
  offs = i / 32;
  flag = ((uint32_t) 1) << (i % 32);
  
  /* Get the flag dword and check that the constant flag is not set */
  cv = iblock_get(m_ns_flag, offs);
  if ((cv & flag) != 0) {
    fprintf(stderr,
      "%s: Can't assign value to constant: %s\n",
      getModule(), pEnt->pKey);
    raiseErr(__LINE__);
  }
  
  /* Get the value on top of the stack and then pop it */
  vblock_get(&v, m_st, vblock_count(m_st) - 1);
  vm_pop();
  
  /* Set the variable or constant value to the value we just got */
  vblock_set(m_ns, i, &v);
  
  /* Reset local structures */
  variant_reset(&v);
}

/*
 * Shastina entity handler for variable or constant read.
 * 
 * Parameters:
 * 
 *   pEnt - the entity
 */
static void handleGet(SNENTITY *pEnt) {
  
  int32_t i = 0;
  VARIANT v;
  
  /* Initialize structures */
  variant_init(&v);
  
  /* Check state */
  if (!m_run) {
    raiseErr(__LINE__);
  }
  
  /* Check parameter */
  if (pEnt == NULL) {
    raiseErr(__LINE__);
  }
  if (pEnt->status != SNENTITY_GET) {
    raiseErr(__LINE__);
  }
  
  /* Check that the variable or constant name is valid */
  if (!validName(pEnt->pKey)) {
    fprintf(stderr,
      "%s: Invalid variable/constant name: %s\n",
      getModule(), pEnt->pKey);
    raiseErr(__LINE__);
  }
  
  /* Look up the variable/constant index */
  i = (int32_t) rfdict_get(m_ns_dict, pEnt->pKey, -1);
  if (i < 0) {
    fprintf(stderr,
      "%s: Variable/constant %s is not declared!\n",
      getModule(), pEnt->pKey);
    raiseErr(__LINE__);
  }
  
  /* Get the variable or constant value */
  vblock_get(&v, m_ns, i);
  
  /* Extend the interpreter stack by one */
  i = vblock_append(m_st);
  if (i < 0) {
    fprintf(stderr, "%s: Interpreter stack overflow!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Put variant onto stack */
  vblock_set(m_st, i, &v);
  
  /* Reset local structures */
  variant_reset(&v);
}

/*
 * Shastina entity handler for begin group.
 */
static void handlBeginGroup(void) {
  
  int32_t c = 0;
  int32_t i = 0;
  
  /* Check state */
  if (!m_run) {
    raiseErr(__LINE__);
  }
  
  /* Get the total number of elements on the stack currently (including
   * hidden elements) */
  c = vblock_count(m_st);
  
  /* Extend the grouping stack by one */
  i = iblock_append(m_gs);
  if (i < 0) {
    fprintf(stderr, "%s: Too much group nesting! Max: %ld\n",
      getModule(), (long) MAX_GROUP_DEPTH);
    raiseErr(__LINE__);
  }
  
  /* Put the current stack count on top of the grouping stack */
  iblock_set(m_gs, i, (uint32_t) c);
}

/*
 * Shastina entity handler for end group.
 */
static void handleEndGroup(void) {
  
  int32_t c = 0;
  int32_t s = 0;
  
  /* Check state */
  if (!m_run) {
    raiseErr(__LINE__);
  }
  
  /* Get the total number of elements on the stack currently (including
   * hidden elements) */
  c = vblock_count(m_st);
  
  /* Pop the top value off the grouping stack */
  s = iblock_get(m_gs, iblock_count(m_gs) - 1);
  iblock_reduce(m_gs);
  
  /* Make sure the total number of elements on the stack currently is
   * exactly one greater than the value we just popped */
  if (c - 1 != s) {
    fprintf(stderr, "%s: Group must result in exactly one value!\n",
            getModule());
    raiseErr(__LINE__);
  }
}

/*
 * Shastina entity handler for array.
 * 
 * Parameters:
 * 
 *   pEnt - the entity
 */
static void handleArray(SNENTITY *pEnt) {
  
  int32_t i = 0;
  VARIANT v;
  
  /* Initialize structures */
  variant_init(&v);
  
  /* Check state */
  if (!m_run) {
    raiseErr(__LINE__);
  }
  
  /* Check parameter */
  if (pEnt == NULL) {
    raiseErr(__LINE__);
  }
  if (pEnt->status != SNENTITY_ARRAY) {
    raiseErr(__LINE__);
  }
  
  /* Store the array count as a floating-point value */
  variant_setFloat(&v, (double) pEnt->count);
  
  /* Extend the interpreter stack by one */
  i = vblock_append(m_st);
  if (i < 0) {
    fprintf(stderr, "%s: Interpreter stack overflow!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Put variant onto stack */
  vblock_set(m_st, i, &v);
  
  /* Reset local structures */
  variant_reset(&v);
}

/*
 * Shastina entity handler for operation.
 * 
 * Parameters:
 * 
 *   pEnt - the entity
 */
static void handleOperation(SNENTITY *pEnt) {
  
  int32_t i = 0;
  
  /* Check state */
  if (!m_run) {
    raiseErr(__LINE__);
  }
  
  /* Check parameter */
  if (pEnt == NULL) {
    raiseErr(__LINE__);
  }
  if (pEnt->status != SNENTITY_OPERATION) {
    raiseErr(__LINE__);
  }
  
  /* Check the name */
  if (!validName(pEnt->pKey)) {
    fprintf(stderr, "%s: Invalid operation name: %s\n",
            getModule(), pEnt->pKey);
    raiseErr(__LINE__);
  }
  
  /* Look up the operation in the operation table */
  i = (int32_t) rfdict_get(m_ops_dict, pEnt->pKey, -1);
  if (i < 0) {
    fprintf(stderr, "%s: Unrecognized operation name: %s\n",
            getModule(), pEnt->pKey);
    raiseErr(__LINE__);
  }
  
  /* Invoke the operation */
  oblock_invoke(m_ops, i);
}

/*
 * Public function implementations
 * -------------------------------
 * 
 * See the header for specifications.
 */

/*
 * vm_register function.
 */
void vm_register(const char *pOpName, fp_vm_op fp) {
  
  int32_t i = 0;
  
  /* Check state */
  if (m_called) {
    raiseErr(__LINE__);
  }
  
  /* Check parameters */
  if ((pOpName == NULL) || (fp == NULL)) {
    raiseErr(__LINE__);
  }
  if (!validName(pOpName)) {
    fprintf(stderr, "%s: Illegal registered operation name: %s\n",
            getModule(), pOpName);
    raiseErr(__LINE__);
  }
  
  /* Initialize if necessary */
  if (m_ops == NULL) {
    m_ops_dict = rfdict_alloc(1);
    m_ops = oblock_alloc(MAX_OPERATION_COUNT);
  }
  
  /* Add this operation to the table */
  i = oblock_append(m_ops, fp);
  if (i < 0) {
    fprintf(stderr, "%s: Too many registered operations! Max: %ld\n",
            getModule(), (long) MAX_OPERATION_COUNT);
    raiseErr(__LINE__);
  }
  
  /* Add a mapping for this operation name */
  if (!rfdict_insert(m_ops_dict, pOpName, (long) i)) {
    fprintf(stderr, "%s: Multiple registerations for op %s\n",
            getModule(), pOpName);
    raiseErr(__LINE__);
  }
}

/*
 * vm_run function.
 */
NODE *vm_run(SNPARSER *pp, SNSOURCE *pSrc) {
  
  SNENTITY ent;
  NODE *pResult = NULL;
  int errnum = 0;
  int32_t fcount = 0;
  
  /* Initialize structures */
  memset(&ent, 0, sizeof(SNENTITY));
  
  /* Check state and set called flag */
  if (m_called) {
    raiseErr(__LINE__);
  }
  m_called = 1;
  
  /* Check parameters */
  if ((pp == NULL) || (pSrc == NULL)) {
    raiseErr(__LINE__);
  }
  
  /* Compute maximum number of 32-bit integers for storing namespace
   * flags */
  fcount = getConfigInt(CFG_NAME_LIMIT) / 32;
  if ((getConfigInt(CFG_NAME_LIMIT) % 32) > 0) {
    fcount++;
  }
  
  /* Initialize running state */
  m_gs = iblock_alloc(MAX_GROUP_DEPTH);
  
  m_st = vblock_alloc(getConfigInt(CFG_STACK_HEIGHT));
  
  m_ns_dict = rfdict_alloc(1);
  m_ns_flag = iblock_alloc(fcount);
  m_ns = vblock_alloc(getConfigInt(CFG_NAME_LIMIT));
  
  /* Turn on running mode */
  m_run = 1;
  
  /* Process all entities */
  for(readEntity(pp, &ent, pSrc);
      ent.status != SNENTITY_EOF;
      readEntity(pp, &ent, pSrc)) {
    
    /* We have a node other than EOF, so dispatch to handler */
    switch (ent.status) {
    
      case SNENTITY_STRING:
        handleString(&ent);
        break;
      
      case SNENTITY_NUMERIC:
        handleNumeric(&ent);
        break;
      
      case SNENTITY_VARIABLE:
        handleDeclare(&ent);
        break;
      
      case SNENTITY_CONSTANT:
        handleDeclare(&ent);
        break;
      
      case SNENTITY_ASSIGN:
        handleAssign(&ent);
        break;
      
      case SNENTITY_GET:
        handleGet(&ent);
        break;
      
      case SNENTITY_BEGIN_GROUP:
        handlBeginGroup();
        break;
      
      case SNENTITY_END_GROUP:
        handleEndGroup();
        break;
      
      case SNENTITY_ARRAY:
        handleArray(&ent);
        break;
      
      case SNENTITY_OPERATION:
        handleOperation(&ent);
        break;
      
      default:
        fprintf(stderr,
          "%s: Unsupported Shastina entity type!\n",
          getModule());
        raiseErr(__LINE__);
    }
  }
  
  /* Consume rest of input */
  errnum = snsource_consume(pSrc);
  if (errnum <= 0) {
    fprintf(stderr,
      "%s: Failed to consume rest of input after |; because: %s!\n",
      getModule(), snerror_str(errnum));
    raiseErr(__LINE__);
  }
  
  /* Check exactly one thing remains on stack */
  if (vblock_count(m_st) != 1) {
    fprintf(stderr,
      "%s: Must leave exactly one node on interpreter stack!\n",
      getModule());
    raiseErr(__LINE__);
  }
  
  /* Check a node remains on stack */
  if (vm_type() != VM_TYPE_NODE) {
    fprintf(stderr,
      "%s: Must leave exactly one node on interpreter stack!\n",
      getModule());
    raiseErr(__LINE__);
  }
  
  /* Pop the result */
  pResult = vm_pop_n();
  
  /* Turn off running mode */
  m_run = 0;
  
  /* Return result */
  return pResult;
}

/*
 * vm_type function.
 */
int vm_type(void) {
  
  uint32_t hide = 0;
  int result = 0;
  VARIANT v;
  
  /* Initialize structures */
  variant_init(&v);
  
  /* Check state */
  if (!m_run) {
    raiseErr(__LINE__);
  }
  
  /* Get the value on top of the grouping stack, or zero if the grouping
   * stack is empty */
  hide = 0;
  if (iblock_count(m_gs) > 0) {
    hide = iblock_get(m_gs, iblock_count(m_gs) - 1);
  }
  
  /* Result by default is UNDEF */
  result = VM_TYPE_UNDEF;
  
  /* We can only read top of stack if interpreter stack count is greater
   * than hide value */
  if (vblock_count(m_st) > hide) {
    /* Get the top of the stack */
    vblock_get(&v, m_st, vblock_count(m_st) - 1);
    
    /* Read variant type, which should not be UNDEF */
    result = variant_type(&v);
    if (result == VM_TYPE_UNDEF) {
      raiseErr(__LINE__);
    }
  }
  
  /* Reset local variant before returning */
  variant_reset(&v);
  
  /* Return result */
  return result;
}

/*
 * vm_pop_f function.
 */
double vm_pop_f(void) {
  
  VARIANT v;
  double result = 0.0;
  
  /* Initialize structures */
  variant_init(&v);
  
  /* Check state of stack */
  if (vm_type() != VM_TYPE_FLOAT) {
    raiseErr(__LINE__);
  }
  
  /* Get value at top of the stack and reduce the stack */
  vblock_get(&v, m_st, vblock_count(m_st) - 1);
  vblock_reduce(m_st);
  
  /* Read the value from the variant */
  result = variant_getFloat(&v);
  
  /* Reset variant structure */
  variant_reset(&v);
  
  /* Return the value */
  return result;
}

/*
 * vm_pop_c function.
 */
uint32_t vm_pop_c(void) {
  
  VARIANT v;
  uint32_t result = 0;
  
  /* Initialize structures */
  variant_init(&v);
  
  /* Check state of stack */
  if (vm_type() != VM_TYPE_COLOR) {
    raiseErr(__LINE__);
  }
  
  /* Get value at top of the stack and reduce the stack */
  vblock_get(&v, m_st, vblock_count(m_st) - 1);
  vblock_reduce(m_st);
  
  /* Read the value from the variant */
  result = variant_getColor(&v);
  
  /* Reset variant structure */
  variant_reset(&v);
  
  /* Return the value */
  return result;
}

/*
 * vm_pop_s function.
 */
void vm_pop_s(ISTR *ps) {
  
  VARIANT v;
  
  /* Initialize structures */
  variant_init(&v);
  
  /* Check parameters */
  if (ps == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Check state of stack */
  if (vm_type() != VM_TYPE_STRING) {
    raiseErr(__LINE__);
  }
  
  /* Get value at top of the stack and reduce the stack */
  vblock_get(&v, m_st, vblock_count(m_st) - 1);
  vblock_reduce(m_st);
  
  /* Copy the value from the variant */
  variant_getString(&v, ps);
  
  /* Reset variant structure */
  variant_reset(&v);
}

/*
 * vm_pop_n function.
 */
NODE *vm_pop_n(void) {
  
  VARIANT v;
  NODE *result = 0;
  
  /* Initialize structures */
  variant_init(&v);
  
  /* Check state of stack */
  if (vm_type() != VM_TYPE_NODE) {
    raiseErr(__LINE__);
  }
  
  /* Get value at top of the stack and reduce the stack */
  vblock_get(&v, m_st, vblock_count(m_st) - 1);
  vblock_reduce(m_st);
  
  /* Read the value from the variant */
  result = variant_getNode(&v);
  
  /* Reset variant structure */
  variant_reset(&v);
  
  /* Return the value */
  return result;
}

/*
 * vm_push_f function.
 */
void vm_push_f(double f) {
  
  int32_t i = 0;
  VARIANT v;
  
  /* Initialize structures */
  variant_init(&v);
  
  /* Check state */
  if (!m_run) {
    raiseErr(__LINE__);
  }
  
  /* Check parameter */
  if (!isfinite(f)) {
    raiseErr(__LINE__);
  }
  
  /* Extend the interpreter stack by one */
  i = vblock_append(m_st);
  if (i < 0) {
    fprintf(stderr, "%s: Interpreter stack overflow!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Write value into variant and put variant onto stack */
  variant_setFloat(&v, f);
  vblock_set(m_st, i, &v);
  
  /* Reset local variant */
  variant_reset(&v);
}

/*
 * vm_push_c function.
 */
void vm_push_c(uint32_t col) {
  
  int32_t i = 0;
  VARIANT v;
  
  /* Initialize structures */
  variant_init(&v);
  
  /* Check state */
  if (!m_run) {
    raiseErr(__LINE__);
  }
  
  /* Extend the interpreter stack by one */
  i = vblock_append(m_st);
  if (i < 0) {
    fprintf(stderr, "%s: Interpreter stack overflow!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Write value into variant and put variant onto stack */
  variant_setColor(&v, col);
  vblock_set(m_st, i, &v);
  
  /* Reset local variant */
  variant_reset(&v);
}

/*
 * vm_push_s function.
 */
void vm_push_s(ISTR *ps) {
  
  int32_t i = 0;
  VARIANT v;
  
  /* Initialize structures */
  variant_init(&v);
  
  /* Check state */
  if (!m_run) {
    raiseErr(__LINE__);
  }
  
  /* Check parameter */
  if (ps == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Extend the interpreter stack by one */
  i = vblock_append(m_st);
  if (i < 0) {
    fprintf(stderr, "%s: Interpreter stack overflow!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Write value into variant and put variant onto stack */
  variant_setString(&v, ps);
  vblock_set(m_st, i, &v);
  
  /* Reset local variant */
  variant_reset(&v);
}

/*
 * vm_push_n function.
 */
void vm_push_n(NODE *pn) {
  
  int32_t i = 0;
  VARIANT v;
  
  /* Initialize structures */
  variant_init(&v);
  
  /* Check state */
  if (!m_run) {
    raiseErr(__LINE__);
  }
  
  /* Check parameter */
  if (pn == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Extend the interpreter stack by one */
  i = vblock_append(m_st);
  if (i < 0) {
    fprintf(stderr, "%s: Interpreter stack overflow!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Write value into variant and put variant onto stack */
  variant_setNode(&v, pn);
  vblock_set(m_st, i, &v);
  
  /* Reset local variant */
  variant_reset(&v);
}

/*
 * vm_dup function.
 */
void vm_dup(void) {
  
  VARIANT v;
  int32_t i = 0;
  
  /* Initialize structures */
  variant_init(&v);
  
  /* Check that something is visible on top of the stack */
  if (vm_type() == VM_TYPE_UNDEF) {
    raiseErr(__LINE__);
  }
  
  /* Get value at top of the stack */
  vblock_get(&v, m_st, vblock_count(m_st) - 1);
  
  /* Extend the interpreter stack by one */
  i = vblock_append(m_st);
  if (i < 0) {
    fprintf(stderr, "%s: Interpreter stack overflow!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Copy the variant we just read onto the stack */
  vblock_set(m_st, i, &v);
  
  /* Reset local variant */
  variant_reset(&v);
}

/*
 * vm_pop function.
 */
void vm_pop(void) {
  
  /* Check that something is visible on top of the stack */
  if (vm_type() == VM_TYPE_UNDEF) {
    raiseErr(__LINE__);
  }
  
  /* Reduce the interpreter stack by one */
  vblock_reduce(m_st);
}
