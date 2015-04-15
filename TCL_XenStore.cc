
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

/** \file TCL_XenStore.cc
 * \brief 
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>  // for mlock

#include <stdint.h>
#include <stdlib.h>

/**
 * Lie about being a Xen tool...
 */
#define __XEN_TOOLS__
#include <xen/xen.h>
#include <xen/sys/privcmd.h>
#include <xen/sysctl.h>
#include <xen/domctl.h>

#include <errno.h>
#include <string.h>
#include <tcl.h>
#include "tcl_util.h"

#include "logger.h"
#include "Hypercall.h"
#include "XenStore.h"

/**
 */
static int
XenStoreRead_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "key" );
        return TCL_ERROR;
    }

    int len;
    char *key = Tcl_GetStringFromObj( objv[1], &len );

    Xen::Store store;
    char *value = store.read( key );
    Tcl_Obj *result = Tcl_NewStringObj( value, -1 );
    Tcl_SetObjResult( interp, result );
    free( value );
    return TCL_OK;
}

/**
 */
static int
XenStoreWrite_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 3 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "key value" );
        return TCL_ERROR;
    }

    int len;
    char *key   = Tcl_GetStringFromObj( objv[1], &len );
    char *value = Tcl_GetStringFromObj( objv[2], &len );

    Xen::Store store;
    if ( store.write(key, value) ) {
        Tcl_SetObjResult( interp, objv[2] );
        return TCL_OK;
    }

    Svc_SetResult( interp, (char *)store.error_message(), TCL_VOLATILE );
    char e[128];
    char *err;
    err = strerror_r( store.error(), e, sizeof(e) );
    Tcl_AppendResult( interp, err, NULL );
    return TCL_ERROR;
}

/**
 */
static int
XenStoreMkdir_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "key" );
        return TCL_ERROR;
    }

    int len;
    char *key = Tcl_GetStringFromObj( objv[1], &len );

    Xen::Store store;
    if ( store.mkdir(key) ) {
        Tcl_ResetResult( interp );
        return TCL_OK;
    }

    Svc_SetResult( interp, (char *)store.error_message(), TCL_VOLATILE );
    char e[128];
    char *err;
    err = strerror_r( store.error(), e, sizeof(e) );
    Tcl_AppendResult( interp, err, NULL );
    return TCL_ERROR;
}

/**
 */
static int
XenStoreRemove_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "key" );
        return TCL_ERROR;
    }

    int len;
    char *key = Tcl_GetStringFromObj( objv[1], &len );

    Xen::Store store;
    if ( store.remove(key) ) {
        Tcl_ResetResult( interp );
        return TCL_OK;
    }

    Svc_SetResult( interp, (char *)store.error_message(), TCL_VOLATILE );
    char e[128];
    char *err;
    err = strerror_r( store.error(), e, sizeof(e) );
    Tcl_AppendResult( interp, err, NULL );
    return TCL_ERROR;
}

/**
 */
static int
XenStoreList_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "key" );
        return TCL_ERROR;
    }

    int len;
    char *key = Tcl_GetStringFromObj( objv[1], &len );

    Tcl_Obj *list = Tcl_NewListObj( 0, 0 );

    Xen::Store store;
    Xen::StorePath *values = store.readdir( key );
    Xen::StorePath *child = values;
    while ( child != 0 ) {
        Tcl_ListObjAppendElement( interp, list, Tcl_NewStringObj(child->path, -1) );
        child = child->next;
    }
    // delete values;

    Tcl_SetObjResult( interp, list );
    return TCL_OK;
}

/**
 */
bool XenStore_Module( Tcl_Interp *interp ) {
    Tcl_Command command;

    Tcl_Namespace *ns = Tcl_CreateNamespace(interp, "Xen", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    ns = Tcl_CreateNamespace(interp, "Xen::Store", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Xen::Store::read", XenStoreRead_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Xen::Store::write", XenStoreWrite_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Xen::Store::mkdir", XenStoreMkdir_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Xen::Store::remove", XenStoreRemove_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Xen::Store::list", XenStoreList_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    return true;
}

app_init( XenStore_Module );

/* vim: set autoindent expandtab sw=4 : */
