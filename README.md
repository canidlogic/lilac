# Lilac README

## 1. Introduction

Lilac is a program for synthesizing raster images.

## 2. Syntax

The syntax of the Lilac program is:

    lilac [script]
    lilac [script] [reduce]

The `[script]` parameter is the path to a Shastina script that specifies how the Lilac program will run.  For the format of this script, see the `LoadScript.md` document in the `doc` directory.

The optional `[reduce]` parameter is an unsigned integer that is greater than zero.  Specifying a reduce value of one has no effect.  Specifying a reduce value greater than one means that only one out of every _n_ pixel rows and only one out of every _n_ pixel columns will be rendered to the output image, where _n_ is the reduce parameter value.  This allows for preview renders at a reduced resolution.

## 3. Utility programs

See the `README.md` file in the `util` directory for information about included utility programs that are auxiliary to the main Lilac program.

## 4. Compilation

[libsophistry](http://www.purl.org/canidtech/r/libsophistry) is required.  Lilac was written against version 0.5.2 beta but also compiles against version 0.5.3 beta and compatible.

Sophistry requires that the program also link against [libpng](http://libpng.org/).  libpng may require zlib, though Sophistry does not directly use that library.

Lilac also requires [libshastina](http://www.purl.org/canidtech/r/shastina) version 0.9.2 beta or compatible.

You must also link Lilac against [liblua](https://www.lua.org/) version 5.4.

The math library `-lm` may be required on certain platforms.

Include all the `.c` source files in the root project directory in the compilation.  Do __not__ include any `.c` source files in the `util` directory.  (See the `README.md` in the `util` directory for instructions how to build the utility programs.)
