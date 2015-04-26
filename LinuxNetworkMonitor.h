
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

/** \file LinuxNetworkMonitor.h
 * \brief Network interface abstraction.
 *
 * Implements NetLinkMonitor to get network events from the kernel.
 * Also, implements NetworkMonitor interface to send generaic network
 * events to the rest of the system.
 */

#ifndef _LINUX_NETWORK_MONITOR_H_
#define _LINUX_NETWORK_MONITOR_H_

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
#include "NetLinkMonitor.h"
#include "NetworkMonitor.h"

namespace ICMPv6 { class Socket; }

/**
 */
namespace Network {
    class Interface;
    class Node;
    class Monitor;
    class Manager;

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
    typedef ListenerInterface *(*ListenerInterfaceFactory)( Tcl_Interp *, NetLink::Monitor *, Network::Interface * );

    /**
     * Monitor thread for watching network events and performing actions
     * when they occur.
     */
    class LinuxNetworkMonitor : public Network::Monitor, NetLink::Monitor {
    private:
    public:
        // Need to pass these to the which subclasses
        LinuxNetworkMonitor( Tcl_Interp *, ListenerInterfaceFactory, Network::Manager * );
        virtual ~LinuxNetworkMonitor() {}

        virtual void run();

        /**
         * The node list is a list of known supernova nodes that have been
         * seen on any/all interfaces.  Each node that has been seen on a
         * specific interface is added to the peer list on that interface.
         */
        Network::Node* intern_node( UUID & );
        bool remove_node( UUID * );
        Network::Node* find_node( UUID * );
        int each_node( Network::NodeIterator& );

        virtual void receive( NetLink::NewLink* );
        virtual void receive( NetLink::DelLink* );
        virtual void receive( NetLink::NewAddress* );
        virtual void receive( NetLink::DelAddress* );

        void update_hosts();
    };

}

#endif

/* vim: set autoindent expandtab sw=4 : */
