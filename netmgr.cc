
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

/** \file netmgr.cc
 * \brief A network monitor/heartbeat service for Linux.
 *
 * \todo send events to spine (new file)
 *
 * \todo change diastole multicast to send UUID in packet (pulse.cc)
 * \todo DHCP?
 * \todo zeroconf for private?
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <syslog.h>
#include <errno.h>
#include <tcl.h>

#include <valgrind/memcheck.h>

#include "util.h"
#include "UUID.h"
#include "BIOS.h"
#include "Thread.h"
#include "Service.h"
#include "Network.h"
#include "NetLink.h"
#include "ICMPv6.h"
#include "pulse.h"

/**
 * strdup the source string and then remote spaces, etc
 */
char *sanitize( const char *s ) {
    if ( s == NULL ) return strdup("INVALID");
    char *result = strdup(s);
    char *r = result;
    while ( *r ) {
        switch (*r) {
        case ' ' : *r = '-'; break;
        case '/' : *r = '-'; break;
        }
        r++;
    }
    return result;
}

/**
 * Look for a platform file based on the baseboard name and version or product name
 * in that order.  The most specific found is loaded to determine the network
 * device naming scheme.  If there is no config found, then report an error to
 * syslog so we know which one to add.
 *
 * The names extracted from SMBIOS are "sanitized" since they contain spaces and
 * other characters that are problematic for pathnames.
 */
void LoadPlatformFile( Tcl_Interp *interp, SMBIOS::System& system, SMBIOS::BaseBoard& baseboard ) {
    char *system_product_name     = sanitize( system.product_name() );
    char *base_board_product_name = sanitize( baseboard.product_name() );
    char *base_board_version      = sanitize( baseboard.version() );

    char filename[256];
    snprintf( filename, sizeof(filename), "/etc/netmgr/platform/%s-%s",
              base_board_product_name, base_board_version );

    if ( access(filename, R_OK) == 0 ) {
        syslog( LOG_NOTICE, "evaluting platform board revision file '%s'", filename );
        if ( Tcl_EvalFile(interp, filename) != TCL_OK ) {
            // add TCL interp error string
            syslog( LOG_WARNING, "error reading platform file" );
        }
        goto done;
    }

    snprintf( filename, sizeof(filename), "/etc/netmgr/platform/%s", system_product_name );
    for ( char *c = filename ; *c ; c++ ) {
        if ( *c == ' ' ) *c = '-';
    }

    if ( access(filename, R_OK) == 0 ) {
        syslog( LOG_NOTICE, "evaluating platform file '%s'", filename );
        if ( Tcl_EvalFile(interp, filename) != TCL_OK ) {
            // add TCL interp error string
            syslog( LOG_WARNING, "error reading platform file" );
        }
        goto done;
    }

    syslog( LOG_ERR, "There is no platform config for system '%s', baseboard '%s-%s'",
                     system_product_name, base_board_product_name, base_board_version );

done:
    free( system_product_name );
    free( base_board_product_name );
    free( base_board_version );
}

/**
 */
void LoadLocalConfig( Tcl_Interp *interp ) {
    static char *filename = "/etc/netmgr/platform/local";

    if ( access(filename, R_OK) == 0 ) {
        syslog( LOG_NOTICE, "evaluating local config file '%s'", filename );
        if ( Tcl_EvalFile(interp, filename) != TCL_OK ) {
            // add TCL interp error string
            syslog( LOG_WARNING, "error reading local config file" );
        }
    }
}

/**
 */
UUID uuid;

void LoadUUID( Tcl_Interp *interp ) {
    FILE *f;
    int count;
    char buffer[128];
    static char *config = "/etc/netmgr/uuid";

    if ( access(config, R_OK) == 0 ) {
        f = fopen( config, "r" );
        count = fscanf(f, "%s", buffer);
        fclose( f );
        if ( count == 1 ) goto done;
    }

    syslog( LOG_NOTICE, "UUID not configured yet, assigning UUID to netmgr" );
    f = fopen( "/proc/sys/kernel/random/uuid", "r" );
    count = fscanf(f, "%s", buffer);
    fclose( f );
    if ( count != 1 ) {
        syslog( LOG_ERR, "ERROR: failed to assign new UUID" );
        exit( 1 );
    }

    f = fopen(config, "w");
    if ( f == NULL ) {
        syslog( LOG_WARNING, "WARNING: Could not save netmgr UUID config" );
    } else {
        fprintf( f, "%s\n", buffer );
        fclose( f );
    }

done:
    uuid.set( buffer );
    syslog( LOG_NOTICE, "UUID set to '%s'", uuid.to_s() );
    // could use LinKVar and make this readonly
    Tcl_SetVar(interp, "UUID", uuid.to_s(), TCL_GLOBAL_ONLY);
}

/**
 */
static bool cluster_name_unknown = true;
static int clustername_err_rate = 0;

void determine_cluster_name() {
    char buffer[128];
    int count;
    if ( getdomainname(buffer, sizeof buffer) == 0 ) {
        if ( (strlen(buffer) > 0) and (strcmp(buffer,"(none)") != 0) ) {
            syslog( LOG_NOTICE, "cluster name is %s", buffer );
            cluster_name_unknown = false;
            return;
        }
    }

    FILE *cluster = fopen( "/etc/sysconfig/cluster_name", "r" );
    if ( cluster == NULL ) {
        if ( (clustername_err_rate++ & 0xF) == 0 ) {
            syslog( LOG_WARNING, "WARNING: no cluster name present" );
        }
        return;
    }
    count = fscanf(cluster, "export CLUSTERNAME=%s", buffer);
    fclose( cluster );

    if ( count != 1 ) {
        syslog( LOG_WARNING, "WARNING: failed to parse cluster name file" );
        return;
    }

    if ( setdomainname(buffer, strlen(buffer)) == 0 ) {
        syslog( LOG_NOTICE, "set cluster name to %s", buffer );
    } else {
        syslog( LOG_WARNING, "WARNING: failed to set cluster name to %s", buffer );
    }

    cluster_name_unknown = false;
}

int advertise_interval = 3;

/**
 * Spine Interface command (for debugging purposes)
 *
 * house
 * % netmgr { ask | tell } ping
 *
 */
static int
ping_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    syslog( LOG_NOTICE, "ping_cmd" );

    Network::Manager *manager = (Network::Manager *)data;
    if ( objc != 1 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "command" );
        return TCL_ERROR;
    }
    if ( manager == NULL ) {
        Tcl_SetResult( interp, "Hmm.  ClientData is NULL", TCL_STATIC );
        return TCL_ERROR;
    }

    char *result = manager->ping();

    if ( result == NULL ) {
        Tcl_ResetResult( interp );
    } else {
        Tcl_SetResult( interp, result, TCL_STATIC );
    }
    return TCL_OK;
}

/**
 * Spine Interface command (for debugging purposes)
 *
 * house
 * % netmgr { ask | tell } error
 *
 */
static int
error_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    syslog( LOG_NOTICE, "error_cmd" );

    Network::Manager *manager = (Network::Manager *)data;
    if ( objc != 1 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "command" );
        return TCL_ERROR;
    }
    if ( manager == NULL ) {
        Tcl_SetResult( interp, "Hmm.  ClientData is NULL", TCL_STATIC );
        return TCL_ERROR;
    }

    Tcl_SetResult( interp, "Error", TCL_STATIC );
    return TCL_ERROR;
}

/**
 * Spine Interface command (for debugging purposes)
 *
 * house
 * % netmgr { ask | tell } dump
 *
 * (output will appear in /var/log/netmgr.log)
 */
static int
dump_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    syslog( LOG_NOTICE, "dump_cmd" );

    Network::Manager *manager = (Network::Manager *)data;
    if ( objc != 1 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "command" );
        return TCL_ERROR;
    }
    if ( manager == NULL ) {
        Tcl_SetResult( interp, "Hmm.  ClientData is NULL", TCL_STATIC );
        return TCL_ERROR;
    }

    manager->dump();

    Tcl_ResetResult( interp );
    return TCL_OK;
}

/**
 */
static void
construct_remote_interface_name( Network::Peer *neighbor, char *name, int size ) {

    if ( neighbor->name() != NULL ) {
        strncpy( name, neighbor->name(), size );
    } else {
        int ordinal = neighbor->ordinal();
        if ( neighbor->is_private() ) {
            if ( ordinal & 0x40 ) {
                int slot = ( ordinal & 0x3c ) >> 2;
                int port = ( ordinal & 0x03);
                snprintf( name, size, "sync_pci%dp%d", slot, port );
            } else {
                snprintf( name, size, "priv%d", ordinal );
            }
        } else {
            snprintf( name, size, "ibiz%d", ordinal );
        }
    }
}

class DumpNode : public Network::NodeIterator {
public:
    DumpNode() {}
    virtual ~DumpNode() {}

    // called for each node
    virtual int operator() ( Network::Node& node ) {

        syslog( LOG_NOTICE, "Node %s ordinal %d %s", node.uuid().to_s(),
                node.ordinal(), node.is_partner() ? "[partner]" : "" );
        return 1;
    }
};

class DumpInterface : public Network::InterfaceIterator {
public:
    DumpInterface() {}
    virtual ~DumpInterface() {}

    // called for each interface known to netmgr
    virtual int operator() ( Network::Interface& interface ) {

        unsigned char *mac = interface.mac();

        syslog( LOG_NOTICE, "Interface %s MAC %02x:%02x:%02x:%02x:%02x:%02x%s%s%s%s%s%s",
                interface.name(), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                interface.is_physical() ? " [physical]" : "",
                interface.is_bridge() ? " [bridge]" : "",
                interface.is_private() ? " [private]" : "",
                interface.is_business() ? " [business]" : "",
                interface.is_sync() ? " [sync]" : "",
                interface.removed() ? " (removed)" : "" );
        return 1;
    }
};

class DumpNeighbor : public Network::NeighborIterator {
    Network::Interface *interface;
public:
    DumpNeighbor( Network::Interface *interface)
    : interface(interface) {}
    virtual ~DumpNeighbor() {}

    // called for each neighbor on interface
    virtual int operator() ( Network::Peer& neighbor ) {

        char remote_interface_name[32];
        construct_remote_interface_name( &neighbor, remote_interface_name,
                                         sizeof(remote_interface_name) );

        char buffer[80];
        struct in6_addr address;
        neighbor.copy_address( &address );
        const char *addr = inet_ntop(AF_INET6, &address, buffer, sizeof(buffer));

        int seconds = neighbor.seconds_since_last_update();

        syslog( LOG_NOTICE, "  Neighbor %s(%s)%s - node %s - %d second%s since last update",
                remote_interface_name, addr, neighbor.is_partner() ? " [partner]" : "",
                (neighbor.node() != NULL) ? neighbor.node()->uuid().to_s() : "not set",
                seconds, ( seconds == 1 ) ? "" : "s" );
        return 1;
    }
};

class DumpInterfaceNeighbors : public Network::InterfaceIterator {
public:
    DumpInterfaceNeighbors() {}
    virtual ~DumpInterfaceNeighbors() {}

    // called for each interface known to netmgr
    virtual int operator() ( Network::Interface& interface ) {

        syslog( LOG_NOTICE, "Interface %s neighbors:", interface.name() );
        DumpNeighbor callback( &interface );

        int dumped_neighbors = interface.each_neighbor( callback );
        return dumped_neighbors;
    }
};

/**
 * Spine Interface command (for debugging purposes)
 *
 * house
 * % netmgr { ask | tell } neighbors
 *
 * (output will appear in /var/log/netmgr.log)
 */
static int
neighbors_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    syslog( LOG_NOTICE, "neighbors_cmd" );

    Network::Monitor *monitor = (Network::Monitor *)data;
    if ( objc != 1 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "command" );
        return TCL_ERROR;
    }
    if ( monitor == NULL ) {
        Tcl_SetResult( interp, "Hmm.  ClientData is NULL", TCL_STATIC );
        return TCL_ERROR;
    }

    DumpInterfaceNeighbors callback;
    int dumped_neighbors = monitor->each_interface( callback );
    syslog( LOG_NOTICE, "dumped %d neighbors", dumped_neighbors );

    Tcl_ResetResult( interp );
    return TCL_OK;
}

/**
 * Spine Interface command (for debugging purposes)
 *
 * house
 * % netmgr { ask | tell } nodes
 *
 * (output will appear in /var/log/netmgr.log)
 */
static int
nodes_cmd( ClientData data, Tcl_Interp *interp,
           int objc, Tcl_Obj * CONST *objv )
{
    syslog( LOG_NOTICE, "nodes_cmd" );

    Network::Monitor *monitor = (Network::Monitor *)data;
    if ( objc != 1 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "command" );
        return TCL_ERROR;
    }
    if ( monitor == NULL ) {
        Tcl_SetResult( interp, "Hmm.  ClientData is NULL", TCL_STATIC );
        return TCL_ERROR;
    }

    DumpNode callback;
    int dumped_nodes = monitor->each_node( callback );
    syslog( LOG_NOTICE, "dumped %d nodes", dumped_nodes );

    Tcl_ResetResult( interp );
    return TCL_OK;
}

/**
 * Spine Interface command (for debugging purposes)
 *
 * house
 * % netmgr { ask | tell } interfaces
 *
 * (output will appear in /var/log/netmgr.log)
 */
static int
interfaces_cmd( ClientData data, Tcl_Interp *interp,
                int objc, Tcl_Obj * CONST *objv )
{
    syslog( LOG_NOTICE, "interfaces_cmd" );

    Network::Monitor *monitor = (Network::Monitor *)data;
    if ( objc != 1 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "command" );
        return TCL_ERROR;
    }
    if ( monitor == NULL ) {
        Tcl_SetResult( interp, "Hmm.  ClientData is NULL", TCL_STATIC );
        return TCL_ERROR;
    }

    DumpInterface callback;
    int dumped_interfaces = monitor->each_interface( callback );
    syslog( LOG_NOTICE, "dumped %d interfaces", dumped_interfaces );

    Tcl_ResetResult( interp );
    return TCL_OK;
}

/**
 * Spine Interface command (for debugging purposes)
 *
 * house
 * % netmgr { ask | tell } insert_node <uuid> <ordinal>
 *
 * Insert an entry into the node_table (to test problems with a full table).
 */
static int
insert_node_cmd( ClientData data, Tcl_Interp *interp,
                 int objc, Tcl_Obj * CONST *objv )
{
    syslog( LOG_NOTICE, "insert_node_cmd" );

    Network::Monitor *monitor = (Network::Monitor *)data;
    if ( objc != 3 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "command" );
        return TCL_ERROR;
    }
    if ( monitor == NULL ) {
        Tcl_SetResult( interp, "Hmm.  ClientData is NULL", TCL_STATIC );
        return TCL_ERROR;
    }
    char *node_uuid = Tcl_GetStringFromObj( objv[1], NULL );
    if ( ( node_uuid == NULL ) or ( strlen( node_uuid ) == 0 ) ) {
        Tcl_SetResult( interp, "Invalid node UUID (insert_node)", TCL_STATIC );
        return TCL_ERROR;
    }
    int node_ordinal;
    if ( Tcl_GetIntFromObj( interp, objv[2], &node_ordinal ) != TCL_OK ) {
        Tcl_SetResult( interp, "Invalid node ordinal (insert_node)", TCL_STATIC ) ;
        return TCL_ERROR;
    }

    UUID uuid( node_uuid );
    Network::Node *node = monitor->intern_node(uuid);
    if ( node == NULL ) {
        Tcl_SetResult( interp, "unable to intern node", TCL_STATIC );
        return TCL_ERROR;
    }
    node->ordinal( node_ordinal );

    Tcl_ResetResult( interp );
    return TCL_OK;
}

/**
 * Spine Interface command
 * Creates a shared network object
 *
 * house
 * % netmgr { ask | tell } create <name> <ordinal> [ <local_interface_name> [ <remote_interface_name> ] ]
 *
 * The remote interface name is for reference only, it is not actually used
 * for anything inside the network service.  This is because with the new link
 * pairing mechanism, the names of the interfaces are not necessarily the same.
 *
 */
static int
create_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    syslog( LOG_NOTICE, "create_cmd" );

    Network::Manager *manager = (Network::Manager *)data;
    if ( (objc != 3) and ( objc != 4) and ( objc != 5 ) ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "command" );
        return TCL_ERROR;
    }
    if ( manager == NULL ) {
        Tcl_SetResult( interp, "Hmm.  ClientData is NULL", TCL_STATIC );
        return TCL_ERROR;
    }
    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    if ( ( name == NULL ) or ( strlen( name ) == 0 ) ) {
        Tcl_SetResult( interp, "Invalid shared network name (create)", TCL_STATIC );
        return TCL_ERROR;
    }
    int ordinal;
    if ( Tcl_GetIntFromObj( interp, objv[2], &ordinal ) != TCL_OK ) {
        Tcl_SetResult( interp, "Invalid ordinal number (create)", TCL_STATIC ) ;
        return TCL_ERROR;
    }
    char *local_interface_name;
    if ( objc >= 4 )
        local_interface_name = Tcl_GetStringFromObj( objv[3], NULL );
    else
        local_interface_name = NULL;
//    if ( ( local_interface_name == NULL ) or ( strlen( local_interface_name ) == 0 ) ) {
//        Tcl_SetResult( interp, "Invalid local interface name (create)", TCL_STATIC );
//        return TCL_ERROR;
//    }
    char *remote_interface_name;
    if ( objc == 5 )
        remote_interface_name = Tcl_GetStringFromObj( objv[4], NULL );
    else
        remote_interface_name = NULL;
//    if ( ( local_interface_name == NULL ) or ( strlen( local_interface_name ) == 0 ) ) {
//        Tcl_SetResult( interp, "Invalid remote interface name (create)", TCL_STATIC );
//        return TCL_ERROR;
//    }
    if ( ( remote_interface_name != NULL ) and ( strlen( remote_interface_name ) == 0 ) )
        remote_interface_name = NULL;

    char *result = manager->create( name, ordinal, local_interface_name, remote_interface_name );
    if ( result != NULL ) {
        Tcl_SetResult( interp, result, TCL_STATIC );
        return TCL_ERROR;
    }

    Tcl_ResetResult( interp );
    return TCL_OK;
}

/**
 * Spine Interface command
 * Modifies a shared network object
 *
 * house
 * % netmgr { ask | tell } modify <name> [ <local_interface_name> [ <remote_interface_name> ] ]
 *
 * The remote interface name is for reference only, it is not actually used
 * for anything inside the network service.  This is because with the new link
 * pairing mechanism, the names of the interfaces are not necessarily the same.
 *
 */
static int
modify_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    syslog( LOG_NOTICE, "modify_cmd" );

    Network::Manager *manager = (Network::Manager *)data;
    if ( (objc != 2) and ( objc != 3) and ( objc != 4 ) ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "command" );
        return TCL_ERROR;
    }
    if ( manager == NULL ) {
        Tcl_SetResult( interp, "Hmm.  ClientData is NULL", TCL_STATIC );
        return TCL_ERROR;
    }
    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    if ( ( name == NULL ) or ( strlen( name ) == 0 ) ) {
        Tcl_SetResult( interp, "Invalid shared network name (modify)", TCL_STATIC );
        return TCL_ERROR;
    }
    char *local_interface_name;
    if ( objc >= 3 )
        local_interface_name = Tcl_GetStringFromObj( objv[2], NULL );
    else
        local_interface_name = NULL;
//    if ( ( local_interface_name == NULL ) or ( strlen( local_interface_name ) == 0 ) ) {
//        Tcl_SetResult( interp, "Invalid local interface name (modify)", TCL_STATIC );
//        return TCL_ERROR;
//    }
    char *remote_interface_name;
    if ( objc == 4 )
        remote_interface_name = Tcl_GetStringFromObj( objv[3], NULL );
    else
        remote_interface_name = NULL;
//    if ( ( local_interface_name == NULL ) or ( strlen( local_interface_name ) == 0 ) ) {
//        Tcl_SetResult( interp, "Invalid remote interface name (modify)", TCL_STATIC );
//        return TCL_ERROR;
//    }
    if ( ( remote_interface_name != NULL ) and ( strlen( remote_interface_name ) == 0 ) )
        remote_interface_name = NULL;

    char *result = manager->modify( name, local_interface_name, remote_interface_name );
    if ( result != NULL ) {
        Tcl_SetResult( interp, result, TCL_STATIC );
        return TCL_ERROR;
    }

    Tcl_ResetResult( interp );
    return TCL_OK;
}

/**
 * Spine Interface command
 * Destroys a shared network object
 *
 * house
 * % netmgr { ask | tell } destroy <name>
 *
 */
static int
destroy_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    syslog( LOG_NOTICE, "destroy_cmd" );

    Network::Manager *manager = (Network::Manager *)data;
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "command" );
        return TCL_ERROR;
    }
    if ( manager == NULL ) {
        Tcl_SetResult( interp, "Hmm.  ClientData is NULL", TCL_STATIC );
        return TCL_ERROR;
    }
    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    if ( ( name == NULL ) or ( strlen( name ) == 0 ) ) {
        Tcl_SetResult( interp, "Invalid shared network name (destroy)", TCL_STATIC );
        return TCL_ERROR;
    }

    char *result = manager->destroy( name );
    if ( result != NULL ) {
        Tcl_SetResult( interp, result, TCL_STATIC );
        return TCL_ERROR;
    }

    Tcl_ResetResult( interp );
    return TCL_OK;
}

/**
 * Spine Interface command (for debugging purposes)
 * Simulates a fault on an interface
 *
 * house
 * % netmgr { ask | tell } fault <name>
 *
 */
static int
fault_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    syslog( LOG_NOTICE, "fault_cmd" );

    Network::Manager *manager = (Network::Manager *)data;
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "command" );
        return TCL_ERROR;
    }
    if ( manager == NULL ) {
        Tcl_SetResult( interp, "Hmm.  ClientData is NULL", TCL_STATIC );
        return TCL_ERROR;
    }
    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    if ( ( name == NULL ) or ( strlen( name ) == 0 ) ) {
        Tcl_SetResult( interp, "Invalid interface name (fault)", TCL_STATIC );
        return TCL_ERROR;
    }

    char *result = manager->fault( name );
    if ( result != NULL ) {
        Tcl_SetResult( interp, result, TCL_STATIC );
        return TCL_ERROR;
    }

    Tcl_ResetResult( interp );
    return TCL_OK;
}

/**
 * Spine Interface command
 * Invoked from remote Spine Dendrite call to set up vtund service.
 *
 * house
 * % netmgr { ask | tell } attach_vtund_client <name>
 *
 */
static int
attach_vtund_client_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    syslog( LOG_NOTICE, "attach_vtund_client_cmd" );

    Network::Manager *manager = (Network::Manager *)data;
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "command" );
        return TCL_ERROR;
    }
    if ( manager == NULL ) {
        Tcl_SetResult( interp, "Hmm.  ClientData is NULL", TCL_STATIC );
        return TCL_ERROR;
    }
    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    if ( ( name == NULL ) or ( strlen( name ) == 0 ) ) {
        Tcl_SetResult( interp, "Invalid interface name (attach_vtund_client)", TCL_STATIC );
        return TCL_ERROR;
    }

    if (not manager->attach_vtund_client( name ) ) {
        Tcl_SetResult( interp, "Unknown interface name (attach_vtund_client)", TCL_STATIC );
        return TCL_ERROR;
    }

    Tcl_ResetResult( interp );
    return TCL_OK;
}

/**
 * Spine Interface command
 * Invoked from remote Spine Dendrite call to tear down vtund service.
 *
 * house
 * % netmgr { ask | tell } detach_vtund_client <name>
 *
 */
static int
detach_vtund_client_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    syslog( LOG_NOTICE, "detach_vtund_client_cmd" );

    Network::Manager *manager = (Network::Manager *)data;
    if ( objc != 2 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "command" );
        return TCL_ERROR;
    }
    if ( manager == NULL ) {
        Tcl_SetResult( interp, "Hmm.  ClientData is NULL", TCL_STATIC );
        return TCL_ERROR;
    }
    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    if ( ( name == NULL ) or ( strlen( name ) == 0 ) ) {
        Tcl_SetResult( interp, "Invalid interface name (attach_vtund_client)", TCL_STATIC );
        return TCL_ERROR;
    }

// XXX detach_vtund_client should return a bool like attach_vtund_client does
//     and we should return an error here if that is the case
    manager->detach_vtund_client( name );

    Tcl_ResetResult( interp );
    return TCL_OK;
}

/**
 * Spine Interface command
 * Sets access type on a shared network to be "business" or "private".
 *
 * house
 * % netmgr { ask | tell } set_access <name> { business | private }
 *
 */
static int
set_access_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    syslog( LOG_NOTICE, "set_access_cmd" );

    Network::Manager *manager = (Network::Manager *)data;
    if ( objc != 3 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "command" );
        return TCL_ERROR;
    }
    if ( manager == NULL ) {
        Tcl_SetResult( interp, "Hmm.  ClientData is NULL", TCL_STATIC );
        return TCL_ERROR;
    }
    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    if ( ( name == NULL ) or ( strlen( name ) == 0 ) ) {
        Tcl_SetResult( interp, "Invalid shared network name (set_access)", TCL_STATIC );
        return TCL_ERROR;
    }
    char *access = Tcl_GetStringFromObj( objv[2], NULL );
    if ( ( access == NULL ) or ( strlen( access ) == 0 ) ) {
        Tcl_SetResult( interp, "Invalid access type (set_access)", TCL_STATIC );
        return TCL_ERROR;
    }

    char *result = manager->set_access( name, access );
    if ( result != NULL) {
        Tcl_SetResult( interp, result, TCL_STATIC );
        return TCL_ERROR;
    }

    Tcl_ResetResult( interp );
    return TCL_OK;
}

/**
 */
static int
AppInit( Tcl_Interp *interp ) {
    recursive_remove( "topology" );
    if ( ( mkdir( "topology", 0755 ) < 0 ) && ( errno != EEXIST ) ) {
        syslog( LOG_ERR, "%%BUG:  Cannot create directory 'topology':  %s", strerror( errno ) );
    }

    if ( UUID_Initialize(interp) == false ) {
        printf( "UUID_Initialize failed\n" );
        exit( 1 );
    }

    if ( BIOS::Initialize(interp) == false ) {
        printf( "BIOS::Initialize failed\n" );
        exit( 1 );
    }

    if ( NetLink::Initialize(interp) == false ) {
        printf( "NetLink::Initialize failed\n" );
        exit( 1 );
    }

    if ( Network::Initialize(interp) == false ) {
        printf( "Network::Initialize failed\n" );
        exit( 1 );
    }

    if ( ICMPv6::Initialize(interp) == false ) {
        printf( "ICMPv6::Initialize failed\n" );
        exit( 1 );
    }

    if ( Pulse_Initialize(interp) == false ) {
        printf( "Pulse_Initialize failed\n" );
        exit( 1 );
    }

    if ( Tcl_LinkVar(interp, "advertise_interval", (char *)&advertise_interval, TCL_LINK_INT) != TCL_OK ) {
        syslog( LOG_ERR, "failed to link advertise_interval" );
        exit( 1 );
    }

    SMBIOS::Header smbios;
    smbios.probe( SMBIOS::Header::locate() );
    UUID *smbios_uuid = smbios.system().uuid();
    syslog( LOG_NOTICE, "System UUID is '%s'\n", smbios_uuid->to_s() );
    delete smbios_uuid;

    LoadUUID( interp );
    LoadPlatformFile( interp, smbios.system(), smbios.baseboard() );
    LoadLocalConfig( interp );

    return TCL_OK;
}

/**
 */
int main( int argc, char **argv ) {

    int node_ordinal;
    char buffer[16];
    gethostname( buffer, sizeof buffer );
    if ( sscanf(buffer, "node%d", &node_ordinal) == 1 ) {
        sethostid( node_ordinal );
        syslog( LOG_NOTICE, "hostid set to node ordinal %d", node_ordinal );
    } else {
        syslog( LOG_NOTICE, "could not determine node ordinal" );
    }

    Service service( "netmgr" );
    service.set_facility( LOG_LOCAL4 );
    service.initialize( argc, argv, AppInit );

    /*
     * Temporarily get the tcl interp from the service -- do not want this
     * \todo change so there is a registry of interface to perferred names
     */
    Network::Manager manager( service.interpreter() );
    manager.start();

    Network::Monitor monitor( service.interpreter(), Diastole::Factory, &manager );
    monitor.start();

    Systole systole( &uuid, monitor );
    systole.start();

    /*
     * Wait for Manager to load persisted configuration
     * before beginning command processing.
     */

    for (int i = 0; i < 30; i++) {
        if ( manager.ready() )  break;
        sleep( 1 );
    }

    if ( !manager.ready() ) {
        syslog( LOG_NOTICE, "WARNING:  Manager was not ready after 30 seconds, starting service anyway" );
    }

    service.add_command( "neighbors", neighbors_cmd, (ClientData)&monitor );
    service.add_command( "nodes", nodes_cmd, (ClientData)&monitor );
    service.add_command( "interfaces", interfaces_cmd, (ClientData)&monitor );

    service.add_command( "insert_node", insert_node_cmd, (ClientData)&monitor );

    /*
     * Register Network Manager commands.
     */
    service.add_command( "ping", ping_cmd, (ClientData)&manager );
    service.add_command( "error", error_cmd, (ClientData)&manager );
    service.add_command( "dump", dump_cmd, (ClientData)&manager );
    service.add_command( "create", create_cmd, (ClientData)&manager );
    service.add_command( "modify", modify_cmd, (ClientData)&manager );
    service.add_command( "destroy", destroy_cmd, (ClientData)&manager );
    service.add_command( "fault", fault_cmd, (ClientData)&manager );
    service.add_command( "attach_vtund_client", attach_vtund_client_cmd, (ClientData)&manager );
    service.add_command( "detach_vtund_client", detach_vtund_client_cmd, (ClientData)&manager );

    service.add_command( "set_access", set_access_cmd, (ClientData)&manager );

    service.start();

    int iter = 0;
    for (;;) {
        if ( cluster_name_unknown ) determine_cluster_name();
        monitor.advertise();
        sleep( advertise_interval );
        if ( ++iter > 100 ) {
            VALGRIND_DO_LEAK_CHECK;
            iter = 0;
        }
        if ( (iter % 10) == 0 )  monitor.probe();
    }
}

/* vim: set autoindent expandtab sw=4 : */
