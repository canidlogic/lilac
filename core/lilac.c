/*
 * lilac.c
 * =======
 * 
 * Lilac 2D renderer main module.
 * 
 * This module contains the program entrypoint.  It sets up
 * configuration variables and reads the header of the rendering script
 * in the process.  Then, it calls into the vm module to interpret the
 * rest of the rendering script and get the root node, and finally it
 * passes this root node to the render module to render the results.
 * 
 * The module also exports a few public functions that other modules can
 * use.  See the header lilac.h for further information.
 * 
 * Program syntax
 * --------------
 * 
 *   ./lilac out.png < script.lilac
 * 
 * The program reads a Lilac rendering script from standard input.  It
 * takes a single program argument, which is the path to the PNG file to
 * create when rendering, which must have a .png extension.
 * 
 * Configuration
 * -------------
 * 
 * This module attempts to autodetect whether it is being compiled on
 * Windows or non-Windows platform.  To manually configure this, define
 * either LILAC_WIN32_PLATFORM or LILAC_POSIX_PLATFORM.
 * 
 * Plug-in dependencies
 * --------------------
 * 
 * This module expects a header named "plugin.h" to be in the include
 * path, and that this header defines a function plugin_init() that
 * takes no arguments and returns no arguments.  A call will be made to
 * this function after the core section of Lilac has been initialized
 * but before the rendering script is parsed.
 * 
 * The purpose of this function call is to allow plug-in modules to
 * register operations in the core vm module and render preparation
 * functions in the render module.  See the README in the plugin
 * directory for further information.
 * 
 * Internal dependencies
 * ---------------------
 * 
 * - istr.c
 * - node.c
 * - render.c
 * - vm.c
 * 
 * External dependencies
 * ---------------------
 * 
 * - libpng (via libsophistry)
 * - librfdict
 * - libshastina
 * - libsophistry
 * - <math.h> (-lm)
 */

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lilac.h"
#include "node.h"
#include "render.h"
#include "vm.h"

#include "plugin.h"

#include "rfdict.h"
#include "sophistry.h"

/*
 * Platform detection
 * ------------------
 * 
 * Attempt to automatically detect whether we are on a Windows or POSIX
 * platform using predefined macros.
 * 
 * If you want to manually set the platform, declare either
 * LILAC_WIN32_PLATFORM or LILAC_POSIX_PLATFORM.
 */

#ifndef LILAC_WIN32_PLATFORM
#ifndef LILAC_POSIX_PLATFORM

#ifdef _WIN32
#define LILAC_WIN32_PLATFORM

#else

#ifdef __WIN32__
#define LILAC_WIN32_PLATFORM
#endif

#endif

#ifndef LILAC_WIN32_PLATFORM
#define LILAC_POSIX_PLATFORM
#endif

#endif
#endif

/*
 * Constants
 * ---------
 */

/*
 * Maximum length limit for both a metacommand string.
 * 
 * This does not include the terminating nul.
 */
#define MAX_STRING_LEN (1023)

/*
 * The types of arguments that can be passed to a particular header
 * metacommand.
 * 
 * ARG_NONE  : there are no arguments
 * 
 * ARG_UINT  : single unsigned integer in a bounded range
 * 
 * ARG_DIM   : two unsigned integers in bounded range, where their
 * product is also bounded
 * 
 * ARG_FRAME : single string storing a path to an image file; the width
 * and height of the image file are read, and the header metacommand is
 * then replaced with a different header metacommand using ARG_DIM with
 * the dimensions that were read from the image file
 */
#define ARG_NONE  (0)
#define ARG_UINT  (1)
#define ARG_DIM   (2)
#define ARG_FRAME (3)

/*
 * The maximum number of header metacommands that can be declared.
 * 
 * This is used to set the size of the array storing the metacommand
 * information structures.
 */
#define MAX_META_CMD (64)

/*
 * The types of configuration variables that can be stored in CONFIG_VAR
 * structures.
 */
#define CVAR_UNDEFINED (0)
#define CVAR_INTEGER   (1)
#define CVAR_STRING    (2)

/*
 * Type declarations
 * -----------------
 */

/*
 * Represents a specific header metacommand, excluding the signature
 * line.
 */
typedef struct {
  
  /*
   * The name of the header metacommand, excluding the % that begins the
   * metacommand.
   */
  const char *pName;
  
  /*
   * Flag indicating whether further header metacommands follow this
   * one.
   * 
   * This should be set to non-zero except for %body.
   * 
   * Ignored for ARG_FRAME metacommands, where the redirected
   * metacommand determines this flag.
   */
  int can_continue;
  
  /*
   * The type of arguments that this metacommand requires.
   */
  int atype;
  
  /*
   * For ARG_UINT and ARG_DIM, the minimum value that each argument can
   * have.
   * 
   * Since these integers are unsigned, the minimum value should always
   * be at least zero.
   */
  int32_t min_val;
  
  /*
   * For ARG_UINT and ARG_DIM, the maximum value that each argument can
   * have.
   */
  int32_t max_val;
  
  /*
   * For ARG_DIM, the maximum value that the two integers multiplied
   * together can have.
   * 
   * Multiplication is performed in 64-bit space so overflow is not an
   * issue.
   */
  int32_t max_product;
  
  /*
   * For ARG_FRAME, the metacommand to replace this metacommand with
   * once the dimensions are read from the image file.
   * 
   * The referenced metacommand must have arguments of type ARG_DIM.
   */
  const char *pRedirect;
  
  /*
   * Flag indicating whether this metacommand has been encountered
   * within the header.
   * 
   * Initially, all structures have a zero value for this flag.  When
   * metacommands are encountered, they set this flag to non-zero.  This
   * prevents a single metacommand from being used more than one time in
   * the header.
   * 
   * When metacommands of ARG_FRAME type are redirected to another
   * metacommand, the flag in both the original metacommand and the
   * redirected metacommand are set.  Both flags are checked to be
   * initially cleared.
   */
  int cmd_present;
  
  /*
   * The integer values recorded.
   * 
   * Only valid for metacommands of type ARG_UINT and ARG_DIM where
   * cmd_present is set.  For ARG_UINT, only the first value is set and
   * the second is always set to zero.  For ARG_DIM, both values are
   * set.
   */
  int32_t vals[2];
  
  /*
   * For ARG_UINT only, the default value to use if the value is not
   * explicitly set, or -1 if there is no default value and the value
   * must always be explicitly set.
   */
  int32_t default_value;
  
} META_CMD;

/*
 * Represents a single configuration variable in the configuration
 * array.
 */
typedef struct {
  
  /*
   * The type of configuration variable.
   * 
   * One of the CVAR_ constants.
   */
  int cvtype;
  
  /*
   * The integer value, if type of configuration value is CVAR_INTEGER.
   */
  int32_t ival;
  
  /*
   * The string value, if type of configuration value is CVAR_STRING.
   */
  const char *sval;
  
} CONFIG_VAR;

/*
 * Local data
 * ----------
 */

/*
 * The name of the executable module, for use in diagnostic messages.
 * 
 * Set at the start of the program entrypoint.
 */
static const char *pModule = NULL;

/*
 * The line number of the most recently read Shastina entity through the
 * readEntity() function.
 * 
 * Only valid if in range [1, LONG_MAX - 1].  Else, the line number is
 * not available.
 */
static long m_line = 0;

/*
 * The mapping of metacommand names to indices in the m_meta array.
 * 
 * NULL until the first metacommand is declared.
 */
static RFDICT *m_meta_dict = NULL;

/*
 * The metacommand structure array.
 * 
 * m_meta_count stores the number of metacommands currently in the
 * array.  If zero, the array hasn't been initialized yet.
 * 
 * m_meta is an array of pointers to the metacommand structures.  The
 * structures are in the (arbitrary) order that the metacommands were
 * declared in.  They are not sorted according to metacommand name in
 * any way.
 */
static int m_meta_count = 0;
static META_CMD *m_meta[MAX_META_CMD];

/*
 * The configuration variable table, along with a flag indicating
 * whether it has been initialized yet.
 */
static int m_cfg_init = 0;
static CONFIG_VAR m_cfg[CFG_MAX_INDEX + 1];

/*
 * Local functions
 * ---------------
 */

/* Prototypes */
static int swapSlashes(void);

static void raiseErr(int sourceLine);
static int32_t parseUnsigned(const char *pstr);

static void cfg_init(void);
static void cfg_setInt(int i, int32_t val);
static void cfg_setStr(int i, const char *pstr);

static void meta_decl_z(
    const char * pName,
          int    can_continue);

static void meta_decl_uint(
    const char    * pName,
          int       can_continue,
          int32_t   min_val,
          int32_t   max_val,
          int32_t   default_value);

static void meta_decl_dim(
    const char    * pName,
          int       can_continue,
          int32_t   min_val,
          int32_t   max_val,
          int32_t   max_product);          

static void meta_decl_frame(
    const char * pName,
    const char * pRedirect);

static void meta_declare(void);
static void meta_finish(void);

static void readSignature(SNPARSER *pp, SNSOURCE *pSrc);
static int readMeta(SNPARSER *pp, SNSOURCE *pSrc);

static int32_t configInt(const char *pMetaName);
static int32_t configDim(const char *pMetaName, int i);
static void configVars(const char *pOutPath);

/*
 * Return non-zero if forward slashes should be changed to backslashes
 * in file path strings.
 * 
 * Return:
 * 
 *   non-zero if forward slashes should be changed to backslashes
 */
static int swapSlashes(void) {
#ifdef LILAC_WIN32_PLATFORM
  return 1;
#else
  return 0;
#endif
}

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
 * Parse a given string as an unsigned decimal integer.
 * 
 * If there is a parsing error, an error message is written and -1 is
 * returned.
 * 
 * Parameters:
 * 
 *   pstr - the string to parse
 * 
 * Return:
 * 
 *   the parsed integer value >= 0, or -1 if there was a parsing error
 */
static int32_t parseUnsigned(const char *pstr) {
  
  int32_t result = 0;
  int     d      = 0;
  
  /* Check parameters */
  if (pstr == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Check that first character is decimal digit */
  if (!((*pstr >= '0') && (*pstr <= '9'))) {
    fprintf(stderr, "%s: Invalid unsigned integer!\n", getModule());
    return -1;
  }
  
  /* Parse the integer */
  for( ; *pstr != 0; pstr++) {
    /* Get current decimal digit */
    if ((*pstr >= '0') && (*pstr <= '9')) {
      d = (int) (*pstr - '0');
    } else {
      fprintf(stderr, "%s: Invalid unsigned integer!\n", getModule());
      return -1;
    }
    
    /* Multiply result by 10, watching for overflow */
    if (result <= INT32_MAX / 10) {
      result *= 10;
    } else {
      fprintf(stderr, "%s: Unsigned integer out of range!\n",
              getModule());
      return -1;
    }
    
    /* Add digit, watching for overflow */
    if (result <= INT32_MAX - d) {
      result += d;
    } else {
      fprintf(stderr, "%s: Unsigned integer out of range!\n",
              getModule());
      return -1;
    }
  }
  
  /* Return the result */
  return result;
}

/*
 * Initialize m_cfg if not yet initialized.
 * 
 * All configuration variables are initialized to CVAR_UNDEFINED.
 */
static void cfg_init(void) {
  int i = 0;
  
  /* Only proceed if not yet initialized */
  if (!m_cfg_init) {
    memset(m_cfg, 0, (size_t) (CFG_MAX_INDEX + 1) * sizeof(CONFIG_VAR));
    for(i = 0; i <= CFG_MAX_INDEX; i++) {
      (m_cfg[i]).cvtype = CVAR_UNDEFINED;
    }
    m_cfg_init = 1;
  }
}

/*
 * Set a configuration variable to an integer value.
 * 
 * Initializes configuration array if necessary.  i must be in the range
 * [0, CFG_MAX_INDEX] and it must refer to a structure that currently
 * has type CVAR_UNDEFINED.
 * 
 * Parameters:
 * 
 *   i - the configuration constant index
 * 
 *   val - the integer value to store for the configuration value
 */
static void cfg_setInt(int i, int32_t val) {
  
  /* Check parameters */
  if ((i < 0) || (i > CFG_MAX_INDEX)) {
    raiseErr(__LINE__);
  }
  
  /* Initialize if necessary and check that given index is currently
   * undefined */
  cfg_init();
  if ((m_cfg[i]).cvtype != CVAR_UNDEFINED) {
    raiseErr(__LINE__);
  }
  
  /* Store the value */
  (m_cfg[i]).cvtype = CVAR_INTEGER;
  (m_cfg[i]).ival = val;
}

/*
 * Set a configuration variable to a string value.
 * 
 * Initializes configuration array if necessary.  i must be in the range
 * [0, CFG_MAX_INDEX] and it must refer to a structure that currently
 * has type CVAR_UNDEFINED.
 * 
 * A dynamic copy is made of the passed string.
 * 
 * Parameters:
 * 
 *   i - the configuration constant index
 * 
 *   pstr - the string value to store for the configuration value
 */
static void cfg_setStr(int i, const char *pstr) {
  
  size_t slen = 0;
  char *pcopy = NULL;
  
  /* Check parameters */
  if ((i < 0) || (i > CFG_MAX_INDEX)) {
    raiseErr(__LINE__);
  }
  if (pstr == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Initialize if necessary and check that given index is currently
   * undefined */
  cfg_init();
  if ((m_cfg[i]).cvtype != CVAR_UNDEFINED) {
    raiseErr(__LINE__);
  }
  
  /* Make a copy of the string */
  slen = strlen(pstr) + 1;
  pcopy = (char *) malloc(slen);
  if (pcopy == NULL) {
    fprintf(stderr, "%s: Out of memory!\n", getModule());
    raiseErr(__LINE__);
  }
  memcpy(pcopy, pstr, slen);
  
  /* Store the value */
  (m_cfg[i]).cvtype = CVAR_STRING;
  (m_cfg[i]).sval = pcopy;
}

/*
 * Declare a header metacommand that takes no arguments.
 * 
 * Parameters:
 * 
 *   pName - the name of the header metacommand
 * 
 *   can_continue - non-zero if additional metacommands can occur after
 *   this one
 */
static void meta_decl_z(
    const char * pName,
          int    can_continue) {
  
  META_CMD *pCmd = NULL;
  
  /* Check parameters */
  if (pName == NULL) {
    raiseErr(__LINE__);
  }
  
  if (can_continue) {
    can_continue = 1;
  } else {
    can_continue = 0;
  }
  
  /* If this is first declaration, initialize data */
  if (m_meta_count < 1) {
    m_meta_count = 0;
    m_meta_dict = rfdict_alloc(1);
    memset(m_meta, 0, ((size_t) MAX_META_CMD) * sizeof(META_CMD *));
  }
  
  /* Check that we still have space to make a metacommand declaration */
  if (m_meta_count >= MAX_META_CMD) {
    /* Too many metacommands */
    raiseErr(__LINE__);
  }
  
  /* Allocate metacommand structure */
  pCmd = (META_CMD *) calloc(1, sizeof(META_CMD));
  if (pCmd == NULL) {
    fprintf(stderr, "%s: Out of memory!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Fill in the structure */
  pCmd->pName = pName;
  pCmd->can_continue = can_continue;
  pCmd->atype = ARG_NONE;
  pCmd->cmd_present = 0;
  
  /* Add mapping for current metacommand name */
  if (!rfdict_insert(m_meta_dict, pName, (long) m_meta_count)) {
    /* Metacommand already declared */
    raiseErr(__LINE__);
  }
  
  /* Add structure into array */
  m_meta[m_meta_count] = pCmd;
  m_meta_count++;
}

/*
 * Declare a header metacommand that takes an unsigned integer argument.
 * 
 * Parameters:
 * 
 *   pName - the name of the header metacommand
 * 
 *   can_continue - non-zero if additional metacommands can occur after
 *   this one
 * 
 *   min_val - the minimum value for this meta value
 * 
 *   max_val - the maximum value for this meta value
 * 
 *   default_value - the default value for this meta value, or -1 if
 *   there is no default
 */
static void meta_decl_uint(
    const char    * pName,
          int       can_continue,
          int32_t   min_val,
          int32_t   max_val,
          int32_t   default_value) {
  
  META_CMD *pCmd = NULL;
  
  /* Check parameters */
  if (pName == NULL) {
    raiseErr(__LINE__);
  }
  
  if (can_continue) {
    can_continue = 1;
  } else {
    can_continue = 0;
  }
  
  if (min_val < 0) {
    raiseErr(__LINE__);
  }
  
  if (max_val < min_val) {
    raiseErr(__LINE__);
  }
  
  if (default_value != -1) {
    if (!((default_value >= min_val) && (default_value <= max_val))) {
      raiseErr(__LINE__);
    }
  }
  
  /* If this is first declaration, initialize data */
  if (m_meta_count < 1) {
    m_meta_count = 0;
    m_meta_dict = rfdict_alloc(1);
    memset(m_meta, 0, ((size_t) MAX_META_CMD) * sizeof(META_CMD *));
  }
  
  /* Check that we still have space to make a metacommand declaration */
  if (m_meta_count >= MAX_META_CMD) {
    /* Too many metacommands */
    raiseErr(__LINE__);
  }
  
  /* Allocate metacommand structure */
  pCmd = (META_CMD *) calloc(1, sizeof(META_CMD));
  if (pCmd == NULL) {
    fprintf(stderr, "%s: Out of memory!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Fill in the structure */
  pCmd->pName = pName;
  pCmd->can_continue = can_continue;
  pCmd->atype = ARG_UINT;
  pCmd->min_val = min_val;
  pCmd->max_val = max_val;
  pCmd->cmd_present = 0;
  pCmd->default_value = default_value;
  
  /* Add mapping for current metacommand name */
  if (!rfdict_insert(m_meta_dict, pName, (long) m_meta_count)) {
    /* Metacommand already declared */
    raiseErr(__LINE__);
  }
  
  /* Add structure into array */
  m_meta[m_meta_count] = pCmd;
  m_meta_count++;
}

/*
 * Declare a header metacommand that takes two unsigned integer
 * arguments.
 * 
 * Parameters:
 * 
 *   pName - the name of the header metacommand
 * 
 *   can_continue - non-zero if additional metacommands can occur after
 *   this one
 * 
 *   min_val - the minimum value for each meta value
 * 
 *   max_val - the maximum value for each meta value
 * 
 *   max_product - the maximum product of the two meta values
 */
static void meta_decl_dim(
    const char    * pName,
          int       can_continue,
          int32_t   min_val,
          int32_t   max_val,
          int32_t   max_product) {
  
  META_CMD *pCmd = NULL;
  
  /* Check parameters */
  if (pName == NULL) {
    raiseErr(__LINE__);
  }
  
  if (can_continue) {
    can_continue = 1;
  } else {
    can_continue = 0;
  }
  
  if (min_val < 0) {
    raiseErr(__LINE__);
  }
  
  if (max_val < min_val) {
    raiseErr(__LINE__);
  }
  
  if (max_product < min_val) {
    raiseErr(__LINE__);
  }
  
  /* If this is first declaration, initialize data */
  if (m_meta_count < 1) {
    m_meta_count = 0;
    m_meta_dict = rfdict_alloc(1);
    memset(m_meta, 0, ((size_t) MAX_META_CMD) * sizeof(META_CMD *));
  }
  
  /* Check that we still have space to make a metacommand declaration */
  if (m_meta_count >= MAX_META_CMD) {
    /* Too many metacommands */
    raiseErr(__LINE__);
  }
  
  /* Allocate metacommand structure */
  pCmd = (META_CMD *) calloc(1, sizeof(META_CMD));
  if (pCmd == NULL) {
    fprintf(stderr, "%s: Out of memory!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Fill in the structure */
  pCmd->pName = pName;
  pCmd->can_continue = can_continue;
  pCmd->atype = ARG_DIM;
  pCmd->min_val = min_val;
  pCmd->max_val = max_val;
  pCmd->max_product = max_product;
  pCmd->cmd_present = 0;
  
  /* Add mapping for current metacommand name */
  if (!rfdict_insert(m_meta_dict, pName, (long) m_meta_count)) {
    /* Metacommand already declared */
    raiseErr(__LINE__);
  }
  
  /* Add structure into array */
  m_meta[m_meta_count] = pCmd;
  m_meta_count++;
}

/*
 * Declare a header metacommand that reads dimensions from an image file
 * and then redirects to another header metacommand.
 * 
 * The redirected metacommand must already be declared and it must have
 * argument type ARG_DIM.
 * 
 * Parameters:
 * 
 *   pName - the name of the header metacommand
 * 
 *   pRedirect - the name of the metacommand this metacommand redirects
 *   to
 */
static void meta_decl_frame(
    const char * pName,
    const char * pRedirect) {
  
  META_CMD *pCmd = NULL;
  long ri = 0;
  
  /* Check parameters */
  if (pName == NULL) {
    raiseErr(__LINE__);
  }
  
  if (pRedirect == NULL) {
    raiseErr(__LINE__);
  }
  
  /* If this is first declaration, initialize data */
  if (m_meta_count < 1) {
    m_meta_count = 0;
    m_meta_dict = rfdict_alloc(1);
    memset(m_meta, 0, ((size_t) MAX_META_CMD) * sizeof(META_CMD *));
  }
  
  /* Check that we still have space to make a metacommand declaration */
  if (m_meta_count >= MAX_META_CMD) {
    /* Too many metacommands */
    raiseErr(__LINE__);
  }
  
  /* Check that redirect metacommand is declared and has ARG_DIM type */
  ri = rfdict_get(m_meta_dict, pRedirect, -1);
  if (ri < 0) {
    /* Redirect metacommand not declared yet */
    raiseErr(__LINE__);
  }
  if ((m_meta[ri])->atype != ARG_DIM) {
    /* Redirect must go to ARG_DIM metacommand */
    raiseErr(__LINE__);
  }
  
  /* Allocate metacommand structure */
  pCmd = (META_CMD *) calloc(1, sizeof(META_CMD));
  if (pCmd == NULL) {
    fprintf(stderr, "%s: Out of memory!\n", getModule());
    raiseErr(__LINE__);
  }
  
  /* Fill in the structure */
  pCmd->pName = pName;
  pCmd->atype = ARG_FRAME;
  pCmd->cmd_present = 0;
  pCmd->pRedirect = pRedirect;
  
  /* Add mapping for current metacommand name */
  if (!rfdict_insert(m_meta_dict, pName, (long) m_meta_count)) {
    /* Metacommand already declared */
    raiseErr(__LINE__);
  }
  
  /* Add structure into array */
  m_meta[m_meta_count] = pCmd;
  m_meta_count++;
}

/*
 * Declare all of the header metacommands.
 */
static void meta_declare(void) {
  
  meta_decl_z("body", 0);
  
  meta_decl_dim("dim", 1, 1, 16384, INT32_C(16777216));
  meta_decl_frame("frame", "dim");
  
  meta_decl_uint("external-disk-mib", 1, 1, 1024, 256);
  meta_decl_uint("external-ram-kib", 1, 1, INT32_C(1048576), 64);
  meta_decl_uint("graph-depth", 1, 1, 16384, 32);
  meta_decl_uint("stack-height", 1, 1, 16384, 64);
  meta_decl_uint("name-limit", 1, 0, 16384, 1024);
}

/*
 * Go through all declared header metacommands, but ignore ARG_FRAME
 * types which are redirected.  For metacommands that do not have their
 * cmd_present flag set:  if it is an ARG_UINT metacommand with a
 * default value, set cmd_present and use the default value; all other
 * cases result in an error.
 * 
 * If this returns successfully, all non-ARG_FRAME metacommands will
 * have cmd_present set.
 */
static void meta_finish(void) {
  int i = 0;
  META_CMD *pmc = NULL;
  
  /* Go through all metacommand structures */
  for(i = 0; i < m_meta_count; i++) {
    
    /* Get the structure */
    pmc = m_meta[i];
    
    /* Ignore ARG_FRAME metacommands, which are redirected */
    if (pmc->atype == ARG_FRAME) {
      continue;
    }
    
    /* Check whether metacommand was declared */
    if (!(pmc->cmd_present)) {
      /* Metacommand not declared -- check whether a default */
      if ((pmc->atype == ARG_UINT) && (pmc->default_value >= 0)) {
        /* Use the default value */
        pmc->cmd_present = 1;
        (pmc->vals)[0] = pmc->default_value;
        (pmc->vals)[1] = 0;
        
      } else {
        /* No default value, so error */
        fprintf(stderr,
          "%s: Metacommand '%s' was not declared and has no default!\n",
          getModule(), pmc->pName);
        raiseErr(__LINE__);
      }
    }
  }
}

/*
 * Read the signature line at the start of a rendering script and verify
 * the version.
 * 
 * Parameters:
 * 
 *   pp - the Shastina parser
 * 
 *   pSrc - the Shastina source
 */
static void readSignature(SNPARSER *pp, SNSOURCE *pSrc) {
  
  SNENTITY ent;
  const char *pVer = NULL;
  const char *pstr = NULL;
  int32_t major_ver = 0;
  int32_t minor_ver = 0;
  int32_t v         = 0;
  int i = 0;
  int d = 0;
  
  /* Initialize structures */
  memset(&ent, 0, sizeof(SNENTITY));
  
  /* Check parameters */
  if ((pp == NULL) || (pSrc == NULL)) {
    raiseErr(__LINE__);
  }
  
  /* Signature begins with BEGIN_META */
  readEntity(pp, &ent, pSrc);
  if (ent.status != SNENTITY_BEGIN_META) {
    fprintf(stderr, "%s: Failed to read Lilac signature!\n",
            getModule());
    reportLine();
    raiseErr(__LINE__);
  }
  
  /* Next comes the "lilac" token */
  readEntity(pp, &ent, pSrc);
  if (ent.status != SNENTITY_META_TOKEN) {
    fprintf(stderr, "%s: Failed to read Lilac signature!\n",
            getModule());
    reportLine();
    raiseErr(__LINE__);
  }
  if (strcmp(ent.pKey, "lilac") != 0) {
    fprintf(stderr, "%s: Failed to read Lilac signature!\n",
            getModule());
    reportLine();
    raiseErr(__LINE__);
  }
  
  /* Now the version number */
  readEntity(pp, &ent, pSrc);
  if (ent.status != SNENTITY_META_TOKEN) {
    fprintf(stderr, "%s: Lilac signature missing version!\n",
            getModule());
    reportLine();
    raiseErr(__LINE__);
  }
  pVer = ent.pKey;
  
  /* Parse major and minor version numbers */
  pstr = pVer;
  for(i = 0; i < 2; i++) {
  
    /* Check that first character is a decimal digit */
    if (!((*pstr >= '0') && (*pstr <= '9'))) {
      fprintf(stderr, "%s: Lilac signature has invalid version!\n",
              getModule());
      reportLine();
      raiseErr(__LINE__);
    }
    
    /* If first character is a zero, check that second character is not
     * a decimal digit */
    if (*pstr == '0') {
      if ((pstr[1] >= '0') && (pstr[1] <= '9')) {
        fprintf(stderr, "%s: Lilac signature has invalid version!\n",
                getModule());
        reportLine();
        raiseErr(__LINE__);
      }
    }
    
    /* Parse the version number */
    v = 0;
    for( ; (*pstr >= '0') && (*pstr <= '9'); pstr++) {
      /* Multiply version number by 10, watching for overflow */
      if (v <= INT32_MAX / 10) {
        v *= 10;
      } else {
        fprintf(stderr, "%s: Lilac signature has invalid version!\n",
                getModule());
        reportLine();
        raiseErr(__LINE__);
      }
      
      /* Parse digit */
      d = (int) (*pstr - '0');
      
      /* Add digit to version number, watching for overflow */
      if (v <= INT32_MAX - d) {
        v += d;
      } else {
        fprintf(stderr, "%s: Lilac signature has invalid version!\n",
                getModule());
        reportLine();
        raiseErr(__LINE__);
      }
    }
    
    /* Check that we stopped on the appropriate character; for all but
     * the last, advance to next */
    if (i == 0) {
      if (*pstr != '.') {
        fprintf(stderr, "%s: Lilac signature has invalid version!\n",
                getModule());
        reportLine();
        raiseErr(__LINE__);
      }
      pstr++;
      
    } else if (i == 1) {
      if (*pstr != 0) {
        fprintf(stderr, "%s: Lilac signature has invalid version!\n",
                getModule());
        reportLine();
        raiseErr(__LINE__);
      }
    
    } else {
      raiseErr(__LINE__);
    }
    
    /* Store the appropriate version number */
    if (i == 0) {
      major_ver = v;
    } else if (i == 1) {
      minor_ver = v;
    } else {
      raiseErr(__LINE__);
    }
  }
  
  /* Check version */
  if (major_ver != 1) {
    fprintf(stderr,
        "%s: Lilac signature has unsupported major version!\n",
        getModule());
    reportLine();
    raiseErr(__LINE__);
  }
  if (minor_ver > 0) {
    fprintf(stderr,
        "%s: WARNING: Lilac signature has unsupported minor version!\n",
        getModule());
    /* Don't stop in this case but just warn */
  }
  
  /* Signature ends with END_META */
  readEntity(pp, &ent, pSrc);
  if (ent.status != SNENTITY_END_META) {
    fprintf(stderr, "%s: Unrecognized content in Lilac signature!\n",
            getModule());
    reportLine();
    raiseErr(__LINE__);
  }
}

/*
 * Read and interpret a metacommand from the script header.
 * 
 * Parameters:
 * 
 *   pp - the Shastina parser
 * 
 *   pSrc - the Shastina source 
 *
 * Return:
 * 
 *   non-zero if metacommand was something other than %body; and more
 *   metacommands should be read; zero if %body; was just read and
 *   header has now concluded
 */
static int readMeta(SNPARSER *pp, SNSOURCE *pSrc) {
  
  SNENTITY ent;
  
  const char *pName = NULL;
  META_CMD *pmc = NULL;
  const char *pc = NULL;
  char *pcc = NULL;
  
  long ri = 0;
  int32_t ia = 0;
  int32_t ib = 0;
  size_t slen = 0;
  
  int errnum = 0;
  char *pPath = NULL;
  SPH_IMAGE_READER *pr = NULL;
  
  /* Initialize structures */
  memset(&ent, 0, sizeof(SNENTITY));
  
  /* Check parameters */
  if ((pp == NULL) || (pSrc == NULL)) {
    raiseErr(__LINE__);
  }
  
  /* Initialize metacommand map if necessary */
  if (m_meta_dict == NULL) {
    m_meta_dict = rfdict_alloc(1);
  }
  
  /* Metacommand begins with BEGIN_META */
  readEntity(pp, &ent, pSrc);
  if (ent.status != SNENTITY_BEGIN_META) {
    fprintf(stderr, "%s: Expecting header metacommand!\n",
            getModule());
    reportLine();
    raiseErr(__LINE__);
  }
  
  /* Next comes the metacommand name */
  readEntity(pp, &ent, pSrc);
  if (ent.status != SNENTITY_META_TOKEN) {
    fprintf(stderr, "%s: Invalid header metacommand!\n",
            getModule());
    raiseErr(__LINE__);
  }
  pName = ent.pKey;
  
  /* Look up the metacommand data structure */
  ri = rfdict_get(m_meta_dict, pName, -1);
  if (ri < 0) {
    fprintf(stderr, "%s: Unrecognized header metacommand '%s'!\n",
            getModule(), pName);
    reportLine();
    raiseErr(__LINE__);
  }
  pmc = m_meta[ri];
  
  /* Check that metacommand hasn't been used yet and set flag */
  if (pmc->cmd_present) {
    fprintf(stderr, "%s: Metacommand '%s' used more than once!\n",
            getModule(), pName);
    reportLine();
    raiseErr(__LINE__);
    
  } else {
    pmc->cmd_present = 1;
  }
  
  /* Get parameters based on metacommand arguments */
  if (pmc->atype == ARG_DIM) { /* =================================== */
    /* Two unsigned integer parameters */
    readEntity(pp, &ent, pSrc);
    if (ent.status != SNENTITY_META_TOKEN) {
      fprintf(stderr, "%s: Expecting metacommand integer parameters!\n",
              getModule());
      raiseErr(__LINE__);
    }
    
    ia = parseUnsigned(ent.pKey);
    if (ia < 0) {
      fprintf(stderr, "%s: Expecting metacommand integer parameters!\n",
              getModule());
      raiseErr(__LINE__);
    }
    
    readEntity(pp, &ent, pSrc);
    if (ent.status != SNENTITY_META_TOKEN) {
      fprintf(stderr, "%s: Expecting metacommand integer parameters!\n",
              getModule());
      raiseErr(__LINE__);
    }
    
    ib = parseUnsigned(ent.pKey);
    if (ib < 0) {
      fprintf(stderr, "%s: Expecting metacommand integer parameters!\n",
              getModule());
      raiseErr(__LINE__);
    }
    
  } else if (pmc->atype == ARG_FRAME) { /* ========================== */
    /* String argument -- check it */
    readEntity(pp, &ent, pSrc);
    if (ent.status != SNENTITY_META_STRING) {
      fprintf(stderr, "%s: Expecting metacommand string parameter!\n",
              getModule());
      raiseErr(__LINE__);
    }
    
    if ((ent.pKey)[0] != 0) {
      fprintf(stderr, "%s: Metacommand string may not have prefix!\n",
              getModule());
      raiseErr(__LINE__);
    }
    
    if (ent.str_type != SNSTRING_QUOTED) {
      fprintf(stderr, "%s: Metacommand string must be double-quoted!\n",
              getModule());
      raiseErr(__LINE__);
    }
    
    for(pc = ent.pValue; *pc != 0; pc++) {
      if (!((*pc >= 0x20) && (*pc <= 0x7e) &&
              (*pc != '\\') && (*pc != '\"'))) {
        fprintf(stderr,
            "%s: Metacommand string contains invalid characters!\n",
            getModule());
        raiseErr(__LINE__);
      }
    }
    
    /* Get string length excluding terminating nul and check it */
    slen = strlen(ent.pValue);
    if (slen > MAX_STRING_LEN) {
      fprintf(stderr,
            "%s: Metacommand string too long!  Maximum length: %ld.\n",
            getModule(), (long) MAX_STRING_LEN);
      raiseErr(__LINE__);
    }
    
    /* Make a dynamic copy of the string */
    pPath = (char *) malloc((size_t) (slen + 1));
    if (pPath == NULL) {
      fprintf(stderr, "%s: Out of memory!\n", getModule());
      raiseErr(__LINE__);
    }
    memcpy(pPath, ent.pValue, (size_t) (slen + 1));
    
    /* If required, swap slashes to backslashes */
    if (swapSlashes()) {
      for(pcc = pPath; *pcc != 0; pcc++) {
        if (*pcc == '/') {
          *pcc = (char) '\\';
        }
      }
    }
    
  } else if (pmc->atype == ARG_UINT) { /* =========================== */
    /* Single unsigned integer parameter */
    readEntity(pp, &ent, pSrc);
    if (ent.status != SNENTITY_META_TOKEN) {
      fprintf(stderr, "%s: Expecting metacommand integer parameter!\n",
              getModule());
      raiseErr(__LINE__);
    }
    
    ia = parseUnsigned(ent.pKey);
    if (ia < 0) {
      fprintf(stderr, "%s: Expecting metacommand integer parameter!\n",
              getModule());
      raiseErr(__LINE__);
    }
  
  } else { /* ======================================================= */
    /* The only other allowed type is ARG_NONE, which has no
     * parameters */
    if (pmc->atype != ARG_NONE) {
      raiseErr(__LINE__);
    }
  }
  
  /* Metacommand should now finish */
  readEntity(pp, &ent, pSrc);
  if (ent.status != SNENTITY_END_META) {
    fprintf(stderr, "%s: Expecting end of header metacommand!\n",
            getModule());
    reportLine();
    raiseErr(__LINE__);
  }
  
  /* For the case of ARG_FRAME metacommands, get the image dimensions
   * from the named image file and then redirect to an ARG_DIM
   * metacommand */
  if (pmc->atype == ARG_FRAME) {
    /* Open image reader on the file */
    pr = sph_image_reader_newFromPath(pPath, &errnum);
    if (pr == NULL) {
      fprintf(stderr, "%s: Failed to open frame image '%s': %s!\n",
              getModule(), pPath, sph_image_errorString(errnum));
      reportLine();
      raiseErr(__LINE__);
    }
    
    /* Grab the image width and height as if they were the two
     * parameters for ARG_DIM */
    ia = sph_image_reader_width(pr);
    ib = sph_image_reader_height(pr);
    
    /* Redirect command */
    pName = pmc->pRedirect;
    ri = rfdict_get(m_meta_dict, pName, -1);
    if (ri < 0) {
      raiseErr(__LINE__);
    }
    pmc = m_meta[ri];
    
    /* Make sure redirect command has not been used yet, and set flag */
    if (pmc->cmd_present) {
      fprintf(stderr,
        "%s: Metacommand '%s' invoked indirectly when already used!\n",
        getModule(), pName);
      reportLine();
      raiseErr(__LINE__);
      
    } else {
      pmc->cmd_present = 1;
    }
  }
  
  /* Free image reader if allocated */
  sph_image_reader_close(pr);
  pr = NULL;
  
  /* Free path copy if allocated */
  if (pPath != NULL) {
    free(pPath);
    pPath = NULL;
  }
  
  /* Handle the specific metacommand format, except for ARG_FRAME which
   * has just been redirected, and ARG_NONE which has no argument that
   * needs processing */
  if (pmc->atype == ARG_DIM) { /* =================================== */
    /* Check the range of the two parameters */
    if ((ia < pmc->min_val) || (ib < pmc->min_val)) {
      fprintf(stderr,
        "%s: Metacommand '%s' values must be at least %ld!\n",
        getModule(), pName, (long) pmc->min_val);
      reportLine();
      raiseErr(__LINE__);
    }
    
    if ((ia > pmc->max_val) || (ib > pmc->max_val)) {
      fprintf(stderr,
        "%s: Metacommand '%s' values may be at most %ld!\n",
        getModule(), pName, (long) pmc->max_val);
      reportLine();
      raiseErr(__LINE__);
    }
    
    if (((int64_t) ia) * ((int64_t) ib) > pmc->max_product) {
      fprintf(stderr,
        "%s: Metacommand '%s' value product may be at most %ld!\n",
        getModule(), pName, (long) pmc->max_product);
      reportLine();
      raiseErr(__LINE__);
    }
    
    /* Record the values */
    (pmc->vals)[0] = ia;
    (pmc->vals)[1] = ib;
    
  } else if (pmc->atype == ARG_UINT) { /* =========================== */
    /* Check the range of the parameter */
    if (ia < pmc->min_val) {
      fprintf(stderr,
        "%s: Metacommand '%s' value must be at least %ld!\n",
        getModule(), pName, (long) pmc->min_val);
      reportLine();
      raiseErr(__LINE__);
    }
    
    if (ia > pmc->max_val) {
      fprintf(stderr,
        "%s: Metacommand '%s' value may be at most %ld!\n",
        getModule(), pName, (long) pmc->max_val);
      reportLine();
      raiseErr(__LINE__);
    }
    
    /* Record the value */
    (pmc->vals)[0] = ia;
    (pmc->vals)[1] = 0;
    
  }
  
  /* Return result based on setting in structure */
  return pmc->can_continue;
}

/*
 * Retrieve a configuration integer from a metacommand structure of type
 * ARG_UINT.
 * 
 * Parameters:
 * 
 *   pMetaName - the name of the metacommand to query
 * 
 * Return:
 * 
 *   the configuration integer
 */
static int32_t configInt(const char *pMetaName) {
  
  long ri = 0;
  META_CMD *pmc = NULL;
  
  /* Check state */
  if ((m_meta_dict == NULL) || (m_meta_count < 1)) {
    raiseErr(__LINE__);
  }
  
  /* Check parameters */
  if (pMetaName == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Look up the structure */
  ri = rfdict_get(m_meta_dict, pMetaName, -1);
  if (ri < 0) {
    raiseErr(__LINE__);
  }
  pmc = m_meta[ri];
  
  /* Check structure state */
  if (pmc->atype != ARG_UINT) {
    raiseErr(__LINE__);
  }
  if (!(pmc->cmd_present)) {
    raiseErr(__LINE__);
  }
  
  /* Return requested value */
  return (pmc->vals)[0];
}

/*
 * Retrieve a configuration integer from a metacommand structure of type
 * ARG_DIM.
 * 
 * i is either 0 or 1 to choose which dimension to query.
 * 
 * Parameters:
 * 
 *   pMetaName - the name of the metacommand to query
 * 
 *   i - the index of the dimension
 * 
 * Return:
 * 
 *   the configuration integer
 */
static int32_t configDim(const char *pMetaName, int i) {
  
  long ri = 0;
  META_CMD *pmc = NULL;
  
  /* Check state */
  if ((m_meta_dict == NULL) || (m_meta_count < 1)) {
    raiseErr(__LINE__);
  }
  
  /* Check parameters */
  if (pMetaName == NULL) {
    raiseErr(__LINE__);
  }
  if ((i != 0) && (i != 1)) {
    raiseErr(__LINE__);
  }
  
  /* Look up the structure */
  ri = rfdict_get(m_meta_dict, pMetaName, -1);
  if (ri < 0) {
    raiseErr(__LINE__);
  }
  pmc = m_meta[ri];
  
  /* Check structure state */
  if (pmc->atype != ARG_DIM) {
    raiseErr(__LINE__);
  }
  if (!(pmc->cmd_present)) {
    raiseErr(__LINE__);
  }
  
  /* Return requested value */
  return (pmc->vals)[i];
}

/*
 * Initialize the configuration variables array.
 * 
 * The m_meta and m_meta_dict must be initialized and all default values
 * should be set if necessary using meta_finish() before calling this
 * function.
 * 
 * Parameters:
 * 
 *   pOutPath - the path to the image file to render, which will be
 *   stored in the appropriate configuration variable
 */
static void configVars(const char *pOutPath) {
  
  /* Check parameters */
  if (pOutPath == NULL) {
    raiseErr(__LINE__);
  }
  
  /* Set the configuration variables */
#ifdef LILAC_WIN32_PLATFORM
  cfg_setInt(CFG_WIN32, 1);
#else
  cfg_setInt(CFG_WIN32, 0);
#endif

  cfg_setInt(CFG_BACKSLASH, swapSlashes());
  
  cfg_setInt(CFG_DIM_WIDTH , configDim("dim", 0));
  cfg_setInt(CFG_DIM_HEIGHT, configDim("dim", 1));
  
  cfg_setInt(CFG_EXTERNAL_DISK_MIB, configInt("external-disk-mib"));
  cfg_setInt(CFG_EXTERNAL_RAM_KIB , configInt("external-ram-kib"));
  cfg_setInt(CFG_GRAPH_DEPTH      , configInt("graph-depth"));
  cfg_setInt(CFG_STACK_HEIGHT     , configInt("stack-height"));
  cfg_setInt(CFG_NAME_LIMIT       , configInt("name-limit"));
  
  cfg_setStr(CFG_OUT_PATH, pOutPath);
}

/*
 * Public functions
 * ----------------
 * 
 * See header for specifications.
 */

/*
 * getModule function.
 */
const char *getModule(void) {
  const char *pResult = NULL;
  pResult = pModule;
  if (pResult == NULL) {
    pResult = "lilac";
  }
  return pResult;
}

/*
 * raiseErrGlobal function.
 */
void raiseErrGlobal(int sourceLine, const char *sourceFile) {
  if (sourceFile == NULL) {
    sourceFile = "unknown";
  }
  fprintf(stderr, "%s: Stopped on error in %s at line %d!\n",
          getModule(), sourceFile, sourceLine);
  exit(1);
}

/*
 * reportLine function.
 */
void reportLine(void) {
  if ((m_line >= 1) && (m_line < LONG_MAX)) {
    fprintf(stderr, "%s: [Script file line %ld]\n",
            getModule(), m_line);
  }
}

/*
 * readEntity function.
 */
void readEntity(SNPARSER *pp, SNENTITY *pEnt, SNSOURCE *pSrc) {
  
  /* Check parameters */
  if ((pp == NULL) || (pEnt == NULL) || (pSrc == NULL)) {
    raiseErr(__LINE__);
  }
  
  /* Call through */
  snparser_read(pp, pEnt, pSrc);
  
  /* Update line number */
  m_line = snparser_count(pp);
  
  /* Check for parsing error */
  if (pEnt->status < 0) {
    fprintf(stderr, "%s: Shastina parsing error: %s!\n",
            getModule(), snerror_str(pEnt->status));
    reportLine();
    raiseErr(__LINE__);
  }
}

/*
 * getConfigInt function.
 */
int32_t getConfigInt(int cfg) {
  
  CONFIG_VAR *pcv = NULL;
  
  /* Check state */
  if (!m_cfg_init) {
    raiseErr(__LINE__);
  }
  
  /* Check parameters */
  if ((cfg < 0) || (cfg > CFG_MAX_INDEX)) {
    raiseErr(__LINE__);
  }
  
  /* Get structure and check proper state */
  pcv = &(m_cfg[cfg]);
  if (pcv->cvtype != CVAR_INTEGER) {
    raiseErr(__LINE__);
  }
  
  /* Return value */
  return pcv->ival;
}

/*
 * getConfigStr function.
 */
const char *getConfigStr(int cfg) {
  
  CONFIG_VAR *pcv = NULL;
  
  /* Check state */
  if (!m_cfg_init) {
    raiseErr(__LINE__);
  }
  
  /* Check parameters */
  if ((cfg < 0) || (cfg > CFG_MAX_INDEX)) {
    raiseErr(__LINE__);
  }
  
  /* Get structure and check proper state */
  pcv = &(m_cfg[cfg]);
  if (pcv->cvtype != CVAR_STRING) {
    raiseErr(__LINE__);
  }
  
  /* Return value */
  return pcv->sval;
}

/*
 * Program entrypoint
 * ------------------
 */

int main(int argc, char *argv[]) {
  
  int i = 0;
  
  const char *pOutPath = NULL;
  SNPARSER *pr  = NULL;
  SNSOURCE *pIn = NULL;
  NODE *pRoot = NULL;
  
  /* Set module name */
  pModule = NULL;
  if ((argc > 0) && (argv != NULL)) {
    pModule = argv[0];
  }
  if (pModule == NULL) {
    pModule = "lilac";
  }
  
  /* Check that arguments are all present */
  if (argc > 0) {
    if (argv == NULL) {
      raiseErr(__LINE__);
    }
    for(i = 0; i < argc; i++) {
      if (argv[i] == NULL) {
        raiseErr(__LINE__);
      }
    }
  }
  
  /* If no arguments passed, print a syntax summary and return error
   * status */
  if (argc <= 1) {
    fprintf(stderr, "Lilac 2D renderer\n");
    fprintf(stderr, "Syntax:\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  lilac [out] < [script]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "[out] is the path to the PNG file to create\n");
    fprintf(stderr, "[script] is Lilac rendering script\n");
    fprintf(stderr, "\n");
    return 1;
  }
  
  /* Initialize plug-in system */
  plugin_init();
  
  /* Get output path */
  pOutPath = argv[1];
  
  /* Set up Shastina source on standard input and allocate a parser */
  pIn = snsource_stream(stdin, SNSTREAM_NORMAL);
  pr  = snparser_alloc();
  
  /* Declare header metacommands */
  meta_declare();
  
  /* Parse and interpret the signature and the header metacommands */
  readSignature(pr, pIn);
  while (readMeta(pr, pIn)) { }
  
  /* Finish setting up the header metacommands with defaults and then
   * set all the configuration variables */
  meta_finish();
  configVars(pOutPath);
  
  /* Run the rest of the render script and get the root node */
  pRoot = vm_run(pr, pIn);
  
  /* Release Shastina parser and source */
  snparser_free(pr);
  snsource_free(pIn);
  pr  = NULL;
  pIn = NULL;
  
  /* Render to output */
  render_go(pRoot);
  
  /* If we got here, return successfully */
  return 0;
}
