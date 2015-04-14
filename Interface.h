
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

/** \file Interface.h
 * \brief Network interface abstraction.
 *
 */

#ifndef _INTERFACE_H_
#define _INTERFACE_H_

#include <stdint.h>
#include <sys/socket.h>
#include <sys/time.h>

#if 0
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <linux/netdevice.h>
#endif

#include <netinet/in.h>
#include <netinet/ip6.h>
#include <unistd.h>
#include <tcl.h>

#include <list>
#include <map>

#if 0
#include "NetLink.h"
#endif

namespace ICMPv6 { class Socket; }

/**
 */
namespace Network {

    class Bridge;
    class Peer;
    class NeighborIterator;

    /**
     * \todo add ethtool requests
     */
    class Interface {
    private:
        Tcl_Interp *interp;
        int _index;
        int _ordinal;
        bool _no_ordinal;	// true if ordinal is invalid (because interface
				// name could not be parsed)

        unsigned int netlink_flags;          // IFF_*
        unsigned int netlink_change;         // IFF_*
        unsigned int last_flags;             // IFF_*
        unsigned int last_processed_flags;   // IFF_*
        unsigned int changed;

        unsigned int type; // ARPHRD_*
        int link;
        char *_name;
        int16_t _speed;
        uint8_t _duplex;
        uint8_t _port;
        bool unknown_carrier;
        bool current_carrier;
        bool previous_carrier;
        // struct net_device_stats stats;
        unsigned char MAC[6];
        struct in6_addr primary_address;
        int outbound;
        ICMPv6::Socket *_icmp_socket;
        void save_sysconfig();
        time_t last_sendto;
        time_t last_no_peer_report;
        time_t last_bounce;
        int bounce_attempts;
        time_t last_bounce_reattempt;
        time_t last_negotiation;

        pthread_mutex_t neighbor_table_lock;
        static const int NEIGHBOR_TABLE_SIZE = 4096;
        Peer *neighbors;
        bool table_warning_reported;
	bool table_error_reported;

        int advertise_errors;

	bool initiated_tunnel;

	bool _removed;

    public:
        Interface( Tcl_Interp * );
        Interface( Tcl_Interp *, char * );
        Interface( Tcl_Interp *, NetLink::NewLink * );
        virtual ~Interface();

        inline operator int() const { return _index; }
        int sendto( void *, size_t, int, const struct sockaddr_in6 * );
        bool advertise();
        int inbound_socket( char *, uint16_t );
        ICMPv6::Socket * icmp_socket();

        bool up()        const { return (last_flags & IFF_UP)        != 0; }
        bool loopback()  const { return (last_flags & IFF_LOOPBACK)  != 0; }
        bool running()   const { return (last_flags & IFF_RUNNING)   != 0; }
        bool multicast() const { return (last_flags & IFF_MULTICAST) != 0; }
        int  index()     const { return _index; }
        int  ordinal()   const { return _ordinal; }
        char *name()     const { return _name; }
        int  speed()     const { return _speed; }
        void eui64( uint8_t * ) const;
        void lladdr( struct in6_addr * );
        unsigned char *mac() { return MAC; }

        char *bus_address() const;
        char *preferred_name() const;
        bool carrier() const;
        bool rename( char * );
        bool negotiate();
        void repair_link_speed();
        void bounce_link();
        void repair_link();
        void configure_addresses();
        void create_sockets();
        void bring_link_down();
        void bring_link_up();
        void linkUp( NetLink::LinkMessage * );
        void linkDown( NetLink::LinkMessage * );
        void update( NetLink::LinkMessage * );
        void get_settings();
        time_t last_sent() const { return last_sendto; }
        void clean_topology() const;

        /**
         * This peer list is updated from the real Peer list from the
         * Monitor class.  This is just a cache of peer references that
         * can be seen on this interface.
         */
        Peer* intern_neighbor( struct in6_addr & );
        bool remove_neighbor( struct in6_addr & );
        Peer* find_neighbor( struct in6_addr & );
        int each_neighbor( NeighborIterator& );

        void accept_ra( bool );
        bool accept_ra();

        bool no_ordinal() const { return _no_ordinal; }

        bool exists() const;
        bool is_primary( struct in6_addr * );
        bool is_physical() const;
        bool is_full_duplex() const;
        bool is_bridge() const;
        bool is_captured() const;
        bool is_private() const;
        bool is_business() const;
        bool is_sync() const;
        bool is_tunnelled() const;
        bool is_named_ethN() const;
        bool is_named_bizN() const;
        bool is_named_netN() const;
        bool is_named_tunN() const;
        bool is_named_vifN() const;

        // this one may be able to be determined easier than this...
        inline bool is_tunnel()   const { return is_named_tunN(); }

        inline bool not_physical()    const { return not is_physical(); }
        inline bool not_full_duplex() const { return not is_full_duplex(); }
        inline bool not_bridge()      const { return not is_bridge(); }
        inline bool not_captured()    const { return not is_captured(); }
        inline bool not_private()     const { return not is_private(); }
	inline bool not_business()    const { return not is_business(); }
        inline bool not_sync()        const { return not is_sync(); }
        inline bool not_tunnelled()   const { return not is_tunnelled(); }
        inline bool not_named_ethN()  const { return not is_named_ethN(); }
	inline bool not_named_bizN()  const { return not is_named_bizN(); }
	inline bool not_named_netN()  const { return not is_named_netN(); }
        inline bool not_named_tunN()  const { return not is_named_tunN(); }
        inline bool not_named_vifN()  const { return not is_named_vifN(); }

        bool is_up() const;
        bool is_running() const;
        bool is_promiscuous() const;
        bool is_dormant() const;
        bool has_link() const;
        bool has_fault_injected() const;
        bool is_quiesced() const;
        inline bool not_up()          const { return not is_up(); }
        inline bool not_running()     const { return not is_running(); }
        inline bool not_promiscuous() const { return not is_promiscuous(); }
        inline bool not_dormant()     const { return not is_dormant(); }
        inline bool not_quiesced()    const { return not is_quiesced(); }

        bool is_listening_to( const char *, u_int16_t ) const;
        inline bool not_listening_to( const char *protocol, u_int16_t port ) const {
            return not is_listening_to(protocol, port);
        }

        bool up_changed() const;
        bool running_changed() const;
        bool promiscuity_changed() const;
        bool link_changed() const;
        bool dormancy_changed() const;
        bool bounce_expired();

        int rx_offload() const;
        int tx_offload() const;
        int sg_offload() const;
        int tso_offload() const;
        int ufo_offload() const;
        int gso_offload() const;
        void rx_offload( int ) const;
        void tx_offload( int ) const;
        void sg_offload( int ) const;
        void tso_offload( int ) const;
        void ufo_offload( int ) const;
        void gso_offload( int ) const;

        Bridge *bridge() const;

/* "initiated_tunnel" keeps track of whether this node initiated the tunnel
   or whether the other node did so.  Should be in the "Tunnel" class. */

        inline bool has_initiated_tunnel() const { return initiated_tunnel; }
        inline void set_initiated_tunnel() { initiated_tunnel = true; }
        inline void clear_initiated_tunnel() { initiated_tunnel = false; }

        void remove() { _removed = true; }
	bool removed() { return _removed; }

        static bool Initialize( Tcl_Interp * );
    };

}

#endif

/* vim: set autoindent expandtab sw=4 : */
