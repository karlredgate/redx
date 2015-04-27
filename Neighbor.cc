
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

/** \file Neighbor.cc
 * \brief 
 *
 */

#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <glob.h>
#include <errno.h>

#include <tcl.h>
#include "tcl_util.h"

#include "logger.h"
#include "util.h"
#include "Neighbor.h"
#include "Interface.h"

/*
 * I do not like this - try to eliminate OS defines
 */
#if defined(__APPLE__) || defined(__darwin__)
/* OSX seems not to define these. */
#ifndef s6_addr16
#define s6_addr16 __u6_addr.__u6_addr16
#endif
#ifndef s6_addr32
#define s6_addr32 __u6_addr.__u6_addr32
#endif
#endif

namespace { int debug = 0; }

/**
 */
void
Network::Node::uuid( UUID& that ) {
    _uuid = that;
    _ordinal = 255;
    valid = true;
    // log_notice( "new node allocated with uuid %s", _uuid.to_s() );
}

/**
 */
void
Network::Node::ordinal( uint8_t value ) {
    _ordinal = value;
    // log_notice( "node%d %s", _ordinal, _uuid.to_s() );
}

/**
 */
bool
Network::Node::operator == ( UUID& that ) {
    return _uuid == that;
}

/**
 */
bool
Network::Node::operator != ( UUID& that ) {
    return _uuid != that;
}

/**
 * Used to iterate through the node list and clear the partner
 * bit in all entries.
 */
class ClearNodePartner : public Network::NodeIterator {
public:
    ClearNodePartner() {}
    virtual ~ClearNodePartner() {}
    virtual int operator() ( Network::Node& node ) {
        if ( node.not_partner() ) return 0;
        log_notice( "clear partner [%s]", node.uuid().to_s() );
        node.clear_partner();
        return 1;
    }
};


/**
 */
Network::Peer::~Peer() {
    if ( _name != NULL ) free( _name );
    _name = NULL;
}

/**
 * This one should be deprecated
 */
Network::Peer::Peer( Network::Node *__node, struct in6_addr *address )
: _node(__node), _ordinal(0), _name(NULL) {
    memcpy( &lladdr, address, sizeof lladdr );
    memset( &neighbor_updated, 0, sizeof neighbor_updated );
}

/**
 */
void
Network::Peer::node( Node *that ) {
    _node = that;

    /*
     * Here we have just discovered a new partner.  If this was the
     * cached partner, then the _node would already be a partner.
     * Since it was not and the neighbor is a partner - it must
     * have been discovered on priv0, which either overrides the
     * cached partner (node replace or the like) or this is the
     * first time we have discovered the partner.  Either way
     * we need to cache the newly discovered partner.
     */
    if ( is_partner() and _node->not_partner() ) {
        _node->make_partner();
        log_notice( "node %s is partner", _node->uuid().to_s() );
    }

    /*
     * Cases that are ignored:
     * peer->not_partner() and _node->not_partner()
     *   -- who cares about this neighbor
     *
     * peer->is_partner() and _node->is_partner()
     *   -- we already have all necessary state
     *
     */

    if ( debug < 2 ) return;
    char buffer[80];
    const char *address_string = inet_ntop(AF_INET6, &lladdr, buffer, sizeof buffer);
    log_notice( "%s at %s is node %s",
                        (_node->is_partner() ? "partner" : "neighbor"),
                        address_string, _node->uuid().to_s() );
}

/**
 */
Network::Node *
Network::Peer::node() const {
    return _node;
}

/**
 * Since this is a sort of allocation method, maybe this should check if
 * already valid and either disallow usage -- or at least report that the
 * address is changing.
 */
void
Network::Peer::address( struct in6_addr *addr ) {
    memcpy( &lladdr, addr, sizeof lladdr );
    valid = true;
    if ( debug < 2 ) return;
    char buffer[80];
    const char *address_string = inet_ntop(AF_INET6, addr, buffer, sizeof buffer);
    log_notice( "new neighbor %s", address_string );
}

/**
 */
void
Network::Peer::set_interface( bool set_private, uint8_t set_ordinal ) {
    _is_private = set_private;
    _ordinal = set_ordinal;
}

/**
 */
void
Network::Peer::set_interface_name( char *name ) {
    if ( name == NULL ) {			// If the new name is NULL...
       if ( _name != NULL ) {			// ...and the Peer had a name...
           free( _name );			//    ...free the old name...
           _name = NULL;			//    ...and invalidate the name.
       }
       return;
    } 

    if ( _name != NULL ) {			// If the Peer already has a name...
        if ( strcmp( name, _name ) ) {		// ...and it is *not* the same name...
            free( _name );			//    ...free the old name...
            _name = strdup( name );		//    ...and copy the new name.
        }					// If it *is* the same name, don't do free()/strdup()
    } else {					// If the Peer does not have a name...
        _name = strdup( name );			// ...copy the new name.
    }
}

/**
 */
void
Network::Peer::copy_address( struct in6_addr *address ) const {
    memcpy( address, &lladdr, sizeof lladdr );
}

/**
 */
void
Network::Peer::touch_advertised() {
    gettimeofday( &neighbor_advertised, 0 );
}

/**
 */
void
Network::Peer::touch() {
    gettimeofday( &neighbor_updated, 0 );
}

/**
 */
int
Network::Peer::seconds_since_last_update() const {
    struct timeval now;
    gettimeofday( &now, 0 );
    return now.tv_sec - neighbor_updated.tv_sec;
}

/**
 */
bool
Network::Peer::operator == ( struct in6_addr& that ) {
    if ( that.s6_addr32[0] != lladdr.s6_addr32[0] ) return false;
    if ( that.s6_addr32[1] != lladdr.s6_addr32[1] ) return false;
    if ( that.s6_addr32[2] != lladdr.s6_addr32[2] ) return false;
    if ( that.s6_addr32[3] != lladdr.s6_addr32[3] ) return false;
    return true;
}

/**
 */
bool
Network::Peer::operator != ( struct in6_addr& that ) {
    if ( that.s6_addr32[0] != lladdr.s6_addr32[0] ) return true;
    if ( that.s6_addr32[1] != lladdr.s6_addr32[1] ) return true;
    if ( that.s6_addr32[2] != lladdr.s6_addr32[2] ) return true;
    if ( that.s6_addr32[3] != lladdr.s6_addr32[3] ) return true;
    return false;
}

/**
 */
bool
Network::Peer::has_address( struct in6_addr *address ) {
    if ( address->s6_addr32[0] != lladdr.s6_addr32[0] ) return false;
    if ( address->s6_addr32[1] != lladdr.s6_addr32[1] ) return false;
    if ( address->s6_addr32[2] != lladdr.s6_addr32[2] ) return false;
    if ( address->s6_addr32[3] != lladdr.s6_addr32[3] ) return false;
    return true;
}

/**
 */
static void
construct_remote_interface_name( Network::Peer *neighbor, char *name, int size ) {

    if ( neighbor->name() != NULL ) {
        strncpy( name, neighbor->name(), size );
    } else {
        int ordinal = neighbor->ordinal();
        if ( neighbor->is_private() ) {
            if ( ordinal & 0x40 ) {
                int slot = ( ordinal & 0x3c ) >> 2;
                int port = ( ordinal & 0x03);
                snprintf( name, size, "sync_pci%dp%d", slot, port );
            } else {
                snprintf( name, size, "priv%d", ordinal );
            }
        } else {
            snprintf( name, size, "ibiz%d", ordinal );
        }
    }
}

/**
 */
void
Network::Peer::topology_changed( Network::Monitor *monitor, Network::Interface *interface ) {
    char node_ordinal = (node() == NULL) ? '?' : ('0' + node()->ordinal());
    char remote_interface_name[32];
    construct_remote_interface_name( this, remote_interface_name,
                                     sizeof(remote_interface_name) );
    log_notice( "Topology change: node%c:%s seen on %s",
                        node_ordinal, remote_interface_name, interface->name() );

    _spine_notified = true;
    monitor->topology_changed();
}

/* vim: set autoindent expandtab sw=4 : */
