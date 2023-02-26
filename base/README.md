# Lilac Base

This part of Lilac includes the basic plug-ins.  Each header in this directory defines an initialization function that registers basic plug-ins with Lilac Core.  The Lilac Plugin module `plugin.c` implementation should import each of these headers and then call each of these initialization functions within `plugin_init()`.

See the `node_constant` module in this directory for a good example of how to define a Lilac plug-in.
