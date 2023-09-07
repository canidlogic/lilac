# Lilac plug-in architecture

Lilac uses a plug-in architecture for extensibility.

The core Lilac program is contained with the `core` directory.  This core program is entirely self contained, with one exception.  The exception is that the core program imports `plugin.h` from the `plugin` directory and makes a call to the `plugin_init()` function from this module near the beginning of the program.

Each plug-in module should modify `plugin.c` to import its header and make a call to the plug-in module's registration function.  This registration function can then use the public API of the core Lilac program to register the extensions defined by the plug-in module.

The core Lilac program is only designed to provide a framework which can then be built on by plug-in modules.  You will need some sort of plug-ins to get Lilac to do something useful.  This Lilac distribution includes a `base` directory with plug-ins that establish a useful baseline of functionality.  The plug-ins in this `base` directory are already registered with the plug-in module.
