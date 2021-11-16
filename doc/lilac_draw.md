# Lilac Draw manual

## 1. Introduction

`lilac_draw` is a program for synthesizing color drawings out of separate image components.

## 2. Syntax

The syntax of the Lilac drawing program is:

    lilac_draw [out] [mask] [pencil] [shading] [table] [texture_1] ... [texture_n]

The `[out]` parameter is the path to write the output image file.  The path must have a PNG format extension.

The `[mask]` parameter is the path to an image file to read as a mask file.  The path must have a PNG format extension.

The `[pencil]` parameter is the path to an image file to read as the pencil file.  The path must have a PNG image format extension.

The `[shading]` parameter is the path to an image file to read as the shading file.  The path must have a PNG image format extension.

The `[table]` parameter is the path to a text file specifying shading information.  The format of this file is described in section 2.1 "Table file syntax".

The `[texture_1]` ... `[texture_n]` is an array of parameters specifying paths to image files to read as texture files.  Each path must have a PNG image format extension.  There must be at least two textures.  The first texture is always the background (paper) texture, and the second texture is always the pencil texture.

### 2.1 Table file syntax

The table file is a plain-text ASCII text file with the following syntax.

The table file is read line-by-line.  Each line ends with CR+LF, LF, or the end of the file.  Each line, not including the line break, must fit within 255 characters.

Lines that are empty or consist only of spaces and horizontal tabs are blank lines.  Blank lines are ignored.

Lines for which the very first character is a `#` are comment lines and are ignored.

All other lines are shading records, which must have the following format:

1. RGB shading index
2. _whitespace_
3. Texture index
4. _whitespace_
5. Shading rate
6. _whitespace_
7. Drawing rate
8. _whitespace_
9. RGB tint
10. _optional whitespace_

The _whitespace_ entries mean at least one space or tab character, while the _optional whitespace_ entry means zero or more space or tab characters.  Note that no whitespace is allowed at the beginning of the line.

The two RGB fields are specified as exactly six base-16 digits (uppercase or lowercase letters) specifying RRGGBB.  The shading index is the RGB color within the shading image file that selects this particular record.  The shading image file therefore acts like an index into the shading table, where the RRGGBB value of each pixel in the shading image is matched with the RGB shading index value of a record in the shading table.  The specific RRGGBB value used as a shading index has no meaning; it must simply be a unique identifier of a shading record.

The tint is the RGB color used for colorization.  Colorization is the last step in rendering before a pixel is output.  During colorization, the input RGB value is first converted to a grayscale value, keeping the brightness but discarding the color.  If the tint value is grayscale (RGB channels have equal values), then the output RGB from the colorizer is the grayscale conversion of the input RGB value.  If the tint value is not grayscale, then the tint is converted into HSL, the L channel is replaced with the input grayscale value, and the adjusted HSL value is then converted back to RGB for the final output value.  If the input grayscale value is pure white, then the output of the colorizer will always be pure white.  If the input grayscale value is pure black, then the output of the colorizer will always be pure black.

In short, the colorizer combines the brightness of the input RGB value with the color of the tint to form the output value.

To disable the colorizer, set a tint value of FFFFFF.  In this case, no colorization step will be performed.

The texture index is an unsigned integer that selects one of the texture files that was passed to the Lilac program.  A value of one selects the first texture file, two selects the second, and so forth.  This texture index will only be used for pixels that are shaded but not covered by the pencil.  For pixels that are covered by the pencil, the second texture will always be selected, and the texture index in the shading record will be ignored.

The shading rate and drawing rate are both unsigned integers in range zero to 255, with zero meaning a complete transparency and 255 meaning a completely opacity.  The alpha channel value of the pixel from the texture is combined with the shading or drawing rate to form the shaded RGB value.  If the pixel is covered by the pencil, the drawing rate is used and the texture is always the second texture.  If the pixel is not covered by the pencil, the shading rate is used and the texture is the one selected by the texture index in this record.  This shaded RGB value is then composited over the background paper texture (always the first texture), and the result of this is then composited over fully opaque white.  Finally, the RGB result is then run through the colorizer (see earlier) before it is sent to output.

The table file may have zero or more shading records.  If any RGB color in the shading image does not have a corresponding entry in the table, a default record is assumed, which has a texture index of one, a shading rate of zero, a drawing rate of 255, and an RGB tint of pure white.

## 3. Operation

`lilac_draw` begins by reading all texture files fully into memory.  Note that this means that texture files should be kept as small as possible to avoid hitting memory limits.  Lilac will automatically tile textures so that they cover the entire image.  It is therefore possible to only use a small pattern for each texture which will then automatically be tiled to the full image size.  Textures may include transparent and partially transparent pixels.

Next, the table file is read and parsed into zero or more records, indexed by RGB color values.  Each record must have a unique RGB color value or there will be an error.

The mask, pencil, and shading images are then opened simultaneously.  They must each have the exact same image dimensions, or there will be an error.  The mask and pencil files are down-converted to black-and-white images by first compositing any transparent pixels over a pure white background to remove alpha, then converting RGB to grayscale if necessary, and finally thresholding with values 128 and greater being black and values 127 and lower being white.  The shading image is down-converted to RGB by compositing any transparent pixels over a pure white background to remove alpha.

The output file is opened, with the same dimensions as the mask, pencil, and shading images.  The images are processed pixel-by-pixel.  For each (_x_, _y_) coordinate, the mask and pencil pixel values are first combined to choose one of three operating modes for the current image pixel:

     Mask pixel | Pencil pixel | Selected mode
    ============+==============+===============
       black    |    black     |    PENCIL
    ------------+--------------+---------------
       black    |    white     |    SHADE
    ------------+--------------+---------------
       white    |    black     |
    ------------+--------------+    BLANK
       white    |    white     |

In `BLANK` mode, the output pixel at this coordinate is always fully transparent.  `PENCIL` and `SHADE` modes, by contrast, always generate fully opaque pixels.  `BLANK` mode is provided to support non-rectangular images, which is useful, for example, when the generated image will be a layer within a greater image.

`PENCIL` and `SHADE` modes work with the same image processing pipeline.  The only difference is that `PENCIL` mode uses the drawing rate value instead of the shading rate value, and `PENCIL` mode always uses the second texture (the pencil texture) as the input texture instead of the input texture selected by the shading table.

The first stage in the image processing pipeline is to get the RRGGBB value of the pixel at the current coordinates in the shading image.  This RRGGBB value is then used as an index to retrieve a specific shading record in the shading table.  Once the shading record has been retrieved, the RRGGBB value from the shading image is discarded and has no further bearing on the image processing pipeline.

The second stage in the image processing pipeline is to get the ARGB value of the current coordinates from one of the textures.  In `PENCIL` mode, the second texture is always used.  In `SHADE` mode, the texture selected by the shading record is used.  Tiling is automatically used if the selected texture does not fully cover the output image area.

The third stage in the image processing pipeline is to adjust the alpha value of the ARGB value from the previous stage by either the drawing rate (in `PENCIL` mode) or the shading rate (in `SHADE` mode).  The alpha value of the input ARGB value and the rate value are both mapped to the normalized floating-point range of [0.0, 1.0] and multiplied together to get the adjusted alpha channel value.  This value is then converted back to integer range [0, 255].  The output ARGB value from this stage is the same as the input ARGB value, except its alpha channel value has been adjusted as just described.

The fourth stage in the image processing pipeline is to composite the ARGB value from the previous stage over the ARGB value from the current coordinates in the first texture (the paper texture).

The fifth stage in the image processing pipeline is to composite the ARGB value from the previous stage over an ARGB value of fully opaque white.  This is used to make sure the alpha channel value is fully opaque, so the output of this pipeline stage is just an RGB value rather than an ARGB value.

The sixth and final stage in the image processing pipeline is to pass the RGB value from the previous stage through the colorizer.  The colorizer converts the input RGB value to grayscale.  It also converts the RGB tint value from the shading record into an HSL color.  The L channel value in the HSL color is replaced by grayscale value derived from the input RGB value, and this adjusted HSL color is then converted back to RGB and sent to output with the alpha channel fully opaque.

The sixth stage is skipped if the tint has the special value FFFFFF.  In this case, the output from the fifth stage goes directly to the rendered output.

## 4. Compilation

For build information, see the README file in the `cli` directory.
