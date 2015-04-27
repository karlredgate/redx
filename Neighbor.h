
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

/** \file Neighbor.h
 * \brief 
 *
 */

#ifndef _NEIGHBOR_H_
#define _NEIGHBOR_H_

#include <stdint.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <unistd.h>
#include <tcl.h>

#include "UUID.h"

namespace ICMPv6 { class Socket; }

/**
 */
namespace Network {

    class Monitor;
    class Interface;

    class Node {
    private:
        UUID _uuid;
        uint8_t _ordinal;
        bool partner;
        bool valid;
    public:
        Node( UUID *node_uuid ) : partner(false) {
            _uuid = *node_uuid;
        }
        void set( UUID *node_uuid ) {
            _uuid = *node_uuid;
        }

        void uuid( UUID& );
        UUID& uuid() { return _uuid; }
        void ordinal( uint8_t );
        uint8_t ordinal() const { return _ordinal; }

        void invalidate() { valid = false; partner = false; }
        void make_partner() { partner = true; }
        void clear_partner() { partner = false; }

        inline bool is_valid() const { return valid; }
        inline bool is_invalid() const { return valid == false; }
        inline bool is_partner()  const { return partner; }
        inline bool not_partner() const { return partner == false; }

        bool operator == ( UUID& );
        bool operator != ( UUID& );
    };

    /**
     * The ordinal for a peer is the interface ordinal for that peer.
     */
    class Peer {
    private:
        Node *_node;
        struct in6_addr lladdr;
        struct timeval neighbor_updated;
        struct timeval neighbor_advertised;
        uint8_t _ordinal;
        char *_name;

        bool valid;
        bool partner;
        bool _is_private;
        bool _spine_notified;
    public:
        Peer() {}
        Peer( Node *, struct in6_addr * );
        ~Peer();

        uint8_t ordinal() const { return _ordinal; }
        char *name()     const { return _name; }
        void invalidate() {
            valid = false; partner = false; reported = false;
            _node = 0;
            _spine_notified = false;
        }
        void make_partner() { partner = true; }
        void set_interface( bool, uint8_t );
        void set_interface_name( char * );
        inline bool is_valid()    const { return valid; }
        inline bool is_invalid()  const { return valid == false; }
        inline bool is_private()  const { return _is_private; }
        inline bool is_partner()  const { return partner; }
        inline bool not_private() const { return _is_private == false; }
        inline bool not_partner() const { return partner == false; }

        bool operator == ( struct in6_addr& );
        bool operator != ( struct in6_addr& );

        void node( Node * );
        Node *node() const;
        void address( struct in6_addr * );
        struct in6_addr& address() { return lladdr; }

        void copy_address( struct in6_addr * ) const;
        void touch_advertised();
        void touch();
        int  seconds_since_last_update() const;
        bool has_address( struct in6_addr * );
        const struct timeval * last_updated()    const { return &neighbor_updated; }
        const struct timeval * last_advertised() const { return &neighbor_advertised; }

        bool has_notified_spine() const { return _spine_notified; }
        bool has_not_notified_spine() const { return _spine_notified == false; }
        void topology_changed( Network::Monitor *, Interface * );

        bool reported;
    };

    /**
     */
    class NeighborIterator {
    public:
        NeighborIterator() {}
        virtual ~NeighborIterator() {}
        virtual int operator() ( Peer& ) = 0;
    };

}

#endif

/* vim: set autoindent expandtab sw=4 : */
