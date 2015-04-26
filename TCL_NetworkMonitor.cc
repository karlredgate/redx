
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

/** \file LinuxNetworkMonitor.cc
 * \brief 
 *
 */

#include <asm/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <glob.h>
#include <errno.h>

#include <tcl.h>
#include "tcl_util.h"

#include "logger.h"
#include "util.h"
#include "NetLink.h"
#include "NetworkMonitor.h"

namespace { int debug = 0; }

/**
 */
static int
Monitor_obj( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    NetLink::Monitor *monitor = (NetLink::Monitor *)data;

    if ( objc == 1 ) {
        Tcl_SetObjResult( interp, Tcl_NewLongObj((long)(monitor)) );
        return TCL_OK;
    }

    char *command = Tcl_GetStringFromObj( objv[1], NULL );
    if ( Tcl_StringMatch(command, "type") ) {
        Svc_SetResult( interp, "Network::Monitor", TCL_STATIC );
        return TCL_OK;
    }

    if ( Tcl_StringMatch(command, "start") ) {
        // this should have some sort of result so we know it has started correctly
        monitor->start();
        /*
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Svc_SetResult( interp, (char *)result, TCL_STATIC );
        return TCL_ERROR;
        */

            Tcl_ResetResult( interp );
            return TCL_OK;
    }

    /**
     */
    if ( Tcl_StringMatch(command, "stop") ) {
        /*
        const char *result = monitor->stop();
        if ( result == NULL ) {
            Tcl_ResetResult( interp );
            return TCL_OK;
        }
        Svc_SetResult( interp, (char *)result, TCL_STATIC );
        return TCL_ERROR;
        */

            Tcl_ResetResult( interp );
            return TCL_OK;
    }

    /*
     * I want an 'interfaces' sub command ...
     */

    Svc_SetResult( interp, "Unknown command for Monitor object", TCL_STATIC );
    return TCL_ERROR;
}

/**
 */
static void
Monitor_delete( ClientData data ) {
    NetLink::Monitor *message = (NetLink::Monitor *)data;
    delete message;
}

/**
 */
static int
Monitor_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    if ( objc != 3 ) {
        Tcl_ResetResult( interp );
        Tcl_WrongNumArgs( interp, 1, objv, "name factory" );
        return TCL_ERROR;
    }

    Network::ListenerInterfaceFactory factory;
    void *p = (void *)&(factory); // avoid strict aliasing errors in the compiler
    if ( Tcl_GetLongFromObj(interp,objv[2],(long*)p) != TCL_OK ) {
        Svc_SetResult( interp, "invalid listener object", TCL_STATIC );
        return TCL_ERROR;
    }
    char *name = Tcl_GetStringFromObj( objv[1], NULL );
    NetLink::Monitor *object = new NetLink::Monitor( interp, factory, NULL );
    Tcl_CreateObjCommand( interp, name, Monitor_obj, (ClientData)object, Monitor_delete );
    Svc_SetResult( interp, name, TCL_VOLATILE );
    return TCL_OK;
}

/**
 * This is a sample command for testing the straight line netlink
 * probe code.
 */
static int
Probe_cmd( ClientData data, Tcl_Interp *interp,
             int objc, Tcl_Obj * CONST *objv )
{
    // Yes, this is intentional, it may get removed later
    pid_t pid  __attribute((unused))  = getpid();

    int result;

    // Discover interfaces and add objects
    int fd;
    fd = socket( AF_NETLINK, SOCK_RAW, NETLINK_ROUTE );
    if ( socket < 0 ) {
        log_err( "Probe socket() failed, %s", strerror(errno) );
        exit( 1 );
    }
    // setup sndbuf and rcvbuf

    // bind
    struct sockaddr_nl address;
    memset( &address, 0, sizeof(address) );
    address.nl_family = AF_NETLINK;
    address.nl_groups = 0;
    if ( bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0 ) {
        log_err( "Probe bind() failed, %s", strerror(errno) );
        exit( 1 );
    }

    // send request
    struct {
        struct nlmsghdr nlh;
        struct rtgenmsg g;
    } nlreq;
    memset( &nlreq, 0, sizeof(nlreq) );
    nlreq.nlh.nlmsg_len = sizeof(nlreq);
    nlreq.nlh.nlmsg_type = RTM_GETLINK;
    nlreq.nlh.nlmsg_flags = NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST;
    nlreq.nlh.nlmsg_pid = 0;
    nlreq.nlh.nlmsg_seq = 34;
    nlreq.g.rtgen_family = AF_PACKET;

    result = sendto( fd, (void*)&nlreq, sizeof(nlreq), 0, (struct sockaddr *)&address, sizeof(address) );
    if ( result < 0 ) {
        log_err( "Probe sendto() failed, %s", strerror(errno) );
        exit( 1 );
    }

    // Now I am going to reuse the "address" structure for receiving the responses.
    memset( &address, 0, sizeof(address) );

    char msg_buf[16384];
    struct iovec iov;
    memset( &iov, 0, sizeof(iov) );
    iov.iov_base = msg_buf;

    struct msghdr rsp_msg;
    rsp_msg.msg_name = &address;
    rsp_msg.msg_namelen = sizeof(address);
    rsp_msg.msg_iov = &iov;
    rsp_msg.msg_iovlen = 1;

    // loop around waiting for netlink messages
    // for each response create an interface object
    for (;;) {
        unsigned int status;
        struct nlmsghdr *h;

        iov.iov_len = sizeof(msg_buf);
        status = recvmsg( fd, &rsp_msg, 0 );
        if ( status < 0 ) {
            if ( errno == EINTR ) {
                // normally just continue here
                printf( "interupted recvmsg\n" );
            } else {
                log_err( "Probe recvmsg() failed, %s", strerror(errno) );
            }
            continue;
        }

        if ( status == 0 ) {
            printf( "finished nlmsgs\n" );
            break;
        }
        printf( "status=%d\n", status );

        h = (struct nlmsghdr *)msg_buf;
        while ( NLMSG_OK(h, status) ) {
            if ( h->nlmsg_type == NLMSG_DONE ) {
                printf( "NLMSG_DONE\n" );
                goto finish;
            }

            if ( h->nlmsg_type == NLMSG_ERROR ) {
                printf( "NLMSG_ERROR\n" );
                goto finish;
            }

            /*
            if ( h->nlmsg_flags & NLM_F_MULTI ) {
                printf( "NLM_F_MULTI\n" );
            } else {
                printf( "not:NLM_F_MULTI\n" );
            }
            */

            printf( "process nlmsg type=%d flags=0x%08x pid=%d seq=%d\n", h->nlmsg_type, h->nlmsg_flags, h->nlmsg_pid, h->nlmsg_seq );
            { // type is in linux/if_arp.h -- flags are in linux/if.h
                NetLink::NewLink *m = new NetLink::NewLink( h );
                Network::Interface *ii = new Network::Interface( interp, m );
                if ( ii == NULL ) { // remove -Wall warning by using this
                }
            }

            h = NLMSG_NEXT( h, status );
        }
        printf( "finished with this message (notOK)\n" );
        if ( rsp_msg.msg_flags & MSG_TRUNC ) {
            printf( "msg truncated" );
            continue;
        }
        if ( status != 0 ) {
            printf( "remnant\n" );
        }
    }

finish:
    close( fd );

    // Discover Bridges

    // Search for existing bridges and create objects for them
    // and for each object create a command
    // the bridge commands should remain in the Network namespace
    //    -- maybe no -- create in whichever namespace you need

    return TCL_OK;
}

/**
 * need some way to find these objects...
 * use TCL ..hmmm
 */
bool NetLinkMonitor_Module( Tcl_Interp *interp ) {

    Tcl_Command command;

    Tcl_Namespace *ns = Tcl_CreateNamespace(interp, "Network", (ClientData)0, NULL);
    if ( ns == NULL ) {
        return false;
    }

    if ( Tcl_LinkVar(interp, "Network::debug", (char *)&debug, TCL_LINK_INT) != TCL_OK ) {
        log_err( "failed to link Network::debug" );
        exit( 1 );
    }

    command = Tcl_CreateObjCommand(interp, "Network::Monitor", Monitor_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    command = Tcl_CreateObjCommand(interp, "Network::Probe", Probe_cmd, (ClientData)0, NULL);
    if ( command == NULL ) {
        // logger ?? want to report TCL Error
        return false;
    }

    return true;
}

/* vim: set autoindent expandtab sw=4 : */
