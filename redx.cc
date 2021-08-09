
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

/** \file redx.cc
 * \brief A diagnostic shell
 *
 * \todo add process status/grep/lookup/etc
 * \todo add dbus
 * \todo add syslog
 */

#include <sys/stat.h>
#include <errno.h>

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#include <string.h>
#include <syslog.h>

#include "platform.h"

#include "tcl_util.h"
#include "AppInit.h"

/**
 */
int RedX_Init( Tcl_Interp *interp ) {
    int interactive = 1;
    Tcl_Obj *interactive_obj;
    interactive_obj = Tcl_GetVar2Ex( interp, "tcl_interactive", NULL, TCL_GLOBAL_ONLY );
    if ( interactive_obj != NULL ) {
        Tcl_GetIntFromObj( interp, interactive_obj, &interactive );
    }

    Tcl_Command command;

    if ( interactive ) printf( " ** RedX debug tool v1.0\n" );
    Tcl_SetVar(interp, "tcl_rcFileName", "~/.redxrc", TCL_GLOBAL_ONLY);

    Tcl_EvalEx( interp, "proc clock {command} { namespace eval ::tcl::clock $command}", -1, TCL_EVAL_GLOBAL );
    Tcl_EvalEx( interp, "proc commands {} {namespace eval commands {info procs}}", -1, TCL_EVAL_GLOBAL );
    const char *help_script = "proc help {args} {\n"
        "  foreach name [namespace children] {\n"
        "    puts \"## $name commands\"\n"
        "    foreach command [info commands \"${name}::*\"] {\n"
        "      puts -nonewline \"[namespace tail $command] \"\n"
        "    }\n"
        "    puts {}\n"
        "  }\n"
        "}\n";
    int retcode = Tcl_EvalEx( interp, help_script, -1, TCL_EVAL_GLOBAL );
    if ( retcode != TCL_OK )  return false;

#if 0
    if ( getuid() != 0 ) {
        if ( interactive ) printf( "BIOS not initialized, no access to /dev/mem\n" );
    } else {
        if ( BIOS::Initialize(interp) == false ) {
            Tcl_StaticSetResult( interp, "BIOS::Initialize failed" );
            return TCL_ERROR;
        }
        if ( interactive ) printf( "BIOS initialized\n" );
    }

    if ( access("/proc/xen/privcmd", R_OK) != 0 ) {
        if ( interactive ) printf( "Xen not initialized, no hypervisor present\n" );
    } else {
        if ( Xen::Initialize(interp) == false ) {
            Tcl_StaticSetResult( interp, "Xen::Initialize failed" );
            return TCL_ERROR;
        }
        if ( interactive ) printf( "Xen initialized\n" );
    }

    if ( Kernel::Initialize(interp) == false ) {
        Tcl_StaticSetResult( interp, "Kernel::Initialize failed" );
        return TCL_ERROR;
    }
    if ( interactive ) printf( "Kernel initialized\n" );

    if ( NetLink::Initialize(interp) == false ) {
        Tcl_StaticSetResult( interp, "NetLink::Initialize failed" );
        return TCL_ERROR;
    }
    if ( interactive ) printf( "NetLink initialized\n" );

#endif

    if ( Tcl_CallAppInitChain(interp) == false ) {
        // this may want to be additive result
        Tcl_StaticSetResult( interp, "AppInit failed" );
        return TCL_ERROR;
    }

    return TCL_OK;
}

/**
 */
int main( int argc, char **argv ) {
    Tcl_Main( argc, argv, RedX_Init );
}

/* vim: set autoindent expandtab sw=4 : */
