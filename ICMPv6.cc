
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

/** \file ICMPv6.cc
 * \brief 
 *
 */

#include <stdint.h>
#include <sys/socket.h>
#if 0
### IS this really necessary for linux - use net/
#include <linux/if_arp.h>  // for ARPHRD_*
#endif
#include <net/if_arp.h>  // for ARPHRD_*
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>

#include <tcl.h>
#include "TCL_Fixup.h"

#include "ICMPv6.h"

namespace {
    typedef ICMPv6::PDU *(*PDUFactory)( struct icmp6_hdr * );
    static const int MAX_FACTORY = 256; // ICMP type is 8 bits
    PDUFactory factories[ MAX_FACTORY ];
    int debug = 0;
}

/**
 */
ICMPv6::PDU *
ICMPv6::PDU::Factory( struct icmp6_hdr *hdr ) {
    using namespace ICMPv6;
    return new PDU( hdr->icmp6_type );
}

/**
 * \todo change ICMPv6 socket to accept address/port/...
 */
ICMPv6::Socket::Socket() : bind_completed(false), binds_attempted(0) {
    pthread_mutex_init( &lock, NULL );
    memset( &binding, 0, sizeof(binding) );
    binding.sin6_family = AF_INET6;

    socket = ::socket( AF_INET6, SOCK_RAW, IPPROTO_ICMPV6 );
    if ( socket == -1 ) {
	syslog( LOG_ERR, "ICMPv6 socket() failed, %s", strerror(errno) );
        // This should change to it having an error state method/member that
        // can be queried, OR have it raise an exception
        ::exit( 1 );
    }

    int size = 64 * 1024;
    setsockopt( socket, SOL_SOCKET, SO_RCVBUF, &size, sizeof size );

    unsigned int flag = 0;
    int result = setsockopt(socket, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &flag, sizeof(flag));
    if ( result < 0 ) {
        syslog( LOG_ERR, "ICMPv6::Socket: failed to turn off loopback" );
    }

    if ( fcntl(socket, F_SETFD, FD_CLOEXEC) < 0 ) {
        syslog( LOG_ERR, "ICMPv6::Socket: could not set close on exec" );
    }

    struct timeval tv;
    tv.tv_sec = 60;
    tv.tv_usec = 0;
    result = setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if ( result < 0 ) {
        syslog( LOG_ERR, "ICMPv6::Socket: could not set receive timeout" );
    }

    if ( debug > 1 ) syslog( LOG_NOTICE, "ICMPv6 socket (%d)", socket );
}

/**
 * \todo change ICMPv6 socket to accept address/port/...
 */
ICMPv6::Socket::~Socket() {
    close( socket );
}

#if 0
  COMMENTED OUT UNTIL I NEED A PROMISC ICMP SOCK
/**
 */
bool ICMPv6::Socket::bind( uint32_t scope ) {
    // bind to IN6ADDR_ANY_INIT and port 0, but to the interface index
    memset( &binding, 0, sizeof(binding) );
    binding.sin6_family = AF_INET6;
    binding.sin6_addr = in6addr_any;
    binding.sin6_port = htons(0);
    binding.sin6_scope_id = scope;

    if ( ::bind(socket, (struct sockaddr *)&binding, sizeof binding) == -1 ) {
	syslog( LOG_ERR, "ICMPv6 bind() failed, %s", strerror(errno) );
        return false;
    }

    if ( setsockopt(socket, IPPROTO_IPV6, IPV6_MULTICAST_IF, &scope, sizeof(scope)) < 0 ) {
        printf( "cannot set if\n" );
        exit( 1 );
    }
    syslog( LOG_NOTICE, "ICMP socket (%d) bound to ANY for intf %d\n", socket, scope );
    bind_completed = true;

    return true;
}
#endif

/**
 */
bool ICMPv6::Socket::bind( struct in6_addr *address, uint32_t scope ) {
    if ( bind_completed == true ) return true;

    memset( &binding, 0, sizeof(binding) );
    binding.sin6_family = AF_INET6;
    binding.sin6_addr = *address;
    // memcpy( &binding.sin6_addr, address, sizeof(struct in6_addr) );
    binding.sin6_port = htons(0);
    binding.sin6_scope_id = scope;

    int result = 0;
    int error  = 0;
    pthread_mutex_lock( &lock );
    if ( bind_completed == false ) {
        result = ::bind(socket, (struct sockaddr *)&binding, sizeof binding);
        error = errno;
        if ( result == 0 ) bind_completed = true;
    }
    pthread_mutex_unlock( &lock );

    char *address_name, buffer[80];
    if ( result < 0 ) {
        char e[128];
        int err = strerror_r( error, e, sizeof(e) );
        address_name = (char *)inet_ntop(AF_INET6, address, buffer, sizeof buffer);
        if ( (binds_attempted++ % 30) == 0 ) {
            syslog( LOG_ERR, "ICMPv6::Socket::bind(\"%s\",%d): %s", address_name, scope, e );
        }
        return false;
    }
    address_name = (char *)inet_ntop(AF_INET6, address, buffer, sizeof buffer);
    if ( debug > 1 ) {
        syslog( LOG_NOTICE, "ICMPv6 socket %d bound to %s (scope:%d)", socket, address_name, scope );
    }
    binds_attempted = 0;

    return true;
}

/**
 */
bool ICMPv6::Socket::send( struct in6_addr *address, void *message, int length ) {
    int result;
    struct sockaddr_in6 recipient;
    memset( &recipient, 0, sizeof recipient );
    recipient.sin6_family = AF_INET6;
    recipient.sin6_addr = *address;
    // memcpy( &recipient.sin6_addr, address, sizeof(struct in6_addr) );
    recipient.sin6_port = htons(0);
    recipient.sin6_scope_id = binding.sin6_scope_id;
    result = sendto( socket, message, length, 0, (struct sockaddr *)&recipient, sizeof(recipient) );
    if ( result < 0 ) {
        char e[128], s[80];
        int err = strerror_r( errno, e, sizeof(e) );
        const char *addr = inet_ntop(AF_INET6, address, s, sizeof s);
        syslog( LOG_ERR, "ICMPv6::Socket::send(\"%s\"): %s", addr, e );
        return false;
    }
    return true;
}

/**
 * This should probably be a send to the broadcast address
 */
bool ICMPv6::Socket::send( void *message, int length ) {
    int result;
    if ( debug > 2 ) syslog( LOG_INFO, "ICMPv6::Socket::send()" );
    result = sendto( socket, message, length, 0, (struct sockaddr *)&binding, sizeof(binding) );
    if ( result < 0 ) {
        return false;
    }
    return true;
}

/**
 * this is the generic socket receive ... override this so the rt socket can construct
 * route objects
 *
 * when receiving and reporting to TCL connection, need to translate to
 * a Tcl_Obj -- 
 */
void ICMPv6::Socket::receive( ICMPv6::ReceiveCallbackInterface *callback ) {
    struct sockaddr_in6 sender;
    socklen_t sender_size;
    char buffer[2048]; // only needs to be 1544? -- might want to size this to MTU

    for (;;) {
        sender_size = sizeof sender;
        int n = recvfrom(socket, buffer, sizeof buffer, 0, (struct sockaddr *)&sender, &sender_size);
        if ( n < 0 ) {
	  if ( errno != ETIMEDOUT) syslog( LOG_ERR, "ICMPv6 recvfrom() failed, %s", strerror(errno) );
            return; // recv error start over
        }
        if ( n == 0 ) continue;

        // construct a message and deliver it
        struct icmp6_hdr *header = (struct icmp6_hdr *)buffer;

        using namespace ICMPv6;
        PDUFactory factory = factories[ header->icmp6_type ];
        if ( factory == NULL ) {
            syslog( LOG_ERR, "ICMPv6::Socket: No FACTORY for ICMPv6 message" );
            continue;
        }
        PDU *pdu = factory( header );
        pdu->deliver( callback );
        delete pdu;
    }
}

/**
 */
ICMPv6::PDU::PDU( uint8_t pdu_type, uint8_t pdu_code ) {
    memset( &header, 0, sizeof header );
    header.icmp6_type = pdu_type;
    header.icmp6_code = pdu_code;
}

/**
 */
ICMPv6::PDU::PDU( struct icmp6_hdr *recv_header ) {
    memcpy( &header, recv_header, sizeof header );
}

/**
 */
bool
ICMPv6::PDU::deliver( ICMPv6::ReceiveCallbackInterface *callback ) {
    callback->receive( this );
    return true;
}

/**
 */
bool ICMPv6::PDU::send( ICMPv6::Socket &socket, struct in6_addr *recipient ) {
    socket.send( recipient, &header, sizeof header );
    return true;
}

/**
 */
ICMPv6::EchoRequest::EchoRequest() : PDU(ICMP6_ECHO_REQUEST) {
}

/**
 */
ICMPv6::EchoRequest::EchoRequest( struct icmp6_hdr *header ) : PDU(header) { }

/**
 */
bool
ICMPv6::EchoRequest::send( ICMPv6::Socket& socket, struct in6_addr *recipient ) {
    socket.send( recipient, &header, sizeof header );
    return true;
}

/**
 */
bool
ICMPv6::EchoRequest::deliver( ICMPv6::ReceiveCallbackInterface *callback ) {
    callback->receive( this );
    return true;
}

/**
 */
ICMPv6::PDU *
ICMPv6::EchoRequest::Factory( struct icmp6_hdr *hdr ) {
    using namespace ICMPv6;
    PDU *message = new EchoRequest();
    // populate with hdr info... and RTAs
    return message;
}

/**
 */
static int
EchoRequest_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace ICMPv6;
    EchoRequest *object = (EchoRequest *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(object)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
      Tcl_SetResult( interp, (char *)"ICMPv6::EchoRequest", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
      Tcl_SetResult( interp, (char *)"send not implemented", TCL_STATIC );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Tcl_SetResult( interp, (char *)"Unknown command for EchoRequest object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
EchoRequest_delete( ClientData data ) {
    using namespace ICMPv6;
    EchoRequest *message = (EchoRequest *)data;
    delete message;
}

/**
 */
static int
EchoRequest_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR; 
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    using namespace ICMPv6;
    EchoRequest *object = new EchoRequest();
    Tcl_CreateObjCommand( interp, name, EchoRequest_obj, (ClientData)object, EchoRequest_delete );
    Tcl_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
ICMPv6::EchoReply::EchoReply() : PDU(ICMP6_ECHO_REPLY) {
}

/**
 */
ICMPv6::EchoReply::EchoReply( struct icmp6_hdr *header ) : PDU(header) { }

/**
 */
bool
ICMPv6::EchoReply::send( ICMPv6::Socket& socket, struct in6_addr *recipient ) {
    socket.send( recipient, &header, sizeof header );
    return true;
}

/**
 */
bool
ICMPv6::EchoReply::deliver( ICMPv6::ReceiveCallbackInterface *callback ) {
    callback->receive( this );
    return true;
}

/**
 */
ICMPv6::PDU *
ICMPv6::EchoReply::Factory( struct icmp6_hdr *hdr ) {
    using namespace ICMPv6;
    PDU *message = new EchoReply();
    // populate with hdr info... and RTAs
    return message;
}

/**
 */
static int
EchoReply_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace ICMPv6;
    EchoReply *object = (EchoReply *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(object)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
      Tcl_SetResult( interp, (char *)"ICMPv6::EchoReply", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
      Tcl_SetResult( interp, (char *)"send not implemented", TCL_STATIC );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Tcl_SetResult( interp, (char *)"Unknown command for EchoReply object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
EchoReply_delete( ClientData data ) {
    using namespace ICMPv6;
    EchoReply *message = (EchoReply *)data;
    delete message;
}

/**
 */
static int
EchoReply_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR; 
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    using namespace ICMPv6;
    EchoReply *object = new EchoReply();
    Tcl_CreateObjCommand( interp, name, EchoReply_obj, (ClientData)object, EchoReply_delete );
    Tcl_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
ICMPv6::NeighborSolicitation::NeighborSolicitation() : PDU(ND_NEIGHBOR_SOLICIT) {
    memset( &pdu, 0, sizeof pdu );
    pdu.nd_ns_hdr.icmp6_type = ND_NEIGHBOR_SOLICIT;
    pdu.nd_ns_hdr.icmp6_code = 0;
}

/**
 */
ICMPv6::NeighborSolicitation::NeighborSolicitation( struct icmp6_hdr *header )
: PDU(ND_NEIGHBOR_SOLICIT) {
    memcpy( &pdu, header, sizeof pdu );
}

/**
 */
bool
ICMPv6::NeighborSolicitation::send( ICMPv6::Socket& socket, struct in6_addr *recipient ) {
    socket.send( recipient, &pdu, sizeof pdu );
    return true;
}

/**
 */
bool
ICMPv6::NeighborSolicitation::deliver( ICMPv6::ReceiveCallbackInterface *callback ) {
    callback->receive( this );
    return true;
}

/**
 */
const struct in6_addr *
ICMPv6::NeighborSolicitation::target() const {
    return &(pdu.nd_ns_target);
}

/**
 */
ICMPv6::PDU *
ICMPv6::NeighborSolicitation::Factory( struct icmp6_hdr *hdr ) {
    using namespace ICMPv6;
    PDU *message = new NeighborSolicitation( hdr );
    // populate with hdr info... and RTAs
    return message;
}

/**
 */
static int
NeighborSolicitation_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace ICMPv6;
    NeighborSolicitation *object = (NeighborSolicitation *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(object)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Tcl_SetResult( interp, (char *)"ICMPv6::NeighborSolicitation", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Tcl_SetResult( interp, (char *)"send not implemented", TCL_STATIC );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Tcl_SetResult( interp, (char *)"Unknown command for NeighborSolicitation object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
NeighborSolicitation_delete( ClientData data ) {
    using namespace ICMPv6;
    NeighborSolicitation *message = (NeighborSolicitation *)data;
    delete message;
}

/**
 */
static int
NeighborSolicitation_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR; 
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    using namespace ICMPv6;
    NeighborSolicitation *object = new NeighborSolicitation();
    Tcl_CreateObjCommand( interp, name, NeighborSolicitation_obj, (ClientData)object, NeighborSolicitation_delete );
    Tcl_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
ICMPv6::NeighborAdvertisement::NeighborAdvertisement( struct in6_addr *target, unsigned char *mac )
: PDU(ND_NEIGHBOR_ADVERT) {
    memset( &pdu, 0, sizeof pdu );
    pdu.na.nd_na_type = ND_NEIGHBOR_ADVERT;
    pdu.na.nd_na_code = 0;
    pdu.na.nd_na_flags_reserved = ND_NA_FLAG_OVERRIDE;
    pdu.na.nd_na_target = *target;
    pdu.opt.nd_opt_type = ND_OPT_TARGET_LINKADDR;
    pdu.opt.nd_opt_len = 1;
    pdu.mac[0] = mac[0];
    pdu.mac[1] = mac[1];
    pdu.mac[2] = mac[2];
    pdu.mac[3] = mac[3];
    pdu.mac[4] = mac[4];
    pdu.mac[5] = mac[5];
}

/**
 */
ICMPv6::NeighborAdvertisement::NeighborAdvertisement( struct icmp6_hdr *header )
: PDU(ND_NEIGHBOR_ADVERT) {
    memcpy( &pdu, header, sizeof pdu );
}

/**
 */
struct in6_addr *
ICMPv6::NeighborAdvertisement::target() { return &pdu.na.nd_na_target; }

/**
 */
void
ICMPv6::NeighborAdvertisement::target( struct in6_addr *target ) {
    pdu.na.nd_na_target = *target;
}

/**
 */
bool
ICMPv6::NeighborAdvertisement::send( ICMPv6::Socket& socket, struct in6_addr *recipient ) {
    return socket.send( recipient, &pdu, sizeof pdu );
}

/**
 */
bool
ICMPv6::NeighborAdvertisement::deliver( ICMPv6::ReceiveCallbackInterface *callback ) {
    callback->receive( this );
    return true;
}

/**
 */
ICMPv6::PDU *
ICMPv6::NeighborAdvertisement::Factory( struct icmp6_hdr *hdr ) {
    using namespace ICMPv6;
    PDU *message = new NeighborAdvertisement( hdr );
    // populate with hdr info... and RTAs
    return message;
}

/**
 */
static int
NeighborAdvertisement_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace ICMPv6;
    NeighborAdvertisement *object = (NeighborAdvertisement *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(object)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Tcl_SetResult( interp, (char *)"ICMPv6::NeighborAdvertisement", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Tcl_SetResult( interp, (char *)"send not implemented", TCL_STATIC );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "target") ) {
        if ( objc == 2 ) {
            // return target
            char s[80];
            const char *target = inet_ntop(AF_INET6, object->target(), s, sizeof s);
            Tcl_SetResult( interp, (char *)target, TCL_VOLATILE );
            return TCL_OK;
        }
        if ( objc > 3 ) {
            Tcl_ResetResult( interp );
            Tcl_WrongNumArgs( interp, 1, objv, "target [address]" );
            return TCL_ERROR; 
        }
        char *address_string = Tcl_GetStringFromObj( objv[2], NULL );
        struct in6_addr address;
        inet_pton( AF_INET6, address_string, &address );
        object->target( &address );
        Tcl_SetObjResult( interp, objv[2] );
        return TCL_OK;
    }

    Tcl_SetResult( interp, (char *)"Unknown command for NeighborAdvertisement object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
NeighborAdvertisement_delete( ClientData data ) {
    using namespace ICMPv6;
    NeighborAdvertisement *message = (NeighborAdvertisement *)data;
    delete message;
}

/**
 */
static int
NeighborAdvertisement_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 3 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name address" );
        return TCL_ERROR; 
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    char *address_string = Tcl_GetStringFromObj( objv[2], NULL );
    struct in6_addr address;
    inet_pton( AF_INET6, address_string, &address );

    /// \todo add MAC address to TCL command for NA msg creation
    unsigned char mac[6];

    using namespace ICMPv6;
    NeighborAdvertisement *object = new NeighborAdvertisement( &address, mac );
    Tcl_CreateObjCommand( interp, name, NeighborAdvertisement_obj, (ClientData)object, NeighborAdvertisement_delete );
    Tcl_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 * \todo add a "bind" sommand to the socket -- accepting either an int or a string
 */
static int
Socket_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace ICMPv6;
    Socket *socket = (Socket *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(socket)) );
        return TCL_OK;
    }
    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Tcl_SetResult( interp, (char *)"ICMPv6::Socket", TCL_STATIC );
        return TCL_OK;
    }

    /**
     * Arguement is a message
     */
    if ( Tcl_StringMatch(command, "send") ) {
        if ( objc != 3 ) {
            Tcl_ResetResult( interp );
            Tcl_WrongNumArgs( interp, 1, objv, "send message recipient" );
            return TCL_ERROR;
        }

        PDU *pdu;
        void *p = (void *)&(pdu);
        if ( Tcl_GetLongFromObj(interp,objv[2],(long*)p) != TCL_OK ) {
            return TCL_ERROR;
        }

        char *address_string = Tcl_GetStringFromObj( objv[3], NULL );
        struct in6_addr address;
        inet_pton( AF_INET6, address_string, &address );

        pdu->send( *socket, &address );
        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Tcl_SetResult( interp, (char *)"Unknown command for Socket object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
Socket_delete( ClientData data ) {
    using namespace ICMPv6;
    Socket *socket = (Socket *)data;
    delete socket;
}

/**
 */
static int
Socket_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR;
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );

    using namespace ICMPv6;
    Socket *object = new Socket();
    // should have a delete proc also
    Tcl_CreateObjCommand( interp, name, Socket_obj, (ClientData)object, Socket_delete );
    Tcl_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
bool ICMPv6::Initialize( Tcl_Interp *interp ) {
    Tcl_Command command;

    for ( int i = 0 ; i < MAX_FACTORY ; i++ ) {
        // This should be a generic route factory, not a simple netlink message
        factories[i] = ICMPv6::PDU::Factory;
    }
    // set specific message factories
    factories[ICMP6_ECHO_REQUEST]  = ICMPv6::EchoRequest::Factory;
    factories[ICMP6_ECHO_REPLY]    = ICMPv6::EchoReply::Factory;
    factories[ND_NEIGHBOR_SOLICIT] = ICMPv6::NeighborSolicitation::Factory;
    factories[ND_NEIGHBOR_ADVERT]  = ICMPv6::NeighborAdvertisement::Factory;

    Tcl_Namespace *ns = Tcl_CreateNamespace(interp, "ICMPv6", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    if ( Tcl_LinkVar(interp, "ICMPv6::debug", (char *)&debug, TCL_LINK_INT) != TCL_OK ) {
        syslog( LOG_ERR, "failed to link ICMPv6::debug" );
        exit( 1 );
    }

    command = Tcl_CreateObjCommand(interp, "ICMPv6::EchoRequest", EchoRequest_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "ICMPv6::EchoReply", EchoReply_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "ICMPv6::NeighborSolicitation", NeighborSolicitation_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "ICMPv6::NeighborAdvertisement", NeighborAdvertisement_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "ICMPv6::Socket", Socket_cmd, (ClientData)0, NULL);
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
