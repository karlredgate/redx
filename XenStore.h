
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

/** \file XenStore.h
 * \brief 
 *
 */

#ifndef _XENSTORE_H_
#define _XENSTORE_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdint.h>
#include <unistd.h>
#include <tcl.h>

#include <errno.h>
#include <xen/xen.h>
#include <xen/io/xs_wire.h>

/**
 */
namespace Xen {

    /**
     */
    class StorePath {
    public:
        char *path;
        StorePath *next;
        StorePath( char *, int );
    };

    /**
     */
    class Store {
    private:
        int _error;
        char error_string[256];
    public:
        Store();
        char* read( char * );
        StorePath* readdir( char * );
        bool write( char *, char * );
        bool mkdir( char * );
        bool remove( char * );
        inline char * operator [] ( char *key ) {
            return read(key);
        }
        const char *error_message() const { return error_string; }
        int error() const { return _error; }
    };

}

#endif

/*
 * vim:autoindent
 * vim:expandtab
 */
