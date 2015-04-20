
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

/** \file LinuxInterface.c
 * \brief 
 *
 */

#define _BSD_SOURCE
#include <features.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "PlatformInterface.h"
#include "logger.h"

int
network_interface_index( const char *_name ) {
    struct ifreq ifr;
    int s;
    s = socket(PF_PACKET, SOCK_DGRAM, 0);
    if (s < 0)  return -1;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, _name, IFNAMSIZ);
    int result = ioctl(s, SIOCGIFINDEX, &ifr);
    close(s);

    if ( result < 0 ) {
        return -1;
    }

    return ifr.ifr_ifindex;
}

int
get_mac_address( char *interface_name, unsigned char *mac ) {
    int sock = socket( AF_INET, SOCK_STREAM, 0 );
    if ( sock < 0 ) {
        log_warn( "could not open socket to get intf mac addr" );
        return 0;
    }

    struct ifreq request;
    memset( &request, 0, sizeof(request) );
    strncpy( request.ifr_name, interface_name, IFNAMSIZ );
    // request->ifr_hwaddr.sa_family = ARPHRD_ETHER;

    int result = ioctl(sock, SIOCGIFHWADDR, &request);
    const char *error = strerror(errno);
    close( sock );

    if ( result < 0 ) {
        log_warn( "failed to get MAC address for '%s': %s",
                            interface_name, error );
        return 0;
    }

    unsigned char *address = (unsigned char *)request.ifr_hwaddr.sa_data;
    mac[0] = address[0];
    mac[1] = address[1];
    mac[2] = address[2];
    mac[3] = address[3];
    mac[4] = address[4];
    mac[5] = address[5];

    return 1;
}

/* vim: set autoindent expandtab sw=4: */
