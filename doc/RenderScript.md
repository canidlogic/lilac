# Lilac render script format

## 1. Overview

Lilac render scripts are [Lua](https://www.lua.org/) scripts that are used to determine the color of each pixel.  They are referenced from Lilac load scripts &mdash; see `LoadScript.md` for further information.

The render script is not run until Lilac has loaded all resources declared in the load script.  After all resources are loaded from the loading script, Lilac runs any top-level code in the render script.  This top-level code must define a rendering function and may perform any initialization tasks.  Lilac makes API functions available in Lua that the top-level code can use to check for the presence of resources, the types of resources, and the main parameters established in the loading script.

The render script must declare a function with a name that matches the render function named established by the loading script (which has a default name of `render` unless explicitly declared as something else).  After the top-level code in the rendering script is run, Lilac will call this rendering function once at each pixel to determine what color value should be written for the pixel in the output image.  The render function can use API functions made available by Lilac to determine the data from each loaded resource that is available for the current pixel location, and make use of that data to compute the pixel color.

## 2. Error handling

If the rendering script needs to stop on an error, use the built-in Lua `error()` function.  Lilac will properly catch errors that occur during the initial run of top-level code or during a call to the render function.  Once an error is thrown, Lilac will not invoke the rendering script again.

If the error object is a string or a number, Lilac will print this as an error message, and indicate that it originates from the rendering script.

## 3. Core Lilac API

The core Lilac API are a set of C callback functions that are made available in Lua to the rendering script.  These functions can be used at any time, both within a call to the render function and within the initial run of the top-level code.

    lilac_rendering() -> [boolean]

Returns `true` if the render script is currently in the midst of a Lilac call to the rendering function, or `false` if the render script is currently running the top-level code before any calls to the rendering function.

    lilac_dim() -> [w:integer], [h:integer]

Returns two integers, which are the width and the height in pixels of the output image.  Both integers are greater than zero.  The dimensions will not change, so it is possible to call this function once during initialization and cache the values.

    lilac_rfunc() -> [string]

Returns a string, which is the name of the render function that will be called.  The render script must define a rendering function with a global name matching the string given here.  This value will not change.

    lilac_type(name:string) -> [string]

Return a string describing the type of a particular named resource.  Pass the name of the resource as a string.  The return value will be one of the following strings:

- `undef` &mdash; no resource defined with that name
- `image` &mdash; either full-frame or texture
- `rpal` &mdash; reverse palette
- `mesh` &mdash; mesh

The mapping of resource names to types is constant and does not change, so you can check this during initialization and then assume it stays the same.

    lilac_log(msg:string)

Print a log message.  Lilac will print this message to its `stderr` stream, with a header indicating the message is from the rendering script.

## 4. Lilac rendering API

The Lilac rendering API is only available during calls from Lilac to the rendering function.  During the top-level initialization code, the rendering API functions are defined, but calling them will result in an error.

    lilac_pos() -> [x:integer], [y:integer]

Get the current pixel position.  Both coordinates are zero or greater.  The X coordinate is less than the width of the output image and the Y coordinate is less than the height of the output image.  Lilac will also guarantee that pixels are rendered starting at the top-left corner of the image, rendering each scanline from left to right, and then rendering scanlines from top to bottom.  However, only a subset of pixels may be rendered if the Lilac program was invoked in a preview reduction mode.

    lilac_image(name:string) -> [r:float], [g:float], [b:float], [a:float]

Get the current color of a specified image resource at the current pixel coordinates.  The given name must be of a defined resource that has an `image` type or an error occurs.  The function returns four floating-point values for the red, green, blue, and alpha channel values, respectively.  Each is in normalized range [0.0, 1.0].  Additionally, the RGB channels are linear, without any gamma correction.

    lilac_rpal(paln:string, imgn:string) -> [string]

Get the current color of the image resource named `imgn` at the current pixel coordinates, and then run this color through the reverse palette resource named `paln` to map the color to a string value, and return the mapped string value.  The given resources must exist and have the proper types, or an error occurs.  The current color in the image resource must exist as a reverse palette entry, or an error occurs.

    lilac_mesh(name:string) -> [normd:float], [norma:float]

Get the current direction vector of a specified mesh resource at the current pixel coordinates.  The given name must be of a defined resource that has the `mesh` type or an error occurs.  The triangle mesh must cover the current pixel coordinate or an error occurs.  The returned `normd` and `norma` values are both normalized values in range [0.0, 1.0].  See `MeshFormat.md` for the interpretation of these parameters.

## 5. Render function callback

The rendering script must define a rendering function.  By default, the name of this function is expected to be `render` but the expected name can be changed by the load script.  The API call `lilac_rfunc()` can determine the expected name of the function.

Assuming the default render function name of `render`, the rendering function has the following syntax:

    render() -> [r:float], [g:float], [b:float], [a:float]
    render() -> [r:float], [g:float], [b:float]
    render() -> [i:float]

If the function returns four values, it returns an RGBA color.  If the function returns three values, it returns an RGB color with the alpha channel assumed to be fully opaque.  If the function returns one value, it returns a grayscale value with the alpha channel assumed to be fully opaque.

All channel values are in range [0.0, 1.0] __and they are linear__ without any gamma correction applied.  Lilac will handle applying the proper gamma correction.

The returned values must either be Lua numbers or strings that are convertible to Lua numbers.
