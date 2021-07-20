
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

/** \file Container.cc
 * \brief 
 *
 */

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

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
 * Kernel::daemonize /path/to/command arg1 arg2 ...
 */
int
Container::Layer::Layer( const char *command, int argc, char **argv )
{
    // char *command = Tcl_GetStringFromObj( objv[1], NULL );

    if ( access(command, X_OK) < 0 ) {
        return -1; // Svc_SetResult( interp, "cannot execute command", TCL_STATIC );
    }

    return 0;
}

/* vim: set autoindent expandtab sw=4 : */
