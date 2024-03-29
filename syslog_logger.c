
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

/** \file syslog_logger.cc
 * \brief 
 *
 */

// THIS SHOULD BE BETTER!!
// #if (__GLIBC__ >= 2)
// #if (__GLIBC_MINOR__ > 19)
#define _DEFAULT_SOURCE
// #else
// #warning glibc minor __GLIBC_MINOR__
// #define _BSD_SOURCE
// #endif
// #else
// #warning glibc __GLIBC__
// #define _BSD_SOURCE
// #endif

#include <stdio.h>
#include <syslog.h>
#include <stdarg.h>

static int interactive = 0;
static int level = LOG_WARNING;

void
log_interactive( int value ) {
    interactive = value;
}

void
log_open_user( const char *name ) {
    openlog( name, LOG_PERROR, LOG_USER );
}

void
log_open_daemon( const char *name ) {
    openlog( name, LOG_PERROR, LOG_DAEMON );
}

void
log_debug( const char *format, ... ) {
    va_list ap;
    va_start( ap, format );
    vsyslog( LOG_DEBUG, format, ap );
    va_end( ap );
}

void
log_info( const char *format, ... ) {
    va_list ap;
    va_start( ap, format );
    vsyslog( LOG_INFO, format, ap );
    va_end( ap );
}

void
log_notice( const char *format, ... ) {
    va_list ap;
    va_start( ap, format );
    if ( interactive ) {
        if ( level >= LOG_NOTICE ) {
            char buffer[1024];
            sprintf( buffer, "NOTICE: %s\n", format );
            vfprintf( stderr, buffer, ap );
        }
    } else {
        vsyslog( LOG_NOTICE, format, ap );
    }
    va_end( ap );
}

void
log_warn( const char *format, ... ) {
    va_list ap;
    va_start( ap, format );
    if ( interactive ) {
        if ( level >= LOG_WARNING ) {
            char buffer[1024];
            sprintf( buffer, "WARN: %s\n", format );
            vfprintf( stderr, buffer, ap );
        }
    } else {
        vsyslog( LOG_WARNING, format, ap );
    }
    va_end( ap );
}

void
log_err( const char *format, ... ) {
    va_list ap;
    va_start( ap, format );
    if ( interactive ) {
        if ( level >= LOG_ERR ) {
            char buffer[1024];
            sprintf( buffer, "ERROR: %s\n", format );
            vfprintf( stderr, buffer, ap );
        }
    } else {
        vsyslog( LOG_ERR, format, ap );
    }
    va_end( ap );
}

void
log_crit( const char *format, ... ) {
    va_list ap;
    va_start( ap, format );
    vsyslog( LOG_CRIT, format, ap );
    va_end( ap );
}

/* vim: set autoindent expandtab sw=4 : */
