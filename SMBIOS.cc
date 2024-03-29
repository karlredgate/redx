
/*
 * Copyright (c) 2012-2021 Karl N. Redgate
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

/** \file BIOS.cc
 * \brief 
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <errno.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <string.h>
#include <tcl.h>
#include "tcl_util.h"

#include "logger.h"
#include "SMBIOS.h"

namespace {
    typedef SMBIOS::Structure *(*StructureFactory)( void * );
    static const int MAX_FACTORY = 256;
    static StructureFactory factories[MAX_FACTORY];
    static const char *unknown = "UNKNOWN";
    int debug = 0;
}

/**
 * There may not be anythign necessary here
 */
SMBIOS::Structure::Structure( void *address ) {
    data = (uint8_t *)address;
    structure_type = data[0];
    header_length = data[1];
    strings = new SMBIOS::StringList( (char *)(data + header_length) );
    SMBIOS::StringList &s = *strings;

    int count = strings->count();
    if ( debug ) printf( "%d strings\n", count );

    for ( int i = 0 ; i < count ; i++ ) {
        if ( debug ) printf( "S[%d]: '%s'\n", i, s[i] );
    }
}

/**
 */
SMBIOS::Structure::~Structure() {
    delete strings;
}

/**
 * There may not be anythign necessary here
 */
const char *
SMBIOS::Structure::string( uint8_t n ) const {
    uint8_t index = data[n] - 1;
    SMBIOS::StringList &s = *strings;
    if ( s.count() < index ) return NULL;
    return s[index];
}

/**
 * read data in address and constuct -- and set string list
 */
SMBIOS::BIOSInformation::BIOSInformation( void *address )
: SMBIOS::Structure(address) {
    _vendor = string(0x4);
    _version = string(0x5);
    _release_date = string(0x8);
}

/**
 */
SMBIOS::System::System( void *address )
: SMBIOS::Structure(address) {
}

/**
 */
const char * SMBIOS::System::manufacturer()  const { return string(0x4); }
const char * SMBIOS::System::product_name()  const { return string(0x5); }
const char * SMBIOS::System::serial_number() const { return string(0x7); }

/**
 */
uint8_t *
SMBIOS::System::uuid_string() const {
    return data + 8;
}

/**
 */
UUID *
SMBIOS::System::uuid() {
    return new UUID( uuid_string() );
}

/**
 */
SMBIOS::BaseBoard::BaseBoard( SMBIOS::System *system )
: SMBIOS::Structure( system->address() ) {
}

/**
 */
SMBIOS::BaseBoard::BaseBoard( void *address )
: SMBIOS::Structure(address) {
}

/**
 */
const char *  SMBIOS::BaseBoard::manufacturer()     const { return string(0x4); }
const char *  SMBIOS::BaseBoard::product_name()     const { return string(0x5); }
const char *  SMBIOS::BaseBoard::version()          const { return string(0x6); }
const char *  SMBIOS::BaseBoard::serial_number()    const { return string(0x7); }
const char *  SMBIOS::BaseBoard::asset_tag()        const { return string(0x8); }
const char *  SMBIOS::BaseBoard::chassis_location() const { return string(0xA); }
uint8_t SMBIOS::BaseBoard::features()         const { return data[0x9]; }
uint8_t SMBIOS::BaseBoard::type_id()          const { return data[0xD]; }

/**
 */
SMBIOS::Chassis::Chassis( void *address )
: SMBIOS::Structure(address) {
}

/**
 */
const char *
SMBIOS::Chassis::manufacturer() const {
    return (header_length < 0x9) ? unknown : string(0x4);
}

/**
 */
const char *
SMBIOS::Chassis::version() const {
    return (header_length < 0x9) ? unknown : string(0x6);
}

/**
 */
const char *
SMBIOS::Chassis::serial_number() const {
    return (header_length < 0x9) ? unknown : string(0x7);
}

/**
 */
const char *
SMBIOS::Chassis::asset_tag() const {
    return (header_length < 0x9) ? unknown : string(0x8);
}

/**
 */
uint8_t SMBIOS::Chassis::chassis_type_id() const {
    return (header_length < 0x9) ? 0 : (data[0x5] & 0x7F);
}

namespace {
    const char * const ChassisTypeName[] = {
        "Other",
        "Unknown",
        "Desktop",
        "Low Profile Desktop",
        "Pizza Box",
        "Mini Tower",
        "Tower",
        "Portable",
        "Laptop",
        "Notebook",
        "Hand Held",
        "Docking Station",
        "All In One",
        "Sub Notebook",
        "Space-saving",
        "Lunch Box",
        "Main Server Chassis", /* master.mif says System */
        "Expansion Chassis",
        "Sub Chassis",
        "Bus Expansion Chassis",
        "Peripheral Chassis",
        "RAID Chassis", 
        "Rack Mount Chassis",   
        "Sealed-case PC",
        "Multi-system" /* 0x19 */
    };
}

/**
 */
const char *
SMBIOS::Chassis::chassis_name() const {
    return ChassisTypeName[chassis_type_id()];
}

/**
 */
uint8_t SMBIOS::Chassis::chassis_lock_id() const {
    return (header_length < 0x9) ? 0 : (data[0x5] >> 7);
}

/**
 */
uint8_t SMBIOS::Chassis::bootup_state_id() const {
    return (header_length < 0xD) ? 0 : data[0x9];
}

/**
 */
uint8_t SMBIOS::Chassis::power_supply_state_id() const {
    return (header_length < 0xD) ? 0 : data[0xA];
}

/**
 */
uint8_t SMBIOS::Chassis::thermal_state_id() const {
    return (header_length < 0xD) ? 0 : data[0xB];
}

/**
 */
uint8_t SMBIOS::Chassis::security_state_id() const {
    return (header_length < 0xD) ? 0 : data[0xC];
}

/**
 */
uint8_t SMBIOS::Chassis::power_cords() const {
    return (header_length < 0x15) ? 0 : data[0x12];
}

/**
 */
SMBIOS::Processor::Processor( void *address )
: SMBIOS::Structure(address) {
}

/**
 */
const char *
SMBIOS::Processor::socket_designation() const {
    if ( header_length < 0x1A ) return unknown;
    return string(0x4);
}

/**
 */
const char *
SMBIOS::Processor::manufacturer() const {
    if ( header_length < 0x1A ) return unknown;
    return string(0x7);
}

/**
 */
const char *
SMBIOS::Processor::version() const {
    if ( header_length < 0x1A ) return unknown;
    return string(0x10);
}

/**
 */
const char *
SMBIOS::Processor::serial_number() const {
    if ( header_length < 0x23 ) return unknown;
    return string(0x20);
}

/**
 */
const char *
SMBIOS::Processor::asset_tag() const {
    if ( header_length < 0x23 ) return unknown;
    return string(0x21);
}

/**
 */
const char *
SMBIOS::Processor::part_number() const {
    if ( header_length < 0x23 ) return unknown;
    return string(0x22);
}

/**
 */
bool SMBIOS::Processor::is_populated()     const { return ( data[0x18] & (1<<6) ) != 0; }
bool SMBIOS::Processor::is_enabled()       const { return ( data[0x18] & 0x7 ) == 1; }
bool SMBIOS::Processor::is_user_disabled() const { return ( data[0x18] & 0x7 ) == 2; }
bool SMBIOS::Processor::is_bios_disabled() const { return ( data[0x18] & 0x7 ) == 3; }
bool SMBIOS::Processor::is_idle()          const { return ( data[0x18] & 0x7 ) == 4; }

/**
 * read each table recursively?
 *
 * do not need to recurse, allocate Structures based on count passed in
 * then probe each one calling the correct constructor method based on the
 * structure type.
 *
 * each structure should probe its own string list and use that to pass back
 * the address of the next Structure in the memory image
 */
SMBIOS::Table::Table( uint8_t *base, uint16_t length, uint16_t count )
: _count(count), bios(0), system(0), baseboard(0), chassis(0) {
    if ( debug ) printf( "DMI is at %p length %hd count %hd\n", base, length, count );

    int fd = open( "/dev/mem", O_RDONLY);
    if ( fd == -1 ) {
        perror( "cannot open mem" );
        exit( 1 );
    }

    dmi = (Structure **)malloc( sizeof(Structure *) * count );
    size_t offset = (size_t)base % getpagesize();

    uint8_t *mapped_address = (uint8_t *)mmap( 0, offset + length, PROT_READ, MAP_SHARED, fd, (off_t)base - offset );
    if ( mapped_address == MAP_FAILED ) {
        perror( "cannot map dmi table address" );
        exit( 1 );
    }
    address = (void *)( mapped_address + offset );
    if ( debug ) printf( "mapped dmi table\n" );
    close( fd );

    // struct dmi_header *h = (struct dmi_header *)address;
    uint8_t *p = (uint8_t*)address;
    for ( int i = 0 ; i < count ; i++ ) {
        uint8_t type = p[0];
        uint8_t len = p[1];

        if ( debug ) printf( "DMI header %d type %d len %d -- ", i, type, len );
        StructureFactory factory = factories[type];
        SMBIOS::Structure *structure = factory( p );
        dmi[i] = structure;

        switch (type) {
        case 0: bios = structure; break;
        case 1: system = structure; break;
        case 2: baseboard = structure; break;
        case 3: chassis = structure; break;
        case 4:
            sockets.push_back( (SMBIOS::Processor*)structure );
            break;
        }

        p = (uint8_t *)( structure->next() );
    }

    if ( baseboard == NULL ) {
        log_warn( "BaseBoard information missing from SMBIOS tables" );
        baseboard = new SMBIOS::BaseBoard( system );
    }
}

/**
 */
SMBIOS::Table::~Table() {
    if ( dmi == NULL ) return;
    for ( int i = 0 ; i < _count ; i++ ) {
        if ( dmi[i] == NULL ) continue;
        delete dmi[i];
    }
    free( dmi );
}

/**
 */
SMBIOS::Header::Header()
: table(0), dmi_address(0), dmi_length(0) { }

/**
 */
SMBIOS::Header::~Header() {
    if ( table != 0 ) delete table;
}

/**
 */
void SMBIOS::Header::probe( uint8_t *data ) {
    if ( data[0x10] == '_' && data[0x11] == 'D' && data[0x12] == 'M' && data[0x13] == 'I' && data[0x14] == '_' ) {
        if ( debug > 3 ) printf( "I see DMI header\n" );
    } else {
        if ( debug > 3 ) printf( "I DO NOT see DMI header\n" );
    }
    major_version = data[0x6];
    minor_version = data[0x7];
    log_notice( "SMBIOS version %u.%u", major_version, minor_version );
    if ( debug ) printf( "SMBIOS version %u.%u\n", major_version, minor_version );

    dmi_address = (uint8_t *) *( (uint32_t*)(data + 0x18) );
    dmi_length  = *( (uint16_t*) (data + 0x16) );
    dmi_number  = *( (uint16_t*) (data + 0x1C) );
    dmi_version = (data[0x6] << 8) + data[0x7];
    table = new Table( dmi_address, dmi_length, dmi_number );
}

/** Factory method
 * 
 */
uint8_t *
SMBIOS::Header::locate() {
    for ( int i = 0 ; i < MAX_FACTORY ; i++ ) {
        factories[i] = SMBIOS::Structure::Factory;
    }
    // \todo -- get rid of this memset... because of ^^^
    memset( factories, 0, sizeof(*factories) );
    factories[0] = SMBIOS::BIOSInformation::Factory;
    factories[1] = SMBIOS::System::Factory;
    factories[2] = SMBIOS::BaseBoard::Factory;
    factories[3] = SMBIOS::Chassis::Factory;
    factories[4] = SMBIOS::Processor::Factory;

    int memfd = open("/dev/mem", O_RDONLY);
    if ( memfd == -1 ) {
        perror( "cannot open mem" );
        exit( 1 );
    }
    void *address = mmap( 0, 0x10000, PROT_READ, MAP_SHARED, memfd, 0xF0000 );
    if ( address == MAP_FAILED ) {
        perror( "cannot map address" );
        exit( 1 );
    }
    if ( debug > 3 ) printf( "mapped SMBIOS\n" );
    close( memfd );

    /*
     * search through this chunk of memory for the SMBIOS header.
     */
    uint8_t *limit = (uint8_t *)address + 0xFFF0;
    uint8_t *p;
    for ( p = (uint8_t *)address ; p < limit ; p += 16 ) {
        if ( p[0] != '_' ) {
            continue;
        }
        if ( p[1] != 'S' ) {
            continue;
        }
        if ( p[2] != 'M' ) {
            continue;
        }
        if ( p[3] != '_' ) {
            continue;
        }
        if ( debug > 3 ) printf( "Found it at %p\n", p );
        goto found_smbios;
    }
    printf( "No SMBIOS table found\n" );
    exit( 1 );

found_smbios:
    return p;
}

/* vim: set autoindent expandtab sw=4 : */
