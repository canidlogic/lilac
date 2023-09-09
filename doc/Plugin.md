# Lilac plug-in architecture

Lilac uses a plug-in architecture for extensibility.

The core Lilac program is contained with the `core` directory.  This core program is entirely self contained, with the sole exception of calls to the plug-in module documented here.

The core program imports `plugin.h` from the `plugin` directory and makes a call to the `plugin_init()` function from this module near the beginning of the program.  This is used to register plug-in modules, as described below.

The core program also makes calls to the `plugin_supports()` to satisfy script requirement declarations, as described below.

## Module registration

Each plug-in module should modify `plugin.c` to import its header and make a call to the plug-in module's registration function.  This registration function can then use the public API of the core Lilac program to register the extensions defined by the plug-in module.

The core Lilac program is only designed to provide a framework which can then be built on by plug-in modules.  You will need some sort of plug-ins to get Lilac to do something useful.  This Lilac distribution includes a `base` directory with plug-ins that establish a useful baseline of functionality.  The plug-ins in this `base` directory are already registered with the plug-in module.

## Requirement support

The header of Lilac scripts may include `%require` metacommands, that look like one of the following:

    %require com.example.Requirement;
    %require com.example.Requirement "1.25";

Both syntax formats have a unique name for the requirement, which must follow the Lilac name identifier format described elsewhere.  The namespace for requirement names is separate from other Lilac namespaces.

The second syntax includes a quoted meta-string that stores a required version number.  The version string must consist of 1 to 63 visible, printing ASCII characters in range 0x21 to 0x7E, excluding backslash and double quote.  The format of the version string is not specified and is specific to the particular requirement.

When a requirement metacommand is encountered in the script header, the `plugin_supports()` function is called with the requirement string and the optional requirement version.  This function either returns that the requirement is fulfilled or that the requirement is not fulfilled.  An unfulfilled requirement results in the script stopping on an error.

The implementation of this function in `plugin.c` should scan for the requirements that are fulfilled by the modules which are registered.  This allows Lilac scripts to ensure that the specific Lilac build being used has all the necessary plug-ins necessary to interpret the script.
