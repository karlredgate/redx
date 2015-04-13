
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
#include "NetLink.h"
#include "Network.h"

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
void Network::Monitor::receive( NetLink::NewLink *message ) {
    bool link_requires_repair = false;
    Interface *interface = interfaces[ message->index() ];

    if ( interface == NULL ) {

        if ( debug > 0 ) {
            log_notice( "adding %s(%d) to interface list", message->name(), message->index() );
        }
        interface = new Network::Interface( interp, message );
        interfaces[ message->index() ] = interface;
        if ( _manager != NULL ) {
            _manager->add_interface( interface );
        }

        interface->update( message );

        if ( interface->is_physical() ) {
//            if ( interface->not_private() ) {
//                if ( interface->not_captured() )  capture( interface );
//            }
            if ( interface->has_link() ) {
	      interface->linkUp( message );
            } else {
	      interface->linkDown( message );
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

// Temporary code to get around problem where NewLink message arrives
// before /sysfs entries for the bridge are constructed.  Otherwise we
// won't bring up the interface and the pulse code will not work.
// Have to check for "net_<N>" names and "biz<N>" names as well due to upgrade.

        const char *name = message->name();

        if ( ( ( ( name[0] == 'n' ) &&
                 ( name[1] == 'e' ) &&
                 ( name[2] == 't' ) ) ||
               ( ( name[0] == 'b' ) &&
                 ( name[1] == 'i' ) &&
                 ( name[2] == 'z' ) ) ) &&
             ( !is_bridge ) ) {
            log_notice( "WARNING: interface %s(%d) does not appear to be a bridge, waiting up to 10 seconds",
                    message->name(), message->index() );
            for (int i = 0; i < 10; i++) {
                sleep( 1 );
                if ( interface->is_bridge() ) {
                    is_bridge = true;
                    break;
                }
            }
            if ( interface->not_bridge() ) {
                log_notice( "WARNING: interface %s(%d) still does not appear to be a bridge, not bringing it up",
                        message->name(), message->index() );
            }
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
	        interface->linkUp( message );
                link_message = ", link up";

                if ( interface->is_up() and interface->is_private() and
                     interface->not_sync() and interface->not_listening_to("udp6", 123) ) { // NTP
                    log_notice( "%s is not listening to port 123 on its primary address, restart ntpd",
                                        interface->name() );
                    system( "/usr/bin/config_ntpd --restart" );
                }

            } else {
	        interface->linkDown( message );
                link_message = ", link down";
                interface->clean_topology();
                topology_changed();
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
                Interface *bridge = interfaces[ bridge_index ];
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
void Network::Monitor::receive( NetLink::DelLink *message ) {

    if ( message->index() == 0 ) {
        log_warn( "received a DelLink message for inteface index 0 (INVALID)" );
        return;
    }

    Interface *interface = interfaces[ message->index() ];

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
	    interface->linkUp( message );
            link_message = ", link up";
        } else {
	    interface->linkDown( message );
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
            Interface *bridge = interfaces[ bridge_index ];
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
        //   need Interface to have a handle on its listener thread
        interface->remove();
    }

}

/**
 */
void Network::Monitor::receive( NetLink::NewRoute *message ) {
    if ( debug > 1 ) {
        log_info( "Network::Monitor => process NewRoute -->  scope %d, family %d\n",
                message->scope(),
                message->family()
        );
    }
}

/**
 */
void Network::Monitor::receive( NetLink::DelRoute *message ) {
    if ( debug > 1 ) {
        log_info( "Network::Monitor => process DelRoute -->  scope %d, family %d\n",
                message->scope(),
                message->family()
        );
    }
}

/**
 */
void Network::Monitor::receive( NetLink::NewAddress *message ) {

    Interface *interface = interfaces[ message->index() ];
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
void Network::Monitor::receive( NetLink::DelAddress *message ) {

    Interface *interface = interfaces[ message->index() ];
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

/**
 */
void Network::Monitor::receive( NetLink::NewNeighbor *message ) {
    if ( debug > 1 ) {
        log_info( "Network::Monitor => process NewNeighbor \n" );
    }
}

/**
 */
void Network::Monitor::receive( NetLink::DelNeighbor *message ) {
    if ( debug > 1 ) {
        log_info( "Network::Monitor => process DelNeighbor\n");
    }
}

/**
 */
void Network::Monitor::receive( NetLink::RouteMessage *message ) {
    if ( debug > 1 ) {
        log_info( "Network::Monitor unknown RouteMessage (%d) -- skipping\n", message->type_code() );
    }
}

/**
 * \todo should have some internal state that tells us the last message
 * we received was an error, and what the error was.
 */
void Network::Monitor::receive( NetLink::RouteError *message ) {
    if ( debug > 1 ) {
        log_info( "Network::Monitor RouteError %d\n", message->error() );
    }
}

/** Send a packet to each interface.
 *
 * The interface code will prune which interfaces are valid to send to
 * based on interface specific policy.  Currently, this means priv0
 * and bizN bridge interfaces.
 *
 * See Interface::sendto() method (in Interface.cc) for description
 * of how this is done.
 */
int Network::Monitor::sendto( void *message, size_t length, int flags, const struct sockaddr_in6 *address) {
    std::map<int, Interface *>::const_iterator iter = interfaces.begin();
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
int Network::Monitor::advertise() {
    std::map<int, Interface *>::const_iterator iter = interfaces.begin();
    while ( iter != interfaces.end() ) {
        Network::Interface *interface = iter->second;
        if ( interface != NULL )  interface->advertise();
        iter++;
    }

    return 0;
}

/** Iterate and call a callback for each Interface.
 */
int Network::Monitor::each_interface( InterfaceIterator& callback ) {
    int result = 0;

    std::map<int, Interface *>::const_iterator iter = interfaces.begin();
    while ( iter != interfaces.end() ) {
        Network::Interface *interface = iter->second;
        if ( interface != NULL )  result += callback( interface );
        iter++;
    }

    return result;
}

/** Iterate and call a callback for each Node.
 */
int Network::Monitor::each_node( NodeIterator& callback ) {
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
Network::Monitor::find_bridge_interface( Interface *interface ) {
    if ( interface->not_physical() ) return NULL;
    if ( interface->not_captured() ) return NULL;

    std::map<int, Interface *>::const_iterator iter = interfaces.begin();
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

/** Return the physical Interface for a Bridge Interface.
 */

Network::Interface *
Network::Monitor::find_physical_interface( Interface *interface ) {
    if ( interface->not_bridge() ) return NULL;

    char path[1024];
    sprintf( path, "/sys/class/net/%s/brif/*", interface->name() );

    glob_t paths;
    memset(&paths, 0, sizeof(paths));

    glob( path, GLOB_NOSORT, NULL, &paths );
    for ( size_t i = 0 ; i < paths.gl_pathc ; i++ ) {
        std::map<int, Interface *>::const_iterator iter = interfaces.begin();
        while ( iter != interfaces.end() ) {
            Network::Interface *interface2 = iter->second;
            if ( ( interface2 != NULL ) && interface2->is_physical() ) {
                if ( strcmp( interface2->name(), basename( paths.gl_pathv[i]) ) == 0 ) {
                    globfree( &paths );
                    return interface2;
                }
            }
            iter++;
        }
    }

    globfree( &paths );

// Note:  This happens when the interface has been temporarily removed from the
// bridge by the tunnel code when the tunnel is set up to use the peer's interface.

    if ( debug > 0 ) log_notice( "Unable to find physical interface for %s", interface->name() );
    return NULL;
}

/**
 */
static bool
send_topology_event( const char *who ) {
    log_notice( "%s sending NetTopologyUpdate event to spine", who );

    char *event_name = const_cast<char*>("SuperNova::NetTopologyUpdate");
    pid_t child = fork();

    if ( child < 0 ) {
        log_err( "failed to send %s event - couldn't fork", event_name );
        return false;
    }

    if ( child > 0 ) {
        int status;
        waitpid( child, &status, 0);
        return true;
    }

    char *argv[] = { const_cast<char*>("genevent"), event_name, 0 };
    char *envp[] = { 0 };
    if ( execve("/usr/lib/spine/bin/genevent", argv, envp) < 0 ) {
        log_err( "failed to send %s event - couldn't execve", event_name );
        _exit( 0 );
    }

    // NOT REACHED
    return true;
}

/**
 */
void Network::Monitor::topology_changed() {

// If no Manager, send the event ourselves.

    if ( _manager != NULL ) {
        _manager->topology_changed();
    } else {
        send_topology_event("Monitor");
    }
}

/** Persist the current interface config.
 */
void Network::Monitor::persist_interface_configuration() {
    log_notice( "persisting the change in interface configuration" );
    FILE *f = fopen( "/etc/udev/rules.d/.tmp", "w" );
    std::map<int, Interface *>::const_iterator iter = interfaces.begin();
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
void Network::Monitor::capture( Interface *interface ) {
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
void Network::Monitor::bring_up( Interface *interface ) {
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
        if ( available == NULL ) {	// EDM ??? wasn't this set above ???
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
void Network::Monitor::run() {

// Disable neighbor and route messages from being reported
// because we do not process them and we can get 100's of them
// every second.  This causes -ENOBUFS and we can miss Link events.
//
//    uint32_t groups = RTMGRP_LINK | RTMGRP_NOTIFY | RTNLGRP_NEIGH |
//                      RTMGRP_IPV4_IFADDR | RTMGRP_IPV4_ROUTE |
//                      RTMGRP_IPV6_IFADDR | RTMGRP_IPV6_ROUTE |
//                      RTMGRP_IPV6_IFINFO | RTMGRP_IPV6_PREFIX ;

    load_cache();

    uint32_t groups = RTMGRP_LINK | RTMGRP_NOTIFY |
                      RTMGRP_IPV4_IFADDR |
                      RTMGRP_IPV6_IFADDR |
                      RTMGRP_IPV6_IFINFO | RTMGRP_IPV6_PREFIX ;

    route_socket = new NetLink::RouteSocket(groups);
    NetLink::RouteSocket &rs = *route_socket;

    probe();
    if ( debug > 0 ) log_notice( "network monitor started" );
    for (;;) {
        rs.receive( this );
        sleep( 1 );
    }
}

void Network::Monitor::probe() {
    if ( route_socket == NULL ) return;
    if ( debug > 0 ) log_notice( "Monitor: sending probe" );
    NetLink::RouteSocket &rs = *route_socket;
    NetLink::GetLink getlink;
    getlink.send( rs );
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
Network::Monitor::Monitor( Tcl_Interp *interp, Network::ListenerInterfaceFactory factory,
                           Network::Manager *manager )
: Thread("netlink.monitor"), interp(interp), route_socket(NULL), factory(factory),
  table_warning_reported(false), table_error_reported(false), _manager(manager) {
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
static int
Monitor_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace Network;
    Monitor *monitor = (Monitor *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(monitor)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Svc_SetResult( interp, "Network::Monitor", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "start") ) {
        // this should have some sort of result so we know it has started correctly
        monitor->start();
        /*
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Svc_SetResult( interp, (char *)result, TCL_STATIC );
        return TCL_ERROR;
        */

            Tcl_ResetResult( interp );
            return TCL_OK;
    }

    /**
     */
    if ( Tcl_StringMatch(command, "stop") ) {
        /*
        const char *result = monitor->stop();
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Svc_SetResult( interp, (char *)result, TCL_STATIC );
        return TCL_ERROR;
        */

            Tcl_ResetResult( interp );
            return TCL_OK;
    }

    /*
     * I want an 'interfaces' sub command ...
     */

    Svc_SetResult( interp, "Unknown command for Monitor object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
Monitor_delete( ClientData data ) {
    using namespace Network;
    Monitor *message = (Monitor *)data;
    delete message;
}

/**
 */
static int
Monitor_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 3 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name factory" );
        return TCL_ERROR;
    }

    using namespace Network;
    ListenerInterfaceFactory factory;
    void *p = (void *)&(factory); // avoid strict aliasing errors in the compiler
    if ( Tcl_GetLongFromObj(interp,objv[2],(long*)p) != TCL_OK ) {
        Svc_SetResult( interp, "invalid listener object", TCL_STATIC );
        return TCL_ERROR;
    }
    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    Monitor *object = new Monitor( interp, factory, NULL );
    Tcl_CreateObjCommand( interp, name, Monitor_obj, (ClientData)object, Monitor_delete );
    Svc_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 * This is a sample command for testing the straight line netlink
 * probe code.
 */
static int
Probe_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    // Yes, this is intentional, it may get removed later
    pid_t pid  __attribute((unused))  = getpid();

    int result;

    // Discover interfaces and add objects
    int fd;
    fd = socket( AF_NETLINK, SOCK_RAW, NETLINK_ROUTE );
    if ( socket < 0 ) {
	log_err( "Probe socket() failed, %s", strerror(errno) );
        exit( 1 );
    }
    // setup sndbuf and rcvbuf

    // bind
    struct sockaddr_nl address;
    memset( &address, 0, sizeof(address) );
    address.nl_family = AF_NETLINK;
    address.nl_groups = 0;
    if ( bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0 ) {
	log_err( "Probe bind() failed, %s", strerror(errno) );
        exit( 1 );
    }

    // send request
    struct {
        struct nlmsghdr nlh;
        struct rtgenmsg g;
    } nlreq;
    memset( &nlreq, 0, sizeof(nlreq) );
    nlreq.nlh.nlmsg_len = sizeof(nlreq);
    nlreq.nlh.nlmsg_type = RTM_GETLINK;
    nlreq.nlh.nlmsg_flags = NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST;
    nlreq.nlh.nlmsg_pid = 0;
    nlreq.nlh.nlmsg_seq = 34;
    nlreq.g.rtgen_family = AF_PACKET;

    result = sendto( fd, (void*)&nlreq, sizeof(nlreq), 0, (struct sockaddr *)&address, sizeof(address) );
    if ( result < 0 ) {
	log_err( "Probe sendto() failed, %s", strerror(errno) );
        exit( 1 );
    }

    // Now I am going to reuse the "address" structure for receiving the responses.
    memset( &address, 0, sizeof(address) );

    char msg_buf[16384];
    struct iovec iov;
    memset( &iov, 0, sizeof(iov) );
    iov.iov_base = msg_buf;

    struct msghdr rsp_msg;
    rsp_msg.msg_name = &address;
    rsp_msg.msg_namelen = sizeof(address);
    rsp_msg.msg_iov = &iov;
    rsp_msg.msg_iovlen = 1;

    // loop around waiting for netlink messages
    // for each response create an interface object
    for (;;) {
        unsigned int status;
        struct nlmsghdr *h;

        iov.iov_len = sizeof(msg_buf);
        status = recvmsg( fd, &rsp_msg, 0 );
        if ( status < 0 ) {
            if ( errno == EINTR ) {
                // normally just continue here
                printf( "interupted recvmsg\n" );
            } else {
	        log_err( "Probe recvmsg() failed, %s", strerror(errno) );
            }
            continue;
        }

        if ( status == 0 ) {
            printf( "finished nlmsgs\n" );
            break;
        }
        printf( "status=%d\n", status );

        h = (struct nlmsghdr *)msg_buf;
        while ( NLMSG_OK(h, status) ) {
            if ( h->nlmsg_type == NLMSG_DONE ) {
                printf( "NLMSG_DONE\n" );
                goto finish;
            }

            if ( h->nlmsg_type == NLMSG_ERROR ) {
                printf( "NLMSG_ERROR\n" );
                goto finish;
            }

            /*
            if ( h->nlmsg_flags & NLM_F_MULTI ) {
                printf( "NLM_F_MULTI\n" );
            } else {
                printf( "not:NLM_F_MULTI\n" );
            }
            */

            printf( "process nlmsg type=%d flags=0x%08x pid=%d seq=%d\n", h->nlmsg_type, h->nlmsg_flags, h->nlmsg_pid, h->nlmsg_seq );
            { // type is in linux/if_arp.h -- flags are in linux/if.h
                NetLink::NewLink *m = new NetLink::NewLink( h );
                Network::Interface *ii = new Network::Interface( interp, m );
                if ( ii == NULL ) { // remove -Wall warning by using this
                }
            }

            h = NLMSG_NEXT( h, status );
        }
        printf( "finished with this message (notOK)\n" );
        if ( rsp_msg.msg_flags & MSG_TRUNC ) {
            printf( "msg truncated" );
            continue;
        }
        if ( status != 0 ) {
            printf( "remnant\n" );
        }
    }

finish:
    close( fd );

    // Discover Bridges

    // Search for existing bridges and create objects for them
    // and for each object create a command
    // the bridge commands should remain in the Network namespace
    //    -- maybe no -- create in whichever namespace you need

    return TCL_OK;
}

/**
 */
Network::Event::Event( const char *name, void *data ) {
    _name = strdup(name);
    _data = data;
}

/**
 */
Network::Event::~Event() {
    if ( _name != NULL ) free(_name);
}

/**
 */
static int
Manager_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace Network;
    Manager *manager = (Manager *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(manager)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Svc_SetResult( interp, "Network::Manager", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "start") ) {
        // this should have some sort of result so we know it has started correctly
        manager->start();
        /*
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Svc_SetResult( interp, (char *)result, TCL_STATIC );
        return TCL_ERROR;
        */

            Tcl_ResetResult( interp );
            return TCL_OK;
    }

    /**
     */
    if ( Tcl_StringMatch(command, "stop") ) {
        /*
        const char *result = manager->stop();
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Svc_SetResult( interp, (char *)result, TCL_STATIC );
        return TCL_ERROR;
        */

            Tcl_ResetResult( interp );
            return TCL_OK;
    }

    Svc_SetResult( interp, "Unknown command for Manager object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
Manager_delete( ClientData data ) {
    using namespace Network;
    Manager *message = (Manager *)data;
    delete message;
}

/**
 */
static int
Manager_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR;
    }

    using namespace Network;
    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    Manager *object = new Manager( interp );
    Tcl_CreateObjCommand( interp, name, Manager_obj, (ClientData)object, Manager_delete );
    Svc_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 * need some way to find these objects...
 * use TCL ..hmmm
 */
bool NetworkMonitor_Module( Tcl_Interp *interp ) {

    Tcl_Command command;

    Tcl_Namespace *ns = Tcl_CreateNamespace(interp, "Network", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    if ( Tcl_LinkVar(interp, "Network::debug", (char *)&debug, TCL_LINK_INT) != TCL_OK ) {
        log_err( "failed to link Network::debug" );
        exit( 1 );
    }

    command = Tcl_CreateObjCommand(interp, "Network::Monitor", Monitor_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Network::Probe", Probe_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Network::Manager", Manager_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    return true;
}

/* vim: set autoindent expandtab sw=4 : */
