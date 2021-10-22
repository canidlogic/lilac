# Lilac documentation

This directory contains additional documentation beyond the `README.md` file in the root project directory.

`LoadScript.md` documents the loading script format.  This is the main Shastina script, which is passed on the command line to the Lilac program.  The loading script is run before the rendering script.

`MeshFormat.md` documents the Shastina format that is used to specify triangle mesh resources that can be loaded by Lilac.

`ReversePalette.md` documents the plain-text reverse palette format that is used to specify reverse palette resources that can be loaded by Lilac.

`RenderScript.md` documents the conventions of the Lua rendering script.  The rendering script runs after all resources have been loaded by the loading script.  The rendering script is what actually renders the image.
