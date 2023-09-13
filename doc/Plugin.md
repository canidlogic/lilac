# Lilac plug-in architecture

Lilac uses a plug-in architecture for extensibility.

The core Lilac program is contained with the `core` directory.  This core program is entirely self contained, with the sole exception of calls to the plug-in module documented here.

The core program imports `plugin.h` from the `plugin` directory and makes a call to the `plugin_init()` function from this module near the beginning of the program.  This is used to register plug-in modules, as described below.

The core program also makes calls to the `plugin_supports()` to satisfy script requirement declarations, as described below.

## Module registration

Each plug-in module should modify `plugin.c` to import its header and make a call to the plug-in module's registration function.  This registration function can then use the public API of the core Lilac program to register the extensions defined by the plug-in module.

The core Lilac program is only designed to provide a framework which can then be built on by plug-in modules.  You will need some sort of plug-ins to get Lilac to do something useful.  This Lilac distribution includes a `base` directory with plug-ins that establish a useful baseline of functionality.  The plug-ins in this `base` directory are already registered with the plug-in module.

## Requirement support

The header of Lilac scripts may include `%require` metacommands, that look like this:

    %require com.example.Requirement;

The unique name for the requirement must match the Lilac name identifier format.  If you need specific versions, the versions can be included as a suffix, such as this:

    %require com.example.Requirement.2.15;

When a requirement metacommand is encountered in the script header, the `plugin_supports()` function is called with an atom representing the requirement name.  This function either returns that the requirement is fulfilled or that the requirement is not fulfilled.  An unfulfilled requirement results in the script stopping on an error.

Since Lilac is extensible, scripts may want to make sure that the specific plug-ins they use are supported by the Lilac build.  The requirement system is intended to provide this functionality.
