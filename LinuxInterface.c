
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

#include "PlatformInterface.h"

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

/* vim: set autoindent expandtab sw=4 : */
