
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

/** \file Bridge.cc
 * \brief 
 *
 * including linux/if.h not net/if.h -- even though brutils uses net/if.h
 * since I needed linux/if.h for some other reason in Network.cc ??
 */

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

namespace { int debug = 0; }

/**
 * \todo need the interface object for the bridge ...
 *
 * other constructors:
 * - With Interface arg to create new bridge ?
 * - 
 */
Network::Bridge::Bridge( const char *bridge_name ) {
    _name = strdup(bridge_name);
}

/**
 */
Network::Bridge::~Bridge() {
    if ( _name != NULL ) free(_name);
}

/**
 */
const char *
Network::Bridge::create() {
    syslog( LOG_NOTICE, "create new bridge '%s'", _name );

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if ( sock < 0 ) {
        // how to report error?
        // syslog means it doesn't get to TCL
        return "failed to create bridge socket";
    }

    int result = ioctl(sock, SIOCBRADDBR, _name);
    int error = errno;

    close( sock );
    if ( result < 0 ) {
        if ( error == EEXIST ) goto bring_up;
        syslog( LOG_ERR, "Bridge: create '%s'", strerror(error) );
        return "failed to create bridge";
    }

bring_up:

    char buffer[128];
    sprintf( buffer, "/sbin/ip link set %s up", _name );
    if ( system(buffer) < 0 ) {
        syslog( LOG_NOTICE, "%s: bridge refused to bring up link", _name );
    }

    return NULL;
}

/**
 */
const char *
Network::Bridge::destroy() {
    // create the socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if ( sock < 0 ) {
        // how to report error?
        // syslog means it doesn't get to TCL
        return "failed to create bridge socket";
    }
    // call the ioctl
    int result = ioctl(sock, SIOCBRDELBR, _name);
    // close the socket
    close( sock );
    if ( result < 0 ) {
        return "failed to delete bridge";
    }
    return NULL;
}

/**
 * Need the index of this bridge
 */
const char *
Network::Bridge::set_mac_address( unsigned char *mac ) {
    syslog( LOG_NOTICE, "set MAC address of bridge '%s' to %02x:%02x:%02x:%02x:%02x:%02x",
                        _name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );

    // call ioctl with interface index [that->index()]
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if ( sock < 0 ) {
        return "failed to create bridge socket";
    }

    struct ifreq request;
    memset( &request, 0, sizeof(request) );

    // request.ifr_hwaddr.sa_family = AF_PACKET;
    request.ifr_hwaddr.sa_family = ARPHRD_ETHER;
    request.ifr_hwaddr.sa_data[ 0] = mac[0];
    request.ifr_hwaddr.sa_data[ 1] = mac[1];
    request.ifr_hwaddr.sa_data[ 2] = mac[2];
    request.ifr_hwaddr.sa_data[ 3] = mac[3];
    request.ifr_hwaddr.sa_data[ 4] = mac[4];
    request.ifr_hwaddr.sa_data[ 5] = mac[5];

    strncpy( request.ifr_name, _name, IFNAMSIZ );

    char buffer[60];
    sprintf( buffer, "/sys/class/net/%s/bridge/address_locked", _name );
    FILE *f = fopen( buffer, "w" );
    if ( f != NULL ) {
        fprintf( f, "%d", 0 );
    }

    int result = ioctl(sock, SIOCSIFHWADDR, &request);
    const char *error = strerror(errno);
    close( sock );

    if ( f != NULL ) {
        rewind( f );
        fprintf( f, "%d", 1 );
        fclose( f );
    }

    if ( result < 0 ) {
        syslog( LOG_NOTICE, "failed to set MAC address for bridge '%s': %s", _name, error );
        return "failed to set MAC address";
    }
    return NULL;
}

/**
 */
bool
Network::Bridge::lock_address() {
    char buffer[60];
    sprintf( buffer, "/sys/class/net/%s/bridge/address_locked", _name );
    FILE *f = fopen( buffer, "r+" );
    if ( f == NULL )  return false;
    int locked;
    fscanf( f, "%d", &locked );
    if ( locked == 0 ) {
        rewind( f );
        fprintf( f, "%d", 1 );
        syslog( LOG_NOTICE, "locking %s address", _name );
    }
    fclose( f );
    return true;
}

/**
 */
bool
Network::Bridge::unlock_address() {
    char buffer[60];
    sprintf( buffer, "/sys/class/net/%s/bridge/address_locked", _name );
    FILE *f = fopen( buffer, "r+" );
    if ( f == NULL )  return false;
    int locked;
    fscanf( f, "%d", &locked );
    if ( locked == 1 ) {
        rewind( f );
        fprintf( f, "%d", 0 );
        syslog( LOG_NOTICE, "unlocking %s address", _name );
    }
    fclose( f );
    return true;
}

/**
 */
const char *
Network::Bridge::add( Interface *that ) {
    syslog( LOG_NOTICE, "capturing interface '%s' in bridge '%s'", that->name(), _name );

    // call ioctl with interface index [that->index()]
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if ( sock < 0 ) {
        return "failed to create bridge socket";
    }

    struct ifreq request;
    memset( &request, 0, sizeof(request) );
    strncpy( request.ifr_name, _name, IFNAMSIZ );
    request.ifr_ifindex = that->index();

    int result = ioctl(sock, SIOCBRADDIF, &request);
    const char *error = strerror(errno);
    close( sock );

    if ( result < 0 ) {
        syslog( LOG_NOTICE, "failed to capture interface '%s' in bridge '%s': %s",
                            that->name(), _name, error );
        return "failed to add interface to bridge";
    }
    return NULL;
}

/**
 */
const char *
Network::Bridge::capture( Interface *that ) { return add(that); }

/**
 * This may want to be by interface index...
 */
const char *
Network::Bridge::remove( Interface *that ) {
    syslog( LOG_NOTICE, "removing interface '%s' from bridge '%s'", that->name(), _name );

    // call ioctl with interface index [that->index()]
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if ( sock < 0 ) {
        return "failed to create bridge socket";
    }

    struct ifreq request;
    memset( &request, 0, sizeof(request) );
    strncpy( request.ifr_name, _name, IFNAMSIZ );
    request.ifr_ifindex = that->index();

    int result = ioctl(sock, SIOCBRDELIF, &request);
    const char *error = strerror(errno);
    close( sock );

    if ( result < 0 ) {
        syslog( LOG_NOTICE, "failed to remove interface '%s' from bridge '%s': %s",
                            that->name(), _name, error );
        return "failed to remove interface from bridge";
    }
    return NULL;
}

/**
 */
unsigned long
Network::Bridge::forward_delay() {
    unsigned long value = 0;
    char path[256];
    snprintf( path, sizeof(path), "/sys/class/net/%s/bridge/forward_delay", name() );
    FILE *f = fopen(path, "r");
    if ( f == NULL ) return 0xdeadbeef;
    fscanf( f, "%ld\n", &value );
    fclose( f );
    return value / HZ;
}

/**
 */
bool
Network::Bridge::forward_delay( unsigned long value ) {
    char path[256];
    snprintf( path, sizeof(path), "/sys/class/net/%s/bridge/forward_delay", name() );
    FILE *f = fopen(path, "w");
    if ( f == NULL ) return false;
    fprintf( f, "%ld\n", value * HZ );
    fclose( f );
    return true;
}

/**
 * True if this bridge has a tunnel interface captured.
 */
    // for each net/*/brport if its bridge is my brdige interface
    // glob /sys/class/net/*/brport/bridge
    // if realname of link is /sys/class/net/<myname> ...
bool Network::Bridge::is_tunnelled() const {
    bool result = false;

    // syslog( LOG_NOTICE, "check if '%s' is tunnelled", name() );

    char port_path[1024];
    char bridge_path[1024];
    sprintf( bridge_path, "../../../../class/net/%s", name() );

    // syslog( LOG_NOTICE, "check if '%s' is tunnelled", bridge_path );

    glob_t paths;
    memset(&paths, 0, sizeof(paths));

    glob( "/sys/class/net/tun*/brport/bridge", GLOB_NOSORT, NULL, &paths );
    for ( size_t i = 0 ; i < paths.gl_pathc ; i++ ) {
        int count = readlink( paths.gl_pathv[i], port_path, sizeof(port_path) );
        if ( count == -1 ) continue;
        port_path[count] = '\0';
        syslog( LOG_NOTICE, "check if '%s' is tunnelled through '%s'", bridge_path, port_path );

        if ( strcmp(port_path, bridge_path) == 0 ) {
            syslog( LOG_NOTICE, "I (%s) am tunnelled through '%s'", bridge_path, paths.gl_pathv[i] );
            result = true;
            break;
        }
    }
    globfree( &paths );

    return result;
}

/**
 */
static int
Bridge_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    using namespace Network;
    Bridge *bridge = (Bridge *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(bridge)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Tcl_SetResult( interp, "Network::Bridge", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "create") ) {
        const char *result = bridge->create();
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Tcl_SetResult( interp, (char *)result, TCL_STATIC );
        return TCL_ERROR;
    }

    /**
     * The destroy sub-command for a bridge object destroys the kernels
     * bridge object.  The program bridge object remains, holding the
     * configuration information etc.  It can be used to recreate the
     * bridge.
     */
    if ( Tcl_StringMatch(command, "destroy") ) {
        const char *result = bridge->destroy();
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Tcl_SetResult( interp, (char *)result, TCL_STATIC );
        return TCL_ERROR;
    }

    /**
     * The add sub-command adds an Interface to this bridge.  The interface
     * is then "captured" by the bridge, and is not usable outside of the
     * bridge.  Linux is supposed to support using the interface outside of
     * the bridge, but I have not found how to make this work reliably.
     */
    if ( Tcl_StringMatch(command, "add") or Tcl_StringMatch(command, "capture") ) {
        if ( objc != 3 ) {
            Tcl_ResetResult( interp );
            Tcl_WrongNumArgs( interp, 1, objv, "add <interface>" );
            return TCL_ERROR;
        }

        Interface *interface;
        void *p = (void *)&(interface); // avoid strict aliasing errors in the compiler
        if ( Tcl_GetLongFromObj(interp,objv[2],(long*)p) != TCL_OK ) {
            // we could do some additional err checks on the data -- ie -- numeric...
            Tcl_SetResult( interp, "invalid interface object", TCL_STATIC );
            return TCL_ERROR;
        }

        const char *result = bridge->add( interface );
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Tcl_SetResult( interp, (char *)result, TCL_STATIC );
        return TCL_ERROR;
    }

    // get the interface from the cmd line
    if ( Tcl_StringMatch(command, "remove") ) {
        if ( objc != 3 ) {
            Tcl_ResetResult( interp );
            Tcl_WrongNumArgs( interp, 1, objv, "remove <interface>" );
            return TCL_ERROR;
        }

        Interface *interface;
        void *p = (void *)&(interface); // avoid strict aliasing errors in the compiler
        if ( Tcl_GetLongFromObj(interp,objv[2],(long*)p) != TCL_OK ) {
            // we could do some additional err checks on the data -- ie -- numeric...
            Tcl_SetResult( interp, "invalid interface object", TCL_STATIC );
            return TCL_ERROR;
        }

        const char *result = bridge->remove( interface );
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Tcl_SetResult( interp, (char *)result, TCL_STATIC );
        return TCL_ERROR;
    }

    Tcl_SetResult( interp, "Unknown command for Bridge object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
Bridge_delete( ClientData data ) {
    using namespace Network;
    Bridge *message = (Bridge *)data;
    delete message;
}

/**
 */
static int
Bridge_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name" );
        return TCL_ERROR;
    }

    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    using namespace Network;
    Bridge *object = new Bridge( name );
    Tcl_CreateObjCommand( interp, name, Bridge_obj, (ClientData)object, Bridge_delete );
    Tcl_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 */
bool Network::Bridge::Initialize( Tcl_Interp *interp ) {

    Tcl_Command command;

    Tcl_Namespace *ns = Tcl_CreateNamespace(interp, "Network::Bridge", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    if ( Tcl_LinkVar(interp, "Network::Bridge::debug", (char *)&debug, TCL_LINK_INT) != TCL_OK ) {
        syslog( LOG_ERR, "failed to link Network::Bridge::debug" );
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Network::Bridge::new", Bridge_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // syslog ?? want to report TCL Error
        return false;
    }

    return true;
}


/*
 * vim:autoindent
 * vim:expandtab
 */
