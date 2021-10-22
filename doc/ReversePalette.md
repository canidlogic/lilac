# Lilac reverse palette format

This document describes the format of text files used to specify reverse palette resources.  For further information about the use of reverse palettes, see `LoadScript.md` and `RenderScript.md`.

A reverse palette file is a US-ASCII text file.  It may optionally begin with a UTF-8 Byte Order Mark (BOM), which is ignored.  Line breaks may be either LF or CR+LF.  CR characters may only occur immediately before LF.  UTF-8 characters may be used within comments, but they will be ignored and not decoded by Lilac.

Reverse palette files are processed as a sequence of one or more lines, where the last line ends with the end of the file, and any lines prior to the last line end with either LF or CR+LF.  The line break characters CR and LF are _not_ counted as part of the line.

There is a fixed limit on line length, which will be at least 1023 bytes, not including line break characters.

Lines that are empty or consist of nothing but space and horizontal tab are blank lines, which are ignored.

Lines for which the first non-space, non-tab character is `#` are comment lines and are ignored.

All other lines are record lines, which have the following format:

1. Optionally, a sequence of space and/or tab characters
2. RGB color value (see below)
3. Optionally, a sequence of space and/or tab characters
4. Double quote `"`
5. Zero or more data characters (see below)
6. Double quote `"`
7. Optionally, a sequence of space and/or tab characters
8. Optionally, `#` followed by a comment

The RGB color value is a sequence of exactly six base-16 digits (uppercase and lowercase both allowed) that give the RRGGBB value of a color.

Data characters include all visible, printing US-ASCII characters except the double quote.  The `#` character is allowed as a data character &mdash; when present within a double-quoted value, it does __not__ indicate the start of a comment.

Each record maps a specific RGB value to a sequence of zero or more data characters.  Each RGB value must be unique.  However, the data character sequences do _not_ need to be unique.  Empty data character sequences are also supported.
