
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

/** \file UUID_Module.cc
 * \brief 
 *
 */

#include <stdlib.h>

#include <tcl.h>
#include "tcl_util.h"

#include "logger.h"
#include "xuid.h"
#include "UUID.h"
#include "AppInit.h"

namespace { int debug = 0; }

/**
 */
static int
UUID_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    UUID *uuid = (UUID *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(uuid)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Svc_SetResult( interp, "UUID", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "data") ) {
        Tcl_Obj *obj = Tcl_NewByteArrayObj( uuid->raw(), 16 );
        Tcl_SetObjResult( interp, obj );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "string") ) {
        Tcl_Obj *obj = Tcl_NewStringObj( uuid->to_s(), -1 );
        Tcl_SetObjResult( interp, obj );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "set") ) {
        if ( objc < 3 ) {
            Tcl_ResetResult( interp );
            Tcl_WrongNumArgs( interp, 1, objv, "set value" );
            return TCL_ERROR;
        }
        char *uuid_string = Tcl_GetStringFromObj( objv[2], NULL );
        uuid->set( uuid_string );
        Tcl_Obj *obj = Tcl_NewStringObj( uuid->to_s(), -1 );
        Tcl_SetObjResult( interp, obj );
        return TCL_OK;
    }

    uint8_t *x = uuid->raw();
    if ( Tcl_StringMatch(command, "dump") ) {
        printf( "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
                x[0], x[1],  x[2],  x[3],  x[4],  x[5],  x[6],  x[7],
                x[8], x[9], x[10], x[11], x[12], x[13], x[14], x[15] );
        Tcl_ResetResult( interp );
        return TCL_OK;
    }

    Svc_SetResult( interp, "Unknown command for UUID object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
UUID_delete( ClientData data ) {
    UUID *uuid = (UUID *)data;
    delete uuid;
}

/**
 */
static int
UUID_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc < 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name [value]" );
        return TCL_ERROR;
    }
    char *name = Tcl_GetStringFromObj( objv[1], NULL );

    if ( objc == 2 ) {
        UUID *object = new UUID();
        Tcl_CreateObjCommand( interp, name, UUID_obj, (ClientData)object, UUID_delete );
        Svc_SetResult( interp, name, TCL_VOLATILE );
        return TCL_OK;
    }

    if ( objc != 3 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name value" );
        return TCL_ERROR;
    }
    char *uuid_string = Tcl_GetStringFromObj( objv[2], NULL );
    UUID *object = new UUID( uuid_string );
    Tcl_CreateObjCommand( interp, name, UUID_obj, (ClientData)object, UUID_delete );
    Svc_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
static bool
UUID_Module( Tcl_Interp *interp ) {
    Tcl_Command command;

    Tcl_Namespace *ns = Tcl_CreateNamespace(interp, "UUID", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    if ( Tcl_LinkVar(interp, "UUID::debug", (char *)&debug, TCL_LINK_INT) != TCL_OK ) {
        log_err( "failed to link ICMPv6::debug" );
        exit( 1 );
    }

    command = Tcl_CreateObjCommand(interp, "UUID", UUID_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        return false;
    }

    return true;
}

app_init( UUID_Module );

/* vim: set autoindent expandtab sw=4 syntax=c : */
