/*
 * test_blank.c
 * ============
 * 
 * Simple test of Lilac renderer that just writes a 640x480 file with
 * opaque blue background to a file.
 * 
 * Syntax
 * ------
 * 
 *   test_blank [out_path]
 * 
 * Arguments
 * ---------
 * 
 * [out_path] is the path to the PNG file to write.  It must end in the
 * extension .png
 * 
 * Requirements
 * ------------
 * 
 * Compile with the lilac library and all its requirements.
 */

#include "lilac.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  int32_t i = 0;
  
  if (argc != 2) {
    fprintf(stderr, "Expecting one program argument!\n");
    exit(EXIT_FAILURE);
  }
  
  lilac_init(640, 480, 64, UINT32_C(0xff0000ff), NULL, NULL);
  
  for(i = 0; i < lilac_tiles(); i++) {
    lilac_begin_tile();
    lilac_end_tile();
  }
  
  lilac_compile(argv[1]);
  
  return EXIT_SUCCESS;
}
