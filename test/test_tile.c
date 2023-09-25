/*
 * test_tile.c
 * ===========
 * 
 * Simple test of Lilac renderer that just writes a 600x400 file with
 * tiles alternating in colors using the locking feature.
 * 
 * Syntax
 * ------
 * 
 *   test_tile [out_path]
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
  LILAC_LOCK sl;
  int32_t y = 0;
  int32_t x = 0;
  int32_t r = 0;
  int32_t c = 0;
  
  memset(&sl, 0, sizeof(LILAC_LOCK));
  
  if (argc != 2) {
    fprintf(stderr, "Expecting one program argument!\n");
    exit(EXIT_FAILURE);
  }
  
  lilac_init(600, 400, 64, UINT32_C(0xff000000), NULL, NULL);
  
  for(i = 0; i < lilac_tiles(); i++) {
    lilac_begin_tile();
    lilac_lock(&sl);
    
    r = sl.y / 64;
    c = sl.x / 64;
    
    if ((r & 0x1) ^ (c & 0x1) == 1) {
      for(y = 0; y < sl.h; y++) {
        for(x = 0; x < sl.w; x++) {
          (sl.pData)[(y * sl.pitch) + x] = UINT32_C(0xff00ff00);
        }
      }
    }
    
    lilac_unlock();
    lilac_end_tile();
  }
  
  lilac_compile(argv[1]);
  
  return EXIT_SUCCESS;
}
