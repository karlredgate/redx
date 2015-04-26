
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
     * Monitor thread for watching network events and performing actions
     * when they occur.
     */
    class LinuxNetworkMonitor : public Network::Monitor, NetLink::Monitor {
    private:
        void persist_interface_configuration();

    public:
        // Need to pass these to the which subclasses
        LinuxNetworkMonitor( Tcl_Interp *, ListenerInterfaceFactory );
        virtual ~LinuxNetworkMonitor();

        virtual void run();

        virtual void receive( NetLink::NewLink* );
        virtual void receive( NetLink::DelLink* );
        virtual void receive( NetLink::NewAddress* );
        virtual void receive( NetLink::DelAddress* );

        void update_hosts();
    };

}

#endif

/* vim: set autoindent expandtab sw=4 : */
