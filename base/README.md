# Lilac base plug-ins

This part of Lilac includes the basic plug-ins.  Each header in this directory defines an initialization function that registers basic plug-ins with Lilac core.  The Lilac plug-in module `plugin.c` implementation should import each of these headers and then call each of these initialization functions within `plugin_init()`.
