
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
#include "InterfaceIterator.h"

namespace ICMPv6 { class Socket; }

/**
 */
namespace Network {

    class Bridge;
    class Interface;
    class Tunnel;
    class Node;
    class Peer;

    class SharedNetwork;
    class Monitor;

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
