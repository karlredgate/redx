
/*
 * Copyright (c) 2012-2021 Karl N. Redgate
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

/** \file TCL_NetLink.cc
 * \brief 
 *
 */

#include <sys/types.h>
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

#include <tcl.h>
#include "tcl_util.h"

#include "logger.h"
#include "NetLink.h"

#include "AppInit.h"

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
        Tcl_StaticSetResult( interp, "NetLink::RouteSocket" );
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
    Tcl_StaticSetResult( interp, "Unknown command for RouteSocket object" );
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
        Tcl_StaticSetResult( interp, "NetLink::GetLink" );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Tcl_StaticSetResult( interp, "send not implemented" );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Tcl_StaticSetResult( interp, "Unknown command for GetLink object" );
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
        Tcl_StaticSetResult( interp, "NetLink::NewLink" );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Tcl_StaticSetResult( interp, "send not implemented" );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Tcl_StaticSetResult( interp, "Unknown command for NewLink object" );
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
        Tcl_StaticSetResult( interp, "NetLink::DelLink" );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Tcl_StaticSetResult( interp, "send not implemented" );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Tcl_StaticSetResult( interp, "Unknown command for DelLink object" );
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
        Tcl_StaticSetResult( interp, "NetLink::NewAddress" );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Tcl_StaticSetResult( interp, "send not implemented" );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Tcl_StaticSetResult( interp, "Unknown command for NewAddress object" );
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
        Tcl_StaticSetResult( interp, "NetLink::DelAddress" );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Tcl_StaticSetResult( interp, "send not implemented" );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Tcl_StaticSetResult( interp, "Unknown command for DelAddress object" );
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
        Tcl_StaticSetResult( interp, "NetLink::GetAddress" );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Tcl_StaticSetResult( interp, "send not implemented" );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Tcl_StaticSetResult( interp, "Unknown command for GetAddress object" );
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
        Tcl_StaticSetResult( interp, "NetLink::GetRoute" );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Tcl_StaticSetResult( interp, "send not implemented" );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Tcl_StaticSetResult( interp, "Unknown command for GetRoute object" );
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
        Tcl_StaticSetResult( interp, "NetLink::NewRoute" );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Tcl_StaticSetResult( interp, "send not implemented" );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Tcl_StaticSetResult( interp, "Unknown command for NewRoute object" );
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
        Tcl_StaticSetResult( interp, "NetLink::DelRoute" );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Tcl_StaticSetResult( interp, "send not implemented" );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Tcl_StaticSetResult( interp, "Unknown command for DelRoute object" );
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
        Tcl_StaticSetResult( interp, "NetLink::NewNeighbor" );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Tcl_StaticSetResult( interp, "send not implemented" );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Tcl_StaticSetResult( interp, "Unknown command for NewNeighbor object" );
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
        Tcl_StaticSetResult( interp, "NetLink::DelNeighbor" );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Tcl_StaticSetResult( interp, "send not implemented" );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Tcl_StaticSetResult( interp, "Unknown command for DelNeighbor object" );
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
        Tcl_StaticSetResult( interp, "NetLink::GetNeighbor" );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "send") ) {
        Tcl_StaticSetResult( interp, "send not implemented" );
        return TCL_ERROR;

        Tcl_ResetResult( interp );
        return TCL_OK;
    }
    Tcl_StaticSetResult( interp, "Unknown command for GetNeighbor object" );
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
static bool
NetLink_Module( Tcl_Interp *interp ) {
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
        log_err( "failed to link NetLink::debug" );
        exit( 1 );
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::GetLink", GetLink_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::NewLink", NewLink_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::DelLink", DelLink_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::GetAddress", GetAddress_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::NewAddress", NewAddress_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::DelAddress", DelAddress_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::GetRoute", GetRoute_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::NewRoute", NewRoute_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::DelRoute", DelRoute_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::GetNeighbor", GetNeighbor_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::NewNeighbor", NewNeighbor_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::DelNeighbor", DelNeighbor_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "NetLink::RouteSocket", RouteSocket_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    return true;
}

app_init( NetLink_Module );

/* vim: set autoindent expandtab sw=4 : */
