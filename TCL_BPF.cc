
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

/** \file TCL_Interface.cc
 * \brief 
 *
 */

#include <stdint.h>
#include <stdlib.h> // for exit() - this one is wrong I think
#include <unistd.h> // for exit() - this one is for linux

#include <arpa/inet.h> // for inet_ntop()

#include "logger.h"

#include "tcl_util.h"
#include "AppInit.h"

namespace {
    int debug = 0;
}
/**
 */
static int
program_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    // using namespace Network;
    // Interface *interface = (Interface *)data;

    if ( objc == 1 ) {
        // Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(interface)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Tcl_StaticSetResult( interp, "Network::Interface" );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "create") ) {
        const char *result = NULL; // interface->create();
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Tcl_SetResult( interp, (char *)result, TCL_STATIC );
        return TCL_ERROR;
    }

    /**
     */
    if ( Tcl_StringMatch(command, "rename") ) {
        if ( objc != 3 ) {
            Tcl_ResetResult( interp );
            Tcl_WrongNumArgs( interp, 1, objv, "rename new_name" );
            return TCL_ERROR;
        }
        char *newname = Tcl_GetStringFromObj( objv[2], NULL );
        bool result = false; // interface->rename( newname );
        Tcl_SetObjResult( interp, Tcl_NewBooleanObj(result) );
        return TCL_ERROR;
    }

    /**
     */
    if ( Tcl_StringMatch(command, "negotiate") ) {
        bool result = false; // interface->negotiate();
        Tcl_SetObjResult( interp, Tcl_NewBooleanObj(result) );
        if ( result ) return TCL_OK;
        return TCL_ERROR;
    }

    /**
     */
    if ( Tcl_StringMatch(command, "destroy") ) {
        const char *result = NULL; // interface->destroy();
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Tcl_SetResult( interp, (char *)result, TCL_STATIC );
        return TCL_ERROR;
    }

    Tcl_StaticSetResult( interp, "Unknown command for BPF object" );
    return TCL_ERROR;
}

/**
 */
#if 0
static void
Interface_delete( ClientData data ) {
    using namespace Network;
    Interface *message = (Interface *)data;
    delete message;
}
#endif

/**
 */
static int
program_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR;
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    // using namespace Network;
    // Interface *object = new Interface( interp, name );
    int object = 3; // this should be a ptr to BPF object
    // Tcl_CreateObjCommand( interp, name, program_obj, (ClientData)object, Interface_delete );
    Tcl_CreateObjCommand( interp, name, program_obj, (ClientData)object, 0 );
    Tcl_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
static int
load_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR;
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    // using namespace Network;
    // Interface *object = new Interface( interp, name );
    // Tcl_CreateObjCommand( interp, name, Interface_obj, (ClientData)object, Interface_delete );
    Tcl_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
bool BPF_Module( Tcl_Interp *interp ) {

    Tcl_Command command;

    Tcl_Namespace *ns = Tcl_CreateNamespace(interp, "BPF", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    if ( Tcl_LinkVar(interp, "BPF::debug", (char *)&debug, TCL_LINK_INT) != TCL_OK ) {
        log_err( "failed to link BPF::debug" );
        _exit( 1 );
    }

    command = Tcl_CreateObjCommand(interp, "BPF::program", program_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "BPF::load", load_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    return true;
}

app_init( BPF_Module );

/* vim: set autoindent expandtab sw=4 : */
