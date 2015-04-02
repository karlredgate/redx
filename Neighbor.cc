
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

#include <asm/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <syslog.h>
#include <string.h>
#include <glob.h>
#include <errno.h>

#include <tcl.h>
#include "tcl_util.h"

#include "util.h"
#include "host_table.h"
#include "NetLink.h"
#include "Network.h"

namespace { int debug = 0; }

/**
 */
void
Network::Node::uuid( UUID& that ) {
    _uuid = that;
    _ordinal = 255;
    valid = true;
    // syslog( LOG_NOTICE, "new node allocated with uuid %s", _uuid.to_s() );
}

/**
 */
void
Network::Node::ordinal( uint8_t value ) {
    _ordinal = value;
    // syslog( LOG_NOTICE, "node%d %s", _ordinal, _uuid.to_s() );
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
        syslog( LOG_NOTICE, "clear partner [%s]", node.uuid().to_s() );
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
        syslog( LOG_NOTICE, "node %s is partner", _node->uuid().to_s() );
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
    syslog( LOG_NOTICE, "%s at %s is node %s",
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
    syslog( LOG_NOTICE, "new neighbor %s", address_string );
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
 * DEPRECATED
 */
bool
Network::Peer::send_topology_event( Network::Interface *interface ) {
    char node_ordinal = (node() == NULL) ? '?' : ('0' + node()->ordinal());
    syslog( LOG_NOTICE, "sending event to spine for node%c:%s%d seen on %s",
                        node_ordinal,
                        is_private() ? "priv" : "biz", ordinal(),
                        interface->name() );

    char *event_name = const_cast<char*>("SuperNova::NetTopologyUpdate");
    pid_t child = fork();

    if ( child < 0 ) {
        syslog( LOG_ERR, "failed to send %s event - couldn't fork", event_name );
        return false;
    }

    _spine_notified = true;
    if ( child > 0 ) {
        int status;
        waitpid( child, &status, 0);
        return true;
    }

    char *argv[] = { const_cast<char*>("genevent"), event_name, 0 };
    char *envp[] = { 0 };
    if ( execve("/usr/lib/spine/bin/genevent", argv, envp) < 0 ) {
        syslog( LOG_ERR, "failed to send %s event - couldn't execve", event_name );
        _exit( 0 );
    }

    // NOT REACHED
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
    syslog( LOG_NOTICE, "Topology change: node%c:%s seen on %s",
                        node_ordinal, remote_interface_name, interface->name() );

    _spine_notified = true;
    monitor->topology_changed();
}

/**
 * need some way to find these objects...
 * use TCL ..hmmm
 */
bool Neighbor_Module( Tcl_Interp *interp ) {

    Tcl_Command command;

    Tcl_Namespace *ns = Tcl_CreateNamespace(interp, "Network", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    if ( Tcl_LinkVar(interp, "Network::debug", (char *)&debug, TCL_LINK_INT) != TCL_OK ) {
        syslog( LOG_ERR, "failed to link Network::debug" );
        exit( 1 );
    }

    command = Tcl_CreateObjCommand(interp, "Network::Monitor", Monitor_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Network::Probe", Probe_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Network::Manager", Manager_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    if ( Network::Interface::Initialize(interp) == false ) {
        syslog( LOG_ERR, "Network::Interface::Initialize failed" );
        return false;
    }

    if ( Network::SharedNetwork::Initialize(interp) == false ) {
        syslog( LOG_ERR, "Network::SharedNetwork::Initialize failed" );
        return false;
    }

    if ( Network::Bridge::Initialize(interp) == false ) {
        syslog( LOG_ERR, "Network::Bridge::Initialize failed" );
        return false;
    }

    if ( Network::Tunnel::Initialize(interp) == false ) {
        syslog( LOG_ERR, "Network::Tunnel:Initialize failed" );
        return false;
    }

    return true;
}

/* vim: set autoindent expandtab sw=4 : */