#ifndef GAMMA_H_INCLUDED
#define GAMMA_H_INCLUDED

/*
 * gamma.h
 * 
 * Gamma-correction module of Lilac.
 */

#include <stddef.h>

/*
 * Initialize the gamma-correction table appropriately for sRGB.
 */
void gamma_sRGB(void);

/*
 * Given a gamma-corrected integer component, return a linearized
 * floating-point component.
 * 
 * The gamma table must have been initialized first with an
 * initialization function or a fault occurs.
 * 
 * c is clamped to range [0, 255] before linearization.  The output is
 * in range [0.0f, 1.0f].
 * 
 * Parameters:
 * 
 *   c - the component to undo gamma-correction for
 * 
 * Return:
 * 
 *   the linearized value
 */
float gamma_undo(int c);

/*
 * Given a linear floating-point component, return a gamma-corrected
 * integer component.
 * 
 * The gamma table must have been initialized first with an
 * initialization function or a fault occurs.
 * 
 * v is clamped to range [0.0f, 1.0f] before beginning.  If v is
 * non-finite, it is set to 0.0f.
 * 
 * Parameters:
 * 
 *   v - the linear component to gamma-correct
 * 
 * Return:
 * 
 *   the gamma-corrected value
 */
int gamma_correct(float v);

#endif
