
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

/** \file Kernel.cc
 * \brief 
 *
 */

// this is for OSX
#include <sys/param.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/reboot.h>

#include <time.h>

#include <tcl.h>
#include "tcl_util.h"

#include "logger.h"
#include "Kernel.h"
#include "AppInit.h"

namespace {
    int debug = 0;
    char *static_envp[] = {
        const_cast<char*>("HOME=/root"),
        NULL
    };
}

/**
 * Kernel::daemonize /path/to/command arg1 arg2 ...
 */
int
Kernel::daemonize( const char *command, int argc, char **argv )
{
    // const char *command = Tcl_GetStringFromObj( objv[1], NULL );

    if ( access(command, X_OK) < 0 ) {
        return -1; // Svc_SetResult( interp, "cannot execute command", TCL_STATIC );
    }

    int pid = fork();
    if ( pid > 0 )  return pid;
    if ( pid < 0 )  return -1;

    close( 0 );
    close( 1 );
    close( 2 );

    if ( fork() != 0 ) { _exit(0); }

    if ( setsid() < 0 ) {
        log_notice( "failed to setsid()" );
    }

    freopen( "/dev/null", "r", stdin );
    freopen( "/dev/null", "w", stdout );
    freopen( "/dev/null", "w", stderr );

    log_open_user( "(redx:background)" );
    log_notice( "spawning '%s'", command );

    if ( execve(command, argv, static_envp) < 0 ) {
        log_notice( "failed to execve('%s')", command );
        _exit(0);
    }

    return 0;
}

/**
 * Look in /proc/filesystems for valid fs types -- then look for modules if it is not loaded
 *
 * type defaults to ext3
 * flags should be defined constants: Kernel::MS_BIND
 *
 * Kernel::mount /dev/drbd5 /shared ext3 {nodev dirsync}
 */
static int
mount_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc < 3 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "source target [type] [flags]" );
        return TCL_ERROR; 
    }

    unsigned long flags = 0;
    char *source = Tcl_GetStringFromObj( objv[1], NULL );
    char *target = Tcl_GetStringFromObj( objv[2], NULL );

    const char *fstype = "ext3";
    if ( objc > 3 ) {
        fstype = Tcl_GetStringFromObj( objv[3], NULL );
    }

    // change this to an array
    if ( objc > 4 ) {
        for ( int i = 4 ; i < objc ; i++ ) {
            char *flag = Tcl_GetStringFromObj( objv[i], NULL );
            // if ( Tcl_StringMatch(flag, "nodev") ) { flags |= MS_NODEV; continue; }
            // if ( Tcl_StringMatch(flag, "dirsync") ) { flags |= MS_NODEV; continue; }
            // if ( Tcl_StringMatch(flag, "bind") ) { flags |= MS_BIND; continue; }
        }
    }

    if ( mount(source, target, fstype, flags, "") < 0 ) {
        char buffer[128];
        char *err = strerror_r(errno, buffer, sizeof(buffer));
        Svc_SetResult( interp, err, TCL_VOLATILE );
        return TCL_ERROR;
    }

    Tcl_ResetResult( interp );
    return TCL_OK;
}

/**
 */
static int
umount_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "target" );
        return TCL_ERROR; 
    }

    char *target = Tcl_GetStringFromObj( objv[1], NULL );
    int result = ::umount( target );
    if ( result == 0 ) {
        Tcl_ResetResult( interp );
        return TCL_OK;
    }

    // \todo set result to perror value
    Svc_SetResult( interp, "Unknown command for EchoRequest object", TCL_STATIC );
    return TCL_OK;
}

/**
 */
static int
reboot_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "message" );
        return TCL_ERROR; 
    }

    char *message = Tcl_GetStringFromObj( objv[1], NULL );
    log_notice( "reboot: %s", message );
    sync();
    int result = ::reboot( RB_AUTOBOOT );
    if ( result < 0 ) {
        Tcl_ResetResult( interp );
        char buffer[128];
        char *err = strerror_r(errno, buffer, sizeof(buffer));
        Svc_SetResult( interp, err, TCL_VOLATILE );
        return TCL_ERROR;
    }

    Tcl_ResetResult( interp );
    return TCL_OK;
}

/**
 */
static int
halt_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "message" );
        return TCL_ERROR; 
    }

    char *message = Tcl_GetStringFromObj( objv[1], NULL );
    log_notice( "reboot: %s", message );
    sync();
    int result = ::reboot( RB_HALT_SYSTEM );
    if ( result < 0 ) {
        Tcl_ResetResult( interp );
        char buffer[128];
        char *err = strerror_r(errno, buffer, sizeof(buffer));
        Svc_SetResult( interp, err, TCL_VOLATILE );
        return TCL_ERROR;
    }

    Tcl_ResetResult( interp );
    return TCL_OK;
}

/**
 */
static int
poweroff_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "message" );
        return TCL_ERROR; 
    }

    char *message = Tcl_GetStringFromObj( objv[1], NULL );
    log_notice( "reboot: %s", message );
    sync();
    int result = ::reboot( RB_POWER_OFF );
    if ( result < 0 ) {
        Tcl_ResetResult( interp );
        char buffer[128];
        char *err = strerror_r(errno, buffer, sizeof(buffer));
        Svc_SetResult( interp, err, TCL_VOLATILE );
        return TCL_ERROR;
    }

    Tcl_ResetResult( interp );
    return TCL_OK;
}

/**
 */
static int
salute_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "bool" );
        return TCL_ERROR; 
    }

    int cad;
    if ( Tcl_GetBooleanFromObj(interp, objv[1], &cad) != TCL_OK ) {
        Svc_SetResult( interp, "invalid boolean for arg", TCL_STATIC );
        return TCL_ERROR;
    }
    int result;
    if ( cad ) {
        result = ::reboot( RB_ENABLE_CAD );
    } else {
        result = ::reboot( RB_DISABLE_CAD );
    }

    if ( result < 0 ) {
        Tcl_ResetResult( interp );
        char buffer[128];
        char *err = strerror_r(errno, buffer, sizeof(buffer));
        Svc_SetResult( interp, err, TCL_VOLATILE );
        return TCL_ERROR;
    }

    Tcl_ResetResult( interp );
    return TCL_OK;
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
static void
sleep_for_second() {
    struct timespec delay = { 1, 0 };
    struct timespec remaining = { 0, 0 };

    while ( nanosleep(&delay, &remaining) < 0 ) {
        if ( errno != EINTR ) return;
        delay.tv_sec = remaining.tv_sec;
        delay.tv_nsec = remaining.tv_nsec;
    }
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
            sleep_for_second();
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
    kill( child, SIGKILL );

    /* pid = waitpid( child, &status, WNOHANG ); */
    pid = waitpid( child, &status, 0 );
    return -1;
}

/**
 */
static int
devno_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "filename" );
        return TCL_ERROR; 
    }

    char *filename = Tcl_GetStringFromObj( objv[1], NULL );
    struct stat s;
    if ( stat(filename, &s) < 0 ) {
        // would be better if these were errno messages
        Tcl_StaticSetResult( interp, "failed to stat file" );
        return TCL_ERROR;
    }
    if ( (S_ISCHR(s.st_mode) || S_ISBLK(s.st_mode)) == false ) {
        Tcl_StaticSetResult( interp, "file is not a device" );
        return TCL_ERROR;
    }

    Tcl_Obj *list = Tcl_NewListObj( 0, 0 );
    Tcl_ListObjAppendElement( interp, list, Tcl_NewIntObj( s.st_rdev >> 8 ) );
    Tcl_ListObjAppendElement( interp, list, Tcl_NewIntObj( s.st_rdev & 0xFF ) );
    Tcl_SetObjResult( interp, list );
    return TCL_OK;
}

/* vim: set autoindent expandtab sw=4 : */
