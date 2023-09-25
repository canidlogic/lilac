/*
 * test_dots.c
 * ===========
 * 
 * Simple test of Lilac renderer that just writes a 600x400 file with a
 * sequence of dots with small radii.
 * 
 * Syntax
 * ------
 * 
 *   test_dots [out_path]
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
  lilac_color(UINT32_C(0xff0000ff));
  
  for(i = 0; i < lilac_tiles(); i++) {
    lilac_begin_tile();
    lilac_begin_path();
    
    lilac_dot(300, 190, 1);
    
    lilac_dot(300, 200, 2);
    lilac_dot(300, 220, 3);
    lilac_dot(300, 240, 4);
    lilac_dot(300, 260, 5);
    lilac_dot(300, 280, 6);
    lilac_dot(300, 300, 7);
    lilac_dot(300, 320, 8);
    lilac_dot(300, 340, 9);
    lilac_dot(300, 365, 10);
    
    lilac_dot(350, 200, 11);
    lilac_dot(350, 240, 12);
    lilac_dot(350, 280, 13);
    lilac_dot(350, 320, 14);
    lilac_dot(350, 365, 15);
    
    lilac_end_path();
    lilac_end_tile();
  }
  
  lilac_compile(argv[1]);
  
  return EXIT_SUCCESS;
}
