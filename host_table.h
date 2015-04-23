
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

#ifndef _HOST_TABLE_H_
#define _HOST_TABLE_H_

#include <stdint.h>
#include <netinet/in.h>

struct host_entry {
    uint8_t node_uuid[16];
    uint8_t cluster_uuid[16];
    struct in6_addr primary_address;
    struct in_addr v4_address;
    union {
	uint32_t raw;
        struct {
        unsigned int valid : 1;
        unsigned int partner : 1;
        unsigned int is_private : 1;
	};
    } flags;
    uint8_t mac[6];
    struct { uint8_t ordinal; } node;
    struct { uint8_t ordinal; } interface;
};

#define HOST_TABLE_ENTRIES 256
#define HOST_TABLE_SIZE (sizeof(struct host_entry)*HOST_TABLE_ENTRIES)

#endif

/* vim: set autoindent expandtab sw=4 : */
