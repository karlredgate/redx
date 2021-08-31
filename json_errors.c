
#include <stdio.h>
#include <stdlib.h>

#include "scanner.h"

void die() { exit( 1 ); }

void
unterminated_string() {
    scanner_report( "unterminated string\n" );
    die();
}

int
invalid_character( int c ) {
    fprintf( stderr, "invalid character in input '%c'n", c );
    scanner_report( "here\n" );
    die();
    return 0;
}

int
parse_error( char *where ) {
    fprintf( stderr, "parse error (%s)\n", where );
    scanner_report( "at this token\n" );
    die();
    return 0;
}

/* vim: set autoindent expandtab sw=4 : */
