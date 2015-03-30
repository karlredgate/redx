
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

/** \file ICMP.h
 * \brief 
 *
 */

#ifndef _ICMP_H_
#define _ICMP_H_

#include <asm/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <tcl.h>
#include "Thread.h"

/**
 * receive netlink events for network devices
 */
namespace ICMPv6 {

    class Socket;
    class ReceiveCallbackInterface;

    /**
     */
    class PDU {
    protected:
        struct icmp6_hdr header;
    public:
        PDU( struct icmp6_hdr * );
        PDU( uint8_t, uint8_t pdu_code=0 );
        virtual ~PDU() {}
        virtual bool deliver( ReceiveCallbackInterface * );
        virtual bool send( Socket&, struct in6_addr * );
        static PDU * Factory( struct icmp6_hdr * );
    };

    /**
     */
    class EchoRequest : public PDU {
    private:
    public:
        EchoRequest();
        EchoRequest( struct icmp6_hdr * );
        virtual ~EchoRequest() {}
        virtual bool send( Socket&, struct in6_addr * );
        virtual bool deliver( ReceiveCallbackInterface * );
        static PDU * Factory( struct icmp6_hdr * );
    };

    /**
     */
    class EchoReply : public PDU {
    private:
    public:
        EchoReply();
        EchoReply( struct icmp6_hdr * );
        virtual ~EchoReply() {}
        virtual bool send( Socket&, struct in6_addr * );
        virtual bool deliver( ReceiveCallbackInterface * );
        static PDU * Factory( struct icmp6_hdr * );
    };

    /**
     */
    class NeighborSolicitation : public PDU {
    private:
        struct nd_neighbor_solicit pdu;
    public:
        NeighborSolicitation();
        NeighborSolicitation( struct icmp6_hdr * );
        virtual ~NeighborSolicitation() {}
        virtual bool send( Socket&, struct in6_addr * );
        virtual bool deliver( ReceiveCallbackInterface * );
        const struct in6_addr * target() const;
        static PDU * Factory( struct icmp6_hdr * );
    };

    /**
     */
    class NeighborAdvertisement : public PDU {
    private:
        struct {
            struct nd_neighbor_advert na;
            struct nd_opt_hdr opt;
            uint8_t mac[6];
        } pdu;
    public:
        NeighborAdvertisement( struct in6_addr *, unsigned char * );
        NeighborAdvertisement( struct icmp6_hdr * );
        virtual ~NeighborAdvertisement() {}
        virtual bool send( Socket&, struct in6_addr * );
        virtual bool deliver( ReceiveCallbackInterface * );
        struct in6_addr *target();
        void target( struct in6_addr * );
        static PDU * Factory( struct icmp6_hdr * );
    };

    /**
     * 
     */
    class ReceiveCallbackInterface {
    public:
        virtual ~ReceiveCallbackInterface() {}
        virtual void receive( EchoRequest * ) = 0;
        virtual void receive( EchoReply * ) = 0;
        virtual void receive( NeighborSolicitation * ) = 0;
        virtual void receive( NeighborAdvertisement * ) = 0;
        virtual void receive( PDU* ) = 0;
    };

    /**
     * 
     */
    class Socket {
    protected:
        int socket;
        pthread_mutex_t lock;
        struct sockaddr_in6 binding;
        bool bind_completed;
        int binds_attempted;
    public:
        Socket();
        ~Socket();
        // bool bind( uint32_t );
        bool bind( struct in6_addr *, uint32_t );
        void receive( ReceiveCallbackInterface * );
        bool send( void *, int );
        bool send( struct in6_addr *, void *, int );
        inline bool bound() const { return bind_completed; }
        inline bool not_bound() const { return bind_completed == false; }
    };

    /**
     */
    bool Initialize( Tcl_Interp * );

}

#endif

/*
 * vim:autoindent:expandtab:
 */
