
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

/** \file Network.h
 * \brief Network interface abstraction.
 *
 */

#ifndef _TUNNEL_H_
#define _TUNNEL_H_

#include <stdint.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <linux/netdevice.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <unistd.h>
#include <tcl.h>

#include <list>
#include <map>

#include "UUID.h"
#include "Thread.h"
#include "NetLink.h"

/**
 */
namespace Network {

    class Bridge;
    class Interface;

    /**
     */
    class Tunnel {
    private:
        char *_name;
        int _ordinal;
        Interface *_interface;
        Interface *_tunnel_interface;
        Bridge *_bridge;
        int _port;

        bool _tunnel_up;

    public:                                                                                     /* "ibiz<N>", "atun<N>", "biz<N>" */
        Tunnel( const char *name, int ordinal );	/* const char *, const char * ); */	/* Interface, Tunnel, Bridge */
        ~Tunnel();
        const char * create();
        const char * destroy();
        const char * attach( Interface * );
        const char * detach( Interface * );
        const char *name() const { return _name; }

        inline Interface *tunnel_interface() { return _tunnel_interface; }
        inline Bridge *bridge() { return _bridge; }

        inline bool is_tunnel_up() const { return _tunnel_up; }
        inline void tunnel_up() { _tunnel_up = true; }
        inline void tunnel_down() { _tunnel_up = false; }

        bool check_for_tunnel();
        void is_tunnel_attached();

        bool is_up(Interface *interface);

        void add_to_bridge( Interface *interface );		/* attach() ? */
        void delete_from_bridge( Interface *interface );	/* detach() ? */

        void setup_tunnel();
        void destroy_tunnel();

        void write_config_file(bool server);

        static bool Initialize( Tcl_Interp * );
    };

}

#endif

/* vim: set autoindent expandtab sw=4 : */
