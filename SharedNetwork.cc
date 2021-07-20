
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

/** \file SharedNetwork.cc
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

#include "netcfg/netcfg.h"

#include "Network.h"

namespace { int debug = 0; }

/**
 */
Network::SharedNetwork::SharedNetwork( const char *shared_network_name, int ordinal )
: _ordinal( ordinal ), _access( ACCESS_BUSINESS), _interface( NULL ), _remote_interface( NULL ),
  _bridge( NULL ) {
    _name = strdup(shared_network_name);
}

/**
 */
Network::SharedNetwork::~SharedNetwork() {
    if ( _name != NULL ) free( _name );
    if ( _remote_interface != NULL ) free( _remote_interface );
    if ( _bridge != NULL) delete _bridge;
}

// This is done in the Manager class since it manages the collection of objects
#if 0
/**
 */
const char *
Network::SharedNetwork::create( const char *shared_network_name, int ordinal ) {
    syslog( LOG_NOTICE, "create new shared network '%s' (%d)", shared_network_name, ordinal );

    SharedNetwork *sn = new SharedNetwork( shared_network_name, ordinal );
    return sn;
}

/**
 */
const char *
Network::SharedNetwork::destroy( const char *shared_network_name ) {
    syslog( LOG_NOTICE, "destroy shared network '%s'", _name );

    return NULL;
}
#endif

/**
 */
const char *
Network::SharedNetwork::add_interface( Interface *interface ) {
    if ( _interface != NULL )
        return "interface already set";
    if ( interface == NULL )
        return "invalid interface";

    syslog( LOG_NOTICE, "adding interface '%s' to shared network '%s'", interface->name(), _name );

    if ( NetCfg_add_interface(_name, interface->name()) < 0 ) {
        /* need a better error */
        return "cannot add interface to shared network";
    }

    _interface = interface;
    return NULL;
}

/**
 */
const char *
Network::SharedNetwork::remove_interface( Interface *interface ) {
    if ( interface == NULL )
        syslog( LOG_NOTICE, "removing interface from shared network '%s'", _name );
    else
        syslog( LOG_NOTICE, "removing interface '%s' from shared network '%s'", interface->name(), _name );

//    if ( ( _interface == NULL) or ( _interface != interface) )
// interface could be NULL, so we don't log it
//        syslog( LOG_NOTICE, "WARNING:  interface does not match current interface, removing current interface" );

    if ( NetCfg_remove_interface(_name, interface->name()) < 0 ) {
        /* need a better error */
        return "cannot remove interface from shared network";
    }

    _interface = NULL;
    return NULL;
}

/**
 */
const char *
Network::SharedNetwork::add_remote_interface( const char *remote_interface ) {
    if ( _remote_interface != NULL )
        return "remote interface already set";
    if ( remote_interface == NULL )
        return "invalid remote interface name";

    syslog( LOG_NOTICE, "adding remote interface '%s' to shared network '%s'", remote_interface, _name );

    _remote_interface = strdup( remote_interface );
    return NULL;
};

/**
 */
const char *
Network::SharedNetwork::remove_remote_interface( const char *remote_interface ) {
    if ( remote_interface == NULL )
        syslog( LOG_NOTICE, "removing remote interface from shared network '%s'", _name );
    else
        syslog( LOG_NOTICE, "removing remote interface '%s' from shared network '%s'", remote_interface, _name );

//    if ( ( _remote_interface == NULL ) or ( remote_interface == NULL ) or
//         ( strcmp( _remote_interface, remote_interface) != 0 ) )
// remote_interface could be NULL, so we don't log it
//        syslog( LOG_NOTICE, "WARNING:  remote interface does not match current remote interface, removing current remote interface" );

    if ( _remote_interface != NULL ) free( _remote_interface );
    _remote_interface = NULL;
    return NULL;
};

/**
 */
const char *
Network::SharedNetwork::add_bridge( Bridge *bridge ) {
    if ( _bridge != NULL )
        return "bridge already set";
    if ( bridge == NULL )
        return "invalid bridge";

    syslog( LOG_NOTICE, "adding bridge '%s' to shared network '%s'", bridge->name(), _name );

    _bridge = bridge;
    return NULL;
}

/**
 */
const char *
Network::SharedNetwork::remove_bridge( Bridge *bridge ) {
    if ( bridge == NULL )
        syslog( LOG_NOTICE, "removing bridge from shared network '%s'", _name );
    else
        syslog( LOG_NOTICE, "removing bridge '%s' from shared network '%s'", bridge->name(), _name );

//    if ( ( _bridge == NULL) or ( _bridge != bridge) )
// bridge could be NULL, so we don't log it
//        syslog( LOG_NOTICE, "WARNING:  bridge does not match current bridge, removing current bridge" );
    _bridge = NULL;
    return NULL;
}

/**
 */
char *
Network::SharedNetwork::set_access( const char *access ) {
    access_t	bizpriv;

    syslog( LOG_NOTICE, "setting access on shared network '%s' to '%s'", _name, access );

    if ( strcmp( access, "private" ) == 0 ) {
        bizpriv = ACCESS_PRIVATE;
    } else if ( strcmp( access, "business" ) == 0 ) {
        bizpriv = ACCESS_BUSINESS;
    } else {
      return (char *)"unknown access type";
    }
    
// can't change access type if shared network is in use.
// if currently private, there must be no active DRBD devices
// if currently business, there must be no guest VMs using it
// this is enforced via policy rules in SMD, not here

    if ( ( _access == ACCESS_BUSINESS ) && ( bizpriv == ACCESS_PRIVATE ) ) {
        _access = ACCESS_PRIVATE;
    } else if ( ( _access = ACCESS_PRIVATE ) && ( bizpriv == ACCESS_BUSINESS ) ) {
        _access = ACCESS_BUSINESS;
    }

    return NULL;
}

/* vim: set autoindent expandtab sw=4 : */
