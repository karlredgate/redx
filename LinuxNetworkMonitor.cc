
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

/** \file LinuxNetworkMonitor.cc
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
#include "NetLink.h"
#include "NetLinkMonitor.h"
#include "Neighbor.h"
#include "Interface.h"
#include "LinuxNetworkMonitor.h"

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

/** When a NewLink message is received, create/update interface object.
 *
 * 
 */
void
Network::LinuxNetworkMonitor::receive( NetLink::NewLink *message ) {
    bool link_requires_repair = false;
    Network::Interface *interface = interfaces[ message->index() ];

    if ( interface == NULL ) {

        if ( debug > 0 ) {
            log_notice( "adding %s(%d) to interface list", message->name(), message->index() );
        }
        interface = new Network::Interface( interp, message );
        interfaces[ message->index() ] = interface;

        interface->update( message );

        if ( interface->is_physical() ) {
//            if ( interface->not_private() ) {
//                if ( interface->not_captured() )  capture( interface );
//            }
            if ( interface->has_link() ) {
              Kernel::NetworkLinkUpEvent event;
              interface->linkUp( &event );
            } else {
              Kernel::NetworkLinkDownEvent event;
              interface->linkDown( &event );
            }
        }

// This can happen on a bridge device if a shared network is deleted before we
// receive the NewLink event announcing that it has been created.
// Typically this happens if there was a buffer overflow on the Netlink socket.

        if ( !interface->exists() ) {
            log_notice( "WARNING: interface %s(%d) does not appear to exist, skipping...",
                    message->name(), message->index() );
            return;
        }

        bool is_bridge = interface->is_bridge();
        bool is_physical = interface->is_physical();

        log_notice( "checking to see if interface %s(%d) should be brought up: %s %s",
                message->name(), message->index(), is_bridge ? "is BRIDGE" : "not BRIDGE",
                is_physical ? "is PHYSICAL" : "not PHYSICAL" );

        const char *name = message->name();

        if ( interface->not_bridge() ) {
            log_notice( "WARNING: interface %s(%d) still does not appear to be a bridge, not bringing it up",
                    message->name(), message->index() );
        }

        if ( is_bridge or is_physical ) {
            bring_up( interface );
            log_notice( "brought up interface %s(%d)", message->name(), message->index() );
        }

        if ( interface->is_up() and interface->is_private() and
             interface->not_sync() and interface->not_listening_to("udp6", 123) ) { // NTP
            log_notice( "%s is not listening to port 123 on its primary address, restart ntpd",
                                interface->name() );
            system( "/usr/bin/config_ntpd --restart" );
        }

    } else { // if we do have it ... look for state changes
        interface->update( message );

        bool report_required = false;
        const char *link_message = "";
        const char *up_message = "";
        const char *running_message = "";
        const char *promisc_message = "";

        if ( interface->link_changed() ) {
            if ( interface->has_link() ) {
                Kernel::NetworkLinkUpEvent event;
                interface->linkUp( &event );
                link_message = ", link up";

                if ( interface->is_up() and interface->is_private() and
                     interface->not_sync() and interface->not_listening_to("udp6", 123) ) { // NTP
                    log_notice( "%s is not listening to port 123 on its primary address, restart ntpd",
                                        interface->name() );
                    system( "/usr/bin/config_ntpd --restart" );
                }

            } else {
                Kernel::NetworkLinkDownEvent event;
                interface->linkDown( &event );
                link_message = ", link down";
                interface->clean_topology();
            }
            report_required = true;
        }

        if ( ( interface->has_link() == false ) and interface->bounce_expired() ) {
            if ( interface->is_physical() and ( interface->not_private() or interface->is_sync() ) ) {
                link_requires_repair = true;
            }
        }

        if ( interface->up_changed() ) {
            up_message = interface->is_up() ? ", oper brought up" : ", oper brought down";
            report_required = true;
        }
        if ( interface->running_changed() ) {
            running_message = interface->has_link() ? ", started running" : ", stopped running";
            report_required = true;
        }
        if ( interface->promiscuity_changed() ) {
            promisc_message = interface->is_promiscuous() ? ", went promiscuous" : ", left promiscuous";
            report_required = true;
        }
        if ( report_required or (debug > 0) ) {
            log_notice( "%s(%d): <NewLink>%s%s%s%s", interface->name(), interface->index(),
                                link_message, up_message, running_message, promisc_message );
        }

        if ( message->family() == AF_BRIDGE ) {
            const char *bridge_name = "UNKNOWN";
            int bridge_index = message->bridge_index();
            if ( bridge_index != 0 ) {
                Network::Interface *bridge = interfaces[ bridge_index ];
                if ( bridge != NULL )  bridge_name = bridge->name();
            }
            log_notice( "%s(%d): added to bridge '%s'",
                                interface->name(), interface->index(), bridge_name );
        }
    }

    if ( link_requires_repair ) {
        if ( interface->has_fault_injected() ) {
            log_notice( "%s(%d) fault injected, not repairing link", interface->name(), interface->index() );
        } else {
            interface->repair_link();
        }
    }
}

/**
 */
void Network::LinuxNetworkMonitor::receive( NetLink::DelLink *message ) {

    if ( message->index() == 0 ) {
        log_warn( "received a DelLink message for inteface index 0 (INVALID)" );
        return;
    }

    Network::Interface *interface = interfaces[ message->index() ];

    if ( interface == NULL ) {
        const char *name = message->name();
        if ( name == NULL ) name = "Unknown";
        log_warn( "unknown interface removed: %s(%d)", name, message->index() );
        return;
    }

    interface->update( message );

    bool report_required = false;  // Change this to false
    const char *link_message = "";
    const char *up_message = "";
    const char *running_message = "";
    const char *promisc_message = "";

    if ( interface->link_changed() ) {
        if ( interface->has_link() ) {
            Kernel::NetworkLinkUpEvent event;
            interface->linkUp( &event );
            link_message = ", link up";
        } else {
            // pass message into the NetworkLinkDownEvent
            Kernel::NetworkLinkDownEvent event;
            interface->linkDown( &event );
            link_message = ", link down";
        }
        report_required = true;
    }

    if ( interface->up_changed() ) {
        up_message = interface->is_up() ? ", oper brought up" : ", oper brought down";
        report_required = true;
    }
    if ( interface->running_changed() ) {
        running_message = interface->has_link() ? ", started running" : ", stopped running";
        report_required = true;
    }
    if ( interface->promiscuity_changed() ) {
        promisc_message = interface->is_promiscuous() ? ", went promiscuous" : ", left promiscuous";
        report_required = true;
    }
    if ( report_required or (debug > 0) ) {
        log_notice( "%s(%d): <DelLink>%s%s%s%s", interface->name(), interface->index(),
                            link_message, up_message, running_message, promisc_message );
    }

    if ( message->family() == AF_BRIDGE ) {
        const char *bridge_name = "UNKNOWN";
        int bridge_index = message->bridge_index();
        if ( bridge_index != 0 ) {
            Network::Interface *bridge = interfaces[ bridge_index ];
            if ( bridge != NULL )  bridge_name = bridge->name();
        }
        if ( interface->is_captured() ) {
            log_notice( "%s(%d): removed from bridge '%s'",
                                interface->name(), interface->index(), bridge_name );
        } else {
            log_notice( "%s(%d): DelLink message from bridge '%s' -- but not removed",
                                interface->name(), interface->index(), bridge_name );
        }
        return;
    }

    if ( message->change() == 0xFFFFFFFF ) {
        log_warn( "%s(%d): removed from system", interface->name(), interface->index() );
        // stop listener thread
        //   need Network::Interface to have a handle on its listener thread
        interface->remove();
    }

}

/**
 */
void
Network::LinuxNetworkMonitor::receive( NetLink::NewAddress *message ) {

    Network::Interface *interface = interfaces[ message->index() ];
    if ( interface == NULL ) {
        // address was removed from an interface we do not
        // track (like a VIF)
        return;
    }

    switch ( message->family() ) {
    case AF_INET6: // skip below and attempt to reconfigure
        break;
    case AF_INET:
        log_notice( "IPv4 address added to '%s'", interface->name() );
        return;
    default:
        log_notice( "Unknown address family added to '%s'", interface->name() );
        return;
    }

    if (  interface->is_primary( message->in6_addr() )  ) {
        log_notice( "primary ipv6 address added to '%s'", interface->name() );
        if ( interface->is_private() and interface->not_sync() and
             interface->not_listening_to("udp6", 123) ) { // NTP
            log_notice( "%s is not listening to port 123 on its primary address, restart ntpd",
                                interface->name() );
            system( "/usr/bin/config_ntpd --restart" );
        }
    } else {
        if ( debug > 0 ) {
            log_notice( "secondary ipv6 address added to '%s', ignoring", interface->name() );
        }
    }

}

/**
 */
void
Network::LinuxNetworkMonitor::receive( NetLink::DelAddress *message ) {

    Network::Interface *interface = interfaces[ message->index() ];
    if ( interface == NULL ) {
        // address was removed from an interface we do not
        // track (like a VIF)
        return;
    }

    switch ( message->family() ) {
    case AF_INET6: // skip below and attempt to reconfigure
        break;
    case AF_INET:
        log_notice( "IPv4 address removed from '%s', ignoring", interface->name() );
        return;
    default:
        log_notice( "Unknown address family removed from '%s', ignoring", interface->name() );
        return;
    }

    if (  interface->is_primary( message->in6_addr() )  ) {
        if ( debug > 0 ) {
            log_notice( "primary ipv6 address removed from '%s', repairing", interface->name() );
        }
        interface->configure_addresses();
    } else {
        if ( debug > 0 ) {
            log_notice( "secondary ipv6 address removed from '%s', ignoring", interface->name() );
        }
    }
}

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
Network::LinuxNetworkMonitor::sendto( void *message, size_t length, int flags, const struct sockaddr_in6 *address) {
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
Network::LinuxNetworkMonitor::advertise() {
    std::map<int, Network::Interface *>::const_iterator iter = interfaces.begin();
    while ( iter != interfaces.end() ) {
        Network::Interface *interface = iter->second;
        if ( interface != NULL )  interface->advertise();
        iter++;
    }

    return 0;
}

/** Iterate and call a callback for each Network::Interface.
 */
int
Network::LinuxNetworkMonitor::each_interface( Network::InterfaceIterator& callback ) {
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
Network::LinuxNetworkMonitor::each_node( NodeIterator& callback ) {
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

/** Return the Bridge Interface for a physical Interface that
 *  has been captured in a Bridge.
 */
Network::Interface *
Network::LinuxNetworkMonitor::find_bridge_interface( Network::Interface *interface ) {
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
 */
void
Network::LinuxNetworkMonitor::persist_interface_configuration() {
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
    if ( fdatasync(fd) < 0 ) {
        log_err( "IO error saving persistent device names" );
    }
    fclose( f );
    rename( "/etc/udev/rules.d/.tmp", "/etc/udev/rules.d/58-net-rename.rules" );
}

/** Capture the interface in a bridge
 *
 * Bridges are numbered based on the backing ibizN number.
 * The number is simply cloned from the backing interface to
 * the bridge name.
 */
void
Network::LinuxNetworkMonitor::capture( Network::Interface *interface ) {
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
Network::LinuxNetworkMonitor::bring_up( Network::Interface *interface ) {
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
 *
 */
Network::Node*
NetLink::LinuxNetworkMonitor::intern_node( UUID& uuid ) {
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
NetLink::LinuxNetworkMonitor::remove_node( UUID *uuid ) {
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
NetLink::LinuxNetworkMonitor::find_node( UUID *uuid ) {
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
NetLink::LinuxNetworkMonitor::save_cache() {
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
NetLink::LinuxNetworkMonitor::clear_partners() {
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
NetLink::LinuxNetworkMonitor::load_cache() {
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
        entry.flags.is_private = interface.is_private();
        entry.interface.ordinal = interface.ordinal();
        interface.lladdr( &entry.primary_address );

        unsigned char *mac = interface.mac();
        entry.mac[0] = mac[0];
        entry.mac[1] = mac[1];
        entry.mac[2] = mac[2];
        entry.mac[3] = mac[3];
        entry.mac[4] = mac[4];
        entry.mac[5] = mac[5];

        if ( interface.not_bridge() and interface.not_private() ) {
            return 0;
        }
        if ( debug > 1 ) log_notice( "write host entry for %s", interface.name() );
        // populate entry for the interface itself
        ssize_t bytes = write( fd, &entry, sizeof(entry) );
        if ( (size_t)bytes < sizeof(entry) ) {
            log_err( "write failed creating hosts entry" );
        }

        // call iterator for each neighbor
        WriteHostsForNeighbors callback(fd);
        interface.each_neighbor( callback );

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
NetLink::LinuxNetworkMonitor::update_hosts() {
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
 * This thread connects a netlink socket and listens for broadcast
 * messages from the kernel.  This should inform us of the network
 * related events.
 *
 * When an event occurs, this needs to trigger some other action.
 * The action will be either a TCL script, or a TCL assembled AST.
 *
 * NewLink -- if not known add -- eval event
 *         -- if known -- call object->update( NewLink )
 *    always -- if bridge -- create bridge?
 *    check if bridge present -- check if private
 *
 * NewRoute ??
 * NewAddr
 *
 * General:  make sure all biz interfaces have bridges, make sure
 * private does not, and make sure all interfaces (bridge and 
 * private) have addresses.  Make sure all interfaces and bridges
 * are named appropriately.
 *
 * Need platform service to determine which is private...
 *
 * For each interface -- ping/ICMP NUD and multicast svc
 * 
 * ZeroConf specific interfaces
 */
void
Network::LinuxNetworkMonitor::run() {
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
Network::LinuxNetworkMonitor::LinuxNetworkMonitor( Tcl_Interp *interp, Network::ListenerInterfaceFactory factory )
: Thread("network.monitor"),
  NetLink::Monitor(),
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
Network::LinuxNetworkMonitor::~LinuxNetworkMonitor() {
}

/* vim: set autoindent expandtab sw=4 : */
