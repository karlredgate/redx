
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

/** \file TCL_SMBIOS.cc
 * \brief 
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <errno.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <string.h>
#include <tcl.h>
#include "tcl_util.h"

#include "logger.h"
#include "SMBIOS.h"

#include "AppInit.h"

/**
 * This is a sample command for testing the straight line netlink
 * probe code.
 */
static int 
Probe_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    SMBIOS::Header smbios;
    smbios.probe( SMBIOS::Header::locate() );
    return TCL_OK;
}

/**
 * we can find the SMBIOS address directly by looking through the BIOS data structures directly
 */
bool SMBIOS_Module( Tcl_Interp *interp ) {
    Tcl_Command command;

    Tcl_Namespace *ns = Tcl_CreateNamespace(interp, "BIOS", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    using namespace SMBIOS;

    ns = Tcl_CreateNamespace(interp, "SMBIOS", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    if ( Tcl_LinkVar(interp, "SMBIOS::debug", (char *)&debug, TCL_LINK_INT) != TCL_OK ) {
        log_err( "failed to link SMBIOS::debug" );
        exit( 1 );
    }

    // create TCL commands for creating BIOS/SMBIOS structures
    command = Tcl_CreateObjCommand(interp, "SMBIOS::Probe", Probe_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    return true;
}

app_init( SMBIOS_Module );
/* vim: set autoindent expandtab sw=4 : */
