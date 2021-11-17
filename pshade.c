/*
 * pshade.c
 * 
 * Implementation of pshade.h
 * 
 * See the header for further information.
 */
#include "pshade.h"

#include <stdlib.h>

/* Lua headers */
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

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
    
    case PSHADE_ERR_UNLOAD:
      pResult = "Programmable shader is not loaded";
      break;
    
    case PSHADE_ERR_NOTFND:
      pResult = "Shader function not found";
      break;
    
    case PSHADE_ERR_SMALLI:
      pResult = "Lua was compiled with integers that are too small";
      break;
    
    case PSHADE_ERR_CALL:
      pResult = "Failed to call shader function";
      break;
    
    case PSHADE_ERR_RETVAL:
      pResult = "Shader function must return exactly one value";
      break;
    
    case PSHADE_ERR_RTYPE:
      pResult = "Shader function must return an integer";
      break;
    
    case PSHADE_ERR_RRANGE:
      pResult = "Shader function returned integer value out of range";
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
  
  /* Lua integers must be large enough to support full unsigned 32-bit
   * range; they are by default 64-bit (large enough) in Lua 5.4, but
   * they might be smaller and not large enough if Lua was compiled
   * specially */
  if ((LUA_MININTEGER > -1) || (LUA_MAXINTEGER < UINT32_MAX)) {
    status = 0;
    *perr = PSHADE_ERR_SMALLI;
  }
  
  /* Allocate new Lua state */
  if (status) {
    m_L = luaL_newstate();
    if (m_L == NULL) {
      status = 0;
      *perr = PSHADE_ERR_LALLOC;
    }
  }
  
  /* Load the Lua standard libraries */
  if (status) {
    luaL_openlibs(m_L);
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
  
  int status = 1;
  const char *pc = NULL;
  lua_Integer retval = 0;
  
  static int32_t s_last_x = 0;
  static int32_t s_last_y = 0;
  
  /* Check parameters */
  if ((pShader == NULL) || (perr == NULL)) {
    abort();
  }
  if ((width < 1) || (height < 1)) {
    abort();
  }
  if ((x < 0) || (x >= width) ||
      (y < 0) || (y >= height)) {
    abort();
  }
  
  /* Enforce proper scanning order and update statistics */
  if (y > s_last_y) {
    /* We've advanced a scanline, so update to new position */
    s_last_x = x;
    s_last_y = y;
  
  } else if (y == s_last_y) {
    /* Still in same scanline, so next check x */
    if (x > s_last_x) {
      /* We've advanced within scanline, so update x */
      s_last_x = x;
    
    } else if (x != s_last_x) {
      /* We must have gone backwards, which is not allowed */
      abort();
    }
  
  } else {
    /* We must have gone backwards in scan order, which is not
     * allowed */
    abort();
  }
  
  /* Check that name is not empty and starts with a letter or an
   * underscore */
  if ((*pShader != '_') &&
        ((*pShader < 'A') || (*pShader > 'Z')) &&
        ((*pShader < 'a') || (*pShader > 'z'))) {
    abort();
  }
  
  /* Check that only ASCII alphanumerics and underscore in name */
  for(pc = pShader; *pc != 0; pc++) {
    if (((*pc < 'A') || (*pc > 'Z')) &&
        ((*pc < 'a') || (*pc > 'z')) &&
        ((*pc < '0') || (*pc > '9')) &&
        (*pc != '_')) {
      abort();
    }
  }
  
  /* Reset error indicator */
  *perr = PSHADE_ERR_NONE;
  
  /* Fail if interpreter is not loaded */
  if (m_L == NULL) {
    status = 0;
    *perr = PSHADE_ERR_UNLOAD;
  }
  
  /* Push the Lua function corresponding to the shader name onto the
   * interpreter stack */
  if (status) {
    if (lua_getglobal(m_L, pShader) != LUA_TFUNCTION) {
      status = 0;
      *perr = PSHADE_ERR_NOTFND;
      lua_settop(m_L, 0); /* Pop everything off stack */
    }
  }
  
  /* Push all the arguments onto the interpreter stack */
  if (status) {
    lua_pushinteger(m_L, x);
    lua_pushinteger(m_L, y);
    lua_pushinteger(m_L, width);
    lua_pushinteger(m_L, height);
  }
  
  /* Invoke the shader function, passing four parameters and expecting
   * one back */
  if (status) {
    if (lua_pcall(m_L, 4, 1, 0)) {
      status = 0;
      *perr = PSHADE_ERR_CALL;
      lua_settop(m_L, 0); /* Pop everything off stack */
    }
  }
  
  /* Shader function should have returned exactly one parameter */
  if (status) {
    if (lua_gettop(m_L) != 1) {
      status = 0;
      *perr = PSHADE_ERR_RETVAL;
      lua_settop(m_L, 0); /* Pop everything off stack */
    }
  }
  
  /* Shader function should have returned an integer */
  if (status) {
    if (!lua_isinteger(m_L, 1)) {
      status = 0;
      *perr = PSHADE_ERR_RTYPE;
      lua_settop(m_L, 0); /* Pop everything off stack */
    }
  }
  
  /* Pop the return value off the stack and store to retval */
  if (status) {
    retval = lua_tointegerx(m_L, 1, NULL);
    lua_settop(m_L, 0); /* Pop everything off stack */
  }
  
  /* Check the range of the returned integer */
  if (status) {
    if ((retval < 0) || (retval > UINT32_MAX)) {
      status = 0;
      *perr = PSHADE_ERR_RRANGE;
    }
  }
  
  /* If there was an error, set return value to zero */
  if (!status) {
    retval = 0;
  }
  
  /* Return the result */
  return (uint32_t) retval;
}
