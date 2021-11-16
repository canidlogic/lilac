# Lilac command line interface (CLI)

This directory contains the main program modules that define the command line interface (CLI) of Lilac.  These programs are linked against various Lilac modules that are contained in the root directory of this project.  See the subsections below for further information about specific programs in the Lilac CLI.

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
