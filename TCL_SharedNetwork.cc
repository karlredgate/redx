
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

/** \file TCL_SharedNetwork.cc
 * \brief 
 *
 * including linux/if.h not net/if.h -- even though brutils uses net/if.h
 * since I needed linux/if.h for some other reason in Network.cc ??
 */

#include <sys/param.h>
#include <sys/socket.h>
// #include <net/if.h>
#include <linux/if.h>
#include <net/if_arp.h>
#include <linux/if_bridge.h>
#include <linux/sockios.h>

// Do I need these?
#include <sys/time.h>
#include <sys/ioctl.h>

#include <stdio.h>  // for sys_errlist
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <glob.h>

#include "netcfg/netcfg.h"

#include "Network.h"
#include "TCL_Fixup.h"

/**
 */
static int
SharedNetwork_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace Network;
    SharedNetwork *shared_network = (SharedNetwork *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(shared_network)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Tcl_SetResult( interp, (char *)"Network::SharedNetwork", TCL_STATIC );
        return TCL_OK;
    }

#if 0
    if ( Tcl_StringMatch(command, "create") ) {
        const char *result = shared_network->create();
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Tcl_SetResult( interp, (char *)result, TCL_STATIC );
        return TCL_ERROR;
    }

    if ( Tcl_StringMatch(command, "destroy") ) {
        const char *result = shared_network->destroy();
        // eliminate dangling pointer
	shared_network = NULL;
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Tcl_SetResult( interp, (char *)result, TCL_STATIC );
        return TCL_ERROR;
    }
#endif

    /**
     * The add_interface sub-command adds an Interface to this shared_network.
     */
    if ( Tcl_StringMatch(command, "add_interface") ) {
        if ( objc != 3 ) {
            Tcl_ResetResult( interp );
            Tcl_WrongNumArgs( interp, 1, objv, "add_interface <interface>" );
            return TCL_ERROR;
        }

        Interface *interface;
        void *p = (void *)&(interface); // avoid strict aliasing errors in the compiler
        if ( Tcl_GetLongFromObj(interp,objv[2],(long*)p) != TCL_OK ) {
            // we could do some additional err checks on the data -- ie -- numeric...
	    Tcl_SetResult( interp, (char *)"invalid interface object", TCL_STATIC );
            return TCL_ERROR;
        }

        const char *result = shared_network->add_interface( interface );
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Tcl_SetResult( interp, (char *)result, TCL_STATIC );
        return TCL_ERROR;
    }

    /**
     * The remove_interface sub-command removes an Interface from this shared_network.
     */
    if ( Tcl_StringMatch(command, "remove_interface") ) {
        if ( objc != 3 ) {
            Tcl_ResetResult( interp );
            Tcl_WrongNumArgs( interp, 1, objv, "remove_interface <interface>" );
            return TCL_ERROR;
        }

        Interface *interface;
        void *p = (void *)&(interface); // avoid strict aliasing errors in the compiler
        if ( Tcl_GetLongFromObj(interp,objv[2],(long*)p) != TCL_OK ) {
            // we could do some additional err checks on the data -- ie -- numeric...
	    Tcl_SetResult( interp, (char *)"invalid interface object", TCL_STATIC );
            return TCL_ERROR;
        }

        const char *result = shared_network->remove_interface( interface );
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Tcl_SetResult( interp, (char *)result, TCL_STATIC );
        return TCL_ERROR;
    }

    Tcl_SetResult( interp, (char *)"Unknown command for SharedNetwork object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
SharedNetwork_delete( ClientData data ) {
    using namespace Network;
    SharedNetwork *message = (SharedNetwork *)data;
    delete message;
}

/**
 */
static int
SharedNetwork_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR;
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    using namespace Network;
// XXX ordinal
    SharedNetwork *object = new SharedNetwork( name, 0 );
    Tcl_CreateObjCommand( interp, name, SharedNetwork_obj, (ClientData)object, SharedNetwork_delete );
    Tcl_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
static bool
SharedNetwork_Module( Tcl_Interp *interp ) {

    Tcl_Command command;

    Tcl_Namespace *ns = Tcl_CreateNamespace(interp, "Network::SharedNetwork", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    if ( Tcl_LinkVar(interp, "Network::SharedNetwork::debug", (char *)&debug, TCL_LINK_INT) != TCL_OK ) {
        syslog( LOG_ERR, "failed to link Network::SharedNetwork::debug" );
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Network::SharedNetwork::new", SharedNetwork_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    return true;
}

app_init( SharedNetwork_Module );

/* vim: set autoindent expandtab sw=4 : */
