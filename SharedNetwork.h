
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

/** \file SharedNetwork.h
 * \brief 
 *
 */

#ifndef _SHARED_NETWORK_H_
#define _SHARED_NETWORK_H_

#include <stdint.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <linux/if.h>
#include <linux/netdevice.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <unistd.h>

#include <list>

namespace ICMPv6 { class Socket; }

/**
 */
namespace Network {

    class Bridge;
    class Interface;
    class Node;
    class Peer;

    class Monitor;

    typedef enum { ACCESS_PRIVATE, ACCESS_BUSINESS } access_t;

    /**
     */
    class SharedNetwork {
    private:
        char *_name;
        int _ordinal;
	access_t _access;
// XXX there really can only be one Interface and one remote interface
//        std::list <Interface*> interfaces;
//        std::list <char*> remote_interfaces;
        Interface *_interface;
        char *_remote_interface;
        Bridge *_bridge;
    public:
        SharedNetwork( const char *, int );
        ~SharedNetwork();
// This is done in the Manager class since it manages the collection of objects
        static const char * create( const char *, int );
        static const char * destroy();
        const char * add_interface( Interface * );
        const char * remove_interface( Interface * );
        const char * add_remote_interface( const char * );
        const char * remove_remote_interface( const char * );
        const char * add_bridge( Bridge * );
        const char * remove_bridge( Bridge *);
        char * set_access( const char * );
        const char *name() const { return _name; }
        const access_t access() const { return _access; }

        Interface *interface() const { return _interface; }
        Bridge *bridge() const { return _bridge; }

// XXX removed
//      char    *tunnel_interface();	// "atun<N>"
//      char    *bridge_interface();	// "biz<N>"
//      char    *physical_interface();	// "ibiz<N>"
//      char    *network_interface();	// "network<N>"  - This is what "_name" is
    };

}

#endif

/* vim: set autoindent expandtab sw=4 : */
