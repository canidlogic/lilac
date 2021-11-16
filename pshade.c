/*
 * pshade.c
 * 
 * Implementation of pshade.h
 * 
 * See the header for further information.
 */
#include "pshade.h"

/* Lua headers */
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>

/*
 * Constants
 * =========
 */

/*
 * Number of entries required on the Lua interpreter stack.
 * 
 * We need space for a function object, four parameters, and one return
 * value.
 */
#define PSHADE_LSTACK_HEIGHT (6)

/*
 * Local data
 * ==========
 */

/*
 * Pointer to the Lua interpreter state object, or NULL if not
 * initialized yet.
 */
static lua_State *m_L = NULL;

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * pshade_errorString function.
 */
const char *pshade_errorString(int code) {
  
  const char *pResult = NULL;
  
  switch (code) {
    case PSHADE_ERR_NONE:
      pResult = "No error";
      break;
    
    case PSHADE_ERR_LALLOC:
      pResult = "Failed to allocate Lua interpreter";
      break;
    
    case PSHADE_ERR_LOADSC:
      pResult = "Failed to load Lua script";
      break;
    
    case PSHADE_ERR_INITSC:
      pResult = "Failed to run initialization of Lua script";
      break;
    
    case PSHADE_ERR_GROWST:
      pResult = "Failed to grow Lua interpreter stack";
      break;
    
    default:
      pResult = "Unknown error";
  }
  
  return pResult;
}

/*
 * pshade_load function.
 */
int pshade_load(const char *pScriptPath, int *perr) {
  
  int status = 1;
  
  /* Check state */
  if (m_L != NULL) {
    abort();
  }
  
  /* Check parameters */
  if ((pScriptPath == NULL) || (perr == NULL)) {
    abort();
  }
  
  /* Reset error indicator */
  *perr = PSHADE_ERR_NONE;
  
  /* Allocate new Lua state */
  m_L = luaL_newstate();
  if (m_L == NULL) {
    status = 0;
    *perr = PSHADE_ERR_LALLOC;
  }
  
  /* Load the script file */
  if (status) {
    if (luaL_loadfile(m_L, pScriptPath)) {
      status = 0;
      *perr = PSHADE_ERR_LOADSC;
    }
  }
  
  /* The compiled script file is now a function object on top of the Lua
   * stack; invoke it so all functions are registered and any startup
   * code is run */
  if (status) {
    if (lua_pcall(m_L, 0, 0, 0)) {
      status = 0;
      *perr = PSHADE_ERR_INITSC;
    }
  }
  
  /* Make sure we have enough room on the Lua stack */
  if (status) {
    if (!lua_checkstack(m_L, PSHADE_LSTACK_HEIGHT)) {
      status = 0;
      *perr = PSHADE_ERR_GROWST;
    }
  }
  
  /* If there was an error, free the Lua state if allocated */
  if (!status) {
    if (m_L != NULL) {
      lua_close(m_L);
      m_L = NULL;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * pshade_close function.
 */
void pshade_close(void) {
  if (m_L != NULL) {
    lua_close(m_L);
    m_L = NULL;
  }
}

/*
 * pshade_pixel function.
 */
uint32_t pshade_pixel(
    const char *pShader,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    int *perr) {
  /* @@TODO: */
  *perr = 1;
  return 0;
}
