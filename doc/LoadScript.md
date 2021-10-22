# Lilac loading script format

## 1. Introduction

The Lilac loading script is the script file that is passed to the `lilac` program, which defines how the image will be synthesized.

The loading script is a [Shastina](http://www.purl.org/canidtech/r/shastina) text file in a specific format described in this document.

## 2. Purpose of loading script

The loading script has the following objectives:

1. Set dimensions of output image.
2. Declare resources that will be used during rendering.
3. Declare the rendering script.

The rendering script is a [Lua](https://www.lua.org/) script with a format described in `RenderScript.md`.  It defines a function that is called for each pixel to determine the color of the pixel.

The rendering script may make use of data from the resources that are declared in the loading script.

## 3. Resource types

There are four kinds of resources that may be declared in a loading script:

1. Full-frame images
2. Texture images
3. Reverse palettes
4. Meshes

The first two resources are based on external PNG image files.  Full-frame images are PNG image files that must have the same dimensions as the dimensions declared for the output image.  Full-frame images only have one scanline at a time in memory, so they can be large.  For each pixel in the output image that is rendered, Lilac makes available the color values from each full-frame image resource at the corresponding pixel location in that image.

Texture images are smaller PNG image files that are loaded entirely into memory.  Unlike full-frame images, texture images do not have to have the same image dimensions as the output image file.  Texture images are automatically tiled to cover the full surface area of the output image, if necessary.

Reverse palettes are external text files that contain a mapping from unique RGB colors to string values.  The format of these reverse palette text files is described in `ReversePalette.md`.  During rendering, the render script is able to transform image resources through a reverse palette.

Meshes are two-dimensional triangle meshes that are loaded from a mesh file that has a format described in `MeshFormat.md`.  Unlike the other resource types, meshes are vector resources that are not based on any underlying raster bitmap.  Lilac has a built-in scanline renderer that renders the triangle meshes for each scanline.

For each pixel, the scanline-rendered mesh either has a special _absent_ value, indicating that no triangle area in the mesh covers that pixel, or the scanline-rendered mesh produces an interpolated direction vector.  See `RenderScript.md` for more specifics about how the scanline renderer for triangle meshes provides data to the rendering script.

The loading script also allows meshes that have been loaded into memory to receive minor edits before they are rendered.  These edits allow vertices to be fused between different meshes, so that there can be seamless boundaries between different meshes.  The specifics of vertex fusion are described later in this document.

## 4. Header

Load scripts must begin with a header, which is a sequence of Shastina metacommands.  The header has the following format:

1. Signature
2. Metavariables

The signature is the following metacommand, which identifies the Shastina file as a Lilac load script:

    %lilac-load;

The metavariables are a sequence of metavariable commands.  Each metavariable command has the following syntax:

1. Opening meta symbol `%`
2. Name of metavariable
3. (Whitespace)
4. Value of metavariable
5. Closing meta symbol `;`

The name of the metavariable must be a Shastina metacommand token that matches a known Lilac metavariable name.  The value of the metavariable must either be a Shastina metacommand token or a double-quoted Shastina metacommand string.  Integer metavariables have a token value that must be a signed integer, while string metavariables must have a double-quoted string value.

String metavariables support two escape codes.  `\\` is the escape code for a literal backslash, while `\"` is the escape code for a double quote.

The following metavariables are __required__ and must be set in every Lilac load script header:

1. [Integer] `width` &mdash; the output image width in pixels
2. [Integer] `height` &mdash; the output image height in pixels
3. [String] `rscript` &mdash; path to the Lua render script

The following metavariable is __optional__:

1. [String] `rfunc` &mdash; name of the render function

If `rfunc` is not defined, its default value is `render`.  The render function name is the name of the function within the Lua render script that is called to render each pixel.  See `RenderScript.md` for further details.

Each metavariable may be set at most once in the header.  The variables may be set in any order.  The header ends at the first encountered Shastina entity that is not part of a metacommand.

An example Lilac load script header looks like this:

    %lilac-load;
    %width   1920;
    %height  1080;
    %rscript "render_script.lua";

@@TODO:
