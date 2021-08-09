
/*
 * Copyright (c) 2021 Karl N. Redgate
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

/** \file TCL_Process.cc
 * \brief 
 *
 * Info about the current process.
 * Add commands for enabling traps etc
 *
 * Extract stack - and other procfs stuff
 * hash that dynamically gets the data
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include <time.h>

#include <tcl.h>
#include "tcl_util.h"

#include "logger.h"
#include "Kernel.h"
#include "AppInit.h"

#include "traps.h"
#include "PlatformThread.h"

namespace {
    int debug = 0;
    int pid = 0;
    int ppid = 0;
    char *static_envp[] = {
        const_cast<char*>("HOME=/root"),
        NULL
    };
}

/**
 */
static int
tracing_enabled( Tcl_Interp *interp ) {
    int enabled = 1;

    Tcl_Obj *obj = Tcl_GetVar2Ex( interp, "trace", NULL, 0 );
    if ( obj == NULL ) return enabled;

    if ( Tcl_GetBooleanFromObj(interp, obj, &enabled) != TCL_OK ) {
        return 1;
    }

    return enabled;
}

/**
 * Process::traps
 */
static int
traps_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc < 1 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "command [args]" );
        return TCL_ERROR; 
    }

    Tcl_StaticSetResult( interp, "enabled" );
    return TCL_ERROR;
}

/**
 * Process::zombie - create a zombie child process.
 */
static int
zombie_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc < 1 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "signal pid" );
        return TCL_ERROR; 
    }

    int count = 1;
    if ( Tcl_GetIntFromObj(interp, objv[1], &count) != TCL_OK ) {
        count = 1;
    }

    bool anyfailed = false;
    for ( int i = 0 ; i < count ; ++i ) {
        // create another zombie - add it to the list of pids
    }
    int pid = ::fork();
    if ( pid < 0 ) { // fork failed - send error
        Tcl_StaticSetResult( interp, "fork failed" );
        return TCL_ERROR;
    }
    if ( pid == 0 ) { // I am the child
        set_thread_name( "FAKE-ZOMBIE" );
        exit( 0 );
    }
    // pid is positive - I must be the parent
    Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(pid)) );
    // DO NOT WAIT for the process
    return TCL_OK;
}

/**
 * Process::kill
 */
static int
wait_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc < 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "pid" );
        return TCL_ERROR; 
    }

    pid_t pid;
    if ( Tcl_GetIntFromObj(interp, objv[1], &pid) != TCL_OK ) {
        Svc_SetResult( interp, "invalid pid value", TCL_STATIC );
        return TCL_ERROR;
    }

    int status;
    int result = waitpid( pid, &status, 0 );
    if ( result == -1 ) {
        Tcl_StaticSetResult( interp, "wait failed" );
        return TCL_ERROR;
    }
    if ( result == 0 ) {
        Tcl_StaticSetResult( interp, "no change" );
        return TCL_OK;
    }
    if ( result == pid ) {
        Tcl_StaticSetResult( interp, "dead" );
        return TCL_OK;
    }

    Tcl_StaticSetResult( interp, "unexpected result" );
    return TCL_ERROR;
}

/**
 * Process::kill
 */
static int
kill_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc < 3 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "signal pid" );
        return TCL_ERROR; 
    }

    int signal;
    if ( Tcl_GetIntFromObj(interp, objv[1], &signal) != TCL_OK ) {
        Svc_SetResult( interp, "invalid signal value", TCL_STATIC );
        return TCL_ERROR;
    }

    // this should be string or int
    pid_t pid;
    if ( Tcl_GetIntFromObj(interp, objv[2], &pid) != TCL_OK ) {
        Svc_SetResult( interp, "invalid pid value", TCL_STATIC );
        return TCL_ERROR;
    }

    int result = kill( pid, signal );
       // int kill(pid_t pid, int sig);

    Tcl_StaticSetResult( interp, "passfail" );
    return TCL_ERROR;
}

/**
 * Process::restart
 */
static int
restart_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc < 1 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "signal pid" );
        return TCL_ERROR; 
    }

    Tcl_Obj *obj = Tcl_GetVar2Ex( interp, "argv0", NULL, 0 );
    char *path = Tcl_GetStringFromObj( obj, NULL );

    char * const argv[] = { "redx", (char *)0 };
    extern char **environ;

    if ( execve(path, argv, environ) == -1 ) {
        Tcl_StaticSetResult( interp, "execve failed" );
        return TCL_ERROR;
    }

    Tcl_SetObjResult( interp, obj );
    return TCL_ERROR;
}

/**
 */
static pid_t
spawn_child( Tcl_Interp *interp, const char *command, int objc, Tcl_Obj * CONST *objv, int *out ) {

    char *argv[30];
    int argc = 1;
    argv[0] = const_cast<char*>(command);

    for ( int i = 0 ; i < objc ; i++ ) {
        argv[argc++] = Tcl_GetStringFromObj( objv[i], NULL );
    }
    argv[argc] = NULL;

    int fd[2];

    /*
     * fd[0] is for reading - parent input fd
     * fd[1] is for writing - child stdout
     */
    if ( pipe(fd) < 0 ) {
        log_notice( "failed to generate pipe for '%s'", command );
        return -1;
    }

    pid_t child = fork();

    /*
     * if child < 0 this is an error
     * if child > 0 this is the parent
     */
    if ( child != 0 ) {
        close( fd[1] );
        *out = fd[0];
        return child;
    }

    /*
     * Redirect stdout and stderr to the pipe
     */
    close( fd[0] );
    close( 1 );
    dup2( fd[1], 1 );
    close( 2 );
    dup2( fd[1], 2 );

    if ( execve(command, argv, static_envp) < 0 ) {
        log_notice( "failed to execve '%s'", command );
        _exit( 0 );
    }
    log_notice( "THIS MESSAGE SHOULD NEVER HAPPEN - exec failed" );
    _exit( 0 );
}

/**
 */
static int
wait_for_child( pid_t child, int timeout ) {
    int status;
    int pid;

    struct timespec delay = { 1, 0 };
    struct timespec remaining = { 0, 0 };

    if ( child == 0 ) {
        log_notice( "asked to wait for an invalid pid" );
        return 0;
    }

    while ( timeout > 0 ) {
        pid = waitpid( child, &status, WNOHANG );
        if ( pid == child ) {
            if ( WIFEXITED(status) ) {
                if ( WEXITSTATUS(status) != 0 ) {
                    log_notice( "%d exited with status %d", pid, WEXITSTATUS(status) );
                }
                return child; 
            }
            if ( WIFSIGNALED(status) ) {
                log_notice( "%d killed by signal %d", pid, WTERMSIG(status) );
                return child;
            }
        }
        if ( pid == 0 ) {
            // sleep_for_second();
            --timeout;
            continue;
        }
        if ( pid < 0 ) {
            switch (errno) {
            case EINTR: continue;
            case EINVAL:
                log_notice( "waitpid() with an invalid argument" );
                break;
            case ECHILD:
                log_notice( "waitpid() for an invalid pid" );
                break;
            default:
                log_notice( "unknown errno from waitpid()" );
                break;
            }
        }
        log_notice(
                "waitpid() returned an invalid value with %d seconds remaining",
                timeout );
        break;
    }

    if ( debug ) log_notice( "timed out, killing pid %d", child );
    ::kill( child, SIGKILL );

    /* pid = waitpid( child, &status, WNOHANG ); */
    pid = waitpid( child, &status, 0 );
    return -1;
}

/**
 */
bool
Process_Module( Tcl_Interp *interp ) {
    Tcl_Command command;

    Tcl_Namespace *ns = Tcl_CreateNamespace(interp, "Process", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    if ( Tcl_LinkVar(interp, "Process::debug", (char *)&debug, TCL_LINK_INT) != TCL_OK ) {
        log_err( "failed to link Process::debug" );
        return false;
    }

    pid = getpid();
    if ( Tcl_LinkVar(interp, "Process::pid", (char *)&pid, TCL_LINK_INT) != TCL_OK ) {
        log_err( "failed to link Process::pid" );
        return false;
    }

    ppid = getppid();
    if ( Tcl_LinkVar(interp, "Process::ppid", (char *)&ppid, TCL_LINK_INT) != TCL_OK ) {
        log_err( "failed to link Process::ppid" );
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Process::zombie", zombie_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Process::wait", wait_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Process::traps", traps_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Process::kill", kill_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Process::restart", restart_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    return true;
}

app_init( Process_Module );

/* vim: set autoindent expandtab sw=4 : */
