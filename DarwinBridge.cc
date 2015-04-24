
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
 * including linux/if.h not net/if.h -- even though brutils uses net/if.h
 * since I needed linux/if.h for some other reason in Network.cc ??
 */

#include <unistd.h>

#include <netinet/in.h>
#include <netinet/ip6.h>

#include <sys/param.h>
#include <sys/socket.h>
// #include <net/if.h>
#include <linux/if.h>
#include <net/if_arp.h>
#include <linux/if_bridge.h>
#include <linux/sockios.h>

// Do I need these?
#include <sys/time.h>
#include <sys/ioctl.h>

#include <stdio.h>  // for sys_errlist
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <glob.h>

#include "logger.h"
#include "Bridge.h"
#include "Interface.h"

namespace { int debug = 0; }

/**
 * \todo need the interface object for the bridge ...
 *
 * other constructors:
 * - With Interface arg to create new bridge ?
 * - 
 */
Network::Bridge::Bridge( const char *bridge_name ) {
    _name = strdup(bridge_name);
}

/**
 */
Network::Bridge::~Bridge() {
    if ( _name != NULL ) free(_name);
}

/**
 */
const char *
Network::Bridge::create() {
    log_notice( "create new bridge '%s'", _name );

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if ( sock < 0 ) {
        // how to report error?
        // logger means it doesn't get to TCL
        return "failed to create bridge socket";
    }

    int result = ioctl(sock, SIOCBRADDBR, _name);
    int error = errno;

    close( sock );
    if ( result < 0 ) {
        if ( error == EEXIST ) goto bring_up;
        log_err( "Bridge: create '%s'", strerror(error) );
        return "failed to create bridge";
    }

bring_up:

    char buffer[128];
    sprintf( buffer, "/sbin/ip link set %s up", _name );
    if ( system(buffer) < 0 ) {
        log_notice( "%s: bridge refused to bring up link", _name );
    }

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
    // call the ioctl
    int result = ioctl(sock, SIOCBRDELBR, _name);
    // close the socket
    close( sock );
    if ( result < 0 ) {
        return "failed to delete bridge";
    }
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

    struct ifreq request;
    memset( &request, 0, sizeof(request) );

    // request.ifr_hwaddr.sa_family = AF_PACKET;
    request.ifr_hwaddr.sa_family = ARPHRD_ETHER;
    request.ifr_hwaddr.sa_data[ 0] = mac[0];
    request.ifr_hwaddr.sa_data[ 1] = mac[1];
    request.ifr_hwaddr.sa_data[ 2] = mac[2];
    request.ifr_hwaddr.sa_data[ 3] = mac[3];
    request.ifr_hwaddr.sa_data[ 4] = mac[4];
    request.ifr_hwaddr.sa_data[ 5] = mac[5];

    strncpy( request.ifr_name, _name, IFNAMSIZ );

    char buffer[60];
    sprintf( buffer, "/sys/class/net/%s/bridge/address_locked", _name );
    FILE *f = fopen( buffer, "w" );
    if ( f != NULL ) {
        fprintf( f, "%d", 0 );
    }

    int result = ioctl(sock, SIOCSIFHWADDR, &request);
    const char *error = strerror(errno);
    close( sock );

    if ( f != NULL ) {
        rewind( f );
        fprintf( f, "%d", 1 );
        fclose( f );
    }

    if ( result < 0 ) {
        log_notice( "failed to set MAC address for bridge '%s': %s", _name, error );
        return "failed to set MAC address";
    }
    return NULL;
}

/**
 */
bool
Network::Bridge::lock_address() {
    char buffer[60];
    sprintf( buffer, "/sys/class/net/%s/bridge/address_locked", _name );
    FILE *f = fopen( buffer, "r+" );
    if ( f == NULL )  return false;
    int locked;
    fscanf( f, "%d", &locked );
    if ( locked == 0 ) {
        rewind( f );
        fprintf( f, "%d", 1 );
        log_notice( "locking %s address", _name );
    }
    fclose( f );
    return true;
}

/**
 */
bool
Network::Bridge::unlock_address() {
    char buffer[60];
    sprintf( buffer, "/sys/class/net/%s/bridge/address_locked", _name );
    FILE *f = fopen( buffer, "r+" );
    if ( f == NULL )  return false;
    int locked;
    fscanf( f, "%d", &locked );
    if ( locked == 1 ) {
        rewind( f );
        fprintf( f, "%d", 0 );
        log_notice( "unlocking %s address", _name );
    }
    fclose( f );
    return true;
}

/**
 */
const char *
Network::Bridge::add( Interface *that ) {
    log_notice( "capturing interface '%s' in bridge '%s'", that->name(), _name );

    // call ioctl with interface index [that->index()]
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if ( sock < 0 ) {
        return "failed to create bridge socket";
    }

    struct ifreq request;
    memset( &request, 0, sizeof(request) );
    strncpy( request.ifr_name, _name, IFNAMSIZ );
    request.ifr_ifindex = that->index();

    int result = ioctl(sock, SIOCBRADDIF, &request);
    const char *error = strerror(errno);
    close( sock );

    if ( result < 0 ) {
        log_notice( "failed to capture interface '%s' in bridge '%s': %s",
                            that->name(), _name, error );
        return "failed to add interface to bridge";
    }
    return NULL;
}

/**
 */
const char *
Network::Bridge::capture( Interface *that ) { return add(that); }

/**
 * This may want to be by interface index...
 */
const char *
Network::Bridge::remove( Interface *that ) {
    log_notice( "removing interface '%s' from bridge '%s'", that->name(), _name );

    // call ioctl with interface index [that->index()]
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if ( sock < 0 ) {
        return "failed to create bridge socket";
    }

    struct ifreq request;
    memset( &request, 0, sizeof(request) );
    strncpy( request.ifr_name, _name, IFNAMSIZ );
    request.ifr_ifindex = that->index();

    int result = ioctl(sock, SIOCBRDELIF, &request);
    const char *error = strerror(errno);
    close( sock );

    if ( result < 0 ) {
        log_notice( "failed to remove interface '%s' from bridge '%s': %s",
                            that->name(), _name, error );
        return "failed to remove interface from bridge";
    }
    return NULL;
}

/**
 */
unsigned long
Network::Bridge::forward_delay() {
    unsigned long value = 0;
    char path[256];
    snprintf( path, sizeof(path), "/sys/class/net/%s/bridge/forward_delay", name() );
    FILE *f = fopen(path, "r");
    if ( f == NULL ) return 0xdeadbeef;
    fscanf( f, "%ld\n", &value );
    fclose( f );
    return value / HZ;
}

/**
 */
bool
Network::Bridge::forward_delay( unsigned long value ) {
    char path[256];
    snprintf( path, sizeof(path), "/sys/class/net/%s/bridge/forward_delay", name() );
    FILE *f = fopen(path, "w");
    if ( f == NULL ) return false;
    fprintf( f, "%ld\n", value * HZ );
    fclose( f );
    return true;
}

/**
 * True if this bridge has a tunnel interface captured.
 */
    // for each net/*/brport if its bridge is my brdige interface
    // glob /sys/class/net/*/brport/bridge
    // if realname of link is /sys/class/net/<myname> ...
bool Network::Bridge::is_tunnelled() const {
    bool result = false;

    // log_notice( "check if '%s' is tunnelled", name() );

    char port_path[1024];
    char bridge_path[1024];
    sprintf( bridge_path, "../../../../class/net/%s", name() );

    // log_notice( "check if '%s' is tunnelled", bridge_path );

    glob_t paths;
    memset(&paths, 0, sizeof(paths));

    glob( "/sys/class/net/tun*/brport/bridge", GLOB_NOSORT, NULL, &paths );
    for ( size_t i = 0 ; i < paths.gl_pathc ; i++ ) {
        int count = readlink( paths.gl_pathv[i], port_path, sizeof(port_path) );
        if ( count == -1 ) continue;
        port_path[count] = '\0';
        log_notice( "check if '%s' is tunnelled through '%s'", bridge_path, port_path );

        if ( strcmp(port_path, bridge_path) == 0 ) {
            log_notice( "I (%s) am tunnelled through '%s'", bridge_path, paths.gl_pathv[i] );
            result = true;
            break;
        }
    }
    globfree( &paths );

    return result;
}

/* vim: set autoindent expandtab sw=4 : */
