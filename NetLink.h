
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

/** \file NetLink.h
 * \brief 
 *
 */

#ifndef _NETLINK_H_
#define _NETLINK_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

// for stats struct below (want this private to the object)
#include <linux/if.h>
#include <linux/netdevice.h>

#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdint.h>
#include <tcl.h>
#include "Thread.h"

/**
 * receive netlink events for network devices
 */
namespace NetLink {

    class Socket;
    class ReceiveCallbackInterface;
    class RouteReceiveCallbackInterface;

    /**
     * netlink message classes contain data they need to send requests, but
     * the messages that are only expected to be received contain pointers into
     * the netlink header/data of the messages received from the kernel.
     *
     * This is done this way since received messages must be consumed as they
     * are read by a callback object.  They callback object is expected to
     * consume the data from the mssage that it is interested in and then the
     * message is recycled, so it does not have an extended lifetime.  This
     * also means that the object can live on the stack and does not require
     * the malloc/free overhead.
     */
    class Message {
    protected:
        uint32_t sequence;
        uint16_t type;
    public:
        Message( uint16_t );
        virtual ~Message() {}
        // Will this make sense with diff sock types
        // static Message * factory( struct nlmsghdr * );
        // struct nlmsghdr * next();
        virtual bool deliver( ReceiveCallbackInterface * );
        static Message * Factory( struct nlmsghdr * );
    };

    /**
     * There are two ways RouteMessages are created.  One is when we
     * send a request, the other when we receive a response.
     *
     * The request is constructed using the a default constuctor
     * for the specific message type.   Responses are constructed
     * with an argument of the nlmsghdr.  The nlmsghdr points
     * at the data (the ifinfo struct) and the route attributes
     * (RTAs).
     */
    class RouteMessage : public Message {
    private:
        struct nlmsghdr event;
        /*
        struct {
            struct nlmsghdr nlh;
            struct rtgenmsg g;
        } request;
        */
    public:
        RouteMessage( uint16_t );
        RouteMessage( struct nlmsghdr * );
        uint16_t type_code() const { return event.nlmsg_type; }
        virtual ~RouteMessage() {}
        virtual bool deliver( RouteReceiveCallbackInterface * );
        virtual void send( Socket& );
        static RouteMessage * Factory( struct nlmsghdr * );
    };

    /**
     */
    class RouteError : public RouteMessage {
    protected:
        struct nlmsgerr message;
    public:
        RouteError( struct nlmsgerr * );
        virtual ~RouteError() {}
        // Will this make sense with diff sock types
        // static Message * factory( struct nlmsghdr * );
        // struct nlmsghdr * next();
        virtual bool deliver( RouteReceiveCallbackInterface * );
        static RouteError * Factory( struct nlmsgerr * );

        int error() const { return message.error; }
    };

    /**
     */
    class LinkMessage : public RouteMessage {
    protected:
        struct {
            struct nlmsghdr n;
            struct ifinfomsg ifi;
        } request;

        struct rtattr *attr[IFLA_MAX+1];
    public:
        LinkMessage( uint16_t );
        LinkMessage( struct nlmsghdr * );
        virtual ~LinkMessage() {}
        virtual void send( Socket& );
        int index() const;
        int bridge_index() const;
        const char *name() const;
        unsigned int flags() const;
        unsigned int change() const;
        unsigned short device_type() const;
        unsigned char family() const { return request.ifi.ifi_family; }
        unsigned char operstate() const;
    };

    /**
     * NewLink messages are sent when a device is created, but also is the response
     * when request for the whole interface table is requested.
     *
     * Not sure if we can create new interfaces by sending this message to the kernel
     * so I am keeping the MAC as _MAC to hold the MAC address to configure ... this
     * may go away if I find we cannot create them in this way.
     *
     * [root@node0.chok ~]# grep . /sys/class/net/ * /flags
     * /sys/class/net/eth0/flags:0x1103
     * /sys/class/net/eth1/flags:0x1003
     * /sys/class/net/lo/flags:0x9
     * /sys/class/net/sit0/flags:0x80
     * /sys/class/net/veth0/flags:0x1002
     * /sys/class/net/veth1/flags:0x1002
     * /sys/class/net/vif0.0/flags:0x1002
     * /sys/class/net/vif0.1/flags:0x1002
     * /sys/class/net/vif1.0/flags:0x183
     * /sys/class/net/vif5.0/flags:0x183
     * /sys/class/net/xenbr0/flags:0x1003
     */
    class NewLink : public LinkMessage {
    private:
        struct ifinfomsg *ifi;
        unsigned char _MAC[6];
    public:
        NewLink();
        NewLink( struct nlmsghdr * );
        virtual ~NewLink() {}
        virtual bool deliver( RouteReceiveCallbackInterface * );
        static RouteMessage * Factory( struct nlmsghdr * );
        unsigned char *MAC() const;
        struct net_device_stats *stats() const;

        bool up_changed() const;
        bool running_changed() const;
        bool promiscuous_changed() const;
        bool link_changed() const;
        bool dormant_changed() const;

        bool is_up() const;
        bool is_running() const;
        bool is_promiscuous() const;
        bool has_link() const;
        bool is_dormant() const;
    };

    /**
     * This message should be broadcast from the kernel when a network
     * interface has been deleted.
     */
    class DelLink : public LinkMessage {
    private:
        struct ifinfomsg *ifi;
    public:
        DelLink();
        DelLink( struct nlmsghdr * );
        virtual ~DelLink() {}
        virtual bool deliver( RouteReceiveCallbackInterface * );
        static RouteMessage * Factory( struct nlmsghdr * );
        unsigned char *MAC() const;
    };

    /**
     * The GetLink message is a request from user space for 1 or
     * all network interface link information structures.  These
     * do not come from the kernel.
     *
     * The response to this message is 1/many NewLink messages
     * each containing a NewLink message.
     */
    class GetLink : public LinkMessage {
    public:
        GetLink();
        virtual ~GetLink() {}
        static RouteMessage * Factory( struct nlmsghdr * );
    };

    /**
     * This needs to change so the base class has a buffer
     * and the specific message struct is a pointer into the 
     * request.
     */
    class AddressMessage : public RouteMessage {
    protected:
        struct {
            struct nlmsghdr n;
            struct ifaddrmsg ifa;
            char buffer[2048];
        } request;

        struct rtattr *attr[IFA_MAX+1];
    public:
        AddressMessage( uint16_t );
        AddressMessage( struct nlmsghdr * );
        virtual ~AddressMessage() {}
        virtual void send( Socket& );
        void append( int, const void *, int );

        unsigned char family() const;
        void family( unsigned char );
        unsigned char prefix() const;
        void prefix( unsigned char );
        unsigned char flags() const;
        void flags( unsigned char );
        unsigned char scope() const;
        void scope( unsigned char );
        int index() const;
        void index( int );
        struct in6_addr *in6_addr();
    };

    /**
     */
    class NewAddress : public AddressMessage {
    public:
        NewAddress( int, struct in_addr * );
        NewAddress( int, struct in6_addr * );
        NewAddress( struct nlmsghdr * );
        virtual ~NewAddress() {}
        virtual bool deliver( RouteReceiveCallbackInterface * );
        static RouteMessage * Factory( struct nlmsghdr * );
    };

    /**
     */
    class DelAddress : public AddressMessage {
    public:
        DelAddress();
        DelAddress( struct nlmsghdr * );
        virtual ~DelAddress() {}
        virtual bool deliver( RouteReceiveCallbackInterface * );
        static RouteMessage * Factory( struct nlmsghdr * );
    };

    /**
     */
    class GetAddress : public AddressMessage {
    public:
        GetAddress();
        virtual ~GetAddress() {}
        static RouteMessage * Factory( struct nlmsghdr * );
    };

    /**
     * Need a new base class so I can reuse the RouteMessage class name ...
     * Maybe the base class for all is RouteSocketMessage
     * Or this could change to RouteTableMessage
     */
    class NewRoute : public RouteMessage {
    private:
        struct rtmsg *rtm;
        struct rtattr *attr[RTA_MAX+1];
    public:
        NewRoute();
        NewRoute( struct nlmsghdr * );
        virtual ~NewRoute() {}
        virtual bool deliver( RouteReceiveCallbackInterface * );
        int family() const;
        int route_type() const;
        int protocol() const;
        int scope() const;
        static RouteMessage * Factory( struct nlmsghdr * );
    };

    /**
     */
    class DelRoute : public RouteMessage {
    private:
        struct rtmsg *rtm;
        struct rtattr *attr[RTA_MAX+1];
    public:
        DelRoute();
        DelRoute( struct nlmsghdr * );
        virtual ~DelRoute() {}
        virtual bool deliver( RouteReceiveCallbackInterface * );
        int family() const;
        int route_type() const;
        int protocol() const;
        int scope() const;
        static RouteMessage * Factory( struct nlmsghdr * );
        // static bool Deliver( struct nlmsghdr * );
    };

    /**
     */
    class GetRoute : public RouteMessage {
    public:
        GetRoute();
        virtual ~GetRoute() {}
        static RouteMessage * Factory( struct nlmsghdr * );
    };

    /**
     */
    class NeighborMessage : public RouteMessage {
    protected:
        struct ndmsg *message;
        struct rtattr *attr[NDA_MAX+1];
    public:
        NeighborMessage( uint16_t );
        NeighborMessage( struct nlmsghdr * );
        virtual ~NeighborMessage() {}
    };

    /**
     */
    class NewNeighbor : public NeighborMessage {
    public:
        NewNeighbor();
        NewNeighbor( struct nlmsghdr * );
        virtual ~NewNeighbor() {}
        virtual bool deliver( RouteReceiveCallbackInterface * );
        static RouteMessage * Factory( struct nlmsghdr * );
    };

    /**
     */
    class DelNeighbor : public NeighborMessage {
    public:
        DelNeighbor();
        DelNeighbor( struct nlmsghdr * );
        virtual ~DelNeighbor() {}
        virtual bool deliver( RouteReceiveCallbackInterface * );
        static RouteMessage * Factory( struct nlmsghdr * );
    };

    /**
     */
    class GetNeighbor : public NeighborMessage {
    public:
        GetNeighbor();
        virtual ~GetNeighbor() {}
        static RouteMessage * Factory( struct nlmsghdr * );
    };

    /**
     * 
     */
    class ReceiveCallbackInterface {
    public:
        virtual ~ReceiveCallbackInterface() {}
        virtual void receive( Message* ) = 0;
    };

    /**
     * 
     */
    class RouteReceiveCallbackInterface {
    public:
        virtual ~RouteReceiveCallbackInterface() {}
        virtual void receive( NewLink* ) = 0;
        virtual void receive( DelLink* ) = 0;
        virtual void receive( NewRoute* ) = 0;
        virtual void receive( DelRoute* ) = 0;
        virtual void receive( NewAddress* ) = 0;
        virtual void receive( DelAddress* ) = 0;
        virtual void receive( NewNeighbor* ) = 0;
        virtual void receive( DelNeighbor* ) = 0;
        virtual void receive( RouteMessage* ) = 0;
        virtual void receive( RouteError* ) = 0;
    };

    /**
     * This is not to be used as a base class for listeners.  They
     * should implement the Interface directly.  This is for handling
     * responses to one off requests.
     */
    class RouteResponseHandler : public RouteReceiveCallbackInterface {
    private:
        int _error;
    public:
        virtual ~RouteResponseHandler() {}
        virtual void receive( NewLink* );
        virtual void receive( DelLink* );
        virtual void receive( NewRoute* );
        virtual void receive( DelRoute* );
        virtual void receive( NewAddress* );
        virtual void receive( DelAddress* );
        virtual void receive( NewNeighbor* );
        virtual void receive( DelNeighbor* );
        virtual void receive( RouteMessage* );
        virtual void receive( RouteError* );
        int error() const;
    };

    /**
     * 
     */
    class Socket {
    protected:
        int socket;
        struct sockaddr_nl address;
    public:
        Socket( int );
        virtual ~Socket();
        bool bind( uint32_t );
        void receive( ReceiveCallbackInterface * );
        void send( void *, int );
    };

    /**
     * A RouteSocket is a netlink socket that is bound to the
     * rtnetlink protocol.
     */
    class RouteSocket : public Socket {
    public:
        RouteSocket( uint32_t group=0 );
        virtual ~RouteSocket();
        void receive( RouteReceiveCallbackInterface * );
    };

    /**
     * Create TCL commands for creating/managing NetLink route sockets
     * and messages.
     */
    bool Initialize( Tcl_Interp * );

}

#endif

/* vim: set autoindent expandtab sw=4 : */
