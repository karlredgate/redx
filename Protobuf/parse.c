
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/errno.h>

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

struct buffer {
    char *filename;
    void *address;
    void *dot;
    int length;
    int remaining;
};

struct buffer *
create_buffer( void *address, unsigned int length ) {
    struct buffer *state = (struct buffer *)malloc( length );
    state->dot = state->address = address;
    state->remaining = state->length = length;
    return state;
}

void
increment( struct buffer *state, int size ) {
    state->dot += size;
    state->remaining -= size;
}

uint64_t
read_varint( struct buffer *state ) {
    unsigned char *dot = state->dot;
    uint64_t value = 0;
    int shift = 0;

    for (;;) {
        uint64_t x = *dot++;
        value |= (x & 0x7F) << shift;
        // printf( "x 0x%02llx  shift %d  value 0x%llx\n", x, shift, value );
        if ( (x & 0x80) == 0 ) break;
        shift += 7;
    }

    state->remaining -= ((void*)dot - state->dot);
    state->dot = dot;
    return value;
}

uint64_t
read_64bit( struct buffer *state ) {
    uint64_t value = *( (uint64_t*)(state->dot) );
    increment( state, sizeof (value) );
    return value;
}

uint32_t
read_32bit( struct buffer *state ) {
    uint32_t value = *( (uint32_t*)(state->dot) );
    increment( state, sizeof (value) );
    return value;
}

int
not_empty( struct buffer *state ) {
    if ( state->remaining > 0 ) return 1;
    return 0;
}

int
parse( struct buffer *state ) {
    while ( not_empty(state) ) {
        uint64_t tag = read_varint( state );
        unsigned char wire_type = tag & 0x07;
        unsigned char field_number = (tag >> 3);
        printf( "TAG '%lld' -> field %d type %d\n", tag, field_number, wire_type );
        switch (wire_type) {
        case 0: { // VARINT
            uint64_t value = read_varint( state );
            fprintf( stdout, "VARINT   %lld\n", value );
            break;
        }
        case 1: { // 64BIT
            if ( state->remaining < sizeof (uint64_t) ) return EMSGSIZE;
            uint64_t value = read_64bit( state );
            fprintf( stdout, "64BIT  %lld  0x%llx\n", value, value );
            break;
        }
        case 2: { // LENGTH
            struct buffer chunk;
            chunk.length = read_varint( state );
            chunk.address = state->dot;
            chunk.dot = chunk.address;
            chunk.remaining = chunk.length;

            if ( chunk.length > state->remaining ) {
                fprintf( stdout, "ERROR READING SUBMESSAGE\n" );
                return EMSGSIZE;
            }
            state->dot += chunk.length;
            state->remaining -= chunk.length;
            fprintf( stdout, "LENGTH %d\n", chunk.length );

            char *here = chunk.address;
            int l = chunk.length;
            while ( l-- > 0 ) {
                printf( "%2d,", *here++ );
            }
            printf( "\n" );
            printf( "====== BEGIN\n" );
            parse( &chunk );
            printf( "====== END\n" );
            break;
        }
        case 3: { // GROUP_START
            fprintf( stdout, "GROUP-S\n" );
            break;
        }
        case 4: { // GROUP_END
            fprintf( stdout, "GROUP-E\n" );
            break;
        }
        case 5: {// 32BIT
            uint32_t value = read_32bit( state );
            fprintf( stdout, "32BIT  %d\n", value );
            break;
        }
        }
    }
    return 0;
}

int
load_file( char *filename, struct buffer *state ) {
    struct stat info;
    int result = stat( filename, &info );

    int fd = open( filename, O_RDONLY );
    if ( fd == -1 ) {
        fprintf( stderr, "Cannot open file\n" );
        exit( 1 );
    }

    fprintf( stderr, "Attempting to mmap '%s' length '%lld' with fd '%d'\n",
            filename, info.st_size, fd );
    void *address = mmap( 0, info.st_size, PROT_READ, MAP_PRIVATE|MAP_FILE, fd, 0 );
    if ( address == MAP_FAILED ) {
        perror( "map failed: " );
        fprintf( stderr, "Cannot map file\n" );
        exit( 1 );
    }

    state->filename = filename;
    state->dot = state->address = address;
    state->remaining = state->length = info.st_size;

    return 0;
}

int main( int argc, char **argv ) {
    struct buffer state;

    if ( load_file(argv[1], &state) != 0 ) {
        fprintf( stderr, "Failed to load protobuf\n" );
        exit( 1 );
    }

    parse( &state );

    return 0;
}

/* vim: set autoindent expandtab sw=4 : */
