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

The name of the metavariable must be a Shastina metacommand token that matches a known Lilac metavariable name.  The value of the metavariable must either be a Shastina metacommand token or a double-quoted Shastina metacommand string.  Integer metavariables have a token value that must be a signed integer in 32-bit range, while string metavariables must have a double-quoted string value.

String metavariables support two escape codes.  `\\` is the escape code for a literal backslash, while `\"` is the escape code for a double quote.

The following metavariables are __required__ and must be set in every Lilac load script header:

1. [Integer] `width` &mdash; the output image width in pixels
2. [Integer] `height` &mdash; the output image height in pixels
3. [String] `rscript` &mdash; path to the Lua render script

The following metavariable is __optional__:

1. [String] `rfunc` &mdash; name of the render function

If `rfunc` is not defined, its default value is `render`.  The render function name is the name of the function within the Lua render script that is called to render each pixel.  See `RenderScript.md` for further details.  If a name is explicitly declared, it must be a sequence of one or more ASCII letters, digits, and/or underscores, and the first character may not be a digit.

Each metavariable may be set at most once in the header.  The variables may be set in any order.  The header ends at the first encountered Shastina entity that is not part of a metacommand.

An example Lilac load script header looks like this:

    %lilac-load;
    %width   1920;
    %height  1080;
    %rscript "render_script.lua";

## 5. Body

The body of the Lilac load script is used to declare all the resources that will be made available to the rendering script.  The body begins at the first entity in the load script that is not part of a metacommand.

Only the following Shastina entities are supported in the body of the loading script:

- `STRING`
- `NUMERIC`
- `OPERATION`

The body ends at the EOF marker `|;`

String entities must be double-quoted with no string prefix before the opening double quote.  They support `\\` as an escape for a literal backslash and `\"` as an escape for a double quote.  The result of a string entity is to push the string value on top of the interpreter stack.

Numeric entities must be signed integers in 32-bit range.  The result of a numeric entity is to push the numeric value on top of the interpreter stack.

Operation entities perform a specific operation using the data on the stack.  Operations are described later in this document.

When the EOF marker `|;` is reached, the interpreter stack must be empty.  Nothing but whitespace is allowed after the EOF marker.

## 6. Operations

This section describes the operations that are available within the body of the loading script.  These operations are used to declare resources.

Each operation header line below shows the syntax of the operation.  To the left of the operation name are the parameters that must be present on top of the interpreter stack when the operation is invoked, with the rightmost parameter on top.  The parameters are popped off the interpreter stack at the start of the operation.  To the right of the operation name are the parameters that are output from the operation, with the rightmost parameter on top of the interpreter stack upon return from the operation.  If a hyphen `-` appears on the left and/or right side of the operation, it means that there are no input and/or output parameters.

Declared resources are each given a name.  The name must be a sequence of one or more ASCII letters, digits, and/or underscores.  Each resource name must be unique (case sensitive) within the loading script, and resources may not be re-declared.

    [name] [path] frame -

Declare a full-frame image resource.  `[name]` is the name of the resource and `[path]` is the path to the image file, which must have a PNG extension.  The image must have the same dimensions as the output image dimensions declared in the header of the script.

    [name] [path] texture -

Declare a texture image resource.  `[name]` is the name of the resource and `[path]` is the path to the image file, which must have a PNG extension.  The image does __not__ need to have the same dimensions as the output image, but its size is limited since texture images must be loaded fully into memory.  Lilac will always support texture images up to 2048 by 2048 pixels.

    [name] [path] rpal -

Declare a reverse palette.  `[name]` is the name of the resource and `[path]` is the path to the reverse palette text file.

    [name] [path] mesh -

Declare a mesh resource.  `[name]` is the name of the resource and `[path]` is the path to the mesh file.

    [anchor] [edit] [x] [y] anchor -

Edit the mesh resource that has already been loaded with name `[edit]` by finding the vertex within that mesh nearest to the point (`[x]`, `[y]`), and changing both its location and direction vector to match the vertex within the mesh resource named `[anchor]` that is nearest to the point (`[x]`, `[y]`).  Both `[x]` and `[y]` must be integers in range [0, 16384], where (0, 0) is the __bottom__ left corner of the mesh coordinate system and (16384, 16384) is the __top__ right corner of the mesh coordinate system.

    [anchor] [edit] [x] [y] anchor_seam -

Same as the `anchor` operator, except that only the location of the vertex is adjusted.  The direction vector is not modified.

    [m1] [m2] [x] [y] fuse -

Edit the two mesh resources with names `[m1]` and `[m2]` that have already been declared.  Find the vertex within each mesh that is nearest to the point (`[x]`, `[y]`), and change the locations and directions of both vertices so that the vertices are the same in both meshes, with the location and direction values being the average of the original values within the two meshes.  Both `[x]` and `[y]` must be integers in range [0, 16384], where (0, 0) is the __bottom__ left corner of the mesh coordinate system and (16384, 16384) is the __top__ right corner of the mesh coordinate system.

    [m1] [m2] [x] [y] fuse_seam -

Same as the `fuse` operator, except that only the locations of the vertices are adjusted.  The direction vectors are not modified.
