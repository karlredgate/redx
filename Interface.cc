
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

/** \file Interface.cc
 * \brief 
 *
 */

#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
# define MAP_ANONYMOUS MAP_ANON
#endif

// do not like this ... want the generic include
/* #include <linux/if.h> */
#include <net/if_arp.h>

#if 0
#include <linux/sockios.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/ethtool.h>
#endif

#include <netinet/in.h>
#include <netinet/ip6.h>
#include <arpa/inet.h>
// #include <linux/ipv6.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <glob.h>

#include "logger.h"
#include "util.h"
#if 0
NEED TO FACTOR OUT NELINK
#include "NetLink.h"
#include "Network.h"
#endif

#include "PlatformCompat.h"

#include "ICMPv6.h"
#include "PlatformInterface.h"
#include "Interface.h"
#include "Neighbor.h"

#include "tcl_util.h"

namespace {
    int debug = 0;
    int link_bounce_interval = 1200;
    int link_bounce_attempts = 2;
    int link_bounce_reattempt = 1200;	// 1 hour total
}

namespace {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
    void shell( const char *command ) {
        system( command );
    }
#pragma GCC diagnostic pop
}

/**
 */
static bool
mac_is_zero( unsigned char *address ) {
    if ( address[0] != 0 ) return false;
    if ( address[1] != 0 ) return false;
    if ( address[2] != 0 ) return false;
    if ( address[3] != 0 ) return false;
    if ( address[4] != 0 ) return false;
    if ( address[5] != 0 ) return false;
    return true;
}

/**
 * This constructor is used by the TCL interpreter to create a command
 * that can be used to control the interface.  It is used specifically
 * in the hotplug_vif house script.
 */
Network::Interface::Interface( Tcl_Interp *interp, int index )
: interp(interp), _index(index), 
  last_sendto(0),
  last_no_peer_report(0),
  table_warning_reported(false),
  table_error_reported(false),
  advertise_errors(0),
  _removed(false)
{
    _name = NULL;
    platform_init();
    get_settings();
    pthread_mutex_init( &neighbor_table_lock, NULL );
    neighbors = NULL;
}

/**
 * This constructor is used by the TCL interpreter to create a command
 * that can be used to control the interface.  It is used specifically
 * in the hotplug_vif house script.
 */
Network::Interface::Interface( Tcl_Interp *interp, char *initname )
: interp(interp), _index(-1), 
  type(0), 
  last_sendto(0),
  last_no_peer_report(0),
  table_warning_reported(false),
  table_error_reported(false),
  advertise_errors(0),
  _removed(false)
{
    _name = strdup(initname);
    platform_init();
    get_settings();
    pthread_mutex_init( &neighbor_table_lock, NULL );
    neighbors = NULL;
}

/**
 */
// Network::Interface::Interface( Tcl_Interp *interp, NetLink::NewLink *message )
Network::Interface::Interface( Tcl_Interp *interp )
: interp(interp), 
  netlink_flags(0),        /* current state of flags reported by netlink */
  netlink_change(0),       /* netlink thinks flags have changed */
  last_flags(0),           /* Last set of flags EKG saw */
  last_processed_flags(0), /* Last set of flags EKG processed */
  changed(0),              /* Ekg has detected flags Have Changed */
  type(0), _speed(0), _duplex(0), _port(0),
  unknown_carrier(true),
  previous_carrier(false),
  outbound(0), _icmp_socket(0),
  last_sendto(0), last_no_peer_report(0), last_bounce(0),
  bounce_attempts(0), last_bounce_reattempt(0),
  last_negotiation(0), table_warning_reported(false),
  table_error_reported(false),
  advertise_errors(0), _removed(false) {

#if 0
// All of these need to come from a kernel generic arg

    _index = message->index();
    _name = strdup( message->name() );

    update( message ) ;
#endif

    _no_ordinal = true;
    int slot = -1;
    int port = -1;
    if ( sscanf(_name, "sync_pci%dp%d", &slot, &port) == 2 ) {
        if ( ( slot >= 0 ) && ( slot <= 15 ) &&
             ( port >= 0 ) && ( port <= 3 ) ) {
            _ordinal = 0x40 | ( (slot & 0xf) << 2 ) | port;
            _no_ordinal = false;
        }
    } else {
        log_notice( "could not parse interface name '%s'", _name );
    }

    if ( !_no_ordinal ) {
        log_notice( "Interface %s ordinal is %d", _name, _ordinal );
    }

#if 0
// need new kernel generic interface
    unsigned char *address = message->MAC();
    if ( mac_is_zero(address) ) {
#endif
    if ( get_mac_address(_name, MAC) ) {
        lladdr( &primary_address );
    }

    bool is_phys = is_physical();

    if ( debug > 5 ) {
        printf( "=> %d: %s%s/%s%s [%02x:%02x:%02x:%02x:%02x:%02x]  ",
            _index, (is_bridge()?"Bridge:":""), (is_phys?"P":"V"), _name, (is_captured()?":captured":""),
            MAC[0], MAC[1], MAC[2], MAC[3], MAC[4], MAC[5] );
        if ( up()        ) printf( " UP" );
        if ( running()   ) printf( " RUNNING" );
        if ( loopback()  ) printf( " LOOPBACK" );
        if ( multicast() ) printf( " MULTICAST" );
        printf( "\n" );
    }

    char buffer[80];
    const char *llname = inet_ntop( AF_INET6, &primary_address, buffer, sizeof(buffer) );

    pthread_mutex_init( &neighbor_table_lock, NULL );
    neighbors = NULL;
}

/**
 */
Network::Interface::~Interface() {
}

/**
 */
void
Network::Interface::linkUp( Kernel::NetworkLinkUpEvent *message ) {
    char cmd[128];
    int status = 1;

    last_processed_flags = netlink_flags;
    previous_carrier = current_carrier;
    unknown_carrier = false;
}

/**
 */
void
Network::Interface::linkDown( Kernel::NetworkLinkDownEvent *message ) {
    char cmd[128];
    int status = 1;

    last_processed_flags = netlink_flags;
    previous_carrier = current_carrier;
    unknown_carrier = false;
}

/**
 * Update State Variables that track message flags
 */
#if 0
 THIS NEEDS TO BE REPLACED WITH A KENEL GENERIC INTERFACE
 probably more than one method
void Network::Interface::update( NetLink::LinkMessage *message ) {
    unsigned int ignore_mask = ~(IFF_NOARP);

    const char *family = "UNKNOWN";
    switch ( message->family() ) {
    case AF_BRIDGE: family = "BRIDGE" ; break;
    case AF_INET6 : family = "INET6" ; break;
    case AF_UNSPEC: family = "UNSPEC"; break;
    }

    netlink_flags = message->flags();
    netlink_change = message->change();

    changed = (last_processed_flags ^ netlink_flags) | netlink_change;

    /* Don't trust IFF_LOWER_UP state from kernel event, read it from /sys/fs */
    current_carrier = carrier();

    /* If we don't know the previous carrier state sent to spine then ensure we re-read it now */
    if ( unknown_carrier ) {
      previous_carrier = current_carrier ? false : true;
    }

    if ( previous_carrier != current_carrier ) {
        changed |= IFF_LOWER_UP;
        netlink_flags = current_carrier ? (netlink_flags | IFF_LOWER_UP) : (netlink_flags & ~IFF_LOWER_UP);
        previous_carrier = current_carrier;
    }

    if ( (changed & ignore_mask) != 0 ) {
      log_notice( "%s(%d) <%s> Flags: last/genevent=0x%08x/0x%08x => new=0x%08x (changed=0x%08x)",
	      message->name(), message->index(), family,
	      last_flags, last_processed_flags, netlink_flags, changed );
    }
    
    last_flags = netlink_flags;

}
#endif

/**
 * Could generate a NullResponseHandler object??
 *
 */
void
Network::Interface::configure_addresses() {
    int error = set_interface_address( index(), &primary_address );

    /**
     * Currently we just ignore all errors from the address config request.
     * There may need to be more specific behavior here.
     *
     * \todo error behavior for failed addr config requests?
     */
    switch ( error ) {
    case       0:
        if ( debug > 0 ) {
            log_notice( "configured address for '%s'", name() );
        }
        break;
    case -EEXIST:
        if ( debug > 0 ) {
            log_notice( "'%s' already configured with address", name() );
        }
        break;
    case -EINVAL: log_err(    "invalid arguments to addr config request"    ); break;
    case -ENETDOWN:
        log_err( "network is down" );
        // Now what??
        break;
    default:
        log_err( "unknown error (%d) from addr config request (%s)", error, name() );
        break;
    }
}

/**
 */
void
Network::Interface::create_sockets() {
    // create the sending multicast socket
    outbound = ::socket( AF_INET6, SOCK_DGRAM, 0 );
    if ( outbound == -1 ) {
        // how to handle this error
        log_err( "Network::Interface: failed to create socket" );
        _exit( 1 );
    }

    /** turn off loopback
     * When the loopback flag is true, all packets we send are returned to us
     * as well as going on the wire.  This flag is true by default.  We do not
     * want this behavior so we turn it off.
     */
    unsigned int flag = 0;
    int result = setsockopt(outbound, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &flag, sizeof(flag));
    if ( result < 0 ) {
        log_warn( "failed to turn off loopback for '%s'", name() );
    }

    // do not bind -- setsocketopt
    if ( ::setsockopt(outbound, IPPROTO_IPV6, IPV6_MULTICAST_IF, &_index, sizeof(_index)) < 0 ) {
        printf( "cannot set if\n" );
        _exit( 1 );
    }

    if ( ::fcntl(outbound, F_SETFD, FD_CLOEXEC) < 0 ) {
        log_err( "could not set close on exec for outbound multicast socket '%s'", name() );
    }

    if ( debug > 0 )  printf( "created outbound socket for %s\n", _name );

    _icmp_socket = new ICMPv6::Socket();
#if 0
    if ( _icmp_socket->bind(&primary_address, _index) == false ) {
        log_notice( "initial bind of ICMPv6 socket failed for '%s'", _name );
    }
#endif
    log_notice( "created outbound socket %d for '%s'", outbound, name() );
}

/** return a bound ICMP socket
 *
 * If we cannot bind the address, and this is the private link, we attempt to
 * bounce the link.  This may want to check the error code of the bind failure.
 *
 * This will hold up pulse and ??
 */
ICMPv6::Socket *
Network::Interface::icmp_socket() {
    if ( _icmp_socket == 0 ) return 0;
    if ( _icmp_socket->bound() ) return _icmp_socket;

    struct timespec delay = { 5, 0 };

    if ( _removed ) return NULL;

    while ( _icmp_socket->bind(&primary_address, _index) == false ) {
        log_warn( "%s(%d): could not bind ICMP socket", name(), index() );

        // If this were a biz/bridge -- we could reset the phys interface
        // if we reset the bridge interface -- we may lose the cluster IP?
#if 0
        if ( is_private() ) {
            log_warn( "%s(%d): reset private interface", name(), index() );
            bounce_link();
        }
#endif

	if ( _removed ) return NULL;

        nanosleep( &delay, NULL );
        delay.tv_sec += 5;
    }

    return _icmp_socket;
}

/**
 * If we have bounced the link more then "link_bounce_attempts"
 * times, then we will wait for "link_bounce_reattempt" seconds
 * before repeatedly trying to bounce the link again to recover it.
 *
 * We wait at least "link_bounce_interval" seconds between attempts.
 */
bool
Network::Interface::bounce_expired() {
    if ( bounce_attempts >= link_bounce_attempts ) {
        long delta = ::time(0) - last_bounce_reattempt;
        if ( delta >= link_bounce_reattempt ) {
            bounce_attempts = 0;
            return true;
        } else {
            return false;
        }
    } else {
        long delta = ::time(0) - last_bounce;
        return (delta >= link_bounce_interval);
    }
}

/**
 */
void
Network::Interface::repair_link_speed() {
// Commented out due to IPS discussion on 5/29/08
#if BUG_5277
    if ( not_full_duplex() or speed() < 1000 ) {
        log_notice( "ERROR : link for %s(%d) came up at %d Mb/s %s duplex (bug 5277)",
                            interface->name(), interface->index(), interface->speed(),
                            (interface->is_full_duplex() ? "full" : "half") );
        interface->negotiate();
    }
#endif
}

/**
 * These need to change to NewLink commands
 */
void
Network::Interface::bring_link_down() {
    if ( debug > 0 )  log_notice( "bring link down for '%s'", name() );
    char buffer[128];
    sprintf( buffer, "/sbin/ip link set %s down", name() );
    shell( buffer );
}

/**
 */
void
Network::Interface::bring_link_up() {
    if ( debug > 0 )  log_notice( "bring link up for '%s'", name() );
    char buffer[128];
    sprintf( buffer, "/sbin/ip link set %s up", name() );
    shell( buffer );
}

/**
 */
void
Network::Interface::bounce_link() {
    if ( is_quiesced() ) {
        log_notice( "%s(%d): is quiesced, not bouncing", name(), index() );
        return;
    }
    bring_link_down();
    bring_link_up();
    ::time( &last_bounce );
    if ( ++bounce_attempts >= link_bounce_attempts ) {
        ::time ( &last_bounce_reattempt );
    }
}

/**
 */
void
Network::Interface::repair_link() {
    if ( debug > 0 ) log_notice( "%s(%d): attempt to repair link", name(), index() );
    bounce_link();
}

/**
 * We have the DEVICE and the HWADDR, but the IPADDR, etal need the zeroconf
 * stuff working.
 *
 * cat /etc/sysconfig/network-scripts/ifcfg-priv0
 * DEVICE=priv0
 * HWADDR=00:13:72:4b:05:41
 * ONBOOT=no
 * IPADDR=169.254.41.43
 * NETMASK=255.255.0.0
 * BROADCAST=169.254.255.255
 *
 * The biz networks use:
 *
 * [root@node1.smokin ~]# cat /etc/sysconfig/network-scripts/ifcfg-ibiz0 
 * # Intel Corporation 82541GI Gigabit Ethernet Controller
 * DEVICE=ibiz0
 * HWADDR=00:13:72:4B:05:42
 * NOZEROCONF=1
 * BOOTPROTO=dhcp
 * ONBOOT=yes
 * BRIDGE=biz0
 * LINKDELAY=5
 */
void
Network::Interface::save_sysconfig() {
#if SAVE_SYSCONFIG
    // open file
    char filename[80];
    sprintf( filename, "/etc/sysconfig/network-scripts/ifcfg-%s", _name );
    FILE *sysconfig = fopen( filename, "w" );
    fprintf( sysconfig, "DEVICE=%s\n", _name );
    fprintf( sysconfig, "HWADDR=%02x:%02x:%02x:%02x:%02x:%02x\n", 
                        MAC[0], MAC[1], MAC[2], MAC[3], MAC[4], MAC[5] );
    fclose( sysconfig );
#endif
}

/** Send a packet out of this interface if it has an open socket.
 *
 * Interfaces will only have an open socket if they are private or are
 * a bridge interface to a biz network.
 */
int
Network::Interface::sendto( void *message, size_t length, int flags, const struct sockaddr_in6 *address) {
    if ( debug > 5 ) {
        log_notice( "%s(%d) sendto message length %d", _name, _index, length );
    }
    if ( outbound == 0 ) {
        if ( debug > 1 ) {
            log_notice( "%s(%d) sendto has no outbound socket", _name, _index );
        }
        return 0;
    }
    int result = ::sendto( outbound, message, length, flags, (struct sockaddr *)address, sizeof(*address) );
    if ( result < 0 ) {
        if ( debug > 1 ) {
            const char *error = strerror(errno);
            log_notice( "%s(%d) sendto failed: %s", _name, _index, error );
        }
    }
    ::time( &last_sendto );
    return result;
}

/** Send an ICMPv6 neighbor advertisement to peers on this interface.
 *
 * Currently, this means send a neighbor advertisement for this interfaces
 * IPv6 link local address to each known SuperNova host on this subnet.
 *
 * Normally I would like this to be a single multicast packet; however,
 * the NUD code in the kernel doesn't seem to be updating the neighbor
 * table with these packets, so we are sending unicast packets to each
 * known peer on this interface.  For the normal case this is not an issue
 * since there will be only one, but in our test area there are many
 * clusters on each biz network.  To keep traffic to a tolerable level we
 * do not send these frequently.
 *
 * \todo prune the biz NA messages to cluster members
 * It is possible for us to prune the list of peers to advertise to the list
 * that are in the current cluster.  We could do this by tracking the
 * specific peers that we see on the private link.  We could also track
 * the cluster UUID of the current node -- but that logic is harder since
 * we want to discover nodes that have not yet joined this cluster.
 *
 * It is possible that the biz network pings could be used as part of the
 * business connectivity logic.
 *
 * NOTE:
 * We cannot send this as an all hosts multicast, because it does not
 * update the neighbor table correctly.  We must send it to each peer
 * directly.  More packets (grumble), but more directed, since most
 * of the time there will be only one.
 */
bool
Network::Interface::advertise() {
    if ( _icmp_socket == 0 ) return false;
#if 0
    if ( not_private() ) {
        if ( debug > 1 ) {
            log_notice( "%s(%d) not advertising non-private interface", _name, _index );
        }
        return true; // so that caller doesn't report an error
    }
#endif
    if ( _icmp_socket->not_bound() ) {
        if ( debug > 1 ) {
            log_notice( "advertise: binding ICMP socket for interface '%s'", _name );
        }
        if ( _icmp_socket->bind(&primary_address, _index) == false ) {
            if ( debug > 0 ) log_notice( "advertise: ICMP bind for '%s' failed", _name );
            return false;
        }
    }
    // printf( "advertise on '%s'\n", _name );
    ICMPv6::NeighborAdvertisement na( &primary_address, MAC );
    int peers_sent = 0;
    struct in6_addr recipient;

    pthread_mutex_lock( &neighbor_table_lock );
    if ( neighbors != NULL) {
        for ( int i = 0 ; i < NEIGHBOR_TABLE_SIZE ; ++i ) {
            Network::Peer& peer = neighbors[i];

            if ( peer.is_invalid() ) continue;
            if ( peer.node() == NULL ) continue;
            if ( peer.node()->not_partner() ) continue;

            peer.copy_address( &recipient );
            if ( na.send( *_icmp_socket, &recipient ) == false ) {
                if ( (debug > 0) or (advertise_errors < 1) ) {
                    log_warn( "failed to send neighbor advertisement out '%s'", _name );
                }
                ++advertise_errors;
            } else {
                advertise_errors = 0;
            }
            peers_sent++;
        }
    }
    pthread_mutex_unlock( &neighbor_table_lock );

    if ( peers_sent == 0 ) {
        long delta = ::time(0) - last_no_peer_report;
        if ( delta > 120 ) {
            log_warn( "no peers found on %s", _name );
#if 0
            if ( is_private() and not_quiesced() and (last_no_peer_report != 0) ) {
	      if (has_fault_injected()) {
		log_warn( "WARNING: sentinel file, detected skip attempt to recover %s", _name );
	      } else {
		  // log_warn( "WARNING: reset private interface" );
		  // bring_link_down();
		  // bring_link_up();
	      }
            }
#endif
            ::time( &last_no_peer_report );
        }
    }

    return true;
}

/**
 */
int
Network::Interface::inbound_socket( char *address, uint16_t port ) {
    struct sockaddr_in6 binding;
    int inbound;

    inbound = ::socket( AF_INET6, SOCK_DGRAM, 0 );
    if ( inbound < 0 ) {
        char *err, e[128];
        char *result = strerror_r( errno, e, sizeof(e) );
        log_err( "could not create diastole socket: %s", e );
        _exit( 1 );
    }

    /** bind to the port from which we want packets
     * If you do not bind the socket to a port (and address) but you still
     * join a multicast group, you will receive all packets for all ports
     * for that multicast group.
     */
    binding.sin6_family = AF_INET6;
    binding.sin6_port = htons(port);
    binding.sin6_scope_id = _index;
    inet_pton(AF_INET6, address, &(binding.sin6_addr) );
    int result = bind( inbound, (struct sockaddr *)&binding, sizeof(binding) );
    if ( result < 0 ) {
        perror( "failed to bind diastole" );
        _exit( 1 );
    }

    // add membership
    struct ipv6_mreq group;
    inet_pton(AF_INET6, address, &(group.ipv6mr_multiaddr));

    /**
     * When using the standard includes the structure name is the same as
     * the documentation (group.ipv6mr_ifindex = _index;), but if you use
     * the Linux specific includes the structure member has a Linux specific
     * name (group.ipv6mr_ifindex = _index;)
     */
    group.ipv6mr_interface = _index;
    result = setsockopt(inbound, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &group, sizeof(group));
    if ( result < 0 ) {
        log_err( "could not join multicast group for interface '%s'", name() );
        // exit( 0 );
    }


    /** turn off loopback
     * When the loopback flag is true, all packets we send are returned to us
     * as well as going on the wire.  This flag is true by default.  We do not
     * want this behavior so we turn it off.
     */
    unsigned int flag = 0;
    result = setsockopt(inbound, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &flag, sizeof(flag));
    if ( result < 0 ) {
        printf( "failed to turn off loopback\n" );
    }

    if ( fcntl(inbound, F_SETFD, FD_CLOEXEC) < 0 ) {
        log_err( "could not set close on exec for inbound multicast socket '%s'", name() );
    }

    struct timeval tv;
    tv.tv_sec = 60;
    tv.tv_usec = 0;
    result = setsockopt(inbound, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if ( result < 0 ) {
        log_err( "could not set receive timeout for inbound multicast socket '%s'", name() );
    }

    if ( debug > 1 ) log_notice( "inbound socket (%d) created for '%s'", inbound, _name );
    return inbound;
}

/**
 * PATH_MAX should be 4096 - from linux/limits.h
 */
char *
Network::Interface::bus_address() const {
    char path[PATH_MAX];
    char link[PATH_MAX];

    sprintf( link, "/sys/class/net/%s/device", _name );
    char *real = realpath( link, path );
    return strdup(real);
}

/**
 */
bool
Network::Interface::carrier() const {
    int value = 0;
    char path[256];

    snprintf( path, sizeof(path), "/sys/class/net/%s/carrier", name() );
    FILE *f = fopen(path, "r");
    if ( f == NULL ) {
        log_err( "%s(%d) could not determine carrier", name(), index() );
        return false;
    }
    int count = fscanf( f, "%d\n", &value  );
    fclose( f );

    if ( count != 1 ) return 0;
    return (value != 0);
}

/** Determine the preferred name for this interface on the current platform.
 *
 * Look up the bus address in the platform config and map it to an
 * interface name.
 */
char *
Network::Interface::preferred_name() const {
    char *bus = bus_address();
    if ( debug > 0 ) {
        printf( "BUS ADDRESS for interface '%s' is '%s'\n", _name, bus );
    }
    Tcl_Obj *obj = Tcl_GetVar2Ex( interp, "interface::name", bus, 0 );
    if ( obj == NULL ) return NULL;
    char *new_name = Tcl_GetStringFromObj( obj, NULL );
    if ( debug > 0 ) {
        printf( "PREFERRED NAME FOR '%s' at '%s' is '%s'\n", _name, bus, new_name );
    }
    return strdup( new_name );
}

/** Add peer to neighbor list.
 *
 * Allocate a neighbor table if this interface does not have one.
 * Either find the neighbor that has this address or add one
 * with this address if it is not already present.
 * If the peer already exists in the neighbor table, then do
 * not add it a second time.
 */
Network::Peer*
Network::Interface::intern_neighbor( struct in6_addr& address ) {
    int in_use_count = 0;
    Network::Peer *result = NULL;
    Network::Peer *available = NULL;
    bool new_entry = false;

    pthread_mutex_lock( &neighbor_table_lock );
    if ( neighbors == NULL ) {
        size_t size = sizeof(Peer) * NEIGHBOR_TABLE_SIZE;
        neighbors = (Peer *)mmap( 0, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0 );
        for ( int i = 0 ; i < NEIGHBOR_TABLE_SIZE ; ++i ) {
            neighbors[i].invalidate();
        }
    }
    int i = 0;
    for ( ; i < NEIGHBOR_TABLE_SIZE ; ++i ) {
        Network::Peer& peer = neighbors[i];
        if ( peer.is_invalid() ) {
            available = &peer;
            continue;
        }
        in_use_count += 1;
        if ( peer != address ) continue;
        result = &peer;
        break;
    }
    if ( in_use_count > 256 ) {
        if ( table_warning_reported == false ) {
            log_warn( "WARNING: %s(%d) neighbor table exceeds 256 entries", name(), index() );
            table_warning_reported = true;
        }
    }
    if ( result == NULL ) {
        if ( available == NULL ) {	// EDM ??? wasn't this set above ???
            for ( ; i < NEIGHBOR_TABLE_SIZE ; ++i ) {
                Network::Peer& peer = neighbors[i];
                if ( peer.is_valid() ) continue;
                available = &peer;
                break;
            }
        }

        if ( available != NULL ) {
            available->set_interface_name( NULL );
            available->address( &address );
            result = available;
            new_entry = true;
        } else {
	    if ( table_error_reported == false ) {
                log_err( "ERROR: %s(%d) neighbor table is full", name(), index() );
                table_error_reported = true;
            }
        }
    }
    pthread_mutex_unlock( &neighbor_table_lock );

#if 0
    if ( ( result != NULL ) && new_entry && is_private() ) {
        result->make_partner();
        char buffer[80];
        const char *address_string = inet_ntop(AF_INET6, &address, buffer, sizeof buffer);
        log_notice( "%s(%d) neighbor %s is partner", name(), index(), address_string );
    }
#endif

    return result;
}

/**
 * Remove all Peer objects from this interface's neighbor list
 * whose address matches the argument.
 */
bool
Network::Interface::remove_neighbor( struct in6_addr& address ) {
    pthread_mutex_lock( &neighbor_table_lock );
    if ( neighbors != NULL ) {
        for ( int i = 0 ; i < NEIGHBOR_TABLE_SIZE ; ++i ) {
            Network::Peer& peer = neighbors[i];
            if ( peer.is_invalid() ) continue;
            if ( peer != address ) continue;
            peer.invalidate();
        }
    }
    pthread_mutex_unlock( &neighbor_table_lock );

    return true;
}

/**
 * Locate the Peer object, that matches the address, from the neighbor
 * list and return a pointer to it.  If the Peer is not in the neighbor
 * list, then return NULL.
 */
Network::Peer*
Network::Interface::find_neighbor( struct in6_addr& address ) {
    Network::Peer *result = NULL;

    pthread_mutex_lock( &neighbor_table_lock );
    if ( neighbors != NULL ) {
        for ( int i = 0 ; i < NEIGHBOR_TABLE_SIZE ; ++i ) {
            Network::Peer& peer = neighbors[i];
            if ( peer.is_invalid() ) continue;
            if ( peer != address ) continue;
            result = &peer;
            break;
        }
    }
    pthread_mutex_unlock( &neighbor_table_lock );

    return result;
}

/** Iterate and call a callback for each neighbor.
 */
int
Network::Interface::each_neighbor( NeighborIterator& callback ) {
    int result = 0;

    pthread_mutex_lock( &neighbor_table_lock );
    if ( neighbors != NULL ) {
        for ( int i = 0 ; i < NEIGHBOR_TABLE_SIZE ; ++i ) {
            Network::Peer& peer = neighbors[i];
            if ( peer.is_invalid() ) continue;
            result += callback( peer );
        }
    }
    pthread_mutex_unlock( &neighbor_table_lock );

    return result;
}

/**
 */
bool
Network::Interface::accept_ra() {
    int value;
    char path[256];
    snprintf( path, sizeof(path), "/proc/sys/net/ipv6/conf/%s/accept_ra", name() );
    FILE *f = fopen(path, "r");
    if ( f == NULL ) {
        log_err( "%s(%d) could not determine accept_ra", name(), index() );
        return false;
    }
    int count = fscanf( f, "%d\n", &value  );
    fclose( f );
    if ( count != 1 ) return 0;
    return (value != 0);
}

/**
 */
void
Network::Interface::accept_ra( bool value ) {
    char path[256];
    snprintf( path, sizeof(path), "/proc/sys/net/ipv6/conf/%s/accept_ra", name() );
    FILE *f = fopen(path, "w");
    if ( f == NULL ) {
        log_err( "%s(%d) could not disable accept_ra", name(), index() );
        return;
    }
    fprintf( f, "%d\n", value ? 1 : 0 );
    fclose( f );
}

/**
 */
bool
Network::Interface::exists() const {
    char path[1024];
    struct stat stat;

    sprintf( path, "/sys/class/net/%s", _name );
    if ( lstat(path, &stat) == 0 ) {
        return true;
    }
    return false;
}

/**
 */
bool
Network::Interface::is_primary( struct in6_addr *address ) {
    if ( address == NULL ) return false;
    if ( address->s6_addr[ 0] != primary_address.s6_addr[ 0] ) return false;
    if ( address->s6_addr[ 1] != primary_address.s6_addr[ 1] ) return false;
    if ( address->s6_addr[ 2] != primary_address.s6_addr[ 2] ) return false;
    if ( address->s6_addr[ 3] != primary_address.s6_addr[ 3] ) return false;
    if ( address->s6_addr[ 4] != primary_address.s6_addr[ 4] ) return false;
    if ( address->s6_addr[ 5] != primary_address.s6_addr[ 5] ) return false;
    if ( address->s6_addr[ 6] != primary_address.s6_addr[ 6] ) return false;
    if ( address->s6_addr[ 7] != primary_address.s6_addr[ 7] ) return false;
    if ( address->s6_addr[ 8] != primary_address.s6_addr[ 8] ) return false;
    if ( address->s6_addr[ 9] != primary_address.s6_addr[ 9] ) return false;
    if ( address->s6_addr[10] != primary_address.s6_addr[10] ) return false;
    if ( address->s6_addr[11] != primary_address.s6_addr[11] ) return false;
    if ( address->s6_addr[12] != primary_address.s6_addr[12] ) return false;
    if ( address->s6_addr[13] != primary_address.s6_addr[13] ) return false;
    if ( address->s6_addr[14] != primary_address.s6_addr[14] ) return false;
    if ( address->s6_addr[15] != primary_address.s6_addr[15] ) return false;
#if 0
    // These are defined on linux - but not on osx
    // osx /usr/include/cups/http.h has workaround code for osx
    // but it wouldn't work for windows
    if ( address->s6_addr32[0] != primary_address.s6_addr32[0] ) return false;
    if ( address->s6_addr32[1] != primary_address.s6_addr32[1] ) return false;
    if ( address->s6_addr32[2] != primary_address.s6_addr32[2] ) return false;
    if ( address->s6_addr32[3] != primary_address.s6_addr32[3] ) return false;
#endif
    return true;
}

/**
 */
bool
Network::Interface::is_quiesced() const {
    char sentinel[128];
    sprintf( sentinel, "/tmp/%s.quiesce", name() );
    return (access(sentinel, F_OK) == 0);
}

/**
 */
bool
Network::Interface::has_fault_injected() const {
    char sentinel[128];
    sprintf( sentinel, "/var/run/interface/%s.fault", name() );
    return (access(sentinel, F_OK) == 0);
}

/**
 */
bool
Network::Interface::is_named_ethN() const {
    if ( _name[0] != 'e' ) return false;
    if ( _name[1] != 't' ) return false;
    if ( _name[2] != 'h' ) return false;
    return true;
}

/**
 */
bool
Network::Interface::is_named_vifN() const {
    if ( _name[0] != 'v' ) return false;
    if ( _name[1] != 'i' ) return false;
    if ( _name[2] != 'f' ) return false;
    return true;
}

/**
 */
bool
Network::Interface::is_named_netN() const {
    if ( _name[0] != 'n' ) return false;
    if ( _name[1] != 'e' ) return false;
    if ( _name[2] != 't' ) return false;
    return true;
}

/**
 */
bool
Network::Interface::is_named_tunN() const {
    if ( _name[0] != 't' ) return false;
    if ( _name[1] != 'u' ) return false;
    if ( _name[2] != 'n' ) return false;
    return true;
}

int Network::Interface::index()           const { return _index; }
int Network::Interface::ordinal()         const { return _ordinal; }
int Network::Interface::speed()           const { return _speed; }

char * Network::Interface::name()         const { return _name; }

/**
 */
void
Network::Interface::eui64( uint8_t *buffer ) const {
    buffer[ 0] = MAC[0] | 0x02;
    buffer[ 1] = MAC[1];
    buffer[ 2] = MAC[2];
    buffer[ 3] = 0xFF;
    buffer[ 4] = 0xFE;
    buffer[ 5] = MAC[3];
    buffer[ 6] = MAC[4];
    buffer[ 7] = MAC[5];
}

/**
 */
void
Network::Interface::lladdr( struct in6_addr *buffer ) {
    buffer->s6_addr[0] = 0xfe;
    buffer->s6_addr[1] = 0x80;
    buffer->s6_addr[2] = 0x00;
    buffer->s6_addr[3] = 0x00;
    buffer->s6_addr[4] = 0x00;
    buffer->s6_addr[5] = 0x00;
    buffer->s6_addr[6] = 0x00;
    buffer->s6_addr[7] = 0x00;
    eui64( buffer->s6_addr + 8 );
}

/**
 */
Network::Bridge *
Network::Interface::bridge() const {
    return NULL;
}

/* vim: set autoindent expandtab sw=4 : */
