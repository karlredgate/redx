
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

/** \file NetLinkMonitor.cc
 * \brief 
 *
 * This is only the generic NetLinkMonitor methods.
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

namespace { int debug = 0; }

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
NetLink::Monitor::process_one_event() {
    NetLink::RouteSocket &rs = *route_socket;
    rs.receive( this );
}

void NetLink::Monitor::probe() {
    if ( debug > 0 ) log_notice( "Monitor: sending probe" );
    NetLink::RouteSocket &rs = *route_socket;
    NetLink::GetLink getlink;
    getlink.send( rs );
}

/**
 */
NetLink::Monitor::Monitor( Tcl_Interp *interp, Network::ListenerInterfaceFactory factory )
{
    /*
     * Disable neighbor and route messages from being reported
     * because we do not process them and we can get 100's of them
     * every second.  This causes -ENOBUFS and we can miss Link events.
     */
    uint32_t groups = RTMGRP_LINK | RTMGRP_NOTIFY |
                      RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR |
#if 0
                      RTMGRP_IPV4_ROUTE | RTMGRP_IPV6_ROUTE |
                      RTNLGRP_NEIGH |
#endif
                      RTMGRP_IPV6_IFINFO | RTMGRP_IPV6_PREFIX ;

    route_socket = new NetLink::RouteSocket( groups );
}

/**
 */
NetLink::Monitor::~Monitor() {
}

/* vim: set autoindent expandtab sw=4 : */
