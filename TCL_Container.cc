
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

/** \file TCL_Container.cc
 * \brief 
 *
 */

#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include <tcl.h>
#include "tcl_util.h"

#include "logger.h"
#include "Container.h"
#include "AppInit.h"

namespace {
    int debug = 0;
    char *static_envp[] = {
        const_cast<char*>("HOME=/root"),
        NULL
    };
}


/**
 * Container::unshare namespace
 */
static int
unshare_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc < 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "command [args]" );
        return TCL_ERROR; 
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );

    char *argv[30];
    int argc = 1;
    argv[0] = command;

    for ( int i = 2 ; i < objc ; i++ ) {
        argv[argc++] = Tcl_GetStringFromObj( objv[i], NULL );
    }
    argv[argc] = NULL;

    int pid = 0;
    // int pid = Kernel::daemonize( command, argc, argv );

    if ( pid > 0 ) {
        Tcl_SetObjResult( interp, Tcl_NewIntObj((pid)) );
        return TCL_OK;
    }

    if ( pid < 0 ) {
        Tcl_StaticSetResult( interp, "cannot execute command" );
        return TCL_ERROR;
    }

    Tcl_StaticSetResult( interp, "unknown result" );
    return TCL_ERROR;
}

/**
 * Container::unshare namespace
 */
static int
unsharepid_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc < 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "command [args]" );
        return TCL_ERROR; 
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );

    char *argv[30];
    int argc = 1;
    argv[0] = command;

    int result = Container::unshare_pid();

    if ( result > 0 ) {
        Tcl_SetObjResult( interp, Tcl_NewIntObj((result)) );
        return TCL_OK;
    }

    if ( result < 0 ) {
        Tcl_StaticSetResult( interp, "cannot unshare pid space" );
        return TCL_ERROR;
    }

    Tcl_StaticSetResult( interp, "unknown result" );
    return TCL_ERROR;
}

/**
 * kcmp - compare two processes to determine if they share a kernel resource
 * SYNOPSIS
 * #include <linux/kcmp.h>
 * int kcmp(pid_t pid1, pid_t pid2, int type,
 * unsigned long idx1, unsigned long idx2);
 *
 * setns - reassociate thread with a namespace
 * SYNOPSIS
 * #define _GNU_SOURCE      - See feature_test_macros(7)
 * #include <sched.h>
 * int setns(int fd, int nstype);
 *
 * nsenter(1),  readlink(1),  unshare(1), clone(2),
 * ioctl_ns(2), setns(2), unshare(2), proc(5), capabilities(7),
 * cgroup_namespaces(7), cgroups(7), credentials(7),
 * network_namespaces(7),  pid_namespaces(7),
 * user_namespaces(7), lsns(8), switch_root(8)
 */
bool
Container_Module( Tcl_Interp *interp ) {
    Tcl_Command command;

    Tcl_Namespace *ns = Tcl_CreateNamespace(interp, "Container", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    if ( Tcl_LinkVar(interp, "Container::debug", (char *)&debug, TCL_LINK_INT) != TCL_OK ) {
        log_err( "failed to link Container::debug" );
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Container::unshare", unshare_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Container::unshare_pid", unsharepid_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    return true;
}

app_init( Container_Module );

/* vim: set autoindent expandtab sw=4 : */
