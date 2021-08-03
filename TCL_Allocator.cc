
/*
 * Copyright (c) 2017 Karl N. Redgate
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

/** \file List_Module.cc
 * \brief 
 *
 */

#include <stdlib.h>

#include <tcl.h>
#include "tcl_util.h"

#include "logger.h"
#include "Allocator.h"
#include "AppInit.h"

namespace { int debug = 0; }

/**
 */
static int
stats_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 1 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "" );
        return TCL_ERROR;
    }

    Tcl_ResetResult( interp );
    return TCL_OK;
}

/**
 */
static int
usage_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 1 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "" );
        return TCL_ERROR;
    }

    Tcl_Obj *result = Tcl_NewListObj( 0, 0 );
    // THIS currently is implemented within the object...
    // this needs to move to the TCL part - need
    // iterator within the class ??
    //heap.usage( interp, result );

    Tcl_SetObjResult( interp, result );
    return TCL_OK;
}

/**
 */
static bool
Allocator_Module( Tcl_Interp *interp ) {
    Tcl_Command command;

    Tcl_Namespace *ns = Tcl_CreateNamespace(interp, "Allocator", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    if ( Tcl_LinkVar(interp, "Allocator::debug", (char *)&debug, TCL_LINK_INT) != TCL_OK ) {
        log_err( "failed to link List::debug" );
        exit( 1 );
    }

    command = Tcl_CreateObjCommand(interp, "Allocator::stats", stats_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Allocator::usage", usage_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    return true;
}

app_init( Allocator_Module );

/* vim: set autoindent expandtab sw=4 syntax=c : */
