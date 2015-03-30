
/*
 * Copyright (c) 2012 Karl N. Redgate
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

/** \file StringList.cc
 * \brief 
 */

#include <stdlib.h>
#include <tcl.h>
#include "StringList.h"

/**
 */
StringList::StringList( char *data ) : strings(0) {
    parse( data, 0 );
}

/**
 */
StringList::~StringList() {
    if ( strings != 0 )  free( strings );
}

/**
 * start with n=0, increment with each recursion
 * at end of recursion set the string pointer
 */
void StringList::parse( const char *data, int n ) {
    if ( *data == '\0' ) { // end of strings
        _count = n;
        if ( _count != 0 ) {
            strings = (const char **)malloc( sizeof(char*) * n );
            _end = data + 1;
        } else {
            strings = 0;
            _end = data + 2;
        }
        return;
    }
    const char *p = data;
    while ( *p != '\0' ) p++;
    parse( p+1, n+1 );
    strings[n] = data;
}

/* vim: set autoindent expandtab sw=4 : */
