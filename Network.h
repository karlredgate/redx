
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

/** \file Network.h
 * \brief Network interface abstraction.
 *
 */

#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <stdint.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <linux/netdevice.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <unistd.h>
#include <tcl.h>

#include <list>
#include <map>

#include "UUID.h"
#include "Thread.h"
#include "NetLink.h"

namespace ICMPv6 { class Socket; }

/**
 */
namespace Network {

    class Node {
    private:
        UUID _uuid;
        uint8_t _ordinal;
        bool partner;
        bool valid;
    public:
        Node( UUID *node_uuid ) : partner(false) {
            _uuid = *node_uuid;
        }
        void set( UUID *node_uuid ) {
            _uuid = *node_uuid;
        }

        void uuid( UUID& );
        UUID& uuid() { return _uuid; }
        void ordinal( uint8_t );
        uint8_t ordinal() const { return _ordinal; }

        void invalidate() { valid = false; partner = false; }
        void make_partner() { partner = true; }
        void clear_partner() { partner = false; }

        inline bool is_valid() const { return valid; }
        inline bool is_invalid() const { return valid == false; }
        inline bool is_partner()  const { return partner; }
        inline bool not_partner() const { return partner == false; }

        bool operator == ( UUID& );
        bool operator != ( UUID& );
    };

    class Interface;

    class Monitor;

    /**
     * The ordinal for a peer is the interface ordinal for that peer.
     */
    class Peer {
    private:
        Node *_node;
        struct in6_addr lladdr;
        struct timeval neighbor_updated;
        struct timeval neighbor_advertised;
        uint8_t _ordinal;
        char *_name;

        bool valid;
        bool partner;
        bool _is_private;
        bool _spine_notified;
    public:
        Peer() {}
        Peer( Node *, struct in6_addr * );
        ~Peer();

        uint8_t ordinal() const { return _ordinal; }
        char *name()     const { return _name; }
        void invalidate() {
            valid = false; partner = false; reported = false;
            _node = 0;
            _spine_notified = false;
// This is now done in Interface::intern_neighbor() before calling Peer::address()
// (which sets valid = true) because Peer::invalidate() is called by the Systole thread
// and Peer::set_interface_name() is called by the Diastole thread, so there was a
// possible race condition on the call to free ( _name ).  So now, _name is only changed
// by the Diastole thread.
//            if ( _name != NULL ) free( _name );
//            _name = NULL;
        }
        void make_partner() { partner = true; }
        void set_interface( bool, uint8_t );
        void set_interface_name( char * );
        inline bool is_valid()    const { return valid; }
        inline bool is_invalid()  const { return valid == false; }
        inline bool is_private()  const { return _is_private; }
        inline bool is_partner()  const { return partner; }
        inline bool not_private() const { return _is_private == false; }
        inline bool not_partner() const { return partner == false; }

        bool operator == ( struct in6_addr& );
        bool operator != ( struct in6_addr& );

        void node( Node * );
        Node *node() const;
        void address( struct in6_addr * );
        struct in6_addr& address() { return lladdr; }

        void copy_address( struct in6_addr * ) const;
        void touch_advertised();
        void touch();
        int  seconds_since_last_update() const;
        bool has_address( struct in6_addr * );
        const struct timeval * last_updated()    const { return &neighbor_updated; }
        const struct timeval * last_advertised() const { return &neighbor_advertised; }

        bool has_notified_spine() const { return _spine_notified; }
        bool has_not_notified_spine() const { return _spine_notified == false; }
        bool send_topology_event( Interface * );
        void topology_changed( Network::Monitor *, Interface * );

        bool reported;
    };

    /**
     */
    class NeighborIterator {
    public:
        NeighborIterator() {}
        virtual ~NeighborIterator() {}
        virtual int operator() ( Peer& ) = 0;
    };

    class Bridge;

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
        struct net_device_stats stats;
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

    /**
     * Interface to allow the monitor to callback this object for each
     * interface.
     */
    class InterfaceIterator {
    public:
        InterfaceIterator() {}
        virtual ~InterfaceIterator() {}
        virtual int operator() ( Interface& ) = 0;
    };

    typedef enum { ACCESS_PRIVATE, ACCESS_BUSINESS } access_t;

    /**
     */
    class SharedNetwork {
    private:
        char *_name;
        int _ordinal;
	access_t _access;
// XXX there really can only be one Interface and one remote interface
//        std::list <Interface*> interfaces;
//        std::list <char*> remote_interfaces;
        Interface *_interface;
        char *_remote_interface;
        Bridge *_bridge;
    public:
        SharedNetwork( const char *, int );
        ~SharedNetwork();
// This is done in the Manager class since it manages the collection of objects
        static const char * create( const char *, int );
        static const char * destroy();
        const char * add_interface( Interface * );
        const char * remove_interface( Interface * );
        const char * add_remote_interface( const char * );
        const char * remove_remote_interface( const char * );
        const char * add_bridge( Bridge * );
        const char * remove_bridge( Bridge *);
        char * set_access( const char * );
        const char *name() const { return _name; }
        const access_t access() const { return _access; }

        Interface *interface() const { return _interface; }
        Bridge *bridge() const { return _bridge; }

// XXX removed
//      char    *tunnel_interface();	// "atun<N>"
//      char    *bridge_interface();	// "biz<N>"
//      char    *physical_interface();	// "ibiz<N>"
//      char    *network_interface();	// "network<N>"  - This is what "_name" is

        static bool Initialize( Tcl_Interp * );
    };

    /**
     */
    class Bridge {
    private:
        char *_name;
        Interface *interface;
        Interface *captured[];
    public:
        Bridge( const char * );
        ~Bridge();
        const char * create();
        const char * destroy();
        const char * add( Interface * );
        const char * capture( Interface * );
        const char * remove( Interface * );
        const char * set_mac_address( unsigned char * );
        const char *name() const { return _name; }
        bool lock_address();
        bool unlock_address();

        unsigned long forward_delay();
        bool forward_delay( unsigned long );

        bool is_tunnelled() const;

        static bool Initialize( Tcl_Interp * );
    };

    /**
     */
    class Tunnel {
    private:
        char *_name;
        int _ordinal;
        Interface *_interface;
        Interface *_tunnel_interface;
        Bridge *_bridge;
        int _port;

        bool _tunnel_up;

    public:                                                                                     /* "ibiz<N>", "atun<N>", "biz<N>" */
        Tunnel( const char *name, int ordinal );	/* const char *, const char * ); */	/* Interface, Tunnel, Bridge */
        ~Tunnel();
        const char * create();
        const char * destroy();
        const char * attach( Interface * );
        const char * detach( Interface * );
        const char *name() const { return _name; }

        inline Interface *tunnel_interface() { return _tunnel_interface; }
        inline Bridge *bridge() { return _bridge; }

        inline bool is_tunnel_up() const { return _tunnel_up; }
        inline void tunnel_up() { _tunnel_up = true; }
        inline void tunnel_down() { _tunnel_up = false; }

        bool check_for_tunnel();
        void is_tunnel_attached();

        bool is_up(Interface *interface);

        void add_to_bridge( Interface *interface );		/* attach() ? */
        void delete_from_bridge( Interface *interface );	/* detach() ? */

        void setup_tunnel();
        void destroy_tunnel();

        void write_config_file(bool server);

        static bool Initialize( Tcl_Interp * );
    };

    /**
     * This is an interface that is used when an Interface is discovered.
     * An implementation of this interface is expected to take a socket
     * from the Interface and listen on it for some protocol.
     *
     * The Monitor reference is used to commuicate state information from
     * the listener back to the monitor object that created it.  The Interface
     * reference is used to update interface state.
     *
     * For a specific implementation of this interface, see the Diastole
     * object in "pulse".
     */
    class ListenerInterface {
    public:
        ListenerInterface() {}
        virtual ~ListenerInterface() {}
        virtual Thread* thread() = 0;
    };
    typedef ListenerInterface *(*ListenerInterfaceFactory)( Tcl_Interp *, Monitor *, Interface * );

    /**
     */
    class NodeIterator {
    public:
        NodeIterator() {}
        virtual ~NodeIterator() {}
        virtual int operator() ( Node& ) = 0;
    };

    class Manager;
    /**
     * Monitor thread for watching network events and performing actions
     * when they occur.
     */
    class Monitor : public Thread, NetLink::RouteReceiveCallbackInterface {
    private:
        Tcl_Interp *interp;
        NetLink::RouteSocket *route_socket;
        ListenerInterfaceFactory factory;
        std::map <int, Interface*> interfaces;

        pthread_mutex_t node_table_lock;
        static const int NODE_TABLE_SIZE = 4096;
        Node *node_table;
        bool table_warning_reported;
	bool table_error_reported;

        Network::Manager *_manager;

        void persist_interface_configuration();
        void capture( Interface * );
        void bring_up( Interface * );

        void load_cache();
        void save_cache();

    public:
        Monitor( Tcl_Interp *, ListenerInterfaceFactory, Network::Manager * );
        virtual ~Monitor() {}
        virtual void run();
        virtual void probe();
        virtual void receive( NetLink::NewLink* );
        virtual void receive( NetLink::DelLink* );
        virtual void receive( NetLink::NewRoute* );
        virtual void receive( NetLink::DelRoute* );
        virtual void receive( NetLink::NewAddress* );
        virtual void receive( NetLink::DelAddress* );
        virtual void receive( NetLink::NewNeighbor* );
        virtual void receive( NetLink::DelNeighbor* );
        virtual void receive( NetLink::RouteMessage* );
        virtual void receive( NetLink::RouteError* );
        int sendto( void *, size_t, int, const struct sockaddr_in6 * );
        int advertise();
        int each_interface( InterfaceIterator& );
        Interface *find_bridge_interface( Interface* );
        Interface *find_physical_interface( Interface* );
        void topology_changed();

        /**
         * The node list is a list of known supernova nodes that have been
         * seen on any/all interfaces.  Each node that has been seen on a
         * specific interface is added to the peer list on that interface.
         */
        Node* intern_node( UUID & );
        bool remove_node( UUID * );
        Node* find_node( UUID * );
        int each_node( NodeIterator& );

        void update_hosts();
    };

    class Event {
    private:
        char *_name;
        void *_data;
    public:
        Event( const char *name, void *data );
        ~Event();
        inline char *name() { return _name; }
        inline void *data() { return _data; }
    };

    class EventReceiver {
    public:
        EventReceiver() {}
        virtual ~EventReceiver() {}
        virtual void operator() ( Event& ) = 0;
    };

    /**
     * Manager thread for handling things that used to be done by the
     * Network Service in Spine.  (I would have used "Service" but there is
     * is already a class by that name in libservice and I wanted to avoid
     * confusion.)
     */
    class Manager : public Thread /*, SuperNova::EventReceiver, SuperNova::DendriteReceiver */ {
    private:
        Tcl_Interp *interp;

        pthread_mutex_t	lock;
        bool _ready;
        bool _topology_changed;

        std::list <Interface*> interfaces;
	std::list <SharedNetwork*> shared_networks;
	std::map <Interface *, Tunnel*> tunnels;

// state variables to control node suicide if we lose both priv0 and biz0 connectivity
// (spine cannot communicate with the other node)
// may need to eventually revisit this algorithm with multiple private links
// but for now priv0 is special because it performs power control via BMC
        bool _suicide;
        int _suicide_epoch;
        bool _priv0_is_up;
        bool _debounced_biz0_is_up;	

    public:
        Manager( Tcl_Interp * );
        virtual ~Manager() {}
        virtual void run();

        bool    ready() { return _ready; }
        void    topology_changed() { _topology_changed = true; }

        void    add_interface( Interface *interface );
        void    remove_interface( Interface *interface );

        static bool    is_primary();
        static bool    is_node0();
        static bool    is_node1();

        void    idle();

// These are now classes in Manager.cc
//      void    event_network_interface_update(void *netif);
//      void    event_net_topology_update(void *event);
//      void    event_link_up(void *event);
//      void    event_link_down(void *event);
//      void    event_priv_link_status(void *event);

        int each_interface( InterfaceIterator& );	// each();
        int each_business( InterfaceIterator& );	// each_biz();
        int each_private( InterfaceIterator& );		// each_priv();

//	/* operator [] */

//	const char	*log_title()	{ return "NetworkManager"; }

//	void	bootstrap();
//	what	get();
// XML	what	to_xml();

// XXX check to see if any of these methods can be made private
// XXX convert these to use Interface objects instead of creating new Device object?

//	SharedNetwork   *find_shared( Device *device);
//	void            update_shared( Device *device);

//	bool    early_link_state( Device *device );
//	bool    link_state( Device *device );
//	bool	set_early_link_state( Device *device, bool new_state );
//	bool    hb_state( Device *device );
//	bool    set_hb_state( Device *device, bool new_state );

//	bool	early_bizcon_state( const char *interface );
//	void	new_tunnel( const char *interface );
        void    maintain_vtund_tunnel( const char *interface_name );
        void    create_vtund_client( const char *interface_name );
        void    enable_vtund_tunnel( const char *interface_name );
        void    disable_vtund_tunnel( const char *interface_name );
        bool    attach_vtund_client( const char *interface_name );	/* DENDRITE IN */
        void    detach_vtund_client( const char *interface_name );	/* DENDRITE IN */

	bool	is_biz_repaired( const char *interface_name );
	bool	is_tunnel_enabled( const char *interface_name );

        /*
         * Network Manager command processing.
         */
        char    *ping();
        void    dump();
        char    *create( char *name, int ordinal, char *local_interface_name, char *remote_interface_name );
        char    *modify( char *name, char *local_interface_name, char *remote_interface_name );
        char    *destroy( char *name );
        char    *fault( char *name );

        char    *set_access( char *name, char *access );

    private:
        Interface       *lookup_interface( const char *interface_name );
        SharedNetwork   *lookup_shared_network( const char *shared_network_name );

    };

    bool Initialize( Tcl_Interp * );

}

#endif

/* vim: set autoindent expandtab sw=4 : */
