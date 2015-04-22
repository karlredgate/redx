
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

/** \file LinuxInterface.c
 * \brief 
 *
 * Contains method definitions that are platform specific for the
 * Interface class.  There are related files for other platforms.
 */

#define _BSD_SOURCE
#include <features.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "PlatformInterface.h"
#include "logger.h"

int
network_interface_index( const char *_name ) {
    struct ifreq ifr;
    int s;
    s = socket(PF_PACKET, SOCK_DGRAM, 0);
    if (s < 0)  return -1;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, _name, IFNAMSIZ);
    int result = ioctl(s, SIOCGIFINDEX, &ifr);
    close(s);

    if ( result < 0 ) {
        return -1;
    }

    return ifr.ifr_ifindex;
}

int
get_mac_address( char *interface_name, unsigned char *mac ) {
    int sock = socket( AF_INET, SOCK_STREAM, 0 );
    if ( sock < 0 ) {
        log_warn( "could not open socket to get intf mac addr" );
        return 0;
    }

    struct ifreq request;
    memset( &request, 0, sizeof(request) );
    strncpy( request.ifr_name, interface_name, IFNAMSIZ );
    // request->ifr_hwaddr.sa_family = ARPHRD_ETHER;

    int result = ioctl(sock, SIOCGIFHWADDR, &request);
    const char *error = strerror(errno);
    close( sock );

    if ( result < 0 ) {
        log_warn( "failed to get MAC address for '%s': %s",
                            interface_name, error );
        return 0;
    }

    unsigned char *address = (unsigned char *)request.ifr_hwaddr.sa_data;
    mac[0] = address[0];
    mac[1] = address[1];
    mac[2] = address[2];
    mac[3] = address[3];
    mac[4] = address[4];
    mac[5] = address[5];

    return 1;
}

int
set_interface_address( int index, struct in6_addr *address ) {
    using namespace NetLink;
    RouteSocket rs;
    NewAddress new_address( index, address );
    new_address.prefix( 64 );

    RouteResponseHandler handler;
    new_address.send( rs );
    rs.receive( &handler );

    return handler.error();
}

/**
 */
void
Network::Interface::platform_init() {
    _index = network_interface_index( _name );

    if ( get_mac_address(_name, MAC) ) {
        lladdr( &primary_address );
    }
}

/**
 */
void
Network::Interface::get_settings() {
    struct ifreq request;
    struct ethtool_cmd data;

    memset( &request, 0, sizeof(request) );
    strcpy( request.ifr_name, name() );
    int ethtool = ::socket( AF_INET, SOCK_DGRAM, 0 );

    data.cmd = ETHTOOL_GSET;
    request.ifr_data = (caddr_t)&data;
    int error = ::ioctl( ethtool, SIOCETHTOOL, &request );

    if ( error < 0 ) {
        log_err( "failed to get ethtool settings for '%s'", name() );
    }
    close( ethtool );

    _speed  = data.speed;
    _duplex = data.duplex;
    _port   = data.port;
}

/**
 */
static int
get_offload( char *name, int command ) {
    struct ifreq request;
    struct ethtool_value eval;

    memset( &request, 0, sizeof(request) );
    strcpy( request.ifr_name, name );
    int ethtool = ::socket( AF_INET, SOCK_DGRAM, 0 );

    eval.cmd = command;
    request.ifr_data = (caddr_t)&eval;
    int result = ioctl(ethtool, SIOCETHTOOL, &request);
    close( ethtool );

    if (result != 0) {
        return -1;
    } else {
        return eval.data;
    }
}

/**
 */
int
Network::Interface::rx_offload() const {
    return get_offload( name(), ETHTOOL_GRXCSUM );
}

/**
 */
int
Network::Interface::tx_offload() const {
    return get_offload( name(), ETHTOOL_GTXCSUM );
}

/**
 */
int
Network::Interface::sg_offload() const {
    return get_offload( name(), ETHTOOL_GSG );
}

/**
 */
int
Network::Interface::tso_offload() const {
    return get_offload( name(), ETHTOOL_GTSO );
}

/**
 */
int
Network::Interface::ufo_offload() const {
    return get_offload( name(), ETHTOOL_GUFO );
}

/**
 */
int
Network::Interface::gso_offload() const {
    return get_offload( name(), ETHTOOL_GGSO );
}

/**
 */
static void
set_offload( char *name, int command, int value ) {
    struct ifreq request;
    struct ethtool_value eval;

    memset( &request, 0, sizeof(request) );
    strcpy( request.ifr_name, name );
    int ethtool = ::socket( AF_INET, SOCK_DGRAM, 0 );

    eval.cmd = command;
    eval.data = value;
    request.ifr_data = (caddr_t)&eval;
    int result = ioctl(ethtool, SIOCETHTOOL, &request);
    close( ethtool );

    if (result != 0) {
        char *err, e[128];
        err = strerror_r( errno, e, sizeof(e) );
        log_err( "failed to set ethtool offload settings for '%s': %s", name, err );
    }
}

/**
 */
void
Network::Interface::rx_offload( int value ) const {
    set_offload( name(), ETHTOOL_SRXCSUM, value );
}

/**
 */
void
Network::Interface::tx_offload( int value ) const {
    set_offload( name(), ETHTOOL_STXCSUM, value );
}

/**
 */
void
Network::Interface::sg_offload( int value ) const {
    set_offload( name(), ETHTOOL_SSG, value );
}

/**
 */
void
Network::Interface::tso_offload( int value ) const {
    set_offload( name(), ETHTOOL_STSO, value );
}

/**
 */
void
Network::Interface::ufo_offload( int value ) const {
    set_offload( name(), ETHTOOL_SUFO, value );
}

/**
 */
void
Network::Interface::gso_offload( int value ) const {
    set_offload( name(), ETHTOOL_SGSO, value );
}

/**
 */
bool
Network::Interface::negotiate() {
    long delta = ::time(0) - last_negotiation;
    if ( delta < 60 ) {
        log_notice( "negotiated link for '%s' too recently, skipping renegotiate", name() );
        return false;
    }

    if ( is_quiesced() ) {
        log_notice( "%s(%d): is quiesced, not negotiating", name(), index() );
        return false;
    }

    log_notice( "%s(%d): negotiate link", name(), index() );

    struct ifreq request;
    struct ethtool_value data;

    memset( &request, 0, sizeof(request) );
    strcpy( request.ifr_name, name() );
    int ethtool = ::socket( AF_INET, SOCK_DGRAM, 0 );

    data.cmd = ETHTOOL_NWAY_RST;
    request.ifr_data = (caddr_t)&data;
    int error = ioctl( ethtool, SIOCETHTOOL, &request );

    bool result = true;
    if ( error < 0 ) {
        log_err( "%s(%d): failed to negotiate link", name(), index() );
        result = false;
    }
    close( ethtool );

    ::time( &last_negotiation );
    return result;
}

/**
 * \todo check lstat errno -- because there may be some cases where this 
 * really is a physical device
 */
bool Network::Interface::is_physical() const {
    char path[1024];
    struct stat stat;

    sprintf( path, "/sys/class/net/%s/device", _name );
// XXX BugzID 17210 TI-COWLEY kernel sysfs has /device for vifs
    if ( ( lstat(path, &stat) == 0 ) && not_named_vifN() ) {
        return true;
    }
    return false;
}

/**
 */
bool Network::Interface::is_full_duplex() const {
    return (_duplex & DUPLEX_FULL) != 0;
}

/**
 * On Linux this is determined through sysfs
 */
bool Network::Interface::is_bridge() const {
    char path[1024];
    struct stat stat;

    sprintf( path, "/sys/class/net/%s/bridge", _name );
    if ( lstat(path, &stat) == 0 ) {
        return true;
    }
    return false;
}

/**
 */
bool Network::Interface::is_captured() const {
    char path[1024];
    struct stat stat;

    sprintf( path, "/sys/class/net/%s/brport", _name );
    if ( lstat(path, &stat) == 0 ) {
        return true;
    }
    return false;
}

/**
 * True if this bridge has a tunnel interface captured.
 */
    // for each net/*/brport if its bridge is my bridge interface
    // glob /sys/class/net/*/brport/bridge
    // if realname of link is /sys/class/net/<myname> ...
bool Network::Interface::is_tunnelled() const {
    bool result = false;

    log_notice( "check if '%s' is tunnelled", name() );

    // if this is not a bridge, it could not be tunnelled.
    if ( not_bridge() ) return false;

    char port_path[1024];
    char bridge_path[1024];
    sprintf( bridge_path, "../../../../class/net/%s", name() );

    log_notice( "check if '%s' is tunnelled", bridge_path );

    glob_t paths;
    memset(&paths, 0, sizeof(paths));

    glob( "/sys/class/net/tun*/brport/bridge", GLOB_NOSORT, NULL, &paths );
    for ( size_t i = 0 ; i < paths.gl_pathc ; i++ ) {
        int count = readlink( paths.gl_pathv[i], port_path, sizeof(port_path) );
        if ( count == -1 ) continue;
        log_notice( "check if '%s' is tunnelled through '%s'", bridge_path, port_path );
        port_path[count] = '\0';

        if ( strcmp(port_path, bridge_path) == 0 ) {
            log_notice( "I (%s) am tunnelled through '%s'", bridge_path, paths.gl_pathv[i] );
            result = true;
            break;
        }
    }
    globfree( &paths );

    return result;
}

/* vim: set autoindent expandtab sw=4: */
