#ifndef VM_H_INCLUDED
#define VM_H_INCLUDED

/*
 * vm.h
 * ====
 * 
 * The rendering script main interpreter module.
 * 
 * This module does not parse the rendering script signature line or
 * header metacommands.  That is handled by the main lilac module during
 * configuration.
 * 
 * This module also does not define the specific operations that are
 * available within rendering scripts.  Instead, it provides an
 * extensible framework that allows other modules to define the
 * rendering operations and plug those into this main interpreter
 * module.
 */

#include <stddef.h>
#include <stdint.h>

#include "istr.h"
#include "node.h"

#include "shastina.h"

/*
 * Constants
 * ---------
 */

/*
 * The interpreter data types.
 */
#define VM_TYPE_UNDEF  (0)
#define VM_TYPE_FLOAT  (1)
#define VM_TYPE_COLOR  (2)
#define VM_TYPE_STRING (3)
#define VM_TYPE_NODE   (4)

/*
 * Type declarations
 * -----------------
 */

/*
 * Function pointer type for operation implementation functions.
 * 
 * These functions are called to run a specific Shastina operation when
 * it is encountered in the rendering script.
 * 
 * The indicated function does not take any direct input or output
 * parameters.  Instead, use the functions of this vm module to access
 * the interpreter stack and get input and output parameters from the
 * interpreter stack.
 */
typedef void (*fp_vm_op)(void);

/*
 * Public functions
 * ----------------
 */

/*
 * Register a specific operation handler.
 * 
 * You must register all operation handlers before calling the vm_run()
 * function.  There are no built-in operations in the vm module, so
 * every operation must be registered through this function.
 * 
 * pOpName is the name of the Shastina operation.  It must have length
 * in range [1, 31], the first character must be a US-ASCII alphabetic
 * letter, and all other characters must be US-ASCII alphanumerics or
 * underscores.
 * 
 * An error occurs if the same operation name is registered more than
 * once.  An error also occurs if the total number of registered
 * operations exceeds an implementation limit.
 * 
 * This function may only be used when vm_run has not been called yet.
 * 
 * fp is the function that will be invoked to handle this operation.
 * See the documentation of fp_vm_op for further information.
 * 
 * Parameters:
 * 
 *   pOpName - the Shastina operation
 * 
 *   fp - the function that implements the operation
 */
void vm_register(const char *pOpName, fp_vm_op fp);

/*
 * Run the body of a rendering script.
 * 
 * pp and pSrc are the Shastina parser and source to use for reading
 * through the file.  All entity read operations will use readEntity()
 * function wrapper defined by the lilac module.
 * 
 * When this function is called, the %body; metacommand must just have
 * been read, such that the next entity read will be the first entity of
 * the rendering script body.  This function does NOT parse the full
 * rendering script, since it is unable to handle the signature or
 * header metacommands.
 * 
 * Before calling this function, all operations must be registered first
 * using the vm_register() function.  Otherwise, no operations will be
 * supported while interpreting the script, since the vm module does not
 * define any operations by itself.
 * 
 * The return value is the root node that was left on the interpreter
 * stack at the end of script interpretation.  This is the node that
 * should be used for rendering the output image.
 * 
 * This function may only be called a single time.  The module switches
 * into running mode while this function is executing.  The module
 * switches out of running mode when returning.
 * 
 * Parameters:
 * 
 *   pp - the Shastina parser
 * 
 *   pSrc - the Shastina source
 * 
 * Return:
 * 
 *   the root node that was left on the interpreter stack
 */
NODE *vm_run(SNPARSER *pp, SNSOURCE *pSrc);

/*
 * Return the type of data on top of the interpreter stack.
 * 
 * VM_TYPE_UNDEF is only returned when nothing is visible on top of the
 * stack, either because the stack is empty or because everything is
 * hidden by a Shastina group.
 * 
 * This function may only be used when the module is in running mode.
 * 
 * Return:
 * 
 *   one of the VM_TYPE constants, with VM_TYPE_UNDEF meaning nothing
 *   visible on top of the stack
 */
int vm_type(void);

/*
 * Pop a float off the top of the interpreter stack.
 * 
 * If there is not currently an accessible value of that type on top of
 * the interpreter stack, an error occurs.
 * 
 * The return value will always be finite.
 * 
 * This function may only be used when the module is in running mode.
 * 
 * Return:
 * 
 *   the popped float value
 */
double vm_pop_f(void);

/*
 * Pop a color off the top of the interpreter stack.
 * 
 * If there is not currently an accessible value of that type on top of
 * the interpreter stack, an error occurs.
 * 
 * The color has the same packed ARGB format that is expected by
 * Sophistry.
 * 
 * This function may only be used when the module is in running mode.
 * 
 * Return:
 * 
 *   the popped color value
 */
uint32_t vm_pop_c(void);

/*
 * Pop a string off the top of the interpreter stack.
 * 
 * If there is not currently an accessible value of that type on top of
 * the interpreter stack, an error occurs.
 * 
 * The string is copied into the given ISTR structure.
 * 
 * This function may only be used when the module is in running mode.
 * 
 * Parameters:
 * 
 *   ps - the initialized ISTR structure to receive the popped string
 */
void vm_pop_s(ISTR *ps);

/*
 * Pop a node off the top of the interpreter stack.
 * 
 * If there is not currently an accessible value of that type on top of
 * the interpreter stack, an error occurs.
 * 
 * This function may only be used when the module is in running mode.
 * 
 * Return:
 * 
 *   the popped node object
 */
NODE *vm_pop_n(void);

/*
 * Push a float value on top of the interpreter stack.
 * 
 * Errors occur if f is not finite or if the stack height exceeds the
 * configured stack height limit.
 * 
 * This function may only be used when the module is in running mode.
 * 
 * Parameters:
 * 
 *   f - the finite floating-point value to push
 */
void vm_push_f(double f);

/*
 * Push a color value on top of the interpreter stack.
 * 
 * An error occurs if the stack height exceeds the configured stack
 * height limit.
 * 
 * col is in the packed ARGB format expected by Sophistry.
 * 
 * This function may only be used when the module is in running mode.
 * 
 * Parameters:
 * 
 *   col - the color to push
 */
void vm_push_c(uint32_t col);

/*
 * Push a string value on top of the interpreter stack.
 * 
 * An error occurs if the stack height exceeds the configured stack
 * height limit.
 * 
 * The string reference (or the empty string) within the given ISTR is
 * copied on top of the interpreter stack.
 * 
 * This function may only be used when the module is in running mode.
 * 
 * Parameters:
 * 
 *   ps - the initialized ISTR structure holding the string to push
 */
void vm_push_s(ISTR *ps);

/*
 * Push a node on top of the interpreter stack.
 * 
 * An error occurs if the stack height exceeds the configured stack
 * height limit.
 * 
 * This function may only be used when the module is in running mode.
 * 
 * Parameters:
 * 
 *   pn - the node to push
 */
void vm_push_n(NODE *pn);

/*
 * Duplicate whatever is currently on top of the stack.
 * 
 * If there is not currently an accessible value on top of the
 * interpreter stack, an error occurs.  An error also occurs if the
 * stack height exceeds the configured stack height limit.
 * 
 * This function may only be used when the module is in running mode.
 * 
 * This operation could technically be implemented with push and pop,
 * but that would require different cases for each data type.  This
 * function is provided for convenience.
 */
void vm_dup(void);

/*
 * Drop whatever is currently on top of the stack.
 * 
 * If there is not currently an accessible value on top of the
 * interpreter stack, an error occurs.
 * 
 * This function may only be used when the module is in running mode.
 * 
 * This operation could technically be implemented with the typed pop
 * functions, but that would require different cases for each data type.
 * This function is provided for convenience.
 */
void vm_pop(void);

#endif
