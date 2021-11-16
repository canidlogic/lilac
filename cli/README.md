# Lilac command line interface (CLI)

This directory contains the main program modules that define the command line interface (CLI) of Lilac.  These programs are linked against various Lilac modules that are contained in the root directory of this project.  See the subsections below for further information about specific programs in the Lilac CLI.

## lilac_draw

The `lilac_draw` program simulates a color drawing by synthesizing an image out of separate image components.

This program requires the following modules of Lilac:

- `gamma.c`
- `pshade.c`
- `texture.c`
- `ttable.c`

This program has the following direct external dependencies:

- [libsophistry](http://www.purl.org/canidtech/r/libsophistry) version 0.5.2 or 0.5.3 or compatible.
- [liblua](https://www.lua.org/) version 5.4

This program has the following indirect external dependencies:

- [libpng](http://libpng.org/) is required by libsophistry
- [zlib](http://www.zlib.net/) may be required by libpng

The math library `-lm` may be required on certain platforms.

If you are in the root directory of this project, you can build the program with the following GCC invocation (all on one line):

    gcc -O2 -o cli/lilac_draw
      -I.
      -I/path/to/sophistry/include
      -I/path/to/liblua/include
      -L/path/to/sophistry/lib
      -L/path/to/liblua/lib
      `pkg-config --cflags libpng`
      cli/lilac_draw.c
      gamma.c
      pshade.c
      texture.c
      ttable.c
      -lm
      -lsophistry
      -llua
      `pkg-config --libs libpng`

## lilacme2json

The `lilacme2json` program reads a Shastina-format Lilac mesh file and outputs a JSON representation of the file in a format compatible with the external [Lilac mesh editor](http://www.purl.org/canidtech/r/lilac_mesh) project.

This program requires the following modules of Lilac:

- `lilac_mesh.c`

This program has the following external dependencies:

- [libshastina](http://www.purl.org/canidtech/r/shastina) beta 0.9.2 or compatible

If you are in the root directory of this project, you can build the program with the following GCC invocation (all on one line):

    gcc -O2 -o cli/lilacme2json
      -I.
      -I/path/to/shastina/include
      -L/path/to/shastina/lib
      cli/lilacme2json.c
      lilac_mesh.c
      -lshastina
