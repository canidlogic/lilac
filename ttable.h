#ifndef TTABLE_H_INCLUDED
#define TTABLE_H_INCLUDED

/*
 * ttable.h
 * 
 * Texture table module of Lilac.
 */

#include <stddef.h>
#include <stdint.h>

/* 
 * Error codes.
 * 
 * Don't forget to update ttable_errorString()!
 */
#define TTABLE_ERR_NONE (0)   /* No error */
#define TTABLE_ERR_OPEN (1)   /* Can't open file */
#define TTABLE_ERR_IO   (2)   /* I/O error */
#define TTABLE_ERR_CR   (3)   /* Stray CR */
#define TTABLE_ERR_LONG (4)   /* Line is too long */
#define TTABLE_ERR_CHAR (5)   /* Character out of ASCII range */
#define TTABLE_ERR_RGB  (6)   /* Can't parse RGB */
#define TTABLE_ERR_SP   (7)   /* Missing whitespace */
#define TTABLE_ERR_INT  (8)   /* Can't parse integer */
#define TTABLE_ERR_UNX  (9)   /* Unexpected content on line */
#define TTABLE_ERR_RATE (10)  /* Shading rate out of range */
#define TTABLE_ERR_TEX  (11)  /* Texture index out of range */
#define TTABLE_ERR_RECS (12)  /* Too many records */
#define TTABLE_ERR_DUP  (13)  /* Duplicate record */ 

/*
 * Shading record structure.
 */
typedef struct {
  
  /*
   * The RGB index of this record.
   * 
   * Only the 24 least significant bits should be used.  The eight most
   * significant bits should be zero.
   */
  int32_t rgbidx;
  
  /*
   * The texture file index.
   * 
   * Index one is for the first texture file, two for the second, and so
   * forth.
   */
  int tidx;
  
  /*
   * The shading rate.
   * 
   * This is in range [0, 100].
   */
  int srate;
  
  /*
   * The RGB tint for colorization.
   * 
   * Only the 24 least significant bits are used.  The eight most
   * significant bits are set to zero.
   * 
   * If this has the special value of 0xffffffff, the colorizer is
   * disabled for this texture.
   */
  uint32_t rgbtint;
  
} SHADEREC;

/*
 * Given a texture table parsing error code, return an error message.
 * 
 * The error message begins with a capital letter but does not have any
 * punctuation or line break at the end.
 * 
 * If zero is passed, "No error" is returned.  If an unknown error code
 * is passed, "Unknown error" is returned.
 * 
 * Parameters:
 * 
 *   code - the error code to look up
 * 
 * Return:
 * 
 *   an error message for the code
 */
const char *ttable_errorString(int code);

/*
 * Parse a texture table text file and add its records to the texture
 * table.
 * 
 * pPath is the path to the file to parse.
 * 
 * pError is either NULL or a pointer to an integer to receive an error
 * code in case of error.
 * 
 * pLineNum is either NULL or a pointer to an integer to receive a line
 * number in case of error.  The line number is set to -1 if not
 * relevant or if overflow.
 * 
 * The return value indicates whether the file was completely parsed or
 * not.  If the file was not completely parsed, some records may have
 * been added.
 * 
 * The error value is zero if successful, otherwise a TTABLE_ERR code.
 * Use ttable_errorString() to get an error message.
 * 
 * Parameters:
 * 
 *   pPath - path to the texture table file to parse
 * 
 *   pError - pointer to error code location or NULL
 * 
 *   pLineNum - pointer to line number location or NULL
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int ttable_parse(const char *pPath, int *pError, int *pLineNum);

/*
 * Fill in a shading record from the table.
 * 
 * psr points to the shading record.  The rgbidx field should be filled
 * in with the RGB index to query.
 * 
 * This function will always fill in the other fields besides rgbidx.
 * If rgbidx is invalid or it is not in the table, default values will
 * be filled in for the other fields.
 * 
 * Parameters:
 * 
 *   psr - the shading record to fill in
 */
void ttable_query(SHADEREC *psr);

#endif
