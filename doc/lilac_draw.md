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

The `[table]` parameter is the path to a text file specifying shading information.  The format of this file is described later.

The `[texture_1]` ... `[texture_n]` is an array of parameters specifying paths to image files to read as texture files.  Each path must have a PNG image format extension.  There must be at least two textures.

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
7. RGB tint
8. _optional whitespace_

The _whitespace_ entries mean at least one space or tab character, while the _optional whitespace_ entry means zero or more space or tab characters.  Note that no whitespace is allowed at the beginning of the line.

The two RGB lines are specified as exactly six base-16 digits (uppercase or lowercase letters) specifying RRGGBB.  The shading index is the RGB color within the shading image file that selects this particular record.  The tint is the RGB color used for the colorization step (see later).

The texture index is an unsigned integer that selects one of the texture files that was passed to the Lilac program.  A value of one selects the first texture file, two selects the second, and so forth.

The shading rate is an unsigned integer in range zero to 255 that indicates the opacity of the brush texture, with zero meaning a completely transparent brush texture and 255 meaning a completely opaque brush texture.

The table file may have zero or more shading records.  If any RGB color in the shading image does not have a corresponding entry in the table, a default record is assumed, which has a texture index of one, a shading rate of zero, and an RGB tint of pure white.

The meaning of the table is explained more fully in the section detailing the program operation.

## 3. Operation

`lilac_draw` begins by reading all texture files fully into memory.  Note that this means that texture files should be kept as small as possible to avoid hitting memory limits.  Lilac will automatically tile textures so that they cover the entire image.  It is therefore possible to only use a small pattern for each texture which will then automatically be tiled to the full image size.  Textures may include transparent pixels.

Next, the table file is read and parsed into zero or more records, indexed by RGB color values.  Each record must have a unique RGB color value or there will be an error.

The mask, pencil, and shading images are then opened simultaneously.  They must each have the exact same image dimensions, or there will be an error.  The mask and pencil files are down-converted to black-and-white images by first compositing any transparent pixels over a pure white background to remove alpha, then converting RGB to grayscale if necessary, and finally thresholding with values 128 and greater being black and values 127 and lower being white.  The shading image is down-converted to RGB by compositing any transparent pixels over a pure white background to remove alpha.

The output file is opened, with the same dimensions as the mask, pencil, and shading images.  The images are processed pixel-by-pixel, with the pixel in the output file determined entirely by the corresponding pixels in the mask, pencil, and shading images.

If the mask file is a white pixel, then the output pixel is fully transparent.

If the mask file is a black pixel and the pencil file is a black pixel, then the output pixel is the corresponding pixel in the second texture, composited over the corresponding pixel in the first texture, composited over pure white, and then passed through the colorizer (described below).

If the mask file is a black pixel and the pencil file is a white pixel, then the output pixel is determined the following way.  First, the RGB value in the shading image is used to select a record in the shading table (or the default record if a corresponding RGB is not found).  Then, the corresponding pixel in the texture selected by the shading record is composited over the first texture with the transparency of the top texture multiplied by the shading rate transparency.  Any remaining transparent pixels are then composited over pure white.  Finally, the pixel is passed through the colorizer (described below).

The colorizer begins by converting the input color to a grayscale value.  Then, the colorizer uses the RGB value in the shading image to select a record in the shading table (or the default record if a corresponding RGB is not found).  The RGB value given for the tint is converted into the HSL (Hue Saturation Lightness) colorspace.  The lightness component is then replaced with the grayscale value coming from input.  The adjusted HSL value is then converted back to RGB for the final output value.

## 4. Compilation

For build information, see the README file in the `cli` directory.
