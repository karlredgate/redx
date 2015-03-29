
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

/** \file NetLink.cc
 * \brief 
 *
 */

#include <asm/types.h>
#include <sys/socket.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if_arp.h>  // for ARPHRD_*

#include <arpa/inet.h>  // for ARPHRD_*

#ifndef NDA_RTA
#define NDA_RTA(r)  ((struct rtattr*)(((char*)(r)) + NLMSG_ALIGN(sizeof(struct ndmsg))))
#endif

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>

#include <tcl.h>
#include "TCL_Fixup.h"

#include "NetLink.h"

/**
 * This is defined in the iproute2 tools, and looked useful.
 */
#define NLMSG_TAIL(nmsg) \
        ((struct rtattr *) (((uint8_t *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

namespace {
    typedef NetLink::Message *(*MessageFactory)( struct nlmsghdr * );
    typedef NetLink::RouteMessage *(*RouteMessageFactory)( struct nlmsghdr * );
    static const int MAX_RTFACTORY = 1024;
    RouteMessageFactory routeFactories[ MAX_RTFACTORY ];
    int debug = 0;
}

/**
 */
NetLink::Message *
NetLink::Message::Factory( struct nlmsghdr *hdr ) {
    using namespace NetLink;
    // type should come from the data -- which should be a nlm_message
    // need to gather RT info in RT msgs
    return new Message( hdr->nlmsg_type );
}

/**
 */
NetLink::RouteError *
NetLink::RouteError::Factory( struct nlmsgerr *hdr ) {
    using namespace NetLink;
    return new RouteError( hdr );
}

/**
 */
NetLink::RouteMessage *
NetLink::RouteMessage::Factory( struct nlmsghdr *hdr ) {
    using namespace NetLink;
    // type should come from the data -- which should be a nlm_message
    // need to gather RT info in RT msgs
    return new RouteMessage( hdr->nlmsg_type );
}

/**
 */
NetLink::Socket::Socket( int protocol ) {
    socket = ::socket( AF_NETLINK, SOCK_RAW, protocol );
    if ( socket == -1 ) {
        perror( "failed to create netlink socket" );
        // This should change to it having an error state method/member that
        // can be queried, OR have it raise an exception
        ::exit( 1 );
    }

    int bufsize;
    socklen_t bufsize_len = sizeof(bufsize);
    if ( getsockopt(socket, SOL_SOCKET, SO_RCVBUF, &bufsize, &bufsize_len) < 0 ) {
        syslog( LOG_ERR, "failed to determine the netlink socket rcvbuf size" );
        return;
    }
    syslog( LOG_NOTICE, "netlink socket rcvbuf size is %d", bufsize );
    bufsize *= 4;
    if ( setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize)) < 0 ) {
        syslog( LOG_ERR, "failed to increase the netlink socket rcvbuf size" );
        return;
    }
    syslog( LOG_NOTICE, "netlink socket rcvbuf size set to %d", bufsize );
}

/**
 */
NetLink::Socket::~Socket() {
    close( socket );
}

/**
 */
bool NetLink::Socket::bind( uint32_t group ) {

    memset( &address, 0, sizeof(address) );
    address.nl_family = AF_NETLINK;
    address.nl_groups = group;

    if ( ::bind(socket, (struct sockaddr *)&address, sizeof address) == -1 ) {
        perror( "failed to bind netlink socket" );
        ::exit( 1 );
    }
    if ( debug > 0 )  printf( "bound NetLink::Socket to %d\n", group );

    // This may go away 
    int length  __attribute((unused))  = sizeof address;

    return true;
}

/**
 */
void NetLink::Socket::send( void *message, int length ) {
    int result;
    result = sendto( socket, message, length, 0, (struct sockaddr *)&address, sizeof(address) );
}

/**
 * this is the generic socket receive ... override this so the rt socket can construct
 * route objects
 *
 * when receiving and reporting to TCL connection, need to translate to
 * a Tcl_Obj -- 
 */
void NetLink::Socket::receive( NetLink::ReceiveCallbackInterface *callback ) {
    struct sockaddr_nl address;
    struct iovec iov;

    struct msghdr msg;
    msg.msg_name = &address;
    msg.msg_namelen = sizeof(address);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    char buffer[16 * 1024];
    iov.iov_base = buffer;

    unsigned int status;

    status = recvmsg( socket, &msg, 0 );

    struct nlmsghdr *header = (struct nlmsghdr *)buffer;
    if ( NLMSG_OK(header, status) ) {
    }

    using namespace NetLink;
    RouteMessageFactory factory = routeFactories[ header->nlmsg_type ];
    if ( factory == NULL ) {
        // print an error?
        return;
    }
    Message *message = factory( header );
    message->deliver( callback );
    delete message;
}

/**
 * The NetLink::Socket base class destructor closes the actual
 * Linux socket.
 */
NetLink::RouteSocket::~RouteSocket() {
}

/**
 * when receiving and reporting to TCL connection, need to translate to
 * a Tcl_Obj -- 
 */
void NetLink::RouteSocket::receive( NetLink::RouteReceiveCallbackInterface *callback ) {
    struct sockaddr_nl address;
    struct iovec iov;
    memset( &iov, 0, sizeof(iov) );
    char buffer[16 * 1024];
    iov.iov_base = buffer;
    char control_buffer[4096];

    struct msghdr msg;
    msg.msg_name = &address;
    msg.msg_namelen = sizeof(address);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;  // # of iovecs
    msg.msg_control = control_buffer;
    msg.msg_controllen = sizeof(control_buffer);

    for (;;) {
        ssize_t status;
        struct nlmsghdr *h;

        iov.iov_len = sizeof(buffer);  // size of the iovec buffer
        status = recvmsg( socket, &msg, 0 );
        if ( status < 0 ) {
            if ( errno == EINTR ) {
                // normally just continue here
                printf( "interupted recvmsg\n" );
            } else if ( errno == ENOBUFS ) {
                syslog( LOG_ERR, "%%BUG recvmsg returned ENOBUFS" );
            } else {
                perror("NetLink::recvmsg");
            }
            continue;
        }

        if ( status == -1 ) {
            perror( "NetLink::RouteSocket::recvmsg" );
            return;
        }

        if ( status == 0 ) {
            if ( debug ) printf( "finished nlmsgs\n" );
            break;
        } else {
            if ( debug ) printf( "status=%d\n", status );
        }

        h = (struct nlmsghdr *)buffer;
        unsigned int message_length = status;
        while ( NLMSG_OK(h, message_length) ) {
            if ( h->nlmsg_type == NLMSG_DONE ) {
                printf( "NLMSG_DONE\n" );
                goto finish;
            }

            if ( h->nlmsg_type == NLMSG_ERROR ) {
                NetLink::RouteError error( (struct nlmsgerr *)NLMSG_DATA(h) );
                error.deliver( callback );
                goto finish;
            }

            /*
            if ( h->nlmsg_flags & NLM_F_MULTI ) {
                printf( "NLM_F_MULTI\n" );
            } else {
                printf( "not:NLM_F_MULTI\n" );
            }
            */

            if ( debug > 1 ) {
                printf( "process nlmsg type=%d flags=0x%08x pid=%d seq=%d\n",
                        h->nlmsg_type, h->nlmsg_flags, h->nlmsg_pid, h->nlmsg_seq );
            }

            { // type is in linux/if_arp.h -- flags are in linux/if.h
                // struct ifinfomsg *ifi = (struct ifinfomsg *)( NLMSG_DATA(h) );
                // int length = h->nlmsg_len - NLMSG_LENGTH( sizeof(*ifi) );
                // Network::Interface *ii = new Network::Interface( ifi, length );

                using namespace NetLink;
                RouteMessageFactory factory = routeFactories[ h->nlmsg_type ];
                if ( factory == NULL ) {
                    // print an error?
                    return;
                }
                RouteMessage *message = factory( h );
                message->deliver( callback );
                delete message;
            }

            h = NLMSG_NEXT( h, message_length );
        }
        if ( debug > 1 )  printf( "finished with this message (notOK)\n" );
        if ( msg.msg_flags & MSG_TRUNC ) {
            printf( "msg truncated" );
            continue;
        }
        if ( message_length != 0 ) {
            printf( "remnant\n" );
        }
    }

finish:
    ; // normally this would close the socket ...
}

/**
 */
NetLink::RouteSocket::RouteSocket( uint32_t group )
: NetLink::Socket(NETLINK_ROUTE) {
    bind(group);
}

/**
 */
static int
RouteSocket_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace NetLink;
    RouteSocket *socket = (RouteSocket *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(socket)) );
        return TCL_OK;
    }
    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Svc_SetResult( interp, "NetLink::RouteSocket", TCL_STATIC );
        return TCL_OK;
    }

    /**
     * Arguement is a message
     */
    if ( Tcl_StringMatch(command, "send") ) {
        if ( objc != 3 ) {
            Tcl_ResetResult( interp );
            Tcl_WrongNumArgs( interp, 1, objv, "send <message>" );
            return TCL_ERROR; 
        }

        RouteMessage *message;
        void *p = (void *)&(message);
        if ( Tcl_GetLongFromObj(interp,objv[2],(long*)p) != TCL_OK ) {
            return TCL_ERROR;
        }
        message->send( *socket );
        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Svc_SetResult( interp, "Unknown command for RouteSocket object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
RouteSocket_delete( ClientData data ) {
    using namespace NetLink;
    RouteSocket *socket = (RouteSocket *)data;
    delete socket;
}

/**
 */
static int
RouteSocket_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR; 
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );

    using namespace NetLink;
    RouteSocket *object = new RouteSocket();
    // should have a delete proc also
    Tcl_CreateObjCommand( interp, name, RouteSocket_obj, (ClientData)object, RouteSocket_delete );
    Svc_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}   

static uint32_t sequence_seed = 1;
/**
 * \todo
 * The sequence number should be part of the socket to sequence 
 * the messages going through a specific socket.  Here we just
 * set it in the message for now.
 */
NetLink::Message::Message( uint16_t message_type ) {
    // this should be locked
    sequence = sequence_seed++;
    type = message_type;
}

/**
 */
bool
NetLink::Message::deliver( NetLink::ReceiveCallbackInterface *callback ) {
    callback->receive( this );
    return true;
}

/**
 */
NetLink::RouteError::RouteError( struct nlmsgerr *data ) : RouteMessage(NLMSG_ERROR) {
    memcpy( &message, data, sizeof message );
}

/**
 * Need to augment the receive interface to handle these...
 */
bool
NetLink::RouteError::deliver( NetLink::RouteReceiveCallbackInterface *callback ) {
    callback->receive( this );
    return true;
}

/**
 * I think this crap is unnecessary
 */
NetLink::RouteMessage::RouteMessage( uint16_t message_type )
: Message(message_type) {
    memset( &event, 0, sizeof(event) );
    event.nlmsg_len = sizeof(event);
    event.nlmsg_type = type;
}

/**
 */
NetLink::RouteMessage::RouteMessage( struct nlmsghdr *hdr )
: Message(hdr->nlmsg_type) {
    memcpy( &event, hdr, sizeof(*hdr) );
    // \todo copy the RTAs
}

/**
 */
bool
NetLink::RouteMessage::deliver( NetLink::RouteReceiveCallbackInterface *callback ) {
    callback->receive( this );
    return true;
}

/**
 * This does not work ... each subtype needs its own send, since
 * the request layout is different...
 *
 * \todo make RouteMessage::send pure virtual ?? and eliminate these send calls
 */
void NetLink::RouteMessage::send( NetLink::Socket &socket ) {
    // socket.send( &request, sizeof(request) );
    socket.send( &event, sizeof(event) );
}

/**
 * This is only called when generating a request into the kernel.  Messages
 * from the kernel are constructed using a nlmsghdr pointer.
 */
NetLink::LinkMessage::LinkMessage( uint16_t type ) : RouteMessage(type) {
    memset( &request, 0, sizeof(request) );

    request.n.nlmsg_len = sizeof(request);
    request.n.nlmsg_type = type;

    // These flags may be too specific to GETLINK messages
    // \todo change LinkMesage base class flags to just REQUEST -- add others in GetLink
    request.n.nlmsg_flags = NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST;

    request.n.nlmsg_pid = 0;
    request.n.nlmsg_seq = sequence;
    request.ifi.ifi_family = AF_PACKET;
}

/**
 * The IFA_RTA macro finds the route attribute header in the
 * current "Address" message.  
 *
 * Need to change this to copy the message into the request
 * buffer (since we must have the request buffer for the request
 * messages.
 */
NetLink::LinkMessage::LinkMessage( struct nlmsghdr *hdr )
: RouteMessage(hdr) {
    memcpy( &(request.n), hdr, sizeof(*hdr) );
    struct ifinfomsg *info = (struct ifinfomsg *)NLMSG_DATA(hdr);
    memcpy( &(request.ifi), info, sizeof request.ifi );

    int length = hdr->nlmsg_len - NLMSG_LENGTH( sizeof(*info) );

    memset( attr, 0, sizeof(attr) );

    // can't I get this from the header directly??
    struct rtattr *ra = IFLA_RTA(info);
    while ( RTA_OK(ra, length) ) {
        if ( ra->rta_type <= IFLA_MAX ) {
            attr[ ra->rta_type ] = ra;
        }
        ra = RTA_NEXT( ra, length );
    }

    if ( length > 0 ) {
        printf( " LM! remnant=%d\n", length );
    }

    if ( attr[IFA_LOCAL]   == 0 ) attr[IFA_LOCAL]   = attr[IFA_ADDRESS];
    if ( attr[IFA_ADDRESS] == 0 ) attr[IFA_ADDRESS] = attr[IFA_LOCAL];
}

/**
 * This will need to increase in size to include the RTATTRs
 */
void NetLink::LinkMessage::send( NetLink::Socket &socket ) {
    socket.send( &request, sizeof(request) );
}

/**
 */
const char *
NetLink::LinkMessage::name() const {
    // return (char *)RTA_DATA( attr[IFLA_IFNAME] );
    if ( attr[IFLA_IFNAME] == NULL ) return "NO_NAME";
    const char *n = (char *)RTA_DATA( attr[IFLA_IFNAME] );
    if ( n == NULL ) n = "NULL_NAME";
    return n;
}

/**
 */
int NetLink::LinkMessage::index() const { return request.ifi.ifi_index; }

/**
 */
int NetLink::LinkMessage::bridge_index() const {
    if ( attr[IFLA_MASTER] == NULL ) return 0;
    int index = *(int*)RTA_DATA( attr[IFLA_MASTER] );
    return index;
}

/**
 */
unsigned char
NetLink::LinkMessage::operstate() const {
    if ( attr[IFLA_OPERSTATE] == NULL ) return 0;
    unsigned char state = *(unsigned char *)RTA_DATA( attr[IFLA_OPERSTATE] );
    return state;
}

/**
 */
unsigned int NetLink::LinkMessage::flags() const { return request.ifi.ifi_flags; }
unsigned int NetLink::LinkMessage::change() const { return request.ifi.ifi_change; }

/**
 * It appears that vifs look like ethernet device types.
 *
 * ARPHRD_ETHER    - ethernet
 * ARPHRD_LOOPBACK - loopback
 * ARPHRD_SIT      - IPv6 in IPv4 tunnel
 */
unsigned short
NetLink::LinkMessage::device_type() const {
    return request.ifi.ifi_type;
}

/**
 */
NetLink::GetLink::GetLink() : LinkMessage(RTM_GETLINK) {
}

/**
 */
NetLink::RouteMessage *
NetLink::GetLink::Factory( struct nlmsghdr *hdr ) {
    using namespace NetLink;
    RouteMessage *message = new GetLink();
    // populate with hdr info... and RTAs
    return message;
}

/**
 */
static int
GetLink_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace NetLink;
    GetLink *object = (GetLink *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(object)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Svc_SetResult( interp, "NetLink::GetLink", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Svc_SetResult( interp, "send not implemented", TCL_STATIC );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Svc_SetResult( interp, "Unknown command for GetLink object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
GetLink_delete( ClientData data ) {
    using namespace NetLink;
    GetLink *message = (GetLink *)data;
    delete message;
}

/**
 */
static int
GetLink_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR; 
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    using namespace NetLink;
    GetLink *object = new GetLink();
    Tcl_CreateObjCommand( interp, name, GetLink_obj, (ClientData)object, GetLink_delete );
    Svc_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
NetLink::NewLink::NewLink() : LinkMessage(RTM_NEWLINK) {
}

/**
 * This wants to copy the relevant information from the ifinfo
 * struct -- not the whole struct...
 * and needs to get the RTAs as well
 *
 * This means I need to change the Interface constructor as well...
 *
 */
NetLink::NewLink::NewLink( struct nlmsghdr *hdr )
: LinkMessage(hdr) {
    ifi = &(request.ifi);
    unsigned char *mac = MAC();
    if ( debug > 1 ) {
        printf( "NewLink: => %d: %s %s addr[%02x:%02x:%02x:%02x:%02x:%02x]\n",
                index(), name(), (device_type() == ARPHRD_ETHER ? "ether" : "???"),
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
    }
}

/**
 */
bool
NetLink::NewLink::deliver( NetLink::RouteReceiveCallbackInterface *callback ) {
    callback->receive( this );
    return true;
}

/**
 */
bool NetLink::NewLink::up_changed() const {
    return (request.ifi.ifi_change & IFF_UP) != 0;
}

/**
 */
bool NetLink::NewLink::is_up() const {
    return (request.ifi.ifi_flags & IFF_UP) != 0;
}

/**
 */
bool NetLink::NewLink::running_changed() const {
    return (request.ifi.ifi_change & IFF_RUNNING) != 0;
}

/**
 */
bool NetLink::NewLink::is_running() const {
    return (request.ifi.ifi_flags & IFF_RUNNING) != 0;
}

/**
 */
bool NetLink::NewLink::promiscuous_changed() const {
    return (request.ifi.ifi_change & IFF_PROMISC) != 0;
}

/**
 */
bool NetLink::NewLink::is_promiscuous() const {
    return (request.ifi.ifi_flags & IFF_PROMISC) != 0;
}

/**
 */
bool NetLink::NewLink::link_changed() const {
    return (request.ifi.ifi_change & IFF_LOWER_UP) != 0;
}

/**
 */
bool NetLink::NewLink::has_link() const {
    return (request.ifi.ifi_flags & IFF_LOWER_UP) != 0;
}

/**
 */
bool NetLink::NewLink::dormant_changed() const {
    return (request.ifi.ifi_change & IFF_DORMANT) != 0;
}

/**
 */
bool NetLink::NewLink::is_dormant() const {
    return (request.ifi.ifi_flags & IFF_DORMANT) != 0;
}

/**
 */
unsigned char *
NetLink::NewLink::MAC() const {
    if ( attr[IFLA_ADDRESS] == 0 ) {
        return (unsigned char *)"\0\0\0\0\0\0\0";
    }
    return (unsigned char *)RTA_DATA( attr[IFLA_ADDRESS] );
}

/**
 */
struct net_device_stats *
NetLink::NewLink::stats() const {
    return (struct net_device_stats *)RTA_DATA( attr[IFLA_STATS] );
}

/**
 */
NetLink::RouteMessage *
NetLink::NewLink::Factory( struct nlmsghdr *hdr ) {
    using namespace NetLink;
    RouteMessage *message = new NewLink( hdr );
    return message;
}

/**
 */
static int
NewLink_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace NetLink;
    NewLink *object = (NewLink *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(object)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Svc_SetResult( interp, "NetLink::NewLink", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Svc_SetResult( interp, "send not implemented", TCL_STATIC );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Svc_SetResult( interp, "Unknown command for NewLink object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
NewLink_delete( ClientData data ) {
    using namespace NetLink;
    NewLink *message = (NewLink *)data;
    delete message;
}

/**
 */
static int
NewLink_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR; 
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    using namespace NetLink;
    NewLink *object = new NewLink();
    Tcl_CreateObjCommand( interp, name, NewLink_obj, (ClientData)object, NewLink_delete );
    Svc_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
NetLink::DelLink::DelLink() : LinkMessage(RTM_DELLINK) {
}

/**
 */
NetLink::DelLink::DelLink( struct nlmsghdr *hdr )
: LinkMessage(hdr) {
    ifi = &(request.ifi);
    unsigned char *mac = MAC();
    if ( debug > 1 ) {
        printf( "DelLink: => %d: %s %s addr[%02x:%02x:%02x:%02x:%02x:%02x]\n",
                index(), name(), (device_type() == ARPHRD_ETHER ? "ether" : "???"),
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
    }
}

/**
 */
bool
NetLink::DelLink::deliver( NetLink::RouteReceiveCallbackInterface *callback ) {
    callback->receive( this );
    return true;
}

/**
 */
unsigned char *
NetLink::DelLink::MAC() const {
    return (unsigned char *)RTA_DATA( attr[IFLA_ADDRESS] );
}


/**
 */
NetLink::RouteMessage *
NetLink::DelLink::Factory( struct nlmsghdr *hdr ) {
    using namespace NetLink;
    RouteMessage *message = new DelLink( hdr );
    // populate with hdr info... and RTAs
    return message;
}

/**
 */
static int
DelLink_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace NetLink;
    DelLink *object = (DelLink *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(object)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Svc_SetResult( interp, "NetLink::DelLink", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Svc_SetResult( interp, "send not implemented", TCL_STATIC );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Svc_SetResult( interp, "Unknown command for DelLink object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
DelLink_delete( ClientData data ) {
    using namespace NetLink;
    DelLink *message = (DelLink *)data;
    delete message;
}

/**
 */
static int
DelLink_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR; 
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    using namespace NetLink;
    DelLink *object = new DelLink();
    Tcl_CreateObjCommand( interp, name, DelLink_obj, (ClientData)object, DelLink_delete );
    Svc_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 * This is only called when generating a request into the kernel.  Messages
 * from the kernel are constructed using a nlmsghdr pointer.
 */
NetLink::AddressMessage::AddressMessage( uint16_t type ) : RouteMessage(type) {
    request.n.nlmsg_len = NLMSG_LENGTH( sizeof(struct ifaddrmsg) );
    request.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    request.n.nlmsg_type = type;
}

/**
 * The IFA_RTA macro finds the route attribute header in the
 * current "Address" message.  
 *
 * Need to change this to copy the message into the request
 * buffer (since we must have the request buffer for the request
 * messages.
 */
NetLink::AddressMessage::AddressMessage( struct nlmsghdr *hdr )
: RouteMessage(hdr) {
    struct ifaddrmsg *addr = (struct ifaddrmsg *)NLMSG_DATA(hdr);
    memcpy( &(request.ifa), addr, sizeof request.ifa );

    int length = hdr->nlmsg_len - NLMSG_LENGTH( sizeof(*addr) );

    memset( attr, 0, sizeof(attr) );

    // can't I get this from the header directly??
    struct rtattr *ra = IFA_RTA(addr);
    while ( RTA_OK(ra, length) ) {
        if ( ra->rta_type <= RTA_MAX ) {
            attr[ ra->rta_type ] = ra;
        }
        ra = RTA_NEXT( ra, length );
    }

    if ( length > 0 ) {
        printf( " AM! remnant=%d\n", length );
    }

    if ( attr[IFA_LOCAL]   == 0 ) attr[IFA_LOCAL]   = attr[IFA_ADDRESS];
    if ( attr[IFA_ADDRESS] == 0 ) attr[IFA_ADDRESS] = attr[IFA_LOCAL];
}

/**
 */
void NetLink::AddressMessage::append( int type, const void *data, int bytes ) {
    int length = RTA_LENGTH(bytes);
    struct rtattr *rta = NLMSG_TAIL( &(request.n) );
    rta->rta_type = type;
    rta->rta_len = length;
    memcpy( RTA_DATA(rta), data, bytes );
    request.n.nlmsg_len = NLMSG_ALIGN(request.n.nlmsg_len) + RTA_ALIGN(length);
}

/**
 */
unsigned char NetLink::AddressMessage::family() const { return request.ifa.ifa_family; }
unsigned char NetLink::AddressMessage::prefix() const { return request.ifa.ifa_prefixlen; }
unsigned char NetLink::AddressMessage::flags()  const { return request.ifa.ifa_flags; }
unsigned char NetLink::AddressMessage::scope()  const { return request.ifa.ifa_scope; }
int NetLink::AddressMessage::index()  const { return request.ifa.ifa_index; }

/**
 */
void NetLink::AddressMessage::family( unsigned char fam  ) { request.ifa.ifa_family = fam; }
void NetLink::AddressMessage::prefix( unsigned char bits ) { request.ifa.ifa_prefixlen = bits; }
void NetLink::AddressMessage::flags(  unsigned char set  ) { request.ifa.ifa_flags = set; }
void NetLink::AddressMessage::scope(  unsigned char id   ) { request.ifa.ifa_scope = id; }
void NetLink::AddressMessage::index(  int intf           ) { request.ifa.ifa_index = intf; }

/**
 */
struct in6_addr *
NetLink::AddressMessage::in6_addr() {
    if ( family() != AF_INET6 ) return NULL;
    if ( attr[IFA_ADDRESS] == 0 ) return NULL;
    struct in6_addr *address = (struct in6_addr *)RTA_DATA( attr[IFA_ADDRESS] );
    return address;
}

/**
 * This will need to increase in size to include the RTATTRs
 */
void NetLink::AddressMessage::send( NetLink::Socket &socket ) {
    socket.send( &request, request.n.nlmsg_len );
}

/**
 * This form is used to generate a NewAddress request to send into the kernel.
 *
 * What information must be provided in order to send a NewAddress message?
 * 1) interface
 * 2) address
 *
 * The address family should be selected by different request messagec
 * constructors.  There needs to be different request constructors for
 * v4 vs. v6 addresses.
 */
NetLink::NewAddress::NewAddress( int interface_index, struct in_addr *address )
: AddressMessage(RTM_NEWADDR) {
    request.ifa.ifa_family = AF_INET;
    request.ifa.ifa_index = interface_index;
    // request.ifa.ifa_flags = IFA_F_PERMANENT;
    request.ifa.ifa_scope = RT_SCOPE_LINK;
    request.ifa.ifa_prefixlen = 32;
    append( IFA_LOCAL, address, sizeof(*address) );
    append( IFA_ADDRESS, address, sizeof(*address) );
}

/**
 * This form is used to generate a NewAddress request to send into the kernel.
 *
 * What information must be provided in order to send a NewAddress message?
 * 1) interface
 * 2) address
 *
 * The address family should be selected by different request messagec
 * constructors.  There needs to be different request constructors for
 * v4 vs. v6 addresses.
 */
NetLink::NewAddress::NewAddress( int interface_index, struct in6_addr *address )
: AddressMessage(RTM_NEWADDR) {
    request.ifa.ifa_family = AF_INET6;
    request.ifa.ifa_index = interface_index;
    request.ifa.ifa_scope = RT_SCOPE_LINK;
    request.ifa.ifa_prefixlen = 128;
    append( IFA_LOCAL, address, sizeof(*address) );
    append( IFA_ADDRESS, address, sizeof(*address) );
}

/**
 * This form is used to receive notification of a new address being configured
 * from the kernel.
 *
 * It should have some setting, such that it isn't modified?
 */
NetLink::NewAddress::NewAddress( struct nlmsghdr *hdr )
: AddressMessage(hdr) {
    const char *link_name = (attr[IFA_LABEL] == NULL)
                            ? "NOT PROVIDED"
                            : (const char *)RTA_DATA( attr[IFA_LABEL] );

    char address_buffer[80];
    char *address_string = (char *)inet_ntop( request.ifa.ifa_family, RTA_DATA(attr[IFA_ADDRESS]),
                                      address_buffer, sizeof address_buffer );

    /*
     switch (address_family) {
     case AF_INET:
     case AF_INET6: return inet_ntop(address_family, addr, buf, buflen);
     case AF_IPX: return ipx_ntop(address_family, addr, buf, buflen);
     case AF_DECnet: {
         struct dn_naddr dna = { 2, { 0, 0, }};
         memcpy(dna.a_addr, addr, 2);
         return dnet_ntop(af, &dna, buf, buflen);
     }
     default: return "???";
     }
     */

    if ( debug > 1 ) {
        syslog( LOG_NOTICE, "%s(%d) NewAddress: %s/%d\n", link_name, request.ifa.ifa_index,
                                                 address_string, request.ifa.ifa_prefixlen );
    }
}

/**
 */
bool
NetLink::NewAddress::deliver( NetLink::RouteReceiveCallbackInterface *callback ) {
    callback->receive( this );
    return true;
}

/**
 */
NetLink::RouteMessage *
NetLink::NewAddress::Factory( struct nlmsghdr *hdr ) {
    using namespace NetLink;
    RouteMessage *message = new NewAddress( hdr );
    return message;
}

/**
 */
static int
NewAddress_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace NetLink;
    NewAddress *object = (NewAddress *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(object)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Svc_SetResult( interp, "NetLink::NewAddress", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Svc_SetResult( interp, "send not implemented", TCL_STATIC );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Svc_SetResult( interp, "Unknown command for NewAddress object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
NewAddress_delete( ClientData data ) {
    using namespace NetLink;
    NewAddress *message = (NewAddress *)data;
    delete message;
}

/**
 * Add to this:
 * NetLink::NewAddress foo ip 10.10.220.1/24
 * NetLink::NewAddress foo ip6 fe80::1:2:3:4/64
 */
static int
NewAddress_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR; 
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    using namespace NetLink;

    // \todo add address to arguments...!
    int interface_index = 0;
    struct in6_addr address;

    NewAddress *object = new NewAddress( interface_index, &address );
    Tcl_CreateObjCommand( interp, name, NewAddress_obj, (ClientData)object, NewAddress_delete );
    Svc_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
NetLink::DelAddress::DelAddress() : AddressMessage(RTM_DELADDR) {
}

/**
 */
NetLink::DelAddress::DelAddress( struct nlmsghdr *hdr )
: AddressMessage(hdr) {
    if ( debug > 1 ) {
        printf( "DelAddress: \n" );
    }
}

/**
 */
bool
NetLink::DelAddress::deliver( NetLink::RouteReceiveCallbackInterface *callback ) {
    callback->receive( this );
    return true;
}

/**
 */
NetLink::RouteMessage *
NetLink::DelAddress::Factory( struct nlmsghdr *hdr ) {
    using namespace NetLink;
    RouteMessage *message = new DelAddress(hdr);
    return message;
}

/**
 */
static int
DelAddress_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace NetLink;
    DelAddress *object = (DelAddress *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(object)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Svc_SetResult( interp, "NetLink::DelAddress", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Svc_SetResult( interp, "send not implemented", TCL_STATIC );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Svc_SetResult( interp, "Unknown command for DelAddress object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
DelAddress_delete( ClientData data ) {
    using namespace NetLink;
    DelAddress *message = (DelAddress *)data;
    delete message;
}

/**
 */
static int
DelAddress_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR; 
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    using namespace NetLink;
    DelAddress *object = new DelAddress();
    Tcl_CreateObjCommand( interp, name, DelAddress_obj, (ClientData)object, DelAddress_delete );
    Svc_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
NetLink::GetAddress::GetAddress() : AddressMessage(RTM_GETADDR) { }

/**
 */
NetLink::RouteMessage *
NetLink::GetAddress::Factory( struct nlmsghdr *hdr ) {
    using namespace NetLink;
    RouteMessage *message = new GetAddress();
    return message;
}

/**
 */
static int
GetAddress_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace NetLink;
    GetAddress *object = (GetAddress *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(object)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Svc_SetResult( interp, "NetLink::GetAddress", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Svc_SetResult( interp, "send not implemented", TCL_STATIC );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Svc_SetResult( interp, "Unknown command for GetAddress object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
GetAddress_delete( ClientData data ) {
    using namespace NetLink;
    GetAddress *message = (GetAddress *)data;
    delete message;
}

/**
 */
static int
GetAddress_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR; 
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    using namespace NetLink;
    GetAddress *object = new GetAddress();
    Tcl_CreateObjCommand( interp, name, GetAddress_obj, (ClientData)object, GetAddress_delete );
    Svc_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
NetLink::GetRoute::GetRoute() : RouteMessage(RTM_GETROUTE) {
}

/**
 */
NetLink::RouteMessage *
NetLink::GetRoute::Factory( struct nlmsghdr *hdr ) {
    using namespace NetLink;
    RouteMessage *message = new GetRoute();
    // populate with hdr info... and RTAs
    return message;
}

/**
 */
static int
GetRoute_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace NetLink;
    GetRoute *object = (GetRoute *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(object)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Svc_SetResult( interp, "NetLink::GetRoute", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Svc_SetResult( interp, "send not implemented", TCL_STATIC );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Svc_SetResult( interp, "Unknown command for GetRoute object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
GetRoute_delete( ClientData data ) {
    using namespace NetLink;
    GetRoute *message = (GetRoute *)data;
    delete message;
}

/**
 */
static int
GetRoute_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR; 
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    using namespace NetLink;
    GetRoute *object = new GetRoute();
    Tcl_CreateObjCommand( interp, name, GetRoute_obj, (ClientData)object, GetRoute_delete );
    Svc_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
NetLink::NewRoute::NewRoute() : RouteMessage(RTM_NEWROUTE) {
}

/**
 * This wants to copy the relevant information from the ifinfo
 * struct -- not the whole struct...
 * and needs to get the RTAs as well
 *
 * This means I need to change the Interface constructor as well...
 *
 */
NetLink::NewRoute::NewRoute( struct nlmsghdr *hdr )
: RouteMessage(hdr) {
    rtm = (struct rtmsg *)NLMSG_DATA(hdr);
    int length = hdr->nlmsg_len - NLMSG_LENGTH( sizeof(*rtm) );

    memset( attr, 0, sizeof(attr) );
    // can't I get this from the header directly??
    struct rtattr *ra = RTM_RTA(rtm);
    while ( RTA_OK(ra, length) ) {
        if ( ra->rta_type <= RTA_MAX ) {
            attr[ ra->rta_type ] = ra;
        }
        ra = RTA_NEXT( ra, length );
    }

    if ( length > 0 ) {
        printf( " NR! remnant=%d\n", length );
    }

    if ( debug > 1 ) {
        printf( "NewRoute: => type=%d scope=%d family=%d : iif=%d oif=%d\n",
                rtm->rtm_type, rtm->rtm_scope, rtm->rtm_family,
                (int)RTA_DATA( attr[RTA_IIF] ), (int)RTA_DATA( attr[RTA_OIF] )
        );
    }
}

/**
 */
bool
NetLink::NewRoute::deliver( NetLink::RouteReceiveCallbackInterface *callback ) {
    callback->receive( this );
    return true;
}

/**
 * AF_UNIX, AF_INET, AF_INET6, AF_ROUTE, AF_PACKET
 */
int
NetLink::NewRoute::family() const {
    return (int)rtm->rtm_family;
}

/**
 * RTN_UNICAST, RTN_LOCAL, RTN_BROADCAST, RTN_UNREACHABLE
 */
int
NetLink::NewRoute::route_type() const {
    return (int)rtm->rtm_type;
}

/**
 * RTPROT_REDIRECT, RTPROT_REDIRECT, RTPROT_KERNEL, RTPROT_BOOT, RTPROT_STATIC
 */
int
NetLink::NewRoute::protocol() const {
    return (int)rtm->rtm_protocol;
}

/**
 */
int
NetLink::NewRoute::scope() const {
    return (int)rtm->rtm_scope;
}

/**
 */
NetLink::RouteMessage *
NetLink::NewRoute::Factory( struct nlmsghdr *hdr ) {
    using namespace NetLink;
    RouteMessage *message = new NewRoute( hdr );
    // populate with hdr info... and RTAs
    return message;
}

/**
 */
static int
NewRoute_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace NetLink;
    NewRoute *object = (NewRoute *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(object)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Svc_SetResult( interp, "NetLink::NewRoute", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Svc_SetResult( interp, "send not implemented", TCL_STATIC );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Svc_SetResult( interp, "Unknown command for NewRoute object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
NewRoute_delete( ClientData data ) {
    using namespace NetLink;
    NewRoute *message = (NewRoute *)data;
    delete message;
}

/**
 */
static int
NewRoute_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR; 
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    using namespace NetLink;
    NewRoute *object = new NewRoute();
    Tcl_CreateObjCommand( interp, name, NewRoute_obj, (ClientData)object, NewRoute_delete );
    Svc_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
NetLink::DelRoute::DelRoute() : RouteMessage(RTM_DELROUTE) {
}

/**
 * This wants to copy the relevant information from the ifinfo
 * struct -- not the whole struct...
 * and needs to get the RTAs as well
 *
 * This means I need to change the Interface constructor as well...
 *
 */
NetLink::DelRoute::DelRoute( struct nlmsghdr *hdr )
: RouteMessage(hdr) {
    rtm = (struct rtmsg *)NLMSG_DATA(hdr);
    int length = hdr->nlmsg_len - NLMSG_LENGTH( sizeof(*rtm) );

    memset( attr, 0, sizeof(attr) );
    // can't I get this from the header directly??
    struct rtattr *ra = RTM_RTA(rtm);
    while ( RTA_OK(ra, length) ) {
        if ( ra->rta_type <= RTA_MAX ) {
            attr[ ra->rta_type ] = ra;
        }
        ra = RTA_NEXT( ra, length );
    }

    if ( length > 0 ) {
        printf( " DR! remnant=%d\n", length );
    }

    if ( debug > 1 ) {
        printf( "DelRoute: => type=%d scope=%d family=%d : iif=%d oif=%d\n",
                rtm->rtm_type, rtm->rtm_scope, rtm->rtm_family,
                (int)RTA_DATA( attr[RTA_IIF] ), (int)RTA_DATA( attr[RTA_OIF] )
        );
    }

    /*
     * look at inet_prefix definition in iproute2...
     *
    if ( attr[RTA_DST] != 0 ) {
        switch (rtm->rtm_family) {
        case AF_INET: {
            uint8_t *a = (uint8_t *)attr[RTA_DST];
            printf( "   dst = %d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);
        }
            break;
        case AF_INET6: {
            uint16_t *a = (uint16_t *)attr[RTA_DST];
            printf( "   dst = %x:%x:%x:%x:%x:%x:%x:%x\n", 
                     a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]
            );
        }
            break;
        }
    }
    */
}

/**
 */
bool
NetLink::DelRoute::deliver( NetLink::RouteReceiveCallbackInterface *callback ) {
    callback->receive( this );
    return true;
}

/**
 * AF_UNIX, AF_INET, AF_INET6, AF_ROUTE, AF_PACKET
 */
int
NetLink::DelRoute::family() const {
    return (int)rtm->rtm_family;
}

/**
 * RTN_UNICAST, RTN_LOCAL, RTN_BROADCAST, RTN_UNREACHABLE
 */
int
NetLink::DelRoute::route_type() const {
    return (int)rtm->rtm_type;
}

/**
 * RTPROT_REDIRECT, RTPROT_REDIRECT, RTPROT_KERNEL, RTPROT_BOOT, RTPROT_STATIC
 */
int
NetLink::DelRoute::protocol() const {
    return (int)rtm->rtm_protocol;
}

/**
 */
int
NetLink::DelRoute::scope() const {
    return (int)rtm->rtm_scope;
}

/**
 */
NetLink::RouteMessage *
NetLink::DelRoute::Factory( struct nlmsghdr *hdr ) {
    using namespace NetLink;
    RouteMessage *message = new DelRoute(hdr);
    // populate with hdr info... and RTAs
    return message;
}

/**
 */
static int
DelRoute_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace NetLink;
    DelRoute *object = (DelRoute *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(object)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Svc_SetResult( interp, "NetLink::DelRoute", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Svc_SetResult( interp, "send not implemented", TCL_STATIC );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Svc_SetResult( interp, "Unknown command for DelRoute object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
DelRoute_delete( ClientData data ) {
    using namespace NetLink;
    DelRoute *message = (DelRoute *)data;
    delete message;
}

/**
 */
static int
DelRoute_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR; 
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    using namespace NetLink;
    DelRoute *object = new DelRoute();
    Tcl_CreateObjCommand( interp, name, DelRoute_obj, (ClientData)object, DelRoute_delete );
    Svc_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
NetLink::NeighborMessage::NeighborMessage( uint16_t type ) : RouteMessage(type) { }

/**
 */
NetLink::NeighborMessage::NeighborMessage( struct nlmsghdr *hdr )
: RouteMessage(hdr) {
    message = (struct ndmsg *)NLMSG_DATA(hdr);
    int length = hdr->nlmsg_len - NLMSG_LENGTH( sizeof(*message) );

    memset( attr, 0, sizeof(attr) );
    // can't I get this from the header directly??
    struct rtattr *ra = NDA_RTA(message);
    while ( RTA_OK(ra, length) ) {
        if ( ra->rta_type <= RTA_MAX ) {
            attr[ ra->rta_type ] = ra;
        }
        ra = RTA_NEXT( ra, length );
    }

    if ( length > 0 ) {
        printf( " !!! remnant=%d\n", length );
    }

    if ( debug > 1 ) {
        printf( "NeighborMessage: => type=%d ifindex=%d family=%d\n",
                message->ndm_type, message->ndm_ifindex, message->ndm_family
        );
    }
}

/**
 */
NetLink::NewNeighbor::NewNeighbor() : NeighborMessage(RTM_NEWNEIGH) {
}

/**
 */
NetLink::NewNeighbor::NewNeighbor( struct nlmsghdr *hdr )
: NeighborMessage(hdr) {
    if ( debug > 1 ) {
        printf( "NewNeighbor: \n" );
    }
}

/**
 */
bool
NetLink::NewNeighbor::deliver( NetLink::RouteReceiveCallbackInterface *callback ) {
    callback->receive( this );
    return true;
}

/**
 */
NetLink::RouteMessage *
NetLink::NewNeighbor::Factory( struct nlmsghdr *hdr ) {
    using namespace NetLink;
    RouteMessage *message = new NewNeighbor( hdr );
    return message;
}

/**
 */
static int
NewNeighbor_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace NetLink;
    NewNeighbor *object = (NewNeighbor *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(object)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Svc_SetResult( interp, "NetLink::NewNeighbor", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Svc_SetResult( interp, "send not implemented", TCL_STATIC );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Svc_SetResult( interp, "Unknown command for NewNeighbor object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
NewNeighbor_delete( ClientData data ) {
    using namespace NetLink;
    NewNeighbor *message = (NewNeighbor *)data;
    delete message;
}

/**
 */
static int
NewNeighbor_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR; 
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    using namespace NetLink;
    NewNeighbor *object = new NewNeighbor();
    Tcl_CreateObjCommand( interp, name, NewNeighbor_obj, (ClientData)object, NewNeighbor_delete );
    Svc_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
NetLink::DelNeighbor::DelNeighbor() : NeighborMessage(RTM_DELNEIGH) {
}

/**
 */
NetLink::DelNeighbor::DelNeighbor( struct nlmsghdr *hdr )
: NeighborMessage(hdr) {
    if ( debug > 1 ) {
        printf( "DelNeighbor: \n" );
    }
}

/**
 */
bool
NetLink::DelNeighbor::deliver( NetLink::RouteReceiveCallbackInterface *callback ) {
    callback->receive( this );
    return true;
}

/**
 */
NetLink::RouteMessage *
NetLink::DelNeighbor::Factory( struct nlmsghdr *hdr ) {
    using namespace NetLink;
    RouteMessage *message = new DelNeighbor(hdr);
    return message;
}

/**
 */
static int
DelNeighbor_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace NetLink;
    DelNeighbor *object = (DelNeighbor *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(object)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Svc_SetResult( interp, "NetLink::DelNeighbor", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Svc_SetResult( interp, "send not implemented", TCL_STATIC );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Svc_SetResult( interp, "Unknown command for DelNeighbor object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
DelNeighbor_delete( ClientData data ) {
    using namespace NetLink;
    DelNeighbor *message = (DelNeighbor *)data;
    delete message;
}

/**
 */
static int
DelNeighbor_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR; 
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    using namespace NetLink;
    DelNeighbor *object = new DelNeighbor();
    Tcl_CreateObjCommand( interp, name, DelNeighbor_obj, (ClientData)object, DelNeighbor_delete );
    Svc_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
NetLink::GetNeighbor::GetNeighbor() : NeighborMessage(RTM_GETNEIGH) {
}

/**
 */
NetLink::RouteMessage *
NetLink::GetNeighbor::Factory( struct nlmsghdr *hdr ) {
    using namespace NetLink;
    RouteMessage *message = new GetNeighbor();
    return message;
}

/**
 */
static int
GetNeighbor_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace NetLink;
    GetNeighbor *object = (GetNeighbor *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(object)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Svc_SetResult( interp, "NetLink::GetNeighbor", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Svc_SetResult( interp, "send not implemented", TCL_STATIC );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Svc_SetResult( interp, "Unknown command for GetNeighbor object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
GetNeighbor_delete( ClientData data ) {
    using namespace NetLink;
    GetNeighbor *message = (GetNeighbor *)data;
    delete message;
}

/**
 */
static int
GetNeighbor_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR; 
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    using namespace NetLink;
    GetNeighbor *object = new GetNeighbor();
    Tcl_CreateObjCommand( interp, name, GetNeighbor_obj, (ClientData)object, GetNeighbor_delete );
    Svc_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
void NetLink::RouteResponseHandler::receive( NetLink::NewLink *message ) {
    if ( debug > 1 )  printf( "=> default handler for NewLink\n" );
}

/**
 */
void NetLink::RouteResponseHandler::receive( NetLink::DelLink *message ) {
    if ( debug > 1 )  printf( "=> default handler for DelLink\n" );
}

/**
 */
void NetLink::RouteResponseHandler::receive( NetLink::NewRoute *message ) {
    if ( debug > 1 )  printf( "=> default handler for NewRoute\n" );
}

/**
 */
void NetLink::RouteResponseHandler::receive( NetLink::DelRoute *message ) {
    if ( debug > 1 )  printf( "=> default handler for DelRoute\n" );
}

/**
 */
void NetLink::RouteResponseHandler::receive( NetLink::NewAddress *message ) {
    if ( debug > 1 )  printf( "=> default handler for NewAddress\n" );
}

/**
 */
void NetLink::RouteResponseHandler::receive( NetLink::DelAddress *message ) {
    if ( debug > 1 )  printf( "=> default handler for DelAddress\n" );
}

/**
 */
void NetLink::RouteResponseHandler::receive( NetLink::NewNeighbor *message ) {
    if ( debug > 1 )  printf( "=> default handler for NewNeighbor\n" );
}

/**
 */
void NetLink::RouteResponseHandler::receive( NetLink::DelNeighbor *message ) {
    if ( debug > 1 )  printf( "=> default handler for DelNeighbor\n" );
}

/**
 */
void NetLink::RouteResponseHandler::receive( NetLink::RouteMessage *message ) {
    if ( debug > 1 )  printf( "=> default handler for unknown RouteMessage\n" );
}

/**
 */
void NetLink::RouteResponseHandler::receive( NetLink::RouteError *message ) {
    if ( debug > 1 )  printf( "=> default handler for NetLink RouteError\n" );
    _error = message->error();
}

/**
 */
int NetLink::RouteResponseHandler::error() const { return _error; }

/**
 */
bool NetLink::Initialize( Tcl_Interp *interp ) {
    Tcl_Command command;

    for ( int i = 0 ; i < MAX_RTFACTORY ; i++ ) {
        // This should be a generic route factory, not a simple netlink message
        routeFactories[i] = NetLink::RouteMessage::Factory;
    }
    // set specific message factories
    routeFactories[RTM_NEWLINK]  = NetLink::NewLink::Factory;
    routeFactories[RTM_DELLINK]  = NetLink::DelLink::Factory;
    routeFactories[RTM_GETLINK]  = NetLink::GetLink::Factory;
    routeFactories[RTM_NEWADDR]  = NetLink::NewAddress::Factory;
    routeFactories[RTM_DELADDR]  = NetLink::DelAddress::Factory;
    routeFactories[RTM_GETADDR]  = NetLink::GetAddress::Factory;
    routeFactories[RTM_NEWROUTE] = NetLink::NewRoute::Factory;
    routeFactories[RTM_DELROUTE] = NetLink::DelRoute::Factory;
    routeFactories[RTM_GETROUTE] = NetLink::GetRoute::Factory;
    routeFactories[RTM_NEWNEIGH]  = NetLink::NewNeighbor::Factory;
    routeFactories[RTM_DELNEIGH]  = NetLink::DelNeighbor::Factory;
    routeFactories[RTM_GETNEIGH]  = NetLink::GetNeighbor::Factory;

    Tcl_Namespace *ns = Tcl_CreateNamespace(interp, "NetLink", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }
    if ( Tcl_LinkVar(interp, "NetLink::debug", (char *)&debug, TCL_LINK_INT) != TCL_OK ) {
        syslog( LOG_ERR, "failed to link NetLink::debug" );
        exit( 1 );
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::GetLink", GetLink_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::NewLink", NewLink_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::DelLink", DelLink_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::GetAddress", GetAddress_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::NewAddress", NewAddress_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::DelAddress", DelAddress_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::GetRoute", GetRoute_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::NewRoute", NewRoute_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::DelRoute", DelRoute_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::GetNeighbor", GetNeighbor_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::NewNeighbor", NewNeighbor_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::DelNeighbor", DelNeighbor_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::RouteSocket", RouteSocket_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    return true;
}

/*
 * vim:autoindent
 * vim:expandtab
 */
