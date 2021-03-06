
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

/** \file util.c
 * \brief 
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <execinfo.h>
#include <glob.h>

static char hexchar[] = { '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f' };
static char hexval[] = {
  /* 0 */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 31 */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1, /* 63 */
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 95 */
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 127 */
};

/**
 */
int
parse_uuid( char *string, uint8_t *data ) {
    data[ 0] = (hexval[ string[ 0] & 0x7F ] << 4) + (hexval[ string[ 1] & 0x7F ]);
    data[ 1] = (hexval[ string[ 2] & 0x7F ] << 4) + (hexval[ string[ 3] & 0x7F ]);
    data[ 2] = (hexval[ string[ 4] & 0x7F ] << 4) + (hexval[ string[ 5] & 0x7F ]);
    data[ 3] = (hexval[ string[ 6] & 0x7F ] << 4) + (hexval[ string[ 7] & 0x7F ]);
    // string[8] == '-'
    data[ 4] = (hexval[ string[ 9] & 0x7F ] << 4) + (hexval[ string[10] & 0x7F ]);
    data[ 5] = (hexval[ string[11] & 0x7F ] << 4) + (hexval[ string[12] & 0x7F ]);
    // string[13] == '-'
    data[ 6] = (hexval[ string[14] & 0x7F ] << 4) + (hexval[ string[15] & 0x7F ]);
    data[ 7] = (hexval[ string[16] & 0x7F ] << 4) + (hexval[ string[17] & 0x7F ]);
    // string[18] == '-'
    data[ 8] = (hexval[ string[19] & 0x7F ] << 4) + (hexval[ string[20] & 0x7F ]);
    data[ 9] = (hexval[ string[21] & 0x7F ] << 4) + (hexval[ string[22] & 0x7F ]);
    // string[23] == '-'
    data[10] = (hexval[ string[24] & 0x7F ] << 4) + (hexval[ string[25] & 0x7F ]);
    data[11] = (hexval[ string[26] & 0x7F ] << 4) + (hexval[ string[27] & 0x7F ]);
    data[12] = (hexval[ string[28] & 0x7F ] << 4) + (hexval[ string[29] & 0x7F ]);
    data[13] = (hexval[ string[30] & 0x7F ] << 4) + (hexval[ string[31] & 0x7F ]);
    data[14] = (hexval[ string[32] & 0x7F ] << 4) + (hexval[ string[33] & 0x7F ]);
    data[15] = (hexval[ string[34] & 0x7F ] << 4) + (hexval[ string[35] & 0x7F ]);
    return 1;
}

/**
 */
static uint8_t
nybble( uint8_t *data, int n ) {
    uint8_t byte = data[ (n>>1)&0xF ];
    return (n&1) ? byte & 0xF : (byte>>4) & 0xF;
}

/**
 */
int
format_uuid( char *string, uint8_t *data ) {
    string[ 0] = hexchar[ nybble(data, 0) ];
    string[ 1] = hexchar[ nybble(data, 1) ];
    string[ 2] = hexchar[ nybble(data, 2) ];
    string[ 3] = hexchar[ nybble(data, 3) ];
    string[ 4] = hexchar[ nybble(data, 4) ];
    string[ 5] = hexchar[ nybble(data, 5) ];
    string[ 6] = hexchar[ nybble(data, 6) ];
    string[ 7] = hexchar[ nybble(data, 7) ];
    string[ 8] = '-';
    string[ 9] = hexchar[ nybble(data, 8) ];
    string[10] = hexchar[ nybble(data, 9) ];
    string[11] = hexchar[ nybble(data,10) ];
    string[12] = hexchar[ nybble(data,11) ];
    string[13] = '-';
    string[14] = hexchar[ nybble(data,12) ];
    string[15] = hexchar[ nybble(data,13) ];
    string[16] = hexchar[ nybble(data,14) ];
    string[17] = hexchar[ nybble(data,15) ];
    string[18] = '-';
    string[19] = hexchar[ nybble(data,16) ];
    string[20] = hexchar[ nybble(data,17) ];
    string[21] = hexchar[ nybble(data,18) ];
    string[22] = hexchar[ nybble(data,19) ];
    string[23] = '-';
    string[24] = hexchar[ nybble(data,20) ];
    string[25] = hexchar[ nybble(data,21) ];
    string[26] = hexchar[ nybble(data,22) ];
    string[27] = hexchar[ nybble(data,23) ];
    string[28] = hexchar[ nybble(data,24) ];
    string[29] = hexchar[ nybble(data,25) ];
    string[30] = hexchar[ nybble(data,26) ];
    string[31] = hexchar[ nybble(data,27) ];
    string[32] = hexchar[ nybble(data,28) ];
    string[33] = hexchar[ nybble(data,29) ];
    string[34] = hexchar[ nybble(data,30) ];
    string[35] = hexchar[ nybble(data,31) ];
    return 1;
}

/**
 */
int mkfile( char *path, size_t size ) {
    int result = 0;
    void *addr;
    int fd;

    fd = open("/dev/zero", O_RDWR);
    if ( fd < 0 ) {
        syslog( LOG_ERR, "[mkfile] could not map /dev/zero" );
        return 0;
    }
    addr = mmap( 0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0 );
    close( fd );

    fd = open( path, O_CREAT | O_WRONLY | O_TRUNC, 0644 );
    if ( fd < 0 ) {
        syslog( LOG_ERR, "[mkfile] open failed creating %s", path );
        goto done;
    }
    ssize_t bytes = write(fd, addr, size);
    if ( (size_t)bytes == size ) {
        result = 1;
    } else {
        syslog( LOG_ERR, "write failed creating %s", path );
    }
    close( fd );

done:
    munmap( addr, size );
    return result;
}

/**
 */
void
recursive_remove( char *path ) {
    glob_t subdirs;
    size_t i;
    char pattern[256];
    struct stat info;

    if ( path == NULL ) return;
    if ( *path == '\0' ) return;

    if ( stat(path, &info) < 0 ) return;
    if ( ! S_ISDIR(info.st_mode) ) {
        unlink( path );
        return;
    }

    sprintf( pattern, "%s/*", path );
    memset( &subdirs, 0, sizeof(subdirs) );

    glob( pattern, GLOB_NOSORT, NULL, &subdirs );

    for ( i = 0 ; i < subdirs.gl_pathc ; i++ ) {
        if ( stat(subdirs.gl_pathv[i], &info) < 0 ) continue;
        if ( S_ISDIR(info.st_mode) ) {
            recursive_remove( subdirs.gl_pathv[i] );
        } else {
            unlink( subdirs.gl_pathv[i] );
        }
    }

    globfree( &subdirs );

    rmdir( path );
}

/* vim: set autoindent expandtab sw=4 : */
