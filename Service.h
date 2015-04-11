
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

/** \file Service.h
 * \brief 
 *
 */

#ifndef _SERVICE_H_
#define _SERVICE_H_

#include <tcl.h>
#include "Thread.h"
#include "Channel.h"

/**
 * run() should not be able to execute unless initialized
 * or maybe when run -- initialize is called -- then it could 
 * be private
 */
class Service : public Thread {
private:
    Tcl_Interp *interp;
    const char *service_name;
    char logfilename[80];
    char rundir[80];
    Channel *channel;
    int facility;
public:
    Service( const char * );
    virtual ~Service();
    bool initialize( int, char **, Tcl_AppInitProc * );
    virtual void run();
    const char *name() const { return service_name; }
    void set_facility( int );
    bool add_command( char *, Tcl_ObjCmdProc *, ClientData );
    bool load_command( const char * );
    bool load_commands( const char * );
    bool load_directory( const char * );
    bool load_file( const char * );
    // this is here ONLY temporarily while I transition ekg/interface to the service
    // interface -- it calls the tcl interp directly -- which I do not want
    Tcl_Interp *interpreter() { return interp; }
};

#endif

/* vim: set autoindent expandtab sw=4 : */
