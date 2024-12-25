/*
 * =====================================================================================
 *
 *       Filename:  bitsop.h
 *
 *    Description:  
 *

 * =====================================================================================
 */


#ifndef __BITS__
#define __BITS__

/* bit is 2^x, where x [0, 31] */

#define IS_BIT_SET(n, bit)      (n & bit)
#define TOGGLE_BIT(n, bit)      (n = (n ^ (bit)))
#define COMPLEMENT(n)           (n = (n ^ 0xFFFFFFFF))
#define UNSET_BIT(n, bit)       (n = (n & ((bit) ^ 0xFFFFFFFF)))
#define SET_BIT(n, bit)         (n = (n | (bit)))

#endif

