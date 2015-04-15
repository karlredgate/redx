
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

/** \file TCL_Bridge.cc
 * \brief 
 *
 * including linux/if.h not net/if.h -- even though brutils uses net/if.h
 * since I needed linux/if.h for some other reason in Network.cc ??
 */

#include <unistd.h>

#include <netinet/in.h>
#include <netinet/ip6.h>

#include <sys/param.h>

// Do I need these?
#include <sys/time.h>
#include <sys/ioctl.h>

#include <stdio.h>  // for sys_errlist
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <glob.h>

#include "logger.h"
#include "Bridge.h"
#include "Interface.h"
#include "tcl_util.h"
#include "AppInit.h"

/**
 */
static int
Bridge_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace Network;
    Bridge *bridge = (Bridge *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(bridge)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Tcl_StaticSetResult( interp, "Network::Bridge" );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "create") ) {
        const char *result = bridge->create();
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Tcl_StaticSetResult( interp, (char *)result );
        return TCL_ERROR;
    }

    /**
     * The destroy sub-command for a bridge object destroys the kernels
     * bridge object.  The program bridge object remains, holding the
     * configuration information etc.  It can be used to recreate the
     * bridge.
     */
    if ( Tcl_StringMatch(command, "destroy") ) {
        const char *result = bridge->destroy();
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Tcl_StaticSetResult( interp, (char *)result );
        return TCL_ERROR;
    }

    /**
     * The add sub-command adds an Interface to this bridge.  The interface
     * is then "captured" by the bridge, and is not usable outside of the
     * bridge.  Linux is supposed to support using the interface outside of
     * the bridge, but I have not found how to make this work reliably.
     */
    if ( Tcl_StringMatch(command, "add") or Tcl_StringMatch(command, "capture") ) {
        if ( objc != 3 ) {
            Tcl_ResetResult( interp );
            Tcl_WrongNumArgs( interp, 1, objv, "add <interface>" );
            return TCL_ERROR;
        }

        Interface *interface;
        void *p = (void *)&(interface); // avoid strict aliasing errors in the compiler
        if ( Tcl_GetLongFromObj(interp,objv[2],(long*)p) != TCL_OK ) {
            // we could do some additional err checks on the data -- ie -- numeric...
            Tcl_StaticSetResult( interp, "invalid interface object" );
            return TCL_ERROR;
        }

        const char *result = bridge->add( interface );
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Tcl_StaticSetResult( interp, (char *)result );
        return TCL_ERROR;
    }

    // get the interface from the cmd line
    if ( Tcl_StringMatch(command, "remove") ) {
        if ( objc != 3 ) {
            Tcl_ResetResult( interp );
            Tcl_WrongNumArgs( interp, 1, objv, "remove <interface>" );
            return TCL_ERROR;
        }

        Interface *interface;
        void *p = (void *)&(interface); // avoid strict aliasing errors in the compiler
        if ( Tcl_GetLongFromObj(interp,objv[2],(long*)p) != TCL_OK ) {
            // we could do some additional err checks on the data -- ie -- numeric...
            Tcl_StaticSetResult( interp, "invalid interface object" );
            return TCL_ERROR;
        }

        const char *result = bridge->remove( interface );
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Tcl_StaticSetResult( interp, (char *)result );
        return TCL_ERROR;
    }

    Tcl_StaticSetResult( interp, "Unknown command for Bridge object" );
    return TCL_ERROR;
}

/**
 */
static void
Bridge_delete( ClientData data ) {
    using namespace Network;
    Bridge *message = (Bridge *)data;
    delete message;
}

/**
 */
static int
Bridge_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR;
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    using namespace Network;
    Bridge *object = new Bridge( name );
    Tcl_CreateObjCommand( interp, name, Bridge_obj, (ClientData)object, Bridge_delete );
    Tcl_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
bool Bridge_Module( Tcl_Interp *interp ) {

    Tcl_Command command;

    Tcl_Namespace *ns = Tcl_CreateNamespace(interp, "Network::Bridge", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    if ( Tcl_LinkVar(interp, "Network::Bridge::debug", (char *)&debug, TCL_LINK_INT) != TCL_OK ) {
        log_err( "failed to link Network::Bridge::debug" );
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Network::Bridge::new", Bridge_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    return true;
}

app_init( Bridge_Module );

/* vim: set autoindent expandtab sw=4 : */
