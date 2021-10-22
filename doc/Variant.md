# Lilac variant type

## 1. Introduction

This document describes the _variant_ data type that is used to store data values used in Lilac script interpretation.

The Lilac variant data type is a structure `LLC_VAR` that is defined in the `lilac_variant.h` header.  This structure must be initialized by using the `llc_var_init()` function.  Before the structure is released, it must be reset by using the `llc_var_clear()` function.  You should only use `llc_var_` functions to manipulate variant structures.  Following these restrictions is necessary to avoid memory and resource leaks.

All data values that arise during (2nd-pass) interpretation of the Lilac Shastina format, both on the interpreter stack and in defined variable and constants, are represented by the same variant type described in this document.

## 2. Data types

The `LLC_VAR` data structure can hold several different kinds of data.  The data types supported by `LLC_VAR` are categorized in three groups:

1. Primitive types
2. Fixed object types
3. Programmable object types

### 2.1 Primitive types

The _primitive types_ are data values that are stored directly within the `LLC_VAR` data structure without any need for external reference.  Lilac variants support only three kinds of primitive types:

1. Null type
2. Integer type
3. Float type

The _null_ type is a special type that means the variant does not hold any data.  Variant structures are always initialized to the null type with `llc_var_init()` and are always cleared to the null type with `llc_var_clear()`.  The null type may also be used as a parameter type to operations as a blank placeholder to mean "no argument here."

The _integer_ type is a signed, 32-bit integer value with the same range as a 32-bit two's-complement integer.  It is mapped to the `int32_t` type defined in the standard `<stdint.h>` C header.

The _float_ type is a double-precision floating point value that is mapped to the C compiler's `double` type (which may be platform-specific).  Only finite values (as determined by the `isfinite()` macro in the standard `<math.h>` C header) are allowed to be stored in variants.

### 2.2 Fixed object types

The _fixed object types_ are data types that are stored by reference within an `LLC_VAR` data structure and that can not be reprogrammed or extended.  Lilac variants support the following kinds of fixed object types:

1. String type
2. Array type
3. Light type
4. Material type
5. Mesh type

All of these object types use reference counting to track when they may be released.

The _string_ type is a nul-terminated sequence of zero or more Unicode codepoints, encoded in UTF-8.  Strings are immutable after construction.

The _array_ type is a sequence of zero or more `LLC_VAR` data structures.  These may store any type of data, and elements do not all have to be the same type.  Arrays may have other arrays as elements as well.  Arrays are immutable after construction, and array elements are only allowed to reference objects that already existed prior to construction of the new array, so there is no possibility of self-reference or circular references (which would cause trouble with reference counting).

The _light_ type contains three `LLC_VAR` data structures.  The first must hold a reference to a normal shader, the second must hold a reference to a linear shader, and the third must hold a reference to a linear shader.  The normal shader determines the direction from each pixel to the light source, one of the linear shaders determines the diffuse RGB light projected onto each pixel, and the other linear shader determines the specular RGB light projected onto each pixel.  Lights are immutable after construction.

The _material_ type contains four `LLC_VAR` data structures.  The first three structures must hold references to linear shaders, and the fourth structure must hold a reference to an intensity shader.  The linear shaders determine the ambient reflection, diffuse reflection, and specular reflection for each pixel in the material.  The intensity shader determines the shininess constant for each pixel in the material.  Materials are immutable after construction.

The _mesh_ type is a complex data structure that contains an embedded set of points with associated coordinates and normals, as well as an embedded set of triangles connecting those points into a triangle mesh.  Meshes are self-contained objects that have no external references.  See `lilac_mesh.h` for specifics.  Mesh types are immutable after construction.

Even though all these object types are immutable, "modifications" can be made with operators that take an immutable input object and produce a copy of the object with the changes made.

### 2.3 Programmable object types

The _programmable object types_ are data types that are stored by reference within an `LLC_VAR` data structure and that are reprogrammable and extensible.  Lilac variants support the following kinds of programmable object types:

1. Nonlinear shader type
2. Linear shader type
3. Normal shader type
4. Intensity shader type
5. Environment shader type
6. Surface shader type

All of these object types use reference counting to track when they may be released.  Since the objects are programmable and extensible, there is no inherent guarantee that self-references or circular references will not arise.  Implementations of these object types are strongly recommended to follow the system of having objects that are immutable after construction and only reference objects that already existed prior to construction.  This will avoid circular and self references.

If circular or self references occur, Lilac will never be able to free the memory of the object (until all memory is freed at the end of the Lilac process).  This may be an acceptable outcome for complex objects that only have a few instances.

Each of these programmable shader types is a function implemented in C that takes the following parameters:

1. The width in pixels of the rendered image
2. The height in pixels of the rendered image
3. The X coordinate of the current pixel
4. The Y coordinate of the current pixel
5. A custom void pointer specific to the shader object

Shader functions are not invoked until after interpretation of the Lilac Shastina script has been completed.  After the script has been interpreted, Lilac renders the image using the shader architecture established during script interpretation.  (See `Architecture.md` for a broad overview of the process.)  The shader objects are invoked at the appropriate times during rendering.

Lilac guarantees that the rendered image will be rendered in a specific order, and that the width and height parameters will always be the same.  Within each scanline, pixels are always rendered from left to right, and scanlines are always rendered from top to bottom.  Shaders are only invoked on pixels that Lilac determines them necessary for.  In _preview_ mode, Lilac may only render a subset of the pixels in the rendered image, but the pixel subset will always obey the left-to-right, top-to-bottom ordering.

Note, however, that if a shader is invoked not directly by Lilac, but rather from another shader, there is no guarantee about the order of pixels of that the rendered dimensions always remain the same.  It is up to specific shader implementations to decide how to query other shaders that they reference.

The different shader types differ only in the kind of output data that they produce.  

__Nonlinear shaders__ produce a 32-bit ARGB value.  The alpha channel is non-premultiplied, linear scale, 255 means fully opaque, and zero means fully transparent.  The RGB channels are in sRGB color space, with nonlinear gamma correction applied.

__Linear shaders__ produce three floating-point values, corresponding to a red channel, a green channel, and a blue channel.  Channel values are assumed to be linear scale in a normalized space from [0.0, 1.0].  However, any floating-point values may be produced, including negative values and non-finite values.  The definition of the "color" produced by linear shaders is intentionally left open-ended to allow for flexibility.

__Normal shaders__ produce a three-dimensional vector of magnitude one.  These can be used anywhere that a 3D direction vector is required.

__Intensity shaders__ produce a single floating-point value.  This can have any floating-point value, including non-finite values.

__Environment shaders__ produce a variant reference to a fixed Array object that contains a set of references to zero or more fixed Light objects.  The Array objects may also contain references to other Array objects, and those may have further nested Arrays, provided that each Array only contains Array references and Light references.  Array nesting has no effect on the interpretation of the light set, with the final set of lights being all the Light objects contained in the given array and any nested arrays.  The ordering of Light objects does not matter.  However, Light objects that are included more than once will have cumulative effect.

__Surface shaders__ produce a variant reference to fixed Material object.

## 3. C API

The variant C API is defined in the `lilac_variant.h` header.  The header defines the `LLC_VAR` structure.  It also defines a set of functions for manipulating the structure.  Clients should never directly access the internal fields of the variant structure, instead relying on the functions provided by the header.

### 3.1 Base functions

    void llc_var_init(LLC_VAR *pv);

Initializes a variant data structure.  This function must only be used on variant structures that have not yet been initialized &mdash; using the function on a variant that is already initialized risks a memory leak.  All other functions provided by the variant API assume that the variant structure has been initialized.

    void llc_var_clear(LLC_VAR *pv);

Resets a variant back to holding a null value.  Variant structures must be cleared with this function before they go out of scope or their memory is released to prevent memory leaks.  This is also the function used to write a null value into a variant.

    void llc_var_copy(LLC_VAR *pDest, const LLC_VAR *pSrc);

Copies the value or reference from one variant into another variant, overwriting the value or reference in the target variant.  If the destination and source variant structures are the same, the function does nothing.

    int llc_var_type(const LLC_VAR *pv);

Determine the type of value currently stored in the given variant.  The return value is one of the following constants that is defined in the header:

1. `LLC_VAR_NULL`
2. `LLC_VAR_INT`
3. `LLC_VAR_FLOAT`
4. `LLC_VAR_STRING`
5. `LLC_VAR_ARRAY`
6. `LLC_VAR_LIGHT`
7. `LLC_VAR_MATERIAL`
8. `LLC_VAR_MESH`
9. `LLC_VAR_NONLINEAR`
10. `LLC_VAR_LINEAR`
11. `LLC_VAR_NORMAL`
12. `LLC_VAR_INTENSITY`
13. `LLC_VAR_ENVIRONMENT`
14. `LLC_VAR_SURFACE`

### 3.2 Numeric functions

This section describes the variant functions that work with integer and floating-point values.  Note that the `llc_var_getFloat` function automatically promotes integer values to floating-point values.

    void llc_var_setInt(LLC_VAR *pv, int32_t val);

Writes an integer value into a variant structure, overwriting the value or reference currently in the variant.

    void llc_var_setFloat(LLC_VAR *pv, double val);

Writes a double value into a variant structure, overwriting the value or reference currently in the variant.  The given value must pass `isfinite()`.

    int32_t llc_var_getInt(const LLC_VAR *pv);

Gets the integer value currently stored in the variant structure.  A fault occurs if something other than an integer is currently stored in the variant.

    double llc_var_getFloat(const LLC_VAR *pv);

Gets the floating-point value currently stored in the variant structure.  If an integer is currently stored in the variant, this function returns the floating-point conversion of the integer value.  (The contents of the variant are not affected.)  A fault occurs if something other than an integer or floating-point value is currently stored in the variant.  The return value is guaranteed to pass `isfinite()`.

### 3.3 String and array functions

This section describes the variant functions that work with string and array references.

    void llc_var_setString(LLC_VAR *pv, const char *pc);

Creates a new string object, copies the given nul-terminated string into the string object, and writes a reference to the new string object into the given variant structure, overwriting any value or reference currently stored in the variant.  Since a copy is made of the string data, changes to the memory buffer pointed to by the `pc` parameter after this function call do not affect the string object.  The function does not check string encoding, though UTF-8 should be used.  A fault occurs if the given string is longer than an implementation-defined maximum length.

    const char *llc_var_getString(const LLC_VAR *pv);

Gets a reference to the nul-terminated string stored in the string object referenced by a variant.  A fault occurs if the given variant does not currently hold a string object reference.  The returned string pointer remains valid until the string object is released.  This means that you should hold a variant reference to the string object while you are using the string pointer, to make sure the string isn't released until you are finished with the pointer.

    void llc_var_array(
              LLC_VAR * pv,
        const LLC_VAR * pa,
              int32_t   count);

Create a new array object from an existing sequence of variant structures.  `pa` points to the first variant structure in the sequence while `count` is the total number of structures in the sequence pointed to by `pa`.  If `count` is zero, then `pa` is ignored and may be set to `NULL`.  The newly-created array object has an internal copy of the provided sequence of variant structures, so modifying or freeing the variant sequence provided as an input parameter after the call returns does not modify the created array object.  A reference to the newly-created array object will be written into `pv`, overwriting any reference or value currently stored therein.  A fault occurs if the `count` is greater than an implementation-defined maximum array length.  `pv` may safely point to one of the structures in the input variant sequence.

    int32_t llc_var_count(const LLC_VAR *pv);

Return the number of elements in the array object referenced from the given variant structure.  Zero may be returned if the array object is empty.  A fault occurs if the given variant structure does not hold an array reference.

    void llc_var_index(
              LLC_VAR * pDest,
        const LLC_VAR * pArray,
              int32_t   i);

Get a copy of a specific array element.  `pArray` must point to a variant structure holding a reference to an array.  `i` is the zero-based index of the array element to return.  `i` must be less than the count of array elements, which can be determined by `llc_var_count()`.  The value or reference stored within the indicated array element will be copied to the variant structure `pDest`, overwriting the current reference or value stored within that structure.  `pDest` may point to the same variant structure as `pArray`.

    void llc_var_concat(
              LLC_VAR * pDest,
        const LLC_VAR * pa,
              int32_t   count);

Concatenate a sequence of strings or arrays together.  `pa` points to the first variant structure in the sequence of source objects while `count` is the total number of structures in the sequence pointed to by `pa`.  `count` must be greater than zero.  All variants in the sequence of source objects must be references to arrays or all variants in the sequence of source objects must be references to strings.  `pDest` may safely point to one of the variant structures in this input sequence; the concatenated object is computed before writing the result into the destination variant.  The result object is a concatenation of all the characters in all the source strings (if sources are strings), or a concatenation of all the elements in all the source arrays (if sources are arrays).  A reference to this result will be written into `pDest`, overwriting any value or reference currently held in that variant structure.  A fault occurs if not all source variants are of the same type, or if any source variant is neither a string nor an array reference.  A fault also occurs if the concatenated length would cross an implementation-defined maximum string or array length.

## 3.4 Light and material functions

This section describes the variant functions that work with light and material object references.  There are separate functions for constructing light and material objects, but one unified function for retrieving properties of both lights and materials.

    void llc_var_light(
              LLC_VAR * pDest,
        const LLC_VAR * pDirection,
        const LLC_VAR * pDiffuse,
        const LLC_VAR * pSpecular);

Construct a new light object and write a reference to the new object into the `pDest` variant structure, overwriting any value or reference currently in the variant.  The result is written only after the object is fully constructed, so `pDest` may point to the same variant structure as any of the other parameters.  `pDirection` must point to a variant that holds a reference to a normal shader, which will be used to determine the direction to the light.  `pDiffuse` must point to a variant that holds a reference to a linear shader, which will be used to determine the diffuse RGB lighting at each point.  `pSpecular` must point to a variant that holds a reference to a linear shader, which will be used to determine the specular RGB lighting at each point.  The diffuse and specular shaders may be the same.  Each shader reference is copied into the constructed light object.

    void llc_var_material(
              LLC_VAR * pDest,
        const LLC_VAR * pAmbient,
        const LLC_VAR * pDiffuse,
        const LLC_VAR * pSpecular,
        const LLC_VAR * pShiny);

Construct a new material object and write a reference to the new object into the `pDest` variant structure, overwriting any value or reference currently in the variant.  The result is written only after the object is fully constructed, so `pDest` may point to the same variant structure as any of the other parameters.  The ambient, diffuse, and specular parameters must all point to variant structures that hold references to linear shaders which define the RGB ambient lighting multiplier, diffuse lighting multiplier, and specular lighting multiplier, respectively.  The same variant structure may be used for ambient, diffuse, and specular, if desired.  `pShiny` must point to a variant structure containing a reference to an intensity shader that determines the shiny constant.  Each shader reference is copied into the constructed material object.

    void llc_var_element(
              LLC_VAR * pDest,
        const LLC_VAR * pSrc,
              int       ele);

Retrieve a reference to a component shader of a light or material object and write the reference into the variant structure `pDest`, overwriting any value or reference currently stored in that variant.  `pSrc` must be a pointer to a variant holding a reference to either a light or a material object.  `pDest` and `pSrc` may point to the same variant structure, in which case the result will still be properly extracted.  `ele` indicates which component shader to retrieve.  The following constants are defined in the header:

1. `LLC_VAR_AMBIENT`
2. `LLC_VAR_DIFFUSE`
3. `LLC_VAR_DIRECTION`
4. `LLC_VAR_SPECULAR`
5. `LLC_VAR_SHINY`

A fault occurs if `ele` is not valid for the object type.  Lights support `DIRECTION`, `DIFFUSE`, and `SPECULAR`, while materials support `AMBIENT`, `DIFFUSE`, `SPECULAR`, and `SHINY`.

@@TODO: