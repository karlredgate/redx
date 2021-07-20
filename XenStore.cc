
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

/** \file XenStore.cc
 * \brief 
 *
 * \todo add Watcher thread that holds a callback when an event occurs.
 * \todo create watch/unwatch commands ... watch returns a watcher object
 *       unwatch takes a watcher argument -- stops the thread and sends
 *      unwatch message
 */

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <errno.h>
#include <string.h>

#include "logger.h"
#include "util.h"
#include "XenStore.h"

static const char *BUS = "/proc/xen/xenbus";

/**
 */
Xen::StorePath::StorePath( char *buffer, int remaining )
: next(0) {
    path = strdup(buffer);
    while ( *buffer != '\0' ) {
        if ( remaining < 1 ) return;
        buffer++; remaining--;
    }
    buffer++; remaining--;
    if ( remaining < 1 ) return;
    next = new Xen::StorePath( buffer, remaining );
}

/**
 */
Xen::Store::Store() {
}

/**
 */
Xen::StorePath* Xen::Store::readdir( char *key ) {
    char buffer[4096];
    struct xsd_sockmsg message;
    memset( &message, 0, sizeof(message) );

    int fd = open(BUS, O_RDWR);
    if ( fd == -1 ) {
        log_err( "failed to open xenbus" );
        exit( 1 );
    }
    message.type = XS_DIRECTORY;
    message.req_id = (uint32_t)key;
    message.len = strlen(key) + 1;
    if ( ::write(fd, &message, sizeof(message)) == -1 ) {
        perror("xenbus write" );
        exit( 1 );
    }
    if ( ::write(fd, key, message.len) == -1 ) {
        perror("xenbus write" );
        exit( 1 );
    }
    if ( ::read(fd, &message, sizeof(message)) == -1 ) {
        perror("xenbus read message" );
        exit( 1 );
    }
    if ( ::read(fd, buffer, message.len) == -1 ) {
        perror("xenbus read data" );
        exit( 1 );
    }
    close( fd );
    buffer[message.len] = '\0';
    if ( message.type == XS_ERROR ) {
        // can mean no such directory ... or ...
        return NULL;
    }
    Xen::StorePath *result = new Xen::StorePath( buffer, message.len );

    return result;
}

/**
 */
char* Xen::Store::read( char *key ) {
    char buffer[1024];
    struct xsd_sockmsg message;
    memset( &message, 0, sizeof(message) );

    int fd = open(BUS, O_RDWR);
    if ( fd == -1 ) {
        log_err( "failed to open xenbus" );
        exit( 1 );
    }
    message.type = XS_READ;
    message.req_id = (uint32_t)key;
    message.len = strlen(key) + 1;
    if ( ::write(fd, &message, sizeof(message)) == -1 ) {
        strlcpy( error_string, "xenbus write", sizeof(error_string) );
        goto fail;
    }
    if ( ::write(fd, key, message.len) == -1 ) {
        strlcpy( error_string, "xenbus write", sizeof(error_string) );
        goto fail;
    }
    if ( ::read(fd, &message, sizeof(message)) == -1 ) {
        strlcpy( error_string, "xenbus read header", sizeof(error_string) );
        goto fail;
    }
    if ( ::read(fd, buffer, message.len) == -1 ) {
        strlcpy( error_string, "xenbus read data", sizeof(error_string) );
        goto fail;
    }
    close( fd );

    if ( message.type == XS_ERROR ) {
        strlcpy( error_string, buffer, sizeof(error_string) );
        goto fail;
    }

    buffer[message.len] = '\0';
    return strdup(buffer);

fail:
    _error = errno;
    close( fd );
    return NULL;
}

/**
 * This should check if the bus is a pipe, and if it not
 * or there is a write failure, check if xenstored is
 * running.
 */
bool Xen::Store::write( char *key, char *value ) {

    int fd = open(BUS, O_RDWR);
    if ( fd == -1 ) {
        log_err( "failed to open xenbus" );
        exit( 1 );
    }

    struct {
        struct xsd_sockmsg hdr;
        char data[2048];
    } message;
    memset( &message, 0, sizeof(message) );
    char *p = message.data;
    int len = strlcpy(p, key, 2048) + 1;
    p += len;
    len += strlcpy(p, value, 2048 - len);

    message.hdr.type = XS_WRITE;
    message.hdr.req_id = (uint32_t)key;
    message.hdr.len = len;

    if ( ::write(fd, &message, sizeof(message)) == -1 ) {
        strlcpy( error_string, "xenbus write", sizeof(error_string) );
        goto fail;
    }
    if ( ::read(fd, &message.hdr, sizeof(message.hdr)) == -1 ) {
        strlcpy( error_string, "xenbus read header", sizeof(error_string) );
        goto fail;
    }
    p = message.data;
    if ( ::read(fd, message.data, message.hdr.len) == -1 ) {
        strlcpy( error_string, "xenbus read data", sizeof(error_string) );
        goto fail;
    }
    close( fd );

    if ( message.hdr.type == XS_ERROR ) {
        strlcpy( error_string, message.data, sizeof(error_string) );
        goto fail;
    }

    message.data[message.hdr.len] = '\0';
    return true;

fail:
    _error = errno;
    close( fd );
    return false;
}

/**
 */
bool Xen::Store::mkdir( char *path ) {
    int fd = open(BUS, O_RDWR);
    if ( fd == -1 ) {
        log_err( "failed to open xenbus" );
        return false;
    }

    char buffer[1024];
    struct xsd_sockmsg message;
    memset( &message, 0, sizeof(message) );

    message.type = XS_MKDIR;
    message.req_id = (uint32_t)path;
    message.len = strlen(path) + 1;

    if ( ::write(fd, &message, sizeof(message)) == -1 ) {
        strlcpy( error_string, "xenbus write", sizeof(error_string) );
        goto fail;
    }
    if ( ::write(fd, path, message.len) == -1 ) {
        strlcpy( error_string, "xenbus write", sizeof(error_string) );
        goto fail;
    }

    if ( ::read(fd, &message, sizeof(message)) == -1 ) {
        strlcpy( error_string, "xenbus read header", sizeof(error_string) );
        goto fail;
    }
    if ( ::read(fd, buffer, message.len) == -1 ) {
        strlcpy( error_string, "xenbus read data", sizeof(error_string) );
        goto fail;
    }
    close( fd );

    if ( message.type == XS_ERROR ) {
        strlcpy( error_string, buffer, sizeof(error_string) );
        goto fail;
    }

    buffer[message.len] = '\0';
    return true;

fail:
    _error = errno;
    close( fd );
    return false;
}

/**
 */
bool Xen::Store::remove( char *path ) {
    int fd = open(BUS, O_RDWR);
    if ( fd == -1 ) {
        log_err( "failed to open xenbus" );
        return false;
    }

    char buffer[1024];
    struct xsd_sockmsg message;
    memset( &message, 0, sizeof(message) );

    message.type = XS_RM;
    message.req_id = (uint32_t)path;
    message.len = strlen(path) + 1;

    if ( ::write(fd, &message, sizeof(message)) == -1 ) {
        strlcpy( error_string, "xenbus write", sizeof(error_string) );
        goto fail;
    }
    if ( ::write(fd, path, message.len) == -1 ) {
        strlcpy( error_string, "xenbus write", sizeof(error_string) );
        goto fail;
    }

    if ( ::read(fd, &message, sizeof(message)) == -1 ) {
        strlcpy( error_string, "xenbus read header", sizeof(error_string) );
        goto fail;
    }
    if ( ::read(fd, buffer, message.len) == -1 ) {
        strlcpy( error_string, "xenbus read data", sizeof(error_string) );
        goto fail;
    }
    close( fd );

    if ( message.type == XS_ERROR ) {
        strlcpy( error_string, buffer, sizeof(error_string) );
        goto fail;
    }

    buffer[message.len] = '\0';
    return true;

fail:
    _error = errno;
    close( fd );
    return false;
}

/* vim: set autoindent expandtab sw=4 : */
