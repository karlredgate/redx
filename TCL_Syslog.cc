
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

/** \file redx.cc
 * \brief A unit test/diagnostic shell for these libraries
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
#include <stdarg.h>
#include <tcl.h>

#include "tcl_util.h"
#include "AppInit.h"

/**
 * log level message
 */
static int
Syslog_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 3 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "level message" );
        return TCL_ERROR; 
    }

    char *level_name = Tcl_GetStringFromObj( objv[1], NULL );
    char *message = Tcl_GetStringFromObj( objv[2], NULL );

    int level = -1;
    if ( Tcl_StringMatch(level_name, "emerg")     )  level = LOG_EMERG;
    if ( Tcl_StringMatch(level_name, "emergency") )  level = LOG_EMERG;
    if ( Tcl_StringMatch(level_name, "alert")     )  level = LOG_ALERT;
    if ( Tcl_StringMatch(level_name, "crit")      )  level = LOG_CRIT;
    if ( Tcl_StringMatch(level_name, "critical")  )  level = LOG_CRIT;
    if ( Tcl_StringMatch(level_name, "err")       )  level = LOG_ERR;
    if ( Tcl_StringMatch(level_name, "error")     )  level = LOG_ERR;
    if ( Tcl_StringMatch(level_name, "warn")      )  level = LOG_WARNING;
    if ( Tcl_StringMatch(level_name, "warning")   )  level = LOG_WARNING;
    if ( Tcl_StringMatch(level_name, "notice")    )  level = LOG_NOTICE;
    if ( Tcl_StringMatch(level_name, "info")      )  level = LOG_INFO;
    if ( Tcl_StringMatch(level_name, "debug")     )  level = LOG_DEBUG;

    if ( level == -1 ) {
        Tcl_StaticSetResult( interp, "invalid level" );
        return TCL_ERROR;
    }

    syslog( level, "%s", message );

    Tcl_ResetResult( interp );
    return TCL_OK;
}

/**
 */
static void
Syslog_delete( ClientData data ) {
    closelog();
}

/**
 * Syslog command_name daemon application_name
 */
static int
Syslog_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 4 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name facility application" );
        return TCL_ERROR; 
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    char *facility_name = Tcl_GetStringFromObj( objv[2], NULL );
    char *application = Tcl_GetStringFromObj( objv[3], NULL );

    int facility = -1;
    if ( Tcl_StringMatch(facility_name, "auth")     )  facility = LOG_AUTHPRIV;
    if ( Tcl_StringMatch(facility_name, "authpriv") )  facility = LOG_AUTHPRIV;
    if ( Tcl_StringMatch(facility_name, "daemon")   )  facility = LOG_DAEMON;
    if ( Tcl_StringMatch(facility_name, "kernel")   )  facility = LOG_KERN;
    if ( Tcl_StringMatch(facility_name, "local0")   )  facility = LOG_LOCAL0;
    if ( Tcl_StringMatch(facility_name, "local1")   )  facility = LOG_LOCAL1;
    if ( Tcl_StringMatch(facility_name, "local2")   )  facility = LOG_LOCAL2;
    if ( Tcl_StringMatch(facility_name, "local3")   )  facility = LOG_LOCAL3;
    if ( Tcl_StringMatch(facility_name, "local4")   )  facility = LOG_LOCAL4;
    if ( Tcl_StringMatch(facility_name, "local5")   )  facility = LOG_LOCAL5;
    if ( Tcl_StringMatch(facility_name, "local6")   )  facility = LOG_LOCAL6;
    if ( Tcl_StringMatch(facility_name, "local7")   )  facility = LOG_LOCAL7;
    if ( Tcl_StringMatch(facility_name, "user")     )  facility = LOG_USER;
    if ( facility == -1 ) {
        Tcl_StaticSetResult( interp, "invalid facility" );
        return TCL_ERROR;
    }
    openlog( strdup(application), 0, facility );

    void * handle = 0;
    Tcl_CreateObjCommand( interp, name, Syslog_obj, (ClientData)handle, Syslog_delete );
    Tcl_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
bool
Syslog_Module( Tcl_Interp *interp ) {
    Tcl_Command command;

    command = Tcl_CreateObjCommand(interp, "Syslog", Syslog_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        return false;
    }

    return true;
}

app_init( Syslog_Module );

/* vim: set autoindent expandtab sw=4 : */
