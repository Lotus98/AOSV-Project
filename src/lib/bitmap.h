/** @file bitmap.h
 *  @brief Macros to implement bitmaps.
 *
 *  This file contains the macros needed to create and manage bitmaps.
 *  This is inspired by the linux kernel implementation, but it is naive and doesn't enforce security.
 *
 *  @author Nalin Dhingra (Lotus98)
 *  @bug No known bugs.
 */
#ifndef LIB_BITMAP_H
#define LIB_BITMAP_H

// includes
#include <stdlib.h>


// This typedef is legitely used to hide the type of bitmap_t
typedef unsigned long *bitmap_t;

// Macros
#define BITS_TO_LONG(nr) ( (nr + 7) / 8 )
// Bound to work only on 64bits architecture
#define BITS_PER_LONG 64

/** @brief Creates a bitmap_t of n bits.
 *
 *  Returns a pointer to an array that needs to be freed, when not needed anymore.
 *
 *  @param n: Number of bits needed in the bitmap.
 */
#define DECLARE_BITMAP(n) ( calloc(BITS_TO_LONG(n), sizeof(unsigned long)) )

/** @brief Sets the i^th bit of the given bitmap.
 *
 *  @param b: The target bitmap.
 *  @param i: The bit to be set.
 */
#define SET_BIT(b, i) ( b[i / BITS_PER_LONG] |= 1 << (i & BITS_PER_LONG-1) )

/** @brief Unsets the i^th bit of the given bitmap.
 *
 *  @param b: The target bitmap.
 *  @param i: The bit to be unset.
 */
#define UNSET_BIT(b, i) ( b[i / BITS_PER_LONG] &= ~(1 << (i & BITS_PER_LONG-1)) )

/** @brief Gets the i^th bit of the given bitmap.
 *
 *  @param b: The target bitmap.
 *  @param i: The bit to be returned.
 */
#define GET_BIT(b, i) ( b[i / BITS_PER_LONG] & (1 << (i & BITS_PER_LONG-1)) ? 1 : 0 )

#endif // !LIB_BITMAP_H
