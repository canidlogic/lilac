# Lilac Core

Lilac Core contains the program entrypoint and all core modules of Lilac.  Lilac Core is entirely self-contained, except for a lone call to a function called `plugin_init()` which is not defined within Lilac Core.  This plug-in function is used to call all plug-in modules, which can then use Lilac Core functions to register script operations and render preparation functions.  See the `plugin` directory for further information.

For build instructions and other specifics, see the documentation at the top of `lilac.c`.  This README provides an overview of the system.

## Basic architecture

Each pixel is computed independently from all other pixels.  The X and Y coordinates of the current pixel are available during rendering, but no information from other pixels can be used.

The Lilac rendering script creates a graph that describes the algorithm used to determine the color of each pixel.  This graph contains nodes that perform various operations and combine information from other nodes.  The root node is a single node in this graph that determines the color of each pixel.  That root node can refer directly or indirectly to other nodes in the graph.

## Script header

Lilac rendering scripts are [Shastina scripts](https://github.com/canidlogic/libshastina) that begin with the following header:

    %lilac 1.0;

Specifically, the first Shastina entities read from the file must be:

1. `BEGIN_META`
2. `META_TOKEN` with case-sensitive value `lilac`
3. `META_TOKEN` with version number
4. `END_META`

The version number is a non-empty sequence of decimal digits, a period, and another non-empty sequence of decimal digits.  Both decimal digit sequences have the following rule:  if they are more than one digit long, then the first digit may not be zero.  The first decimal digit sequence is the major version number and the second decimal digit sequence is the minor version number.  Version 1.5 is a lower version than version 1.12.

Lilac interpreters following this specification should fail with an error if the major version number is not exactly one.  If the major version number is exactly one but the minor version is greater than zero, then the Lilac interpreter should provide a warning that the script targets a more recent version of the Lilac interpreter but still try to interpret the file.

Following the header line, there is a sequence of one or more additional Shastina metacommands that establish global parameter values.  The last of these Shastina metacommands must be `%body;` which ends the header of the Lilac rendering script and begins the body.  After the body begins, header metacommands are no longer allowed.

The following subsections document the header metacommands.

### Dimension metacommands

The dimension metacommands establish the pixel dimensions of the output image.  Exactly one dimension metacommand must appear within the header.  There are two different metacommands to choose from:

    %dim 1024 768;
    %frame "path/to/image.png";

The `%dim` metacommand directly gives the width and the height in pixels.  Both must be unsigned decimal integers that are greater than zero.  The `%frame` metacommand takes a quoted string that is the path to a PNG image file.  The dimensions of the Lilac output image will match the dimensions of this PNG image file.

The path string passed to the `%frame` metacommand must be double-quoted and may contain only US-ASCII characters in range [0x20, 0x7E], except neither backslash nor double-quote characters may be used within the string.  On Windows, use forward slash as the path separator, and the Lilac interpreter will automatically change these two backslashes.

Lilac interpreters may impose limits on maximum output image dimensions.

### External metacommands

The external metacommands establish limits for buffering pixel data from external files referenced by external nodes:

    %external-disk-mib 1024;
    %external-ram-kib  16;

The first metacommand sets the limit for the disk file pixel buffer to an unsigned decimal integer value measured in MiB units, each of which is 1,048,576 bytes.  The second metacommand sets the limit for the in-memory pixel buffer to an unsigned decimal integer value measured in KiB units, each of which is 1,024 bytes.

The size given in both metacommands must be at least enough to cover all the data for a single pixel or an error occurs.  Lilac interpreters may impose limits on the maximum supported values.  Higher values will be faster but consume more space, while lower values will consume less space but be slower.

Both of these metacommands are optional.  If not specified, default values chosen by the Lilac interpreter will be used instead.

### Complexity metacommands

Complexity metacommands establish limits on script complexity and memory usage.

    %graph-depth  64;
    %stack-height 32;
    %name-limit   1024;

The `%graph-depth` metacommand must be an unsigned decimal integer greater than zero.  Node objects that do not refer to any other node objects have a depth of one.  Node objects that refer to other node objects have a depth one greater than the maximum depth of a referred node object.  The graph depth therefore establishes a limit on how many function calls are made when computing a pixel color.  Higher depth values allow for more complex graphs, but are more likely to exhaust the process stack space, which will lead to a program crash.

The `%stack-height` metacommand must be an unsigned decimal integer greater than zero.  It specifies the maximum number of elements that can be on the Shastina interpreter stack at a time.

The `%name-limit` metacommand must be an unsigned decimal integer zero or greater.  It specifies the total number of Shastina variables and constants that can be defined.

## Script trailer

The body of the script must end with the `|;` Shastina EOF token.  Nothing except whitespace and blank lines is allowed after the EOF token.

When the trailer is reached, a single value of the node type must be on the interpreter stack, or an error occurs.  This node value is the root node that is used for the Lilac rendering graph.

## Data types

The following data types may be used within a Lilac rendering script:

1. Float
2. Color
3. String
4. Node

Floats are floating-point numbers that may have any finite value.

Colors are 32-bit ARGB values where each channel is an 8-bit integer.  RGB values are sRGB, and the alpha channel is a linear value that is non-premultiplied with respect to the RGB channels.

Strings are sequences of US-ASCII characters.  Strings may have at most 1023 characters and they may also be empty.  They can contain any ASCII characters in range [0x20, 0x7E] except for backslash and double quotes.  When used on Windows platforms, the Lilac interpreter will automatically change forward slashes in string literals to backslashes.

Nodes are functions that map each (X, Y) coordinate in the output image to an ARGB color value in the format specified for colors above.  Lilac has an extensible framework where nodes are defined abstractly and there are no built-in concrete node implementations.  All node types are defined as plug-ins to the core Lilac architecture.

## Shastina entities

`STRING` entities cause a literal string value to be pushed onto the interpreter stack.  Any US-ASCII characters in range [0x20, 0x7E] are allowed in the string literal except for backslash and double-quote.  Forward slashes will automatically be converted into backslashes in the loaded strings on Windows platforms.  Strings may be empty of length zero.  Lilac implementations may impose limits on maximum string length.  String literal entities must be double-quoted and not have any string prefix.

`NUMERIC` entities cause a literal floating-point value to pushed onto the interpreter stack.  The C library's `strtod()` parser is used to parse the numeric string into a floating-point value.  Parsing must be successful and result in a finite value.

`VARIABLE` and `CONSTANT` entities declare variables and constants in the interpreter namespace.  Both variables and constants share the same namespace.  Names are case sensitive and must consist of 1 to 31 US-ASCII alphanumerics or underscores, where the first character must be an alphabetic letter.  It is an error to declare the same name more than once.  A value of any type is popped off the interpreter stack and used to initialize the variable or constant.  The `%name-limit` metacommand in the header controls the maximum number of variables and constants that may be declared.

`ASSIGN` entities pop a value of any type off the stack and assign that value to a declared variable, overwriting the previous value of that variable.  The variable must have been previously declared with a `VARIABLE` entity.  An error occurs if an `ASSIGN` entity is used with the name of a declared constant.

`GET` entities retrieve the current value of a declared variable or constant and push that value on top of the interpreter stack.  The variable or constant name must have been previously declared with a `VARIABLE` or `CONSTANT` entity.  The value assigned to the variable or constant is not affected.

`BEGIN_GROUP` and `END_GROUP` are used for grouping, which is a form of syntax checking.  When `BEGIN_GROUP` occurs, all values currently on the stack are hidden.  When `END_GROUP` occurs, there must be exactly one non-hidden element on the stack, and then the elements that were hidden at the start of the group are restored.  Groups may be nested.  Non-empty arrays implicitly create groups around each of the array elements.

`ARRAY` entities push the array count on top of the stack as a floating-point value that is equal to the integer number of array elements.  Double-precision floating-point values are used, so exact integer counts can be represented up to `(2^53 - 1)`.  The `ARRAY` entity occurs at the closing `]` token of the array.  Arrays may be empty and they may be nested.

`OPERATION` entities perform an operation.  Operations may pop and push values from the interpreter stack for sake of input and output.  The core Lilac interpreter does not have any built-in operations, but rather provides an extensible method where modules can register operations with the interpreter.
