
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

/** \file Tunnel.cc
 * \brief 
 *
 * including linux/if.h not net/if.h -- even though brutils uses net/if.h
 * since I needed linux/if.h for some other reason in Network.cc ??
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/socket.h>
// #include <net/if.h>
#include <linux/if.h>
#include <net/if_arp.h>
#include <linux/if_bridge.h>
#include <linux/sockios.h>

// Do I need these?
#include <sys/time.h>
#include <sys/ioctl.h>

#include <stdio.h>  // for sys_errlist
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <glob.h>

#include "Network.h"
#include "TCL_Fixup.h"

#include <string>

namespace { int debug = 0; }

/**
 */
Network::Tunnel::Tunnel( const char *name, int ordinal )
: _ordinal ( ordinal ), _interface( NULL ), _tunnel_interface( NULL ), _bridge( NULL ), _tunnel_up( false ) {
    _name = strdup( name );
    _port = 5000 + ordinal;

    write_config_file(true);	// Server configuration file
    write_config_file(false);	// Client configuration file
}

/**
 */
Network::Tunnel::~Tunnel() {
    if ( _name != NULL ) free(_name);
}

/**
 */
const char *
Network::Tunnel::create() {
    syslog( LOG_NOTICE, "create new tunnel '%s'", _name );

    return NULL;
}

/**
 */
const char *
Network::Tunnel::destroy() {
    syslog( LOG_NOTICE, "destroy tunnel '%s'", _name );

    return NULL;
}

/**
 */
const char *
Network::Tunnel::attach( Interface *interface ) {
    syslog( LOG_NOTICE, "attaching interface '%s' to tunnel '%s'", interface->name(), _name );

    return NULL;
}

/**
 */
const char *
Network::Tunnel::detach( Interface *interface ) {
    syslog( LOG_NOTICE, "detaching interface '%s' from tunnel '%s'", interface->name(), _name );

    return NULL;
}

/**
 */
bool Network::Tunnel::check_for_tunnel() {
    char fname[128];
    FILE *f;
    int count;
    int pid;
    char dirname[16];
    struct stat statbuf;

    sprintf( fname, "/var/run/vtund-%s.pid", _tunnel_interface->name() );	/* _interface->name() ? */
    f = fopen( fname, "r" );
    if ( f != NULL ) {
        count = fscanf( f, "%d", &pid );
        fclose( f );
        if ( count != 1 ) {
            sprintf( dirname, "/proc/%d", pid );
            if ( stat( dirname, &statbuf ) == 0 )
                return true;
        }
    }
    return false;
}

/**
 */
void Network::Tunnel::is_tunnel_attached() {
    char cmd[128];
    FILE *f;
    char buffer[1024];
    bool result = false;	// avoid setting _tunnel_up temporarily to false

    sprintf( cmd, "/usr/sbin/brctl show %s", _tunnel_interface->name() );
    f = popen( cmd, "r" );
    while (not feof( f ) ) {
        fgets( buffer, sizeof(buffer), f );
        if ( strlen( buffer ) > 0 )
            result = true;
    }
    pclose( f );
    _tunnel_up = result;
}

/**
 */
bool Network::Tunnel::is_up(Interface *interface) {
    char cmd[128];
    int status = 1;

// XXX I am not exactly sure why these two mechanisms are equivalent

    if (interface == _interface)
        return check_for_tunnel();
    else {
        sprintf( cmd, "/sbin/ifconfig %s", interface->name() );
        syslog( LOG_NOTICE, "*** Executing: \"%s\"", cmd);
//YYstatus = system( cmd );
    }
    return ( status == 0 );
}

/**
 */
void Network::Tunnel::add_to_bridge( Interface *interface ) {
    char cmd[128];
//YYint status = 1;
    if ( debug ) syslog( LOG_NOTICE, "adding tunnel '%s' interface '%s' to bridge", _name, interface->name() );

    int count = 0;
    while ( not interface->up() ) {
        count++;
        if ( count >= 10 ) {

// XXX this code in Spine is a hack -- we should have a separate
// method for adding the Tunnel interface vs. the business interface
// to the bridge so that we do not have to test for it here.

            if ( strncmp( interface->name(), "atun", 4 ) == 0 ) {
                syslog( LOG_WARNING, "Destroy tunnnel, since it never came up after 10s" );
	        destroy_tunnel();
            }
            return;
        }
        sleep( 1 );	/* XXX better not to sleep in Network Manager */
    }

// XXX should examine status and track state of whether the bridge has the interface added

    sprintf( cmd, "/usr/sbin/brctl addif %s %s", _bridge->name(), interface->name() );
//YYstatus = system( cmd );
    syslog( LOG_NOTICE, "*** Executing: \"%s\"", cmd);
}

/**
 */
void Network::Tunnel::delete_from_bridge( Interface *interface ) {
    char cmd[128];
//YYint status = 1;
    if ( debug ) syslog( LOG_NOTICE, "deleting tunnel '%s' interface '%s' from bridge", _name, interface->name() );

    sprintf( cmd, "/usr/sbin/brctl delif %s %s", _bridge->name(), interface->name() );
//YYstatus = system( cmd );
    syslog( LOG_NOTICE, "*** Executing: \"%s\"", cmd);
}

/**
 */
void Network::Tunnel::setup_tunnel() {
    char cmd[128];
//YYint status = 1;
    if ( debug ) syslog( LOG_NOTICE, "setting up tunnel '%s' '%s'", _name, _tunnel_interface->name() );

//EXT REF
//  if ( ( not SuperNova.nodes.peer_alive() ) or ( upgrade_from != NULL ) )
//     if ( is_tunnel_up() )
//         destroy_tunnel();

    if ( is_tunnel_up() )
        return;

    if ( Network::Manager::is_node0() ) {
// XXX should examine status and track state of whether the tunnel was successfully created (spine didn't)
        sprintf( cmd, "/usr/sbin/vtund -s -t 600 -f /etc/vtund-%s-server.conf -P %d", _tunnel_interface->name(), _port);
//YY    status = system( cmd );
        syslog( LOG_NOTICE, "*** Executing: \"%s\"", cmd);
    }

    if ( Network::Manager::is_node1() ) {
// XXX should examine status and track state of whether the tunnel was successfully created (spine didn't)
        sprintf( cmd, "/usr/sbin/vtund -t 600 -f /etc/vtund-%s.conf %s peer -P %d", _tunnel_interface->name(), _name, _port);
//YY    status = system( cmd );
        syslog( LOG_NOTICE, "*** Executing: \"%s\"", cmd);
    }

    sleep( 1 );	/* XXX was 0.3s - and better not to sleep in Network Manager */
}

/**
 */
void Network::Tunnel::destroy_tunnel() {
    char fname[128];
    FILE *f;
    int count;
    int pid;
    char dirname[16];
    struct stat statbuf;
    char cmd[128];
//YYint status = 1;

    if ( debug ) syslog( LOG_NOTICE, "destroying tunnel '%s' '%s'", _name, _tunnel_interface->name() );
    sprintf( fname, "/var/run/vtund-%s.pid", _tunnel_interface->name() );	/* _interface->name() ? */
    f = fopen( fname, "r" );
    if ( f != NULL ) {
        count = fscanf( f, "%d", &pid );
        fclose( f );
        if ( count != 1 ) {
            sprintf( dirname, "/proc/%d", pid );
            if ( stat( dirname, &statbuf ) == 0 ) {
                sprintf( cmd, "kill %d\n", pid );
//YY            status = system( cmd );
                syslog( LOG_NOTICE, "*** Executing: \"%s\"", cmd);
            }
            unlink( fname );
        } else
	    goto bad;
    }

bad:
    syslog( LOG_NOTICE, "Could not read the pidfile for %s - the daemon must have died prematurely.",
            _tunnel_interface->name() );	/* _interface->name() ? */
}

/**
 */
void Network::Tunnel::write_config_file(bool server) {
    char fname[128];
    FILE *fsrc, *fdst;
    char buffer[1024];
    unsigned int pos;

    using std::string;

    if (server)
        sprintf( fname, "/etc/vtund.conf.server.templ" );
    else
        sprintf( fname, "/etc/vtund.conf.templ" );
    fsrc = fopen( fname, "r" );

    if (server)
        sprintf( fname, "/etc/vtund-%s-server.conf", _tunnel_interface->name() );	/* _interface->name() ? */
    else
        sprintf( fname, "/etc/vtund-%s.conf", _tunnel_interface->name() );	/* _interface->name() ? */
    fdst = fopen( fname, "w" );

    while (not feof( fsrc ) ) {
        fgets( buffer, sizeof(buffer), fsrc );
        string s( buffer );
        if ( ( pos = s.find( "%{VTUN_DEVICE}%" ) ) != string::npos ) {
            s.erase( pos, strlen( "%{VTUN_DEVICE}%" ) );
            s.insert( pos, _tunnel_interface->name() );
        }
        if ( ( pos = s.find( "%{VTUN_PIDFILE}%" ) ) != string::npos ) {
            s.erase( pos, strlen( "%{VTUN_PIDFILE}%" ) );
            sprintf( fname, "/var/run/vtund-%s.pid", _tunnel_interface->name() );
            s.insert( pos, fname );
        }
        if ( ( pos = s.find( "tunnel {" ) ) != string::npos ) {
            s.erase(pos, strlen( "tunnel {" ) );
            sprintf( fname, "%s {", _tunnel_interface->name() );
        }

        if (server) {
            char MAC[6];
            char bigmac[18];
            strncpy( MAC, (char *)_interface->mac(), sizeof(MAC) );
            sprintf( bigmac, "%02x:%02x:%02x:%02x:%02x:%02x", 
                     MAC[0], MAC[1], MAC[2], MAC[3], MAC[4], MAC[5] );

            if ( ( pos = s.find( "%{BRIDGE_MAC}%" ) ) != string::npos ) {
                s.erase( pos, strlen( "%{BRIDGE_MAC}%" ) );
                sprintf( fname, "/var/run/vtund-%s.pid", bigmac );
                s.insert( pos, fname );
            }
        }   

        fputs( s.c_str(), fdst);
    }

    fclose (fsrc);
    fclose (fdst);
}


/**
 */
static int
Tunnel_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace Network;
    Tunnel *tunnel = (Tunnel *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(tunnel)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Tcl_SetResult( interp, (char *)"Network::Tunnel", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "create") ) {
        const char *result = tunnel->create();
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Tcl_SetResult( interp, (char *)result, TCL_STATIC );
        return TCL_ERROR;
    }

    /**
     * The destroy sub-command for a tunnel object destroys the kernels
     * tunnel object.  The program tunnel object remains, holding the
     * configuration information etc.  It can be used to recreate the
     * tunnel.
     */
    if ( Tcl_StringMatch(command, "destroy") ) {
        const char *result = tunnel->destroy();
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Tcl_SetResult( interp, (char *)result, TCL_STATIC );
        return TCL_ERROR;
    }

    /**
     * The attach sub-command attaches an Interface to this tunnel.
     */
    if ( Tcl_StringMatch(command, "attach") ) {
        if ( objc != 3 ) {
            Tcl_ResetResult( interp );
            Tcl_WrongNumArgs( interp, 1, objv, "attach <interface>" );
            return TCL_ERROR;
        }

        Interface *interface;
        void *p = (void *)&(interface); // avoid strict aliasing errors in the compiler
        if ( Tcl_GetLongFromObj(interp,objv[2],(long*)p) != TCL_OK ) {
            // we could do some additional err checks on the data -- ie -- numeric...
	    Tcl_SetResult( interp, (char *)"invalid interface object", TCL_STATIC );
            return TCL_ERROR;
        }

        const char *result = tunnel->attach( interface );
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Tcl_SetResult( interp, (char *)result, TCL_STATIC );
        return TCL_ERROR;
    }

    /**
     * The detach sub-command detaches an Interface from this tunnel.
     */
    if ( Tcl_StringMatch(command, "detach") ) {
        if ( objc != 3 ) {
            Tcl_ResetResult( interp );
            Tcl_WrongNumArgs( interp, 1, objv, "detach <interface>" );
            return TCL_ERROR;
        }

        Interface *interface;
        void *p = (void *)&(interface); // avoid strict aliasing errors in the compiler
        if ( Tcl_GetLongFromObj(interp,objv[2],(long*)p) != TCL_OK ) {
            // we could do some additional err checks on the data -- ie -- numeric...
	    Tcl_SetResult( interp, (char *)"invalid interface object", TCL_STATIC );
            return TCL_ERROR;
        }

        const char *result = tunnel->detach( interface );
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Tcl_SetResult( interp, (char *)result, TCL_STATIC );
        return TCL_ERROR;
    }

    Tcl_SetResult( interp, (char *)"Unknown command for Tunnel object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
Tunnel_delete( ClientData data ) {
    using namespace Network;
    Tunnel *message = (Tunnel *)data;
    delete message;
}

/**
 */
static int
Tunnel_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    int ordinal;

    if ( objc != 3 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR;
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_GetIntFromObj( interp, objv[2], &ordinal ) != TCL_OK ) {
        // we could do some additional err checks on the data -- ie -- numeric...
        Tcl_SetResult( interp, (char *)"invalid ordinal", TCL_STATIC );
        return TCL_ERROR;
    }
    
    using namespace Network;

    Tunnel *object = new Tunnel( name, ordinal );
    Tcl_CreateObjCommand( interp, name, Tunnel_obj, (ClientData)object, Tunnel_delete );
    Tcl_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
bool Network::Tunnel::Initialize( Tcl_Interp *interp ) {

    Tcl_Command command;

    Tcl_Namespace *ns = Tcl_CreateNamespace(interp, "Network::Tunnel", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    if ( Tcl_LinkVar(interp, "Network::Tunnel::debug", (char *)&debug, TCL_LINK_INT) != TCL_OK ) {
        syslog( LOG_ERR, "failed to link Network::Tunnel::debug" );
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Network::Tunnel::new", Tunnel_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    return true;
}

/* vim: set autoindent expandtab sw=4 : */
