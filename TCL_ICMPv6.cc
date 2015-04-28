
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

/** \file TCL_ICMPv6.cc
 * \brief 
 *
 */

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <tcl.h>
#include "tcl_util.h"

#include "logger.h"
#include "string_util.h"

#include "ICMPv6.h"
#include "AppInit.h"

namespace {
    int debug = 0;
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
static bool
ICMPv6_Module( Tcl_Interp *interp ) {
    Tcl_Command command;

    Tcl_Namespace *ns = Tcl_CreateNamespace(interp, "ICMPv6", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    if ( Tcl_LinkVar(interp, "ICMPv6::debug", (char *)&debug, TCL_LINK_INT) != TCL_OK ) {
        log_err( "failed to link ICMPv6::debug" );
        exit( 1 );
    }

    command = Tcl_CreateObjCommand(interp, "ICMPv6::EchoRequest", EchoRequest_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "ICMPv6::EchoReply", EchoReply_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "ICMPv6::NeighborSolicitation", NeighborSolicitation_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "ICMPv6::NeighborAdvertisement", NeighborAdvertisement_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "ICMPv6::Socket", Socket_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    return true;
}

app_init( ICMPv6_Module );

/* vim: set autoindent expandtab sw=4 : */
