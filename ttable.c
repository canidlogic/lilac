/*
 * ttable.c
 * 
 * Implementation of ttable.h
 * 
 * See the header for further information.
 */

#include "ttable.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "texture.h"

/*
 * Constants
 * =========
 */

/*
 * The maximum number of records in the table.
 */
#define MAX_RECORDS (1024)

/*
 * The maximum line length for text files, including termination byte.
 */
#define IN_MAXLINE (256)

/*
 * ASCII codes.
 */
#define ASCII_HT      (0x9)
#define ASCII_LF      (0xa)
#define ASCII_CR      (0xd)
#define ASCII_SP      (0x20)
#define ASCII_AMP     (0x23)    /* # */
#define ASCII_ZERO    (0x30)    /* 0 */
#define ASCII_NINE    (0x39)    /* 9 */
#define ASCII_UPPER_A (0x41)    /* A */
#define ASCII_UPPER_F (0x46)    /* F */
#define ASCII_LOWER_A (0x61)    /* a */
#define ASCII_LOWER_F (0x66)    /* f */

/*
 * Table
 * =====
 */

static int m_table_count = 0;
static SHADEREC m_table[MAX_RECORDS];

/*
 * Local functions
 * ===============
 */

/* Function prototypes */
static void initTable(void);
static void shiftRecs(int start);
static int addRecord(
    int32_t   rgb_index,
    int       tex_index,
    int       shade_rate,
    int       draw_rate,
    int32_t   rgb_tint,
    int     * pError);

static int readchar(FILE *pf, int *pChar);
static int is_blank(char *pstr);
static char *skipSpace(char *pstr, int optional);
static char *readRGB(char *pstr, int32_t *pRGB);
static char *readInt(char *pstr, int *pv);
static int parseLine(char *pstr, int *pError);

/*
 * Initialize the shading table, if necessary.
 * 
 * This function clears the table to zero if the count is zero.
 */
static void initTable(void) {
  if (m_table_count == 0) {
    memset(m_table, 0, MAX_RECORDS * sizeof(SHADEREC));
  }
}

/*
 * Shift records one to the right starting at a given index.
 * 
 * start must be a valid index in the table or a fault occurs.
 * 
 * If start is the last record in the table, the last record is blanked
 * to zero.
 * 
 * Else, records from the second-to-last record in the table down to
 * start are shifted one to the right.  The last record is overwritten
 * in this process.  Finally, the record at start is blanked to zero.
 * 
 * Parameters:
 * 
 *   start - the index to start shifting records
 */
static void shiftRecs(int start) {
  
  int i = 0;
  
  /* Check parameters */
  if ((start < 0) || (start >= m_table_count)) {
    abort();
  }
  
  /* Shift records */
  for(i = m_table_count - 2; i >= start; i--) {
    memcpy(&(m_table[i + 1]), &(m_table[i]), sizeof(SHADEREC));
  }
  
  /* Blank record at start */
  memset(&(m_table[start]), 0, sizeof(SHADEREC));
}

/*
 * Add a record to the shading table.
 * 
 * rgb_index is the RGB index value.  A fault occurs if out of range.
 * An error occurs if that RGB index is already in the table.
 * 
 * tex_index is the texture index.  A fault occurs if it is less than
 * one.  An error occurs if it does not correspond to a registered
 * texture in the texture module.
 * 
 * shade_rate is the shading rate.  A fault occurs if it is not in the
 * range [0, 255].
 * 
 * draw_rate is the drawing rate.  A fault occurs if it is not in the
 * range [0, 255].
 * 
 * rgb_tint is the RGB tint.  A fault occurs if out of range.
 * 
 * pError points to the variable to receive the error code in case of
 * error.  It may not be NULL.
 * 
 * Parameters:
 * 
 *   rgb_index - the RGB index of the record
 * 
 *   tex_index - the texture index
 * 
 *   shade_rate - the shading rate
 * 
 *   draw_rate - the drawing rate
 * 
 *   rgb_tint - the RGB tint
 * 
 *   pError - pointer to the error code variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int addRecord(
    int32_t   rgb_index,
    int       tex_index,
    int       shade_rate,
    int       draw_rate,
    int32_t   rgb_tint,
    int     * pError) {
  
  int status = 1;
  int lbound = 0;
  int ubound = 0;
  int mid = 0;
  SHADEREC *psr = NULL;
  
  /* Check parameters */
  if ((rgb_index < 0) || (rgb_index > INT32_C(0xffffff))) {
    abort();
  }
  if (tex_index < 1) {
    abort();
  }
  if ((shade_rate < 0) || (shade_rate > 255)) {
    abort();
  }
  if ((draw_rate < 0) || (draw_rate > 255)) {
    abort();
  }
  if ((rgb_tint < -1) || (rgb_tint > INT32_C(0xffffff))) {
    abort();
  }
  if (pError == NULL) {
    abort();
  }
  
  /* Verify that tex_index is valid with texture module */
  if (tex_index > texture_count()) {
    *pError = TTABLE_ERR_TEX;
    status = 0;
  }
  
  /* Initialize shading table if necessary */
  if (status) {
    initTable();
  }
  
  /* Fail if table is full */
  if (status && (m_table_count >= MAX_RECORDS)) {
    *pError = TTABLE_ERR_RECS;
    status = 0;
  }
  
  /* Check cases */
  if (status) {
    if (m_table_count < 1) {
      /* Table is empty -- insert at start */
      m_table_count = 1;
      psr = &(m_table[0]);
    
    } else if ((m_table[0]).rgbidx > rgb_index) {
      /* Insert record at start of table */
      m_table_count++;
      shiftRecs(0);
      psr = &(m_table[0]);
  
    } else if ((m_table[m_table_count - 1]).rgbidx < rgb_index) {
      /* Insert record at end of table */
      m_table_count++;
      psr = &(m_table[m_table_count - 1]);
    
    } else if (((m_table[0]).rgbidx == rgb_index) ||
               ((m_table[m_table_count - 1]).rgbidx == rgb_index)) {
      /* New record is duplicate of a boundary index -- fail */
      *pError = TTABLE_ERR_DUP;
      status = 0;
      
    } else {
      /* Insert record somewhere in middle */
      assert(m_table_count > 1);
      
      /* Set search boundaries to full table */
      lbound = 0;
      ubound = m_table_count - 1;
      
      /* Zero in on a record */
      while (ubound > lbound) {
        
        /* Choose a record halfway between, but greater than ubound */
        mid = lbound + ((ubound - lbound) / 2);
        if (mid <= lbound) {
          mid = lbound + 1;
        }
        
        /* Compare to mid record */
        if ((m_table[mid]).rgbidx > rgb_index) {
          /* Midpoint greater than new record, so new upper bound is
           * less than that */
          ubound = mid - 1;
          
        } else if ((m_table[mid]).rgbidx < rgb_index) {
          /* Midpoint less than new record, so new lower bound is
           * that */
          lbound = mid;
          
        } else if ((m_table[mid]).rgbidx == rgb_index) {
          /* Duplicate record error */
          *pError = TTABLE_ERR_DUP;
          status = 0;
          break;
          
        } else {
          /* Shouldn't happen */
          abort();
        }
      }
      
      /* Insert after lbound */
      if (status) {
        assert(lbound < m_table_count - 1);
        m_table_count++;
        shiftRecs(lbound + 1);
        psr = &(m_table[lbound + 1]);
      }
    }
  }
  
  /* Write the record */
  if (status) {
    psr->rgbidx = rgb_index;
    psr->tidx = tex_index;
    psr->srate = shade_rate;
    psr->drate = draw_rate;
    if (rgb_tint >= 0) {
      psr->rgbtint = rgb_tint;
    } else {
      psr->rgbtint = UINT32_C(0xffffffff);
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Read a character from an input stream.
 * 
 * pf is the input stream to read from.  Undefined behavior occurs if
 * this is not open for reading.
 * 
 * pChar points to the integer to receive the character that was read,
 * or -1 if EOF.
 * 
 * The return value indicates whether reading was successful.  If
 * failure, the state of *pChar is undefined.
 * 
 * Parameters:
 * 
 *   pf - the file to read from
 * 
 *   pChar - pointer to variable to receive read character or -1 for EOF
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int readchar(FILE *pf, int *pChar) {
  
  int status = 1;
  int c = 0;
  
  /* Check parameters */
  if ((pf == NULL) || (pChar == NULL)) {
    abort();
  }
  
  /* Get a character */
  c = getc(pf);
  
  /* Check return */
  if (c != EOF) {
    /* Successfully read a character */
    *pChar = c;
  
  } else {
    /* Check if error or EOF */
    if (feof(pf)) {
      /* EOF */
      *pChar = -1;
      
    } else {
      /* Error */
      status = 0;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Check whether a given string is blank.
 * 
 * This is true only if the string is empty or consists only of spaces
 * and tabs.
 * 
 * Parameters:
 * 
 *   pstr - the string to check
 * 
 * Return:
 * 
 *   non-zero if blank, zero if not
 */
static int is_blank(char *pstr) {
  
  int result = 0;
  
  /* Check parameters */
  if (pstr == NULL) {
    abort();
  }
  
  /* Check if blank */
  result = 1;
  for( ; *pstr != 0; pstr++) {
    if ((*pstr != ASCII_SP) && (*pstr != ASCII_HT)) {
      result = 0;
      break;
    }
  }
  
  /* Return result */
  return result;
}

/*
 * Skip whitespace at the beginning of the given string, returning a
 * pointer to the first non-whitespace character (or the terminating
 * nul).
 * 
 * pstr is the pointer to the string.
 * 
 * optional is non-zero if it is acceptable for there to be no
 * whitespace at the beginning of the string, in which case the return
 * value equals pstr.  If zero, at least one character of whitespace is
 * required, and NULL is returned if not present.
 * 
 * Whitespace is either ASCII SP or HT.
 * 
 * Parameters:
 * 
 *   pstr - pointer to the string
 * 
 *   optional - non-zero if whitespace optional, zero if at least one
 *   character of whitespace is required
 * 
 * Return:
 * 
 *   pointer to first non-whitespace, or NULL if required whitespace is
 *   missing
 */
static char *skipSpace(char *pstr, int optional) {
  
  /* Check parameters */
  if (pstr == NULL) {
    abort();
  }
  
  /* Fail if not optional and first character not whitespace */
  if ((!optional) && (*pstr != ASCII_SP) && (*pstr != ASCII_HT)) {
    pstr = NULL;
  }
  
  /* Advance to first non-whitespace */
  if (pstr != NULL) {
    for( ; (*pstr == ASCII_SP) || (*pstr == ASCII_HT); pstr++);
  }
  
  /* Return result or NULL */
  return pstr;
}

/*
 * Read an RGB value from the beginning of the given string.
 * 
 * pstr is the string to read from.
 * 
 * pRGB points to the variable to receive the packed RGB value.
 * 
 * The RGB value must be exactly six base-16 digits (either lowercase or
 * uppercase letters).
 * 
 * The return value points to the character immediately after the RGB
 * value if successful, NULL if error.
 * 
 * Parameters:
 * 
 *   pstr - the string
 * 
 *   pRGB - pointer to variable to receive RGB value
 * 
 * Return:
 * 
 *   pointer to character after RGB value, or NULL if parsing error
 */
static char *readRGB(char *pstr, int32_t *pRGB) {
  
  int status = 1;
  int32_t v = 0;
  int x = 0;
  
  /* Check parameters */
  if ((pstr == NULL) || (pRGB == NULL)) {
    abort();
  }
  
  /* Read six base-16 digits */
  for(x = 0; x < 6; x++) {
    
    /* Check current character */
    if ((*pstr >= ASCII_ZERO) && (*pstr <= ASCII_NINE)) {
      /* Decimal digit */
      v = (v << 4) + (*pstr - ASCII_ZERO);
      
    } else if ((*pstr >= ASCII_LOWER_A) && (*pstr <= ASCII_LOWER_F)) {
      /* Lowercase letter */
      v = (v << 4) + (*pstr - ASCII_LOWER_A + 10);
      
    } else if ((*pstr >= ASCII_UPPER_A) && (*pstr <= ASCII_UPPER_F)) {
      /* Uppercase letter */
      v = (v << 4) + (*pstr - ASCII_UPPER_A + 10);
      
    } else {
      /* Not a base-16 digit, so error */
      status = 0;
    }
    
    /* Leave loop if error */
    if (!status) {
      break;
    }
    
    /* Move to next character */
    pstr++;
  }
  
  /* Write the final value */
  if (status) {
    *pRGB = v;
  }
  
  /* If failure, blank to NULL */
  if (!status) {
    pstr = NULL;
  }
  
  /* Return pointer to next character or NULL */
  return pstr;
}

/*
 * Read an unsigned decimal integer from the beginning of the given
 * string.
 * 
 * pstr is the string to read from.
 * 
 * pv points to the variable to receive the decimal value.
 * 
 * The decimal value may only include ASCII digits 0-9.
 * 
 * The return value points to the character immediately after the
 * decimal value if successful, NULL if error.
 * 
 * Parameters:
 * 
 *   pstr - the string
 * 
 *   pv - pointer to variable to receive integer value
 * 
 * Return:
 * 
 *   pointer to character after decimal value, or NULL if parsing error
 */
static char *readInt(char *pstr, int *pv) {
  
  int status = 1;
  int v = 0;
  int d = 0;
  
  /* Check parameters */
  if ((pstr == NULL) || (pv == NULL)) {
    abort();
  }
  
  /* Fail if first character is not decimal digit */
  if ((*pstr < ASCII_ZERO) || (*pstr > ASCII_NINE)) {
    status = 0;
  }
  
  /* Read decimal digits */
  if (status) {
    for( ; (*pstr >= ASCII_ZERO) && (*pstr <= ASCII_NINE); pstr++) {
    
      /* Multiply value by ten, watching for overflow */
      if (v <= INT_MAX / 10) {
        v *= 10;
      } else {
        status = 0;
      }
      
      /* Get digit value */
      if (status) {
        d = *pstr - ASCII_ZERO;
      }
      
      /* Add digit value, watching for overflow */
      if (status) {
        if (v <= INT_MAX - d) {
          v += d;
        } else {
          status = 0;
        }
      }
    
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
  }
  
  /* Write the final value */
  if (status) {
    *pv = v;
  }
  
  /* If failure, blank to NULL */
  if (!status) {
    pstr = NULL;
  }
  
  /* Return pointer to next character or NULL */
  return pstr;
}

/*
 * Parse a line within a text file.
 * 
 * pstr points to the text line.  The line break must NOT be included.
 * The line should be nul-terminated.  The line may be modified by this
 * function by shortening.
 * 
 * pError points to the variable to receive the error code if the
 * function fails.  It may not be NULL.
 * 
 * Parameters:
 * 
 *   pstr - pointer to the text line
 * 
 *   pError - pointer to the error status return
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int parseLine(char *pstr, int *pError) {
  
  int status = 1;
  char *pc = NULL;
  
  int32_t rgb_index = 0;
  int tex_index = 0;
  int shade_rate = 0;
  int draw_rate = 0;
  int32_t rgb_tint = 0;
  
  /* Check parameters */
  if ((pstr == NULL) || (pError == NULL)) {
    abort();
  }
  
  /* If there is an ampersand in the line, replace it with a nul so that
   * the comment is stripped out */
  for(pc = pstr; *pc != 0; pc++) {
    if (*pc == ASCII_AMP) {
      *pc = (char) 0;
      break;
    }
  }
  
  /* Only proceed if line is not blank, else ignore */
  if (!is_blank(pstr)) {
    
    /* Skip optional whitespace */
    pstr = skipSpace(pstr, 1);
    assert(pstr != NULL);
    
    /* Read RGB index */
    pstr = readRGB(pstr, &rgb_index);
    if (pstr == NULL) {
      *pError = TTABLE_ERR_RGB;
      status = 0;
    }
    
    /* Required whitespace */
    if (status) {
      pstr = skipSpace(pstr, 0);
      if (pstr == NULL) {
        *pError = TTABLE_ERR_SP;
        status = 0;
      }
    }
    
    /* Read texture index */
    if (status) {
      pstr = readInt(pstr, &tex_index);
      if (pstr == NULL) {
        *pError = TTABLE_ERR_INT;
        status = 0;
      }
    }
    
    /* Required whitespace */
    if (status) {
      pstr = skipSpace(pstr, 0);
      if (pstr == NULL) {
        *pError = TTABLE_ERR_SP;
        status = 0;
      }
    }
    
    /* Read shading rate */
    if (status) {
      pstr = readInt(pstr, &shade_rate);
      if (pstr == NULL) {
        *pError = TTABLE_ERR_INT;
        status = 0;
      }
    }
    
    /* Required whitespace */
    if (status) {
      pstr = skipSpace(pstr, 0);
      if (pstr == NULL) {
        *pError = TTABLE_ERR_SP;
        status = 0;
      }
    }
    
    /* Read drawing rate */
    if (status) {
      pstr = readInt(pstr, &draw_rate);
      if (pstr == NULL) {
        *pError = TTABLE_ERR_INT;
        status = 0;
      }
    }
    
    /* Only proceed for RGB tint if present */
    if (status) {
      if (!is_blank(pstr)) {
    
        /* Required whitespace */
        if (status) {
          pstr = skipSpace(pstr, 0);
          if (pstr == NULL) {
            *pError = TTABLE_ERR_SP;
            status = 0;
          }
        }
    
        /* Read RGB tint */
        if (status) {
          pstr = readRGB(pstr, &rgb_tint);
          if (pstr == NULL) {
            *pError = TTABLE_ERR_RGB;
            status = 0;
          }
        }
    
        /* Skip optional whitespace */
        if (status) {
          pstr = skipSpace(pstr, 1);
          assert(pstr != NULL);
        }
    
        /* Nothing should remain */
        if (status) {
          if (*pstr != 0) {
            *pError = TTABLE_ERR_UNX;
            status = 0;
          }
        }
      
      } else {
        /* No tint provided, so disabled colorizer for this texture */
        rgb_tint = -1;
      }
    }
    
    /* Range-check texture index lower bound */
    if (status) {
      if (tex_index < 1) {
        *pError = TTABLE_ERR_TEX;
        status = 0;
      }
    }
    
    /* Range-check shading rate */
    if (status) {
      if ((shade_rate < 0) || (shade_rate > 255)) {
        *pError = TTABLE_ERR_RATE;
        status = 0;
      }
    }
    
    /* Range-check drawing rate */
    if (status) {
      if ((draw_rate < 0) || (draw_rate > 255)) {
        *pError = TTABLE_ERR_DRAW;
        status = 0;
      }
    }
    
    /* Add record */
    if (status) {
      if (!addRecord(
            rgb_index, tex_index, shade_rate,
            draw_rate, rgb_tint, pError)) {
        status = 0;
      }
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * ttable_errorString function.
 */
const char *ttable_errorString(int code) {
  
  const char *pResult = NULL;
  
  switch (code) {
    
    case TTABLE_ERR_NONE:
      pResult = "No error";
      break;
    
    case TTABLE_ERR_OPEN:
      pResult = "Can't open file";
      break;
    
    case TTABLE_ERR_IO:
      pResult = "I/O error";
      break;
    
    case TTABLE_ERR_CR:
      pResult = "Stray CR character without LF";
      break;
    
    case TTABLE_ERR_LONG:
      pResult = "Text line is too long";
      break;
    
    case TTABLE_ERR_CHAR:
      pResult = "Non-ASCII character encountered";
      break;
    
    case TTABLE_ERR_RGB:
      pResult = "Can't parse RGB value";
      break;
    
    case TTABLE_ERR_SP:
      pResult = "Missing whitespace";
      break;
    
    case TTABLE_ERR_INT:
      pResult = "Can't parse integer (or overflow)";
      break;
      
    case TTABLE_ERR_UNX:
      pResult = "Unexpected content on end of line";
      break;
    
    case TTABLE_ERR_RATE:
      pResult = "Shading rate out of range";
      break;
    
    case TTABLE_ERR_TEX:
      pResult = "Texture index out of range";
      break;
    
    case TTABLE_ERR_RECS:
      pResult = "Too many records";
      break;
    
    case TTABLE_ERR_DUP:
      pResult = "Duplicate record for RGB index";
      break;
    
    case TTABLE_ERR_DRAW:
      pResult = "Drawing rate out of range";
      break;
    
    default:
      pResult = "Unknown error";
  }
  
  return pResult;
}

/*
 * ttable_parse function.
 */
int ttable_parse(const char *pPath, int *pError, int *pLineNum) {
  
  int dummy = 0;
  int status = 1;
  FILE *pf = NULL;
  
  char buf[IN_MAXLINE];
  int bufcount = 0;
  int c = 0;
  int eoff = 0;
  int linenum = 0;
  
  /* Initialize buffer */
  memset(buf, 0, IN_MAXLINE);
  
  /* Check parameters */
  if (pPath == NULL) {
    abort();
  }
  
  /* If error pointers are NULL, set to dummy */
  if (pError == NULL) {
    pError = &dummy;
  }
  if (pLineNum == NULL) {
    pLineNum = &dummy;
  }
  
  /* Clear error */
  *pError = TTABLE_ERR_NONE;
  *pLineNum = -1;
  
  /* Open text file for reading */
  pf = fopen(pPath, "r");
  if (pf == NULL) {
    status = 0;
    *pError = TTABLE_ERR_OPEN;
  }
  
  /* Read each line of the file */
  while (status) {
    
    /* Increment line number, or set to -1 if overflow */
    if (linenum >= 0) {
      if (linenum < INT_MAX) {
        linenum++;
      } else {
        linenum = -1;
      }
    }
    
    /* Clear the line buffer */
    memset(buf, 0, IN_MAXLINE);
    bufcount = 0;
    
    /* Read characters into line buffer */
    while (status) {
      
      /* Read a character */
      if (!readchar(pf, &c)) {
        status = 0;
        *pError = TTABLE_ERR_IO;
      }
      
      /* If character is CR, read next character, which must be LF */
      if (status && (c == ASCII_CR)) {
        
        /* Read next character */
        if (!readchar(pf, &c)) {
          status = 0;
          *pError = TTABLE_ERR_IO;
        }
        
        /* Make sure next character is LF */
        if (status && (c != ASCII_LF)) {
          status = 0;
          *pError = TTABLE_ERR_CR;
          *pLineNum = linenum;
        }
      }
      
      /* Check character range */
      if (status && ((c < -1) || (c > 127))) {
        status = 0;
        *pError = TTABLE_ERR_CHAR;
        *pLineNum = linenum;
      }
      
      /* If we read LF or EOF, we are done */
      if (status && ((c == ASCII_LF) || (c == -1))) {
        break;
      }
      
      /* If we got here successfully, we have another character for the
       * buffer; add it, watching for buffer overflow */
      if (status) {
        bufcount++;
        if (bufcount >= IN_MAXLINE) {
          status = 0;
          *pError = TTABLE_ERR_LONG;
          *pLineNum = linenum;
        }
      }
      
      if (status) {
        buf[bufcount - 1] = (char) c;
      }
    }
    
    /* If we left the loop because of EOF, set EOF flag */
    if (status && (c == -1)) {
      eoff = 1;
    }
    
    /* Parse the line */
    if (status) {
      if (!parseLine(buf, pError)) {
        *pLineNum = linenum;
        status = 0;
      }
    }
    
    /* If EOF flag set, we are done */
    if (status && eoff) {
      break;
    }
  }
  
  /* Close file if open */
  if (pf != NULL) {
    fclose(pf);
    pf = NULL;
  }
  
  /* Return status */
  return status;
}

/*
 * ttable_query function.
 */
void ttable_query(SHADEREC *psr) {
  
  SHADEREC *pt = NULL;
  int32_t rgb_index;
  int lbound = 0;
  int ubound = 0;
  int mid = 0;
  
  /* Check parameter */
  if (psr == NULL) {
    abort();
  }
  
  /* Get index */
  rgb_index = psr->rgbidx;
  
  /* Only proceed with search if table non-empty */
  if (m_table_count > 0) {
    
    /* Set search boundaries */
    lbound = 0;
    ubound = m_table_count - 1;
    
    /* Zoom in on one record */
    while (ubound > lbound) {
      
      /* Choose a midpoint somewhere halfway but greater than lbound */
      mid = lbound + ((ubound - lbound) / 2);
      
      /* Compare to midpoint */
      if ((m_table[mid]).rgbidx > rgb_index) {
        /* Desired record less than midpoint */
        ubound = mid - 1;
        if (ubound < lbound) {
          ubound = lbound;
        }
        
      } else if ((m_table[mid]).rgbidx < rgb_index) {
        /* Desired record greater than midpoint */
        lbound = mid + 1;
        if (lbound > ubound) {
          lbound = ubound;
        }
        
      } else if ((m_table[mid]).rgbidx == rgb_index) {
        /* We found the record, so zoom in on that */
        ubound = mid;
        lbound = mid;
        
      } else {
        /* Shouldn't happen */
        abort();
      }
    }
    
    /* Compare to selected record */
    if ((m_table[lbound]).rgbidx == rgb_index) {
      /* We found the record */
      pt = &(m_table[lbound]);
    
    } else {
      /* We didn't find the record */
      pt = NULL;
    }
  }
  
  /* Fill in either with record from table or with default */
  if (pt != NULL) {
    memcpy(psr, pt, sizeof(SHADEREC));
  } else {
    /* Default record */
    psr->tidx = 1;
    psr->srate = 0;
    psr->drate = 255;
    psr->rgbtint = UINT32_C(0xffffffff);
  }
}
