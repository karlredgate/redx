
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

/** \file DarwinNetworkMonitor.h
 * \brief Network interface abstraction.
 *
 * Implements NetLinkMonitor to get network events from the kernel.
 * Also, implements NetworkMonitor interface to send generaic network
 * events to the rest of the system.
 */

#ifndef _DARWIN_NETWORK_MONITOR_H_
#define _DARWIN_NETWORK_MONITOR_H_

#include <stdint.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <unistd.h>
#include <tcl.h>

#include <list>
#include <map>

#include "UUID.h"
#include "Thread.h"
#include "InterfaceIterator.h"
#include "NodeIterator.h"
#include "NetworkMonitor.h"

namespace ICMPv6 { class Socket; }

/**
 */
namespace Network {
    class Interface;
    class Node;
    class Monitor;

    /**
     * Monitor thread for watching network events and performing actions
     * when they occur.
     */
    class DarwinNetworkMonitor : public Network::Monitor {
    private:
        void persist_interface_configuration();

    public:
        // Need to pass these to the which subclasses
        DarwinNetworkMonitor( Tcl_Interp *, ListenerInterfaceFactory );
        virtual ~DarwinNetworkMonitor();

        virtual void run();
        virtual void probe();

        void update_hosts();
        virtual void capture( Network::Interface * );
        virtual void bring_up( Network::Interface * );
        virtual Interface *find_bridge_interface( Network::Interface* );
    };

}

#endif

/* vim: set autoindent expandtab sw=4 : */
