
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

/** \file NetworkMonitor.cc
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
#include <string.h>
#include <glob.h>
#include <errno.h>

#include <tcl.h>
#include "tcl_util.h"

#include "logger.h"
#include "util.h"
#include "host_table.h"
#include "NetworkMonitor.h"
#include "Neighbor.h"
#include "Interface.h"

namespace { int debug = 0; }

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

/** Send a packet to each interface.
 *
 * The interface code will prune which interfaces are valid to send to
 * based on interface specific policy.  Currently, this means priv0
 * and bizN bridge interfaces.
 *
 * See Network::Interface::sendto() method (in Interface.cc) for description
 * of how this is done.
 */
int
Network::Monitor::sendto( void *message, size_t length, int flags, const struct sockaddr_in6 *address) {
    std::map<int, Network::Interface *>::const_iterator iter = interfaces.begin();
    while ( iter != interfaces.end() ) {
        Network::Interface *interface = iter->second;
        if ( interface != NULL ) {
            interface->sendto( message, length, flags, address );
        }
        iter++;
    }
    return 0;
}

/** Send ICMPv6 neighbor advertisements for each interface.
 * 
 * Iterate through each known priv/biz network and send neighbor
 * advertisements out of this interface.  This will keep the peer's
 * neighbor table up to date for this host.
 *
 * When looking at the peer's * neighbor table (ip -6 neighbor ls) you
 * should see "REACHABLE" for this hosts addresses if this node is up and
 * running netmgr.
 *
 * See the Interface.cc advertise code for how this is done.
 */
int
Network::Monitor::advertise() {
    std::map<int, Network::Interface *>::const_iterator iter = interfaces.begin();
    while ( iter != interfaces.end() ) {
        Network::Interface *interface = iter->second;
        if ( interface != NULL )  interface->advertise();
        iter++;
    }

    return 0;
}

/**
 */
void
Network::NetworkMonitor::capture( Network::Interface *interface ) {
    fprintf( stderr, "UNIMPLEMENTED capture\n" );
    abort();
}

/**
 */
void
Network::NetworkMonitor::bring_up( Network::Interface *interface ) {
    fprintf( stderr, "UNIMPLEMENTED bring_up\n" );
    abort();
}

/**
 */
Network::Interface *
Network::NetworkMonitor::find_bridge_interface( Network::Interface *interface ) {
    fprintf( stderr, "UNIMPLEMENTED find_bridge_interface\n" );
    abort();
}

/** Iterate and call a callback for each Network::Interface.
 */
int
Network::Monitor::each_interface( Network::InterfaceIterator& callback ) {
    int result = 0;

    std::map<int, Network::Interface *>::const_iterator iter = interfaces.begin();
    while ( iter != interfaces.end() ) {
        Network::Interface *interface = iter->second;
        if ( interface != NULL )  result += callback( interface );
        iter++;
    }

    return result;
}

/** Iterate and call a callback for each Node.
 */
int
Network::Monitor::each_node( NodeIterator& callback ) {
    int result = 0;

    pthread_mutex_lock( &node_table_lock );
    for ( int i = 0 ; i < NODE_TABLE_SIZE ; ++i ) {
        Network::Node& node = node_table[i];
        if ( node.is_invalid() ) continue;
        result += callback( node );
    }
    pthread_mutex_unlock( &node_table_lock );

    return result;
}

/**
 *
 */
Network::Node*
Network::Monitor::intern_node( UUID& uuid ) {
    int in_use_count = 0;
    Network::Node *result = NULL;
    Network::Node *available = NULL;

    pthread_mutex_lock( &node_table_lock );
    int i = 0;
    for ( ; i < NODE_TABLE_SIZE ; ++i ) {
        Network::Node& node = node_table[i];
        if ( node.is_invalid() ) {
            available = &node;
            continue;
        }
        in_use_count += 1;
        if ( node != uuid ) continue;
        result = &node;
        break;
    }
    if ( in_use_count > 256 ) {
        if ( table_warning_reported == false ) {
            log_warn( "WARNING: node table exceeds 256 entries" );
            table_warning_reported = true;
        }
    }
    if ( result == NULL ) {
        if ( available == NULL ) {        // EDM ??? wasn't this set above ???
            for ( ; i < NODE_TABLE_SIZE ; ++i ) {
                Network::Node& node = node_table[i];
                if ( node.is_valid() ) continue;
                available = &node;
                break;
            }
        }

        if ( available != NULL ) {
            available->uuid( uuid );
            result = available;
        } else {
            if ( table_error_reported == false ) {
                log_err( "ERROR: node table is full" );
                table_error_reported = true;
            }
        }
    }
    pthread_mutex_unlock( &node_table_lock );

    return result;
}

/**
 *
 */
bool
Network::Monitor::remove_node( UUID *uuid ) {
    pthread_mutex_lock( &node_table_lock );
    for ( int i = 0 ; i < NODE_TABLE_SIZE ; ++i ) {
        Network::Node& node = node_table[i];
        if ( node.is_invalid() ) continue;
        if ( node != *uuid ) continue;
        node.invalidate();
    }
    pthread_mutex_unlock( &node_table_lock );

    return true;
}

/**
 *
 */
Network::Node*
Network::Monitor::find_node( UUID *uuid ) {
    using namespace Network;
    Node *result = NULL;

    pthread_mutex_lock( &node_table_lock );
    for ( int i = 0 ; i < NODE_TABLE_SIZE ; ++i ) {
        Network::Node& node = node_table[i];
        if ( node.is_invalid() ) continue;
        if ( node != *uuid ) continue;
        result = &node;
    }
    pthread_mutex_unlock( &node_table_lock );

    return result;
}

/**
 * Save the partner node id for later usage as a cache - in case
 * priv0 is down and we cannot discover the node id dynamically.
 */
void
Network::Monitor::save_cache() {
    FILE *f = fopen( "partner-cache", "w" );
    if ( f == NULL ) {
        log_notice( "could not save partner cache" );
        return;
    }

    int partner_count = 0;

    pthread_mutex_lock( &node_table_lock );
    for ( int i = 0 ; i < NODE_TABLE_SIZE ; ++i ) {
        Network::Node& node = node_table[i];
        if ( node.is_invalid() ) continue;
        if ( node.not_partner() ) continue;
        if ( debug ) log_notice( "save partner [%s]", node.uuid().to_s() );
        if ( partner_count == 0 ) {
            fprintf( f, "%s\n", node.uuid().to_s() );
        }
        partner_count++;
    }
    pthread_mutex_unlock( &node_table_lock );

    fclose( f );

    if ( partner_count > 1 ) {
        log_err( "%%BUG multiple partner entries in node table" );
    }
}

/**
 */
void
Network::Monitor::clear_partners() {
    ClearNodePartner callback;
    int partner_count = each_node( callback );
    if ( partner_count > 0 ) {
        log_notice( "cleared %d partners", partner_count );
    }
}

/**
 * This is called when netmgr starts.  It simply loads the previous saved
 * value of the partner node id and creates an entry in the node table
 * marked as a partner.  This value was written the last time netmgr
 * discovered a partner node on priv0.
 */
void
Network::Monitor::load_cache() {
    char buffer[80];
    FILE *f = fopen( "partner-cache", "r" );

    if ( f == NULL ) {
        log_notice( "partner cache not present" );
        return;
    }

    fscanf( f, "%s\n", buffer );
    fclose( f );

    log_notice( "loaded partner as [%s]", buffer );

    /*
     * This should not be necessary - when netmgr starts it should
     * not have any partner node table entries.
     */
    ClearNodePartner callback;
    int partner_count = each_node( callback );
    if ( partner_count > 0 ) {
        log_notice( "cleared %d partners", partner_count );
    }

    UUID uuid(buffer);
    Network::Node *node = intern_node( uuid );
    node->make_partner();
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
        entry.flags.is_private = neighbor.is_private();
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
Network::Monitor::update_hosts() {
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
Network::Monitor::Monitor( Tcl_Interp *interp, Network::ListenerInterfaceFactory factory )
: Thread("network.monitor"),
  interp(interp),
  factory(factory),
  table_warning_reported(false),
  table_error_reported(false)
{
    pthread_mutex_init( &node_table_lock, NULL );
    size_t size = sizeof(Node) * NODE_TABLE_SIZE;
    node_table = (Node *)mmap( 0, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0 );

    if ( node_table == MAP_FAILED ) {
        log_err( "node table alloc failed" );
        exit( errno );
    }

    for ( int i = 0 ; i < NODE_TABLE_SIZE ; i++ ) {
        node_table[i].invalidate();
    }

    if ( debug > 0 ) log_err( "node table is at %p (%zu)", node_table, size );
}

/**
 */
Network::Monitor::~Monitor() {
}

/* vim: set autoindent expandtab sw=4 : */
