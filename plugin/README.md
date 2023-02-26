# Lilac Plugin

This part of Lilac manages registration of all plug-in modules with Lilac Core.

A single module is defined here: `plugin`.  This defines the `plugin_init()` function that Lilac Core expects to be defined.  The implementation of this function should call into all plug-in module initialization functions.

Plug-ins register themselves with Lilac Core in two different ways.  The `vm_register()` function in the `vm` module of Lilac Core allows plug-ins to define operations that rendering scripts can invoke.  The `render_prepare()` function in the `render` module of Lilac Core allows plug-ins to define preparation functions that will run after rendering script interpretation is complete but before rendering starts.
