
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

/** \file TCL_Interface.cc
 * \brief 
 *
 */

#include <stdint.h>
#include <stdlib.h> // for exit()

#include <arpa/inet.h> // for inet_ntop()

#include "logger.h"

#include "PlatformInterface.h"
#include "Interface.h"

#include "tcl_util.h"
#include "AppInit.h"

// Need a way to connect the real vars to the TCL implementation
// without the TCL interfaces present in the real interface
namespace {
    int debug = 0;
    // These may not really be necessary anymore
    int link_bounce_interval = 1200;
    int link_bounce_attempts = 2;
    int link_bounce_reattempt = 1200;   // 1 hour total
}
/**
 */
static int
Interface_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace Network;
    Interface *interface = (Interface *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(interface)) );
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
        bool result = interface->rename( newname );
        Tcl_SetObjResult( interp, Tcl_NewBooleanObj(result) );
        return TCL_ERROR;
    }

    /**
     */
    if ( Tcl_StringMatch(command, "negotiate") ) {
        bool result = interface->negotiate();
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

    /**
     */
    if ( Tcl_StringMatch(command, "primary_address") ) {
        struct in6_addr primary_address;
        interface->lladdr( &primary_address );
        char buffer[80];
        const char *llname = inet_ntop( AF_INET6, &primary_address, buffer, sizeof(buffer) );
        Tcl_SetResult( interp, (char *)llname, TCL_VOLATILE );
        return TCL_OK;
    }

    Tcl_StaticSetResult( interp, "Unknown command for Interface object" );
    return TCL_ERROR;
}

/**
 */
static void
Interface_delete( ClientData data ) {
    using namespace Network;
    Interface *message = (Interface *)data;
    delete message;
}

/**
 */
static int
Interface_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR;
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    using namespace Network;
    //
    // Interface *object = new Interface( name );
    //
    // maybe want there to be a constructor that looks up the interface by name?
    //
    Interface *object = new Interface( interp, name );
    Tcl_CreateObjCommand( interp, name, Interface_obj, (ClientData)object, Interface_delete );
    Tcl_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
bool NetworkInterface_Module( Tcl_Interp *interp ) {

    Tcl_Command command;

    Tcl_Namespace *ns = Tcl_CreateNamespace(interp, "Network::Interface", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    if ( Tcl_LinkVar(interp, "Network::Interface::debug", (char *)&debug, TCL_LINK_INT) != TCL_OK ) {
        log_err( "failed to link Network::Interface::debug" );
        _exit( 1 );
    }

    if ( Tcl_LinkVar(interp, "link_bounce_interval", (char *)&link_bounce_interval, TCL_LINK_INT) != TCL_OK ) {
        log_err( "failed to link Network::Interface::link_bounce_interval" );
        exit( 1 );
    }

    if ( Tcl_LinkVar(interp, "link_bounce_attempts", (char *)&link_bounce_attempts, TCL_LINK_INT) != TCL_OK ) {
        log_err( "failed to link Network::Interface::link_bounce_attempts" );
        exit( 1 );
    }

    if ( Tcl_LinkVar(interp, "link_bounce_reattempt", (char *)&link_bounce_reattempt, TCL_LINK_INT) != TCL_OK ) {
        log_err( "failed to link Network::Interface::link_bounce_reattempt" );
        exit( 1 );
    }

    command = Tcl_CreateObjCommand(interp, "Network::Interface::new", Interface_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    // Create a namespace for the platform configuration of interface names
    ns = Tcl_CreateNamespace(interp, "interface", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    const char *script =
    "namespace eval interface { array set name {} }\n"
    "proc interface {config} {\n"
    "   namespace eval interface $config\n"
    "}\n";
    int retcode = Tcl_EvalEx( interp, script, -1, TCL_EVAL_GLOBAL );
    if ( retcode != TCL_OK )  return false;

    return true;
}

app_init( NetworkInterface_Module );

/* vim: set autoindent expandtab sw=4 : */
