
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

/** \file Network.cc
 * \brief 
 *
 * Can I create a tunnel device or the like that creates a pseudo-interface
 * called mgmt0 that can be used by HB etc..?
 *
 * Make sure to set PID for socket (in NetLink.cc)
 *
 * Add Interface multicast thread - clone from old ekg
 *  - sender/receiver
 *  - started when Interface constructed?
 *  - stopped in destructor?
 *  - Should this be a full Thread subclass -- or is there a way to make
 *    it a simple Thread object with a simple method for run?
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
#include <syslog.h>
#include <string.h>
#include <glob.h>
#include <errno.h>

#include <tcl.h>
#include "tcl_util.h"

#include "util.h"
#include "host_table.h"
#include "NetLink.h"
#include "Network.h"

namespace { int debug = 0; }

/** When a NewLink message is received, create/update interface object.
 *
 * 
 */
void Network::Monitor::receive( NetLink::NewLink *message ) {
    bool link_requires_repair = false;
    Interface *interface = interfaces[ message->index() ];

    if ( interface == NULL ) {

        if ( debug > 0 ) {
            syslog( LOG_NOTICE, "adding %s(%d) to interface list", message->name(), message->index() );
        }
        interface = new Network::Interface( interp, message );
        interfaces[ message->index() ] = interface;

        interface->update( message );

        if ( !interface->exists() ) {
            syslog( LOG_NOTICE, "WARNING: interface %s(%d) does not appear to exist, skipping...",
                    message->name(), message->index() );
            return;
        }

        bool is_bridge = interface->is_bridge();
        bool is_physical = interface->is_physical();

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
                    syslog( LOG_NOTICE, "%s is not listening to port 123 on its primary address, restart ntpd",
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

        if ( message->family() == AF_BRIDGE ) {
            const char *bridge_name = "UNKNOWN";
            int bridge_index = message->bridge_index();
            if ( bridge_index != 0 ) {
                Interface *bridge = interfaces[ bridge_index ];
                if ( bridge != NULL )  bridge_name = bridge->name();
            }
            syslog( LOG_NOTICE, "%s(%d): added to bridge '%s'",
                                interface->name(), interface->index(), bridge_name );
        }
    }

}

/**
 */
void Network::Monitor::receive( NetLink::DelLink *message ) {

    if ( message->index() == 0 ) {
        syslog( LOG_WARNING, "received a DelLink message for inteface index 0 (INVALID)" );
        return;
    }

    Interface *interface = interfaces[ message->index() ];

    if ( interface == NULL ) {
        const char *name = message->name();
        if ( name == NULL ) name = "Unknown";
        syslog( LOG_WARNING, "unknown interface removed: %s(%d)", name, message->index() );
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

    if ( message->family() == AF_BRIDGE ) {
        const char *bridge_name = "UNKNOWN";
        int bridge_index = message->bridge_index();
        if ( bridge_index != 0 ) {
            Interface *bridge = interfaces[ bridge_index ];
            if ( bridge != NULL )  bridge_name = bridge->name();
        }
        if ( interface->is_captured() ) {
            syslog( LOG_NOTICE, "%s(%d): removed from bridge '%s'",
                                interface->name(), interface->index(), bridge_name );
        } else {
            syslog( LOG_NOTICE, "%s(%d): DelLink message from bridge '%s' -- but not removed",
                                interface->name(), interface->index(), bridge_name );
        }
        return;
    }

    if ( message->change() == 0xFFFFFFFF ) {
        syslog( LOG_WARNING, "%s(%d): removed from system", interface->name(), interface->index() );
        // stop listener thread
        //   need Interface to have a handle on its listener thread
        interface->remove();
    }

}

/**
 */
void Network::Monitor::receive( NetLink::NewRoute *message ) {
    if ( debug > 1 ) {
        syslog( LOG_INFO, "Network::Monitor => process NewRoute -->  scope %d, family %d\n",
                message->scope(),
                message->family()
        );
    }
}

/**
 */
void Network::Monitor::receive( NetLink::DelRoute *message ) {
    if ( debug > 1 ) {
        syslog( LOG_INFO, "Network::Monitor => process DelRoute -->  scope %d, family %d\n",
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
        syslog( LOG_NOTICE, "IPv4 address added to '%s'", interface->name() );
        return;
    default:
        syslog( LOG_NOTICE, "Unknown address family added to '%s'", interface->name() );
        return;
    }

    if (  interface->is_primary( message->in6_addr() )  ) {
        syslog( LOG_NOTICE, "primary ipv6 address added to '%s'", interface->name() );
        if ( interface->is_private() and interface->not_sync() and
             interface->not_listening_to("udp6", 123) ) { // NTP
            syslog( LOG_NOTICE, "%s is not listening to port 123 on its primary address, restart ntpd",
                                interface->name() );
            system( "/usr/bin/config_ntpd --restart" );
        }
    } else {
        if ( debug > 0 ) {
            syslog( LOG_NOTICE, "secondary ipv6 address added to '%s', ignoring", interface->name() );
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
        syslog( LOG_NOTICE, "IPv4 address removed from '%s', ignoring", interface->name() );
        return;
    default:
        syslog( LOG_NOTICE, "Unknown address family removed from '%s', ignoring", interface->name() );
        return;
    }

    if (  interface->is_primary( message->in6_addr() )  ) {
        if ( debug > 0 ) {
            syslog( LOG_NOTICE, "primary ipv6 address removed from '%s', repairing", interface->name() );
        }
        interface->configure_addresses();
    } else {
        if ( debug > 0 ) {
            syslog( LOG_NOTICE, "secondary ipv6 address removed from '%s', ignoring", interface->name() );
        }
    }
}

/**
 */
void Network::Monitor::receive( NetLink::NewNeighbor *message ) {
    if ( debug > 1 ) {
        syslog( LOG_INFO, "Network::Monitor => process NewNeighbor \n" );
    }
}

/**
 */
void Network::Monitor::receive( NetLink::DelNeighbor *message ) {
    if ( debug > 1 ) {
        syslog( LOG_INFO, "Network::Monitor => process DelNeighbor\n");
    }
}

/**
 */
void Network::Monitor::receive( NetLink::RouteMessage *message ) {
    if ( debug > 1 ) {
        syslog( LOG_INFO, "Network::Monitor unknown RouteMessage (%d) -- skipping\n", message->type_code() );
    }
}

/**
 * \todo should have some internal state that tells us the last message
 * we received was an error, and what the error was.
 */
void Network::Monitor::receive( NetLink::RouteError *message ) {
    if ( debug > 1 ) {
        syslog( LOG_INFO, "Network::Monitor RouteError %d\n", message->error() );
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

/** Iterate and call a callback for each Interface.
 */
int Network::Monitor::each_interface( InterfaceIterator& callback ) {
    int result = 0;

    std::map<int, Interface *>::const_iterator iter = interfaces.begin();
    while ( iter != interfaces.end() ) {
        Network::Interface *interface = iter->second;
        if ( interface != NULL )  result += callback( *interface );
        iter++;
    }

    return result;
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
    uint32_t groups = RTMGRP_LINK | RTMGRP_NOTIFY | RTNLGRP_NEIGH |
                      RTMGRP_IPV4_IFADDR | RTMGRP_IPV4_ROUTE |
                      RTMGRP_IPV6_IFADDR | RTMGRP_IPV6_ROUTE |
                      RTMGRP_IPV6_IFINFO | RTMGRP_IPV6_PREFIX ;

    route_socket = new NetLink::RouteSocket(groups);
    NetLink::RouteSocket &rs = *route_socket;

    probe();
    if ( debug > 0 ) syslog( LOG_NOTICE, "network monitor started" );
    for (;;) {
        rs.receive( this );
        sleep( 1 );
    }
}

void Network::Monitor::probe() {
    if ( route_socket == NULL ) return;
    if ( debug > 0 ) syslog( LOG_NOTICE, "Monitor: sending probe" );
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
        syslog( LOG_ERR, "node table alloc failed" );
        exit( errno );
    }
    for ( int i = 0 ; i < NODE_TABLE_SIZE ; i++ ) {
        node_table[i].invalidate();
    }
    if ( debug > 0 ) syslog( LOG_ERR, "node table is at %p (%d)", node_table, size );
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
        perror( "failed to open netlink rt socket" );
        exit( 1 );
    }
    // setup sndbuf and rcvbuf

    // bind
    struct sockaddr_nl address;
    memset( &address, 0, sizeof(address) );
    address.nl_family = AF_NETLINK;
    address.nl_groups = 0;
    if ( bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0 ) {
        perror( "cannot bind netlink socket" );
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
        perror("sendto");
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
                perror("recvmsg");
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
bool Network::Initialize( Tcl_Interp *interp ) {

    Tcl_Command command;

    Tcl_Namespace *ns = Tcl_CreateNamespace(interp, "Network", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    if ( Tcl_LinkVar(interp, "Network::debug", (char *)&debug, TCL_LINK_INT) != TCL_OK ) {
        syslog( LOG_ERR, "failed to link Network::debug" );
        exit( 1 );
    }

    command = Tcl_CreateObjCommand(interp, "Network::Monitor", Monitor_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Network::Probe", Probe_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Network::Manager", Manager_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    if ( Network::Interface::Initialize(interp) == false ) {
        syslog( LOG_ERR, "Network::Interface::Initialize failed" );
        return false;
    }

    if ( Network::SharedNetwork::Initialize(interp) == false ) {
        syslog( LOG_ERR, "Network::SharedNetwork::Initialize failed" );
        return false;
    }

    if ( Network::Bridge::Initialize(interp) == false ) {
        syslog( LOG_ERR, "Network::Bridge::Initialize failed" );
        return false;
    }

    if ( Network::Tunnel::Initialize(interp) == false ) {
        syslog( LOG_ERR, "Network::Tunnel:Initialize failed" );
        return false;
    }

    return true;
}

/* vim: set autoindent expandtab sw=4 : */
