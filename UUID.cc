
/*
 * Copyright (c) 2012-2021 Karl N. Redgate
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/** \file UUID.cc
 * \brief 
 */

#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#include "tcl_util.h"
#include "UUID.h"
#include "AppInit.h"

/**
 * An uninitialized UUID is all 0's.
 */
UUID::UUID() {
    memset( data, 0, sizeof data );
    format();
}

/**
 * An uninitialized UUID is all 0's.
 */
UUID::UUID( UUID& that ) {
    set( that.raw() );
}

/**
 * Create a UUID from an ASCII string representation.
 */
UUID::UUID( char *input ) {
    set( input );
}

/**
 * Create a UUID from a 16 byte binary representation.
 */
UUID::UUID( uint8_t *input ) {
    set( input );
}

/**
 * The string is parsed from an ASCII UUID into its 16 bytes binary form.
 * After parsing, we reformat so the output is always lower case.
 */
bool
UUID::set( char *input ) {
    memcpy( string, input, sizeof string );
    string[36] = '\0';
    parse();
    format();
    return true;
}

/**
 * Set the UUID to a 16 byte binary value, then generate the ASCII formatted
 * string representation.
 */
bool
UUID::set( uint8_t *input ) {
    memcpy( data, input, sizeof data );
    format();
    return true;
}

/**
 */
bool
UUID::operator == ( UUID& that ) {
    if ( data[ 0] != that.data[ 0] ) return false;
    if ( data[ 1] != that.data[ 1] ) return false;
    if ( data[ 2] != that.data[ 2] ) return false;
    if ( data[ 3] != that.data[ 3] ) return false;
    if ( data[ 4] != that.data[ 4] ) return false;
    if ( data[ 5] != that.data[ 5] ) return false;
    if ( data[ 6] != that.data[ 6] ) return false;
    if ( data[ 7] != that.data[ 7] ) return false;
    if ( data[ 8] != that.data[ 8] ) return false;
    if ( data[ 9] != that.data[ 9] ) return false;
    if ( data[10] != that.data[10] ) return false;
    if ( data[11] != that.data[11] ) return false;
    if ( data[12] != that.data[12] ) return false;
    if ( data[13] != that.data[13] ) return false;
    if ( data[14] != that.data[14] ) return false;
    if ( data[15] != that.data[15] ) return false;
    return true;
}

/**
 */
bool
UUID::operator != ( UUID& that ) {
    if ( data[ 0] != that.data[ 0] ) return true;
    if ( data[ 1] != that.data[ 1] ) return true;
    if ( data[ 2] != that.data[ 2] ) return true;
    if ( data[ 3] != that.data[ 3] ) return true;
    if ( data[ 4] != that.data[ 4] ) return true;
    if ( data[ 5] != that.data[ 5] ) return true;
    if ( data[ 6] != that.data[ 6] ) return true;
    if ( data[ 7] != that.data[ 7] ) return true;
    if ( data[ 8] != that.data[ 8] ) return true;
    if ( data[ 9] != that.data[ 9] ) return true;
    if ( data[10] != that.data[10] ) return true;
    if ( data[11] != that.data[11] ) return true;
    if ( data[12] != that.data[12] ) return true;
    if ( data[13] != that.data[13] ) return true;
    if ( data[14] != that.data[14] ) return true;
    if ( data[15] != that.data[15] ) return true;
    return false;
}

namespace {
    char hex[] = { '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f' };
    char hexval[] = {
      /* 0 */
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 31 */
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1, /* 63 */
        -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 95 */
        -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 127 */
    };
}

/**
 */
void UUID::parse() {
    data[ 0] = (hexval[ string[ 0] & 0x7F ] << 4) + (hexval[ string[ 1] & 0x7F ]);
    data[ 1] = (hexval[ string[ 2] & 0x7F ] << 4) + (hexval[ string[ 3] & 0x7F ]);
    data[ 2] = (hexval[ string[ 4] & 0x7F ] << 4) + (hexval[ string[ 5] & 0x7F ]);
    data[ 3] = (hexval[ string[ 6] & 0x7F ] << 4) + (hexval[ string[ 7] & 0x7F ]);
    // string[8] == '-'
    data[ 4] = (hexval[ string[ 9] & 0x7F ] << 4) + (hexval[ string[10] & 0x7F ]);
    data[ 5] = (hexval[ string[11] & 0x7F ] << 4) + (hexval[ string[12] & 0x7F ]);
    // string[13] == '-'
    data[ 6] = (hexval[ string[14] & 0x7F ] << 4) + (hexval[ string[15] & 0x7F ]);
    data[ 7] = (hexval[ string[16] & 0x7F ] << 4) + (hexval[ string[17] & 0x7F ]);
    // string[18] == '-'
    data[ 8] = (hexval[ string[19] & 0x7F ] << 4) + (hexval[ string[20] & 0x7F ]);
    data[ 9] = (hexval[ string[21] & 0x7F ] << 4) + (hexval[ string[22] & 0x7F ]);
    // string[23] == '-'
    data[10] = (hexval[ string[24] & 0x7F ] << 4) + (hexval[ string[25] & 0x7F ]);
    data[11] = (hexval[ string[26] & 0x7F ] << 4) + (hexval[ string[27] & 0x7F ]);
    data[12] = (hexval[ string[28] & 0x7F ] << 4) + (hexval[ string[29] & 0x7F ]);
    data[13] = (hexval[ string[30] & 0x7F ] << 4) + (hexval[ string[31] & 0x7F ]);
    data[14] = (hexval[ string[32] & 0x7F ] << 4) + (hexval[ string[33] & 0x7F ]);
    data[15] = (hexval[ string[34] & 0x7F ] << 4) + (hexval[ string[35] & 0x7F ]);
}

/**
 */
uint8_t UUID::nybble( int n ) {
    uint8_t byte = data[ (n>>1)&0xF ];
    return (n&1) ? byte & 0xF : (byte>>4) & 0xF;
}

/**
 */
void UUID::format() {
    string[ 0] = hex[ nybble( 0) ];
    string[ 1] = hex[ nybble( 1) ];
    string[ 2] = hex[ nybble( 2) ];
    string[ 3] = hex[ nybble( 3) ];
    string[ 4] = hex[ nybble( 4) ];
    string[ 5] = hex[ nybble( 5) ];
    string[ 6] = hex[ nybble( 6) ];
    string[ 7] = hex[ nybble( 7) ];
    string[ 8] = '-';
    string[ 9] = hex[ nybble( 8) ];
    string[10] = hex[ nybble( 9) ];
    string[11] = hex[ nybble(10) ];
    string[12] = hex[ nybble(11) ];
    string[13] = '-';
    string[14] = hex[ nybble(12) ];
    string[15] = hex[ nybble(13) ];
    string[16] = hex[ nybble(14) ];
    string[17] = hex[ nybble(15) ];
    string[18] = '-';
    string[19] = hex[ nybble(16) ];
    string[20] = hex[ nybble(17) ];
    string[21] = hex[ nybble(18) ];
    string[22] = hex[ nybble(19) ];
    string[23] = '-';
    string[24] = hex[ nybble(20) ];
    string[25] = hex[ nybble(21) ];
    string[26] = hex[ nybble(22) ];
    string[27] = hex[ nybble(23) ];
    string[28] = hex[ nybble(24) ];
    string[29] = hex[ nybble(25) ];
    string[30] = hex[ nybble(26) ];
    string[31] = hex[ nybble(27) ];
    string[32] = hex[ nybble(28) ];
    string[33] = hex[ nybble(29) ];
    string[34] = hex[ nybble(30) ];
    string[35] = hex[ nybble(31) ];
    string[36] = '\0';
}

/* vim: set autoindent expandtab sw=4 : */
