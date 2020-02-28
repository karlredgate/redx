
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

/** \file DarwinBridge.cc
 * \brief 
 *
 */

#include <unistd.h>

#include <netinet/in.h>
#include <netinet/ip6.h>

#include <sys/param.h>
#include <sys/socket.h>
// #include <net/if.h>
#include <net/if_arp.h>

// Do I need these?
#include <sys/time.h>
#include <sys/ioctl.h>

#include <stdio.h>  // for sys_errlist
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "logger.h"
#include "Bridge.h"
#include "Interface.h"

namespace { int debug = 0; }

/**
 */
const char *
Network::Bridge::create() {
    fprintf( stderr, "create new bridge '%s'", _name );
    log_notice( "create new bridge '%s'", _name );

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if ( sock < 0 ) {
        // how to report error?
        // logger means it doesn't get to TCL
        return "failed to create bridge socket";
    }

    int error = errno;

    close( sock );

    return NULL;
}

/**
 */
const char *
Network::Bridge::destroy() {
    // create the socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if ( sock < 0 ) {
        // how to report error?
        // logger means it doesn't get to TCL
        return "failed to create bridge socket";
    }
    close( sock );
    return NULL;
}

/**
 * Need the index of this bridge
 */
const char *
Network::Bridge::set_mac_address( unsigned char *mac ) {
    log_notice( "set MAC address of bridge '%s' to %02x:%02x:%02x:%02x:%02x:%02x",
                        _name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );

    // call ioctl with interface index [that->index()]
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if ( sock < 0 ) {
        return "failed to create bridge socket";
    }

    return NULL;
}

/**
 */
bool
Network::Bridge::lock_address() {
    return true;
}

/**
 */
bool
Network::Bridge::unlock_address() {
    return true;
}

/**
 */
const char *
Network::Bridge::add( Interface *that ) {
    return NULL;
}

/**
 */
const char *
Network::Bridge::capture( Interface *that ) {
    return add(that);
}

/**
 * This may want to be by interface index...
 */
const char *
Network::Bridge::remove( Interface *that ) {
    return NULL;
}

/**
 */
unsigned long
Network::Bridge::forward_delay() {
    unsigned long value = 0;
    // return value / HZ;
    return value;
}

/**
 */
bool
Network::Bridge::forward_delay( unsigned long value ) {
    return true;
}

/**
 */
bool Network::Bridge::is_tunnelled() const {
    bool result = false;
    return result;
}

/* vim: set autoindent expandtab sw=4 : */
