
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

/** \file Key_Module.cc
 * \brief 
 *
 */

#include <stdlib.h>

#include <tcl.h>
#include "tcl_util.h"

#include "logger.h"
#include "IntKey.h"
#include "CStringKey.h"
#include "AppInit.h"

namespace { int debug = 0; }

/**
 */
static int
IntKey_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    IntKey *key = (IntKey *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(key)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Tcl_StaticSetResult( interp, "Key::IntKey" );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "reference") ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(key)) );
        return TCL_OK;
    }

    /*
    if ( Tcl_StringMatch(command, "value") ) {
        Tcl_SetObjResult( interp, Tcl_NewIntObj((long)(key->value)) );
        Tcl_SetObjResult( interp, obj );
        return TCL_OK;
    }
    */

    if ( Tcl_StringMatch(command, "compare") ) {
        if ( objc < 3 ) {
            Tcl_ResetResult( interp );
            Tcl_WrongNumArgs( interp, 1, objv, "compare that" );
            return TCL_ERROR;
        }
        IntKey *that;
        if ( Tcl_GetLongFromObj(interp,objv[2],(long*)that) != TCL_OK ) {
            Svc_SetResult( interp, "invalid key reference", TCL_STATIC );
            return TCL_ERROR;
        }

        int difference = key->compare( that );
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(difference)) );
        return TCL_OK;
    }

    Tcl_StaticSetResult( interp, "Unknown command for IntKey object" );
    return TCL_ERROR;
}

/**
 */
static void
IntKey_delete( ClientData data ) {
    IntKey *key = (IntKey *)data;
    delete key;
}

/**
 */
static int
IntKey_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 3 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name value" );
        return TCL_ERROR;
    }
    char *name = Tcl_GetStringFromObj( objv[1], NULL );

    int value;
    if ( Tcl_GetIntFromObj(interp,objv[2],&value) != TCL_OK ) {
        Svc_SetResult( interp, "invalid key value", TCL_STATIC );
        return TCL_ERROR;
    }

    IntKey *object = new IntKey( value );
    Tcl_CreateObjCommand( interp, name, IntKey_obj, (ClientData)object, IntKey_delete );
    Svc_SetResult( interp, name, TCL_VOLATILE );

    return TCL_OK;
}

/**
 */
static bool
Key_Module( Tcl_Interp *interp ) {
    Tcl_Command command;

    Tcl_Namespace *ns = Tcl_CreateNamespace(interp, "Key", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    if ( Tcl_LinkVar(interp, "Key::debug", (char *)&debug, TCL_LINK_INT) != TCL_OK ) {
        log_err( "failed to link ICMPv6::debug" );
        exit( 1 );
    }

    command = Tcl_CreateObjCommand(interp, "Key::IntKey", IntKey_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        return false;
    }

    return true;
}

app_init( Key_Module );

/* vim: set autoindent expandtab sw=4 syntax=c : */
