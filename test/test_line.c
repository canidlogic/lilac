/*
 * test_line.c
 * ===========
 * 
 * Simple test of Lilac renderer that just writes a 600x400 file with a
 * filled shape involving lines.
 * 
 * Syntax
 * ------
 * 
 *   test_line [out_path]
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
#include <string.h>

int main(int argc, char *argv[]) {
  int32_t i = 0;
  
  if (argc != 2) {
    fprintf(stderr, "Expecting one program argument!\n");
    exit(EXIT_FAILURE);
  }
  
  lilac_init(600, 400, 64, UINT32_C(0xffffffff), NULL, NULL);
  lilac_color(UINT32_C(0xff000000));
  
  for(i = 0; i < lilac_tiles(); i++) {
    lilac_begin_tile();
    lilac_begin_path();
    
    lilac_line(50.0, 50.0, 550.0, 50.0);
    lilac_line(550.0, 50.0, 550.0, 350.0);
    lilac_line(550.0, 350.0, 50.0, 50.0);
    
    lilac_end_path();
    lilac_end_tile();
  }
  
  lilac_compile(argv[1]);
  
  return EXIT_SUCCESS;
}
