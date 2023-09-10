# Lilac deep bitmaps

Lilac uses _deep bitmaps,_ which are bitmap images that have complex and potentially many-dimensional pixel formats.  Lilac has a single input deep bitmap and a single output deep bitmap, both of which have the same image dimensions but different pixel formats.  Lilac is essentially a program to transform the complex pixel format of the input deep bitmap into a different complex pixel format of the output deep bitmap.

The challenge is that the actual image input and output files are PNG files, which are _not_ deep bitmaps.  The Lilac deep bitmap system therefore needs to interleave the data of multiple input PNG images together to form a single input deep bitmap, and it needs to separate the single output deep bitmap into multiple output PNG images.  Each individual PNG image is called a _layer,_ because each PNG pixel represents one small part of a deep bitmap pixel.

The deep bitmaps used by Lilac are potentially gigantic, because they might have high-resolution image dimensions, and then very large individual pixels on top of that.  Lilac uses a two-level cache with disk file buffers to ensure it can process these deep bitmaps without running out of memory.  It also is designed such that no more than one PNG image codec is open at a single time, even when both the input and output deep bitmaps are being used simultaneously.

## Layer compression

To alleviate some of the deep bitmap storage requirements, Lilac supports a simple compression system.  This compression system only operates on individual pixels.  Normally, each PNG image layer contributes 4 bytes to each deep bitmap pixel, representing the ARGB color channels stored in the PNG image layer pixels.  However, Lilac compression allows the 4-byte ARGB color channels to be compressed down to a single byte.  This is only 1/4 the storage requirements, since there is only a single byte in a compressed layer versus four bytes in a full layer.

A _compressor_ in Lilac is a function that takes a 4-byte ARGB color from an input PNG file and returns a 1-byte layer value that will be stored in the deep bitmap pixel.  A _decompressor_ in Lilac is a function that takes a 1-byte layer value from a deep bitmap pixel and returns a 4-byte ARGB color that will be stored in an output PNG file.

The set of compressors and decompressors is extensible.  Each compressor and decompressor must have a unique name that follows the format given separately for Lilac name identifiers.  The compressors and decompressors share a namespace, such that a compressor may not have the same name as a decompressor.  It is recommended that the proper name of compressors begin with a `C` and the proper name of decompressors begin with a `D`.

## Base compression

The Lilac base modules define the following compressors and decompressors.

    lilac.CAlpha

Compressor based on alpha channel.  The 8-bit alpha channel of input PNG images is stored as the layer value, and the RGB color channels are ignored.

    lilac.CGray

Compressor based on grayscale value.  If the ARGB value from the PNG image has a fully opaque alpha channel and all three RGB channels are equal, then the compressed 8-bit value is equal to the value shared by the three RGB values.  If the ARGB value is not fully opaque, it is composited against an opaque white background.  If the RGB channels do not share a single value, they are converted to grayscale using sRGB formulas.  Note, however, that the alpha compositing and grayscale conversion algorithms of this compressor value speed and simplicity over accuracy, so it's recommended to use this compressor with images that are already fully opaque grayscale.

    lilac.CGrayInv

Same as the `lilac.CGray` compressor, except each grayscale value is inverted by subtracting it from 255.  While `lilac.CGray` compresses white to 255 and black to zero, `lilac.CGrayInv` compresses white to zero and black to 255.

    lilac.DBlack

Decompressor based on alpha channel.  The RGB channels are always set to zero for full black.  The alpha channel is set to the 8-bit layer value.

    lilac.DWhite

Decompressor based on alpha channel.  The RGB channels are always set to 255 for full white.  The alpha channel is set to the 8-bit layer value.

    lilac.DGray

Decompressor based on grayscale value.  The alpha channel is always set to 255 for full opacity.  The RGB channels are each set to match the 8-bit layer value, such that layer value 255 will be full white and layer value zero will be full black.

    lilac.DGrayInv

Same as the `lilac.DGray` decompressor, except each grayscale value is inverted by subtracting it from 255.  In this case, layer value 255 will be full black and layer value zero will be full white.

## Input interface

The input deep bitmap is initialized with a sequence of zero or more layer definitions.  Each layer definition specifies the path to a PNG file corresponding to that layer (as a `lilac.String`), and the name of the compressor to use for the layer (as a `lilac.atom`) or a `null` value to indicate no compression is used on the layer.

Each of the input PNG files must have the exact same image dimensions.  It is also possible to explicitly provide the image dimensions which each input PNG file must match.  This declaration can be made in the header of the Lilac script.  The image dimension declaration is required if there are no layer declarations, because there is no way to determine the image dimensions automatically if there are no input images.

Layer definitions each return a `lilac.int` that indicate the layer index, which is zero or greater.  This layer index can then be used as a port number in the special core layer node at node index one.  Given a layer index, the input deep bitmap can return the offset of the first byte within each deep pixel of this layer, and whether this particular layer is compressed (1 byte) or uncompressed (4 bytes).  The input deep bitmap is also able to return how many total layers have been declared.

After all layer declarations are made, the input deep bitmap provides a function that takes an X and Y coordinate and returns a read-only pointer to a sequence of bytes representing that particular deep pixel.  (This function can't be used if no layers were declared.)

During rendering, each pixel process begins by using the input deep bitmap function to get a pointer to the deep pixel (unless there are no layers).  Then, port queries to the core layer node at node index one are routed to the corresponding layers in that deep pixel using the layer mapping functions.

## Output interface

The output deep bitmap is initialized with image dimensions taken from the input deep bitmap and also with a sequence of zero or more layer definitions.  Each layer definition specifies the path to a PNG file that will be written for that layer (as a `lilac.String`), and the name of the decompressor to use for the layer (as a `lilac.atom`) or a `null` value to indicate no compression is used on the layer.

Layer definitions each return a `lilac.int` that indicate the layer index, which is zero or greater.  This layer index can then be used with port listeners that copy values from a specific port into a specific output layer after all the nodes have been computed for each pixel.  Given a layer index, the output deep bitmap can return the offset of the first byte within each deep pixel of this layer, and whether this particular layer is compressed (1 byte) or uncompressed (4 bytes).  The output deep bitmap is also able to return how many total layers have been declared.

After all layer declarations are made, the input deep bitmap provides a function that takes an X and Y coordinate and returns a read-write pointer to a sequence of bytes representing that particular deep pixel.  (This function can't be used if no layers were declared.)

During rendering, each pixel process ends with using port listeners to copy values from ports into output layers for the deep pixel at the current coordinates.  After all rendering is completed for all pixels, a compilation function is invoked which outputs each individual layer to a separate PNG file.

## Disk file cache

Both input and output deep bitmaps use disk file caches.  For the input deep bitmap, the disk file cache must be a multiple of the size of each deep pixel, but it does _not_ need to be large enough to fit the entire input deep bitmap.  For the output deep bitmap, the disk file cache must be large enough to hold the entire output deep bitmap (which is less of a problem because there are typically far fewer output layers than input layers).

Since the output deep bitmap cache stores the entire deep bitmap, its operation is straightforward.

The input deep bitmap is more complex because it does not necessarily cache the entire deep bitmap.  Instead, the input deep bitmap is divided into a number of sections, each of which has a size matching the disk file cache except for the last section, which includes any remaining data in the deep bitmap.

The input memory cache (described later) requests deep pixels starting at a specific offset, for a given maximum number of deep pixels.  If the given pixel offset is within the currently loaded section of the input deep bitmap, the input disk file cache then returns a number of deep pixels equal to the minimum of the requested number of deep pixels and the remaining number of deep pixels in the currently loaded section of the input deep bitmap.

If the given pixel offset is not within the currently loaded section (or if no section is currently loaded), the specific section that needs to be loaded is determined.  Then, each of the input PNG files is read to fill the disk file cache with the interleaved data for the requested section.  For each input PNG file, the read operation skips ahead to the start of the needed pixel data and then reads only the requested range before closing the file.

The size of the input disk file cache can be configured in the header of the Lilac script.

## Memory cache

On top of the disk file cache, both the input and output deep bitmap have memory caches.  The size of the memory caches must be multiples of the size of the respective deep pixels.

The memory caches divide the deep bitmap into a number of pages, with all pages except the last having the same size as the memory cache.  When a request is made for a page that is not currently loaded, the input bitmap will make read requests of the disk file cache until the whole page is filled.  The output bitmap will first flush any data out to the disk file cache if the page is dirty and then make a single read to load the new page from the disk cache.

The size of the input and the output memory cache can be configured in the header of the Lilac script.
