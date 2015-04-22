
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

/** \file DarwinInterface.c
 * \brief 
 *
 * Contains method definitions that are platform specific for the
 * Interface class.  There are related files for other platforms.
 */

#include <sys/ioctl.h>
#include <net/if.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "PlatformInterface.h"
#include "Interface.h"
#include "logger.h"

int
network_interface_index( const char *_name ) {
    return -1;
}

int
get_mac_address( char *interface_name, unsigned char *mac ) {
    return 1;
}

int
set_interface_address( int index, struct in6_addr *address ) {
    return -1;
}

/**
 */
void
Network::Interface::platform_init() {
}

/**
 */
void
Network::Interface::get_settings() {
}

/**
 */
static int
get_offload( char *name, int command ) {
    return -1;
}

/**
 */
int
Network::Interface::rx_offload() const {
    return 0;
}

/**
 */
int
Network::Interface::tx_offload() const {
    return 0;
}

/**
 */
int
Network::Interface::sg_offload() const {
    return 0;
}

/**
 */
int
Network::Interface::tso_offload() const {
    return 0;
}

/**
 */
int
Network::Interface::ufo_offload() const {
    return 0;
}

/**
 */
int
Network::Interface::gso_offload() const {
    return 0;
}

/**
 */
static void
set_offload( char *name, int command, int value ) {
}

/**
 */
void
Network::Interface::rx_offload( int value ) const {
}

/**
 */
void
Network::Interface::tx_offload( int value ) const {
}

/**
 */
void
Network::Interface::sg_offload( int value ) const {
}

/**
 */
void
Network::Interface::tso_offload( int value ) const {
}

/**
 */
void
Network::Interface::ufo_offload( int value ) const {
}

/**
 */
void
Network::Interface::gso_offload( int value ) const {
}

/**
 */
bool
Network::Interface::negotiate() {
    return false;
}

/**
 * \todo check lstat errno -- because there may be some cases where this 
 * really is a physical device
 */
bool
Network::Interface::is_physical() const {
    return false;
}

/**
 */
bool
Network::Interface::is_full_duplex() const {
    return false;
}

/**
 * On Linux this is determined through sysfs
 */
bool
Network::Interface::is_bridge() const {
    return false;
}

/**
 */
bool
Network::Interface::is_captured() const {
    return false;
}

/**
 * True if this bridge has a tunnel interface captured.
 */
bool
Network::Interface::is_tunnelled() const {
    bool result = false;
    return result;
}

/** Rename a network interface.
 */
bool
Network::Interface::rename( char *new_name ) {
    return false;
}

bool Network::Interface::is_up()          const { return false; }
bool Network::Interface::is_running()     const { return false; }
bool Network::Interface::is_promiscuous() const { return false; }
bool Network::Interface::is_dormant()     const { return false; }
bool Network::Interface::has_link()       const { return false; }

/**
 */
bool Network::Interface::up_changed()          const { return false; }
bool Network::Interface::running_changed()     const { return false; }
bool Network::Interface::promiscuity_changed() const { return false; }
bool Network::Interface::dormancy_changed()    const { return false; }
bool Network::Interface::link_changed()        const { return false; }

bool Network::Interface::up()                  const { return false; }
bool Network::Interface::loopback()            const { return false; }
bool Network::Interface::running()             const { return false; }
bool Network::Interface::multicast()           const { return false; }

/**
 */
bool
Network::Interface::is_listening_to( const char *protocol, u_int16_t port ) const {
    bool result = false;
    return result;
}

/* vim: set autoindent expandtab sw=4: */
