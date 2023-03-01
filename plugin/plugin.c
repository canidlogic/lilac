/*
 * plugin.c
 * ========
 * 
 * Implementation of plugin.h
 * 
 * See the header for further information.
 */

#include "plugin.h"

/* @@TODO: Import plug-in module headers here */
#include "node_constant.h"
#include "node_external.h"
#include "node_select.h"

/*
 * The plug-in initialization function implementation.
 */
void plugin_init(void) {
  /* @@TODO: Insert calls to plug-in module registrations here */
  node_constant_init();
  node_external_init();
  node_select_init();
}
