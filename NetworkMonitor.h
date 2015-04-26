
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

/** \file NetworkMonitor.h
 * \brief Network interface abstraction.
 *
 */

#ifndef _NETWORK_MONITOR_H_
#define _NETWORK_MONITOR_H_

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
#include "NodeIterator.h"

namespace ICMPv6 { class Socket; }

/**
 */
namespace Network {

    class Bridge;
    class Interface;
    class Tunnel;
    class Node;
    class Peer;

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

    class Manager;

    /**
     * Monitor thread for watching network events and performing actions
     * when they occur.
     */
    class Monitor : public Thread {
    // I want to change this so there are accessors instead
    protected:
        Tcl_Interp *interp;
        std::map <int, Network::Interface*> interfaces;

        void load_cache();
        void save_cache();

        virtual void bring_up( Network::Interface * );
        virtual void capture( Network::Interface * );

    private:
        ListenerInterfaceFactory factory;

        pthread_mutex_t node_table_lock;
        static const int NODE_TABLE_SIZE = 4096;
        Node *node_table;
        bool table_warning_reported;
	bool table_error_reported;

    public:
        Monitor( Tcl_Interp *, ListenerInterfaceFactory );
        virtual ~Monitor() {}

        virtual void run();

        int sendto( void *, size_t, int, const struct sockaddr_in6 * );
        int advertise();
        int each_interface( InterfaceIterator& );
        Interface *find_bridge_interface( Interface* );

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

}

#endif

/* vim: set autoindent expandtab sw=4 : */
