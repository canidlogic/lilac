# Lilac architecture

This document gives an overview of the full architecture of the Lilac 2D renderer.

## 1. Compiling and linking

The Lilac renderer is a C library, not a full program.  In order to use Lilac, you must write a C program that invokes the `llc_main()` function of the Lilac library.  The `llc_main()` function has the same interface as the C entrypoint `main` function.  If you do not need to add any extensions to Lilac, then the C program can be as simple as this:

    #include "lilac.h"

    int main(int argc, char *argv[]) {
      /* Lilac extensions would be registered here */

      /* Invoke the Lilac renderer main function */
      return llc_main(argc, argv);
    }

If you need to add extensions to Lilac, those extensions should be registered before calling the `llc_main()` function, as shown in the example above.  If your custom Lilac program needs to accept command-line parameters through `argv` then you can process the given command-line parameters and pass a different `argc` and `argv` combination through to `llc_main()` that has the arguments specific to your Lilac extension filtered out.

Lilac has minimal dependencies in order to make it easy to link.  You only need to link the following with your Lilac program:

1. Lilac renderer library
2. Shastina library
3. `libpng` library

Suppose the following files are at the following locations:

1. `/path/to/shastina/inc/shastina.h`
2. `/path/to/shastina/lib/libshastina.a`
3. `/path/to/lilac/inc` contains Lilac headers
4. `/path/to/lilac/lib/liblilac.a`

Suppose also that the `libpng` development library is installed on the system and registered with `pkg-config`, that the custom Lilac renderer program source is `my_lilac.c` in the current directory and the custom Lilac renderer should be compiled into the program binary `my_lilac`.  Then, the following is a sample compilation line for `gcc` (everything should be on one line at the prompt):

    gcc
      -o my_lilac
      -I/path/to/shastina/inc
      -I/path/to/lilac/inc
      -L/path/to/shastina/lib
      -L/path/to/lilac/lib
      `pkg-config --cflags libpng`
      -O2
      my_lilac.c
      -lshastina
      -llilac
      `pkg-config --libs libpng`

After running this command successfully, `my_lilac` will be the custom Lilac renderer that will be used for rendering.

## 2. Running

Once the Lilac renderer is built with any needed extensions as described in the preceding section, the renderer can then be used to render images.  Recompilation of the renderer is only necessary when changing the extensions that are compiled in.

If command-line parameters are passed through to `llc_main()` without modification by the custom rendering program, then the custom renderer (called `my_lilac` here) will have the following syntax:

    ./mylilac [script_path]

The `[script_path]` parameter is the path to the Lilac Shastina script that controls rendering.  All additional parameters for the Lilac renderer are provided within this Shastina script.

The following alternate invocation syntax is also available:

    ./mylilac -string [script]

In this case, the `[script]` parameter contains the full Shastina script to run as a string.  This invocation syntax is mainly intended for use by custom rendering programs that wish to embed their Shastina script within the C source file and then pass it directly to the renderer rather than having to dump it to a file first.

## 3. Scripting

The Shastina script provided to the `llc_main()` routine is responsible for setting up the Lilac rendering state prior to rendering.  Special Shastina metacommands are provided so that the Shastina script can check for the presence of specific extensions to the Lilac renderer.  This allows the Shastina rendering script to be bound to specific extensions that are provided in the C program file.

## 4. Rendering

After script interpretation finishes, Lilac rendering begins.

The Shastina script sets an output path that will receive the rendered file.  Lilac always uses PNG format images due to their lossless storage of data and their support for transparency.  The Shastina script also sets the dimensions in pixels of the output image.  These dimensions can either be directly specified in the Shastina script, or they can be copied from an existing PNG image.

Lilac rendering always proceeds pixel by pixel from left to right within scanlines, and from top to bottom in terms of scanlines.  Shaders may always assume this rendering order of pixels, though note that there is no guarantee that each shader is invoked on each pixel.

The Shastina script establishes one specific _nonlinear shader_ as the "primary" shader for the image.  Nonlinear shaders are functions that are given the output image dimensions and the current pixel coordinates, and produce a 32-bit ARGB value with non-precomposed alpha in nonlinear sRGB space as output.  All ARGB values produced by the primary shader must have an alpha value of 255, meaning fully opaque.

The RGB channels of the primary shader output are not actually interpreted as color channels.  Instead, all three channels together are used as a 24-bit index value to select one out of up to 16,777,216 "palette shaders."  Palette shaders are also nonlinear shaders.  For each pixel, the primary shader is consulted to generate an index into the palette shaders, and then the palette shader chosen for that pixel is used to determine the ARGB value that is written into the rendered image at this point.  (Palette shaders are allowed to use the full range of alpha values.)

## 5. Shading

Additional kinds of shaders can be set up by the Shastina script if needed for the specific image.  The previous section defined a _nonlinear shader_ class and defined the "primary" shader for the image and the "palette shaders" for the image, all of which must be nonlinear shaders.  This section provides an overview of the other kinds of shaders that are available.

A _linear shader_ is similar to a nonlinear shader, except its output is three floating-point RGB values in __linear__ RGB space.  Any floating-point value is allowed for the channels, including negative and non-finite values.

A _normal shader_ takes the dimensions of the rendered image and the current pixel coordinates as input and yields a 3D floating-point vector of magnitude one.  The 3D space of the image is defined with the X axis running from the left to right, the Y axis running __from bottom to top__ (note that this is the opposite from the top-down pixel rendering order!), and the Z axis running from back to front (right-handed orientation).

An _intensity shader_ takes the dimensions of the rendered image and the current pixel coordinates as input and yields a single floating-point value.

A _light_ is not actually a shader, but rather is a bundle of three specific shaders.  The first shader within a light is a normal shader.  This normal shader defines, for each pixel in the rendered image, the 3D direction from that pixel to the light.  (Note that this normal shader points __towards__ the light.)  The second shader within a light is a linear shader that defines, for each pixel in the rendered image, the diffuse RGB value of the light projected onto that point.  The third shader within a light is another linear shader that defines, for each pixel in the rendered image, the specular RGB value of the light projected onto that point.

An _environment shader_ takes the dimensions of the rendered image and the current pixel coordinates as input and yields an array of zero or more lights that are active at the current point.  The order of lights in the array does not matter, but lights that are present more than once in the array will have cumulative effect.

A _material_ is not actually a shader, but rather is a bundle of four specific shaders.  The first shader is a linear shader that specifies, for each pixel of the image, the ambient RGB reflection.  The second shader is a linear shader that specifies, for each pixel of the image, the diffuse RGB reflection.  The third shader is a linear shader that specifies, for each pixel of the image, the specular RGB reflection.  The fourth shader is an intensity shader that specifies, for each pixel of the image, the shininess constant of the material.

A _surface shader_ takes the dimensions of the rendered image and the current pixel coordinates as input and yields a material that is present at the current point on the surface.

All of the shader types defined above as well as the material and light objects can be used in any way.  However, they are defined to allow for 3D lighting according to the Phong reflection model.  A Phong shader can be defined as a linear shader that takes the following parameters:

1. Linear shader for ambient lighting
2. Normal shader for surface normals
3. Normal shader for vector to viewer
4. Surface shader for defining material
5. Environment shader for defining lights

The 3D shading of the Phong shader can then be converted to nonlinear RGB values using a nonlinear shader on top of the linear Phong shader, and this nonlinear shader can then be used as a palette shader to include the 3D-lighted shading in the rendered output.

## 6. Resources

Lilac has built-in support for packaging certain types of resource files external to the Shastina script as shaders that can be used within the script.

PNG images that have the exact same dimensions as the output image of the script may be loaded by the built-in plate shader.  A plate shader is a built-in class of nonlinear shader that, for every pixel, returns the ARGB color in that plate image for that pixel.  The plate shader only loads one scanline at a time into memory, so it is possible to have multiple plate shaders with large image dimensions and not run out of memory.

It is also possible to load smaller PNG images fully into memory and tile them using the built-in tile shader.  A tile shader is a built-in class of nonlinear shader that has a full PNG image loaded in memory, which is not necessarily the same size as the output image.  For each pixel of the output image, the tile shader returns a pixel of the loaded tile, duplicating the tile across the whole area of the output image.

The plate shader and tile shader are both standard types of nonlinear shader that do not require any kind of special support from Lilac.  Extension shaders can be added for more resource types, and the built-in plate and tile shaders are no different in performance or functionality from extension shaders for resources.

However, the last kind of built-in shader adds another kind of object type to the Lilac Shastina interpreter &mdash; a _mesh_.  A mesh is loaded from an external Lilac mesh file, which is also a kind of Shastina file, but it has a different format from the main Lilac Shastina script.  See `MeshFormat.md` for the specific format of a Lilac mesh file.  The external Lilac mesh editor project is able to create and edit these mesh files graphically.

Mesh objects loaded into the Lilac interpreter may be modified by commands.  For example, if two different meshes should smoothly connect at an edge, a welding command can be defined that adjusts the two meshes so that they have matching points and matching normals near given point locations.  This only modifies the mesh objects that are stored in the Shastina interpreter memory, not the external mesh files.

Mesh objects can then be integrated into a rendered image by running them through the built-in scanline shader.  The scanline shader is a type of normal shader that uses scanline rendering to compute a normal value for each pixel by using the triangle mesh defined by the underlying mesh object.

Resources can be used in various ways.  For example, mesh objects that are run through a scanline shader can be used for defining surface normals, and also for defining light direction normal shaders within light objects, and for anything else that accepts a normal shader as input.  Tile and plate shaders can be used to mix in images, but they can also be converted to linear RGB values and then used for ambient light maps, and anywhere else a linear shader is accepted.

## 7. Backwards compatibility

The new Lilac renderer architecture is backwards-compatible with the previous Lilac architecture.  The previous Lilac architecture was a fixed-function pipeline that can be recreated using the new programmable architecture of Lilac.  No special compatibility support is required in the new Lilac architecture because it is flexible enough to be reprogrammed to match the functionality of the previous Lilac version.

For full backwards compatibility, a custom Lilac renderer can be designed that accepts command-line parameters with the same syntax as the previous version, and then generates an appropriate Shastina Lilac script for the new renderer and uses the new rendering architecture.  This allows for the custom Lilac renderer to be a drop-in replacement for the previous version.
