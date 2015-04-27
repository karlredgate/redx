
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

/** \file DarwinNetworkMonitor.cc
 * \brief 
 *
 */

#include <sys/types.h>
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

#include <libgen.h> // for basename()

#include <tcl.h>
#include "tcl_util.h"

#include "logger.h"
#include "util.h"
#include "host_table.h"
#include "Neighbor.h"
#include "Interface.h"
#include "Bridge.h"
#include "NetworkMonitor.h"
#include "DarwinNetworkMonitor.h"

namespace { int debug = 0; }

/**
 * The network monitor needs to probe the current system for network
 * devices and keep a list of devices that are being monitored.
 *
 * The discover interface bit here -- creates a RouteSocket, send
 * a GetLink request, and process each response by calling the monitor
 * object's receive callback interface.  This callback will popoulate the
 * Interface table.
 *
 * Tcl_Interp arg to the constructor is for handlers that are registered
 * for network events.  (?? also how to handle ASTs for handlers)
 */
Network::DarwinNetworkMonitor::DarwinNetworkMonitor( Tcl_Interp *interp, Network::ListenerInterfaceFactory factory )
: Network::Monitor(interp, factory)
{
}

/**
 */
Network::DarwinNetworkMonitor::~DarwinNetworkMonitor() {
}

/** Return the Bridge Interface for a physical Interface that
 *  has been captured in a Bridge.
 */
Network::Interface *
Network::DarwinNetworkMonitor::find_bridge_interface( Network::Interface *interface ) {
    if ( interface->not_physical() ) return NULL;
    if ( interface->not_captured() ) return NULL;

    std::map<int, Network::Interface *>::const_iterator iter = interfaces.begin();
    while ( iter != interfaces.end() ) {
        Network::Interface *interface2 = iter->second;
        if ( ( interface2 != NULL ) && interface2->is_bridge() ) {

            char path[1024];
            sprintf( path, "/sys/class/net/%s/brif/*", interface2->name() );

            glob_t paths;
            memset(&paths, 0, sizeof(paths));

            glob( path, GLOB_NOSORT, NULL, &paths );
            for ( size_t i = 0 ; i < paths.gl_pathc ; i++ ) {
                if ( strcmp( interface->name(), basename( paths.gl_pathv[i]) ) == 0 ) {
                    globfree( &paths );
                    return interface2;
                }
            }

            globfree( &paths );
        }
        iter++;
    }

    log_notice( "Unable to find bridge interface for %s", interface->name() );
    return NULL;
}

/** Persist the current interface config.
 *
 * This is currently incorrect for Darwin...
 */
void
Network::DarwinNetworkMonitor::persist_interface_configuration() {
    log_notice( "persisting the change in interface configuration" );
    FILE *f = fopen( "/etc/udev/rules.d/.tmp", "w" );
    std::map<int, Network::Interface *>::const_iterator iter = interfaces.begin();
    for ( ; iter != interfaces.end() ; iter++ ) {
        Network::Interface *interface = iter->second;
        if ( interface == NULL ) continue;
        if ( interface->not_physical() ) continue;
        // save this interface??
        // KERNEL=="eth*", SYSFS{address}=="<mac>", NAME="<newname>"
        const unsigned char *mac = interface->mac();
        fprintf( f, "KERNEL==\"eth*\", SYSFS{address}==\"%02x:%02x:%02x:%02x:%02x:%02x\", NAME=\"%s\", OPTIONS=\"last_rule\"\n",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], interface->name() );
    }
    int fd = fileno(f);
#if 0
    // not present in darwin - alternative?
    if ( fdatasync(fd) < 0 ) {
        log_err( "IO error saving persistent device names" );
    }
#endif
    fclose( f );
    rename( "/etc/udev/rules.d/.tmp", "/etc/udev/rules.d/58-net-rename.rules" );
}

/** Capture the interface in a bridge
 *
 * Bridges are numbered based on the backing ibizN number.
 * The number is simply cloned from the backing interface to
 * the bridge name.
 *
 * TODO
 * This is bogus - it assumes ibiz and biz - need to remove
 * maybe change so there is a second arg for the bridge? or bridge name
 */
void
Network::DarwinNetworkMonitor::capture( Network::Interface *interface ) {
    if ( interface->has_fault_injected() ) {
        log_notice( "%s(%d) fault injected, not capturing in bridge", interface->name(), interface->index() );
        return;
    }

    log_notice( "need to capture '%s'", interface->name() );
    int id = 0xdead;
    sscanf( interface->name(), "ibiz%d", &id );
    if ( id == 0xdead ) {
        log_err( "ERROR : invalid interface name for bridge" );
        return;
    }
    char buffer[40];
    sprintf( buffer, "biz%d", id );

    Bridge *bridge = new Bridge( buffer );
    const char *error = bridge->create();
    if ( error != NULL ) {
        log_err( "ERROR : Failed to create bridge: %s", error );
        // \todo when bridge create fails -- what do we do?
        return;
    }

    bridge->set_mac_address( interface->mac() );

    if ( bridge->is_tunnelled() ) {
        log_warn( "WARNING '%s' is tunnelled, not capturing '%s'", bridge->name(), interface->name() );
    } else {
        bridge->capture( interface );
    }

    // Don't need this after creation
    delete bridge;

    interface->bring_link_up();
}

/** Bring up link and addresses for this interface
 *
 */
void
Network::DarwinNetworkMonitor::bring_up( Network::Interface *interface ) {
    if ( debug > 0 ) log_notice( "bring up '%s'", interface->name() );

    if ( interface->has_fault_injected() ) {
        log_notice( "%s(%d) fault injected, not bringing link up", interface->name(), interface->index() );
    } else {
        interface->bring_link_up();
        interface->configure_addresses();
    }

    interface->create_sockets();
    Network::ListenerInterface *listener = factory( interp, this, interface );
    listener->thread()->start();
}

/**
 */
class WriteHostsForNeighbors : public Network::NeighborIterator {
    int fd;
public:
    WriteHostsForNeighbors( int fd ) : fd(fd) {}
    virtual ~WriteHostsForNeighbors() {}
    virtual int operator() ( Network::Peer& neighbor ) {
        struct host_entry entry;
        memset( &entry, 0, sizeof(entry) );

        Network::Node *node = neighbor.node();
        if ( node == NULL ) {
            return 0;
        }
        if ( node->not_partner() ) {
            return 0;
        }

        entry.flags.valid = 1;
        entry.flags.partner = neighbor.is_partner();
        entry.node.ordinal = node->ordinal();
        entry.interface.ordinal = neighbor.ordinal();
        entry.flags.is_private = 0 /* neighbor.is_private() */ ;
        neighbor.copy_address( &(entry.primary_address) );

        if ( debug > 1 ) log_notice( "write host entry for peer" );
        // populate entry for the interface itself
        ssize_t bytes = write( fd, &entry, sizeof(entry) );
        if ( (size_t)bytes < sizeof(entry) ) {
            log_err( "write failed creating hosts entry peer" );
        }

        return 0;
    }
};
/**
 */
class WriteHostsForInterface : public Network::InterfaceIterator {
    int fd;
public:
    WriteHostsForInterface( int fd ) : fd(fd) {}
    virtual ~WriteHostsForInterface() {}
    virtual int operator() ( Network::Interface * interface ) {
        struct host_entry entry;
        memset( &entry, 0, sizeof(entry) );

        entry.flags.valid = 1;
        entry.flags.partner = 0;
        entry.node.ordinal = gethostid();
        entry.flags.is_private = 0 /* interface->is_private() */ ;
        entry.interface.ordinal = interface->ordinal();
        interface->lladdr( &entry.primary_address );

        unsigned char *mac = interface->mac();
        entry.mac[0] = mac[0];
        entry.mac[1] = mac[1];
        entry.mac[2] = mac[2];
        entry.mac[3] = mac[3];
        entry.mac[4] = mac[4];
        entry.mac[5] = mac[5];

        if ( interface->not_bridge() /* and interface->not_private() */ ) {
            return 0;
        }
        if ( debug > 1 ) log_notice( "write host entry for %s", interface->name() );
        // populate entry for the interface itself
        ssize_t bytes = write( fd, &entry, sizeof(entry) );
        if ( (size_t)bytes < sizeof(entry) ) {
            log_err( "write failed creating hosts entry" );
        }

        // call iterator for each neighbor
        WriteHostsForNeighbors callback(fd);
        interface->each_neighbor( callback );

        return 0;
    }
};

/** Update hosts file with partner addresses
 *
 * For each interface, check each neighbor, if it is a partner
 * then add a host file entry for that name/uuid and interface
 *
 * node0.ip6.ibiz0    fe80::XXXX
 */
void
Network::DarwinNetworkMonitor::update_hosts() {
    if ( mkfile(const_cast<char*>("hosts.tmp"), HOST_TABLE_SIZE) == 0 ) {
        log_err( "could not create the tmp hosts table" );
        return;
    }

    int fd = open("hosts.tmp", O_RDWR);
    if ( fd < 0 ) {
        if ( debug > 0 ) log_err( "could not open the hosts table for writing" );
        return;
    }

    WriteHostsForInterface callback(fd);
    each_interface( callback );

    fsync( fd );
    close( fd );

    unlink( "hosts.1" );
    link( "hosts", "hosts.1" );
    rename( "hosts.tmp", "hosts" );
}


/**
 * When an event occurs, this needs to trigger some other action.
 * The action will be either a TCL script, or a TCL assembled AST.
 *
 * For each interface -- ping/ICMP NUD and multicast svc
 * 
 * ZeroConf specific interfaces
 */
void
Network::DarwinNetworkMonitor::run() {
    load_cache();
    probe();

    if ( debug > 0 ) log_notice( "network monitor started" );
    for (;;) {
        // This should block - we should not need the sleep
        process_one_event();
        sleep( 1 );
    }
}

/**
 * send message to cause creation events to start
 */
void
Network::DarwinNetworkMonitor::probe() {
}

/* vim: set autoindent expandtab sw=4 : */
