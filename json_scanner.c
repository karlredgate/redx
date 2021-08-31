
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "errors.h"
#include "scanner.h"

static int c = 0;
static int stringpos;

char input[4096];
static int input_pos;

int current_token;
char current_identifier[1024];
char current_string[4096];
long current_integer;

char *
report_token( int token ) {
    static char s[4096];

    switch (token) {
    case ':':     return "<:>";
    case ',':     return "<,>";
    case '{':     return "<{>";
    case '}':     return "<}>";
    case '[':     return "<[>";
    case ']':     return "<]>";
    case EOI:     return "<EOI>";
    case IDENTIFIER:
        sprintf( s, "IDENTIFIER<%s>", current_string );
        return s;
    case INTEGER:
        sprintf( s, "INTEGER<%ld>", current_integer );
        return s;
    case STRING:
        sprintf( s, "STRING<%s>", current_string );
        return s;
    case KEYWORD_TRUE: return "true";
    case KEYWORD_FALSE: return "false";
    case KEYWORD_NULL: return "null";
    default:
        sprintf( s, "<? %d ?>", token );
        return s;
    }
}

void
nextchar() {
    c = getchar();
    input[input_pos++] = c;
    input[input_pos] = '\0';
}

void
init_scanner() {
    input_pos = 0;
    nextchar();
}

void
scanner_report( char *message ) {
    int i;
    int limit = input_pos - 1;

    fprintf( stderr, "%s\n", input );
    for ( i = 0 ; i < limit ; ++i )  putc( ' ', stderr );
    fprintf( stderr, "^\n" );
    fprintf( stderr, "%s\n", message );
}

void
check_for_keyword() {
    if ( strcmp(current_string, "null") == 0 ) {
        current_token = KEYWORD_NULL;
        return;
    }
    if ( strcmp(current_string, "true") == 0 ) {
        current_token = KEYWORD_TRUE;
        return;
    }
    if ( strcmp(current_string, "false") == 0 ) {
        current_token = KEYWORD_FALSE;
        return;
    }
}

void
get_next_token() {
    int digit;

start_scan:
    switch (c) {
    case EOF:
        current_token = EOI;
        return;
    case ' ':
    case '\t':
    case '\r':
    case '\n': goto eat_whitespace;
    case '"':
        stringpos = 0;
        nextchar();
        goto recognize_string;
    case '{':
    case '}':
    case '[':
    case ']':
    case ':':
    case ',':
        current_token = c;
        nextchar();
        return;
    case '0': goto determine_base;
    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8':
    case '9': goto recognize_decimal;
    }

    stringpos = 0;
    goto recognize_identifier;

eat_whitespace:
    nextchar();
    switch (c) {
    case ' ':
    case '\t':
    case '\r':
    case '\n': goto eat_whitespace;
    }
    goto start_scan;

recognize_decimal:
    switch (c) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        current_integer *= 10;
        current_integer += c - '0';
        nextchar();
        goto recognize_decimal;
    default:
        current_token = INTEGER;
        return;
    }

recognize_hex:
    switch (c) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        digit = c - '0';
        goto add_hex_digit;
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
        digit = (c - 'a') + 10;
        goto add_hex_digit;
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
        digit = (c - 'A') + 10;
        goto add_hex_digit;
    default:
        current_token = INTEGER;
        return;
    }
add_hex_digit:
    current_integer *= 16;
    current_integer += digit;
    nextchar();
    goto recognize_hex;

recognize_octal:
    switch (c) {
    case '0': case '1': case '2': case '3':
    case '4': case '5': case '6': case '7':
        current_integer *= 8;
        current_integer += c - '0';
        nextchar();
        goto recognize_octal;
    case '8':
    case '9':
        invalid_character( c );
    default:
        current_token = INTEGER;
        return;
    }

determine_base:
    nextchar();
    switch (c) {
    case 'x':
        nextchar();
        goto recognize_hex;
    case '0': case '1': case '2': case '3': case '4': case '5': case '6':
    case '7': goto recognize_octal;
    case '8': case '9':
        invalid_character( c );
    default:
        current_integer = 0;
        current_token = INTEGER;
        return;
    }

recognize_string:
    switch (c) {
    case EOF:
        unterminated_string();
    case '"':
        current_string[stringpos] = '\0';
        nextchar();
        current_token = STRING;
        return;
    case '\\':
        nextchar();
        goto recognize_metachar;
    default:
        current_string[stringpos++] = c;
        nextchar();
        goto recognize_string;
    }

recognize_metachar:
    switch (c) {
    case EOF:
        unterminated_string();
    case '"':
        current_string[stringpos++] = '"';
        nextchar();
        goto recognize_string;
    case 'n':
        current_string[stringpos++] = '\n';
        nextchar();
        goto recognize_string;
    case 'r':
        current_string[stringpos++] = '\r';
        nextchar();
        goto recognize_string;
    case 't':
        current_string[stringpos++] = '\t';
        nextchar();
        goto recognize_string;
    default:
        invalid_character( c );
    }

add_char_to_identifier:
    current_string[stringpos++] = c;
    nextchar();

recognize_identifier:
    switch (c) {
    case '_':
    case '$': goto add_char_to_identifier;
    }

    if ( c < 'A' ) goto identifier_recognized;
    if ( c <= 'Z' ) goto add_char_to_identifier;
    if ( c < 'a' ) goto identifier_recognized;
    if ( c <= 'z' ) goto add_char_to_identifier;
    if ( stringpos == 0 ) invalid_character( c );
    goto identifier_recognized;

identifier_recognized:
    current_string[stringpos] = '\0';
    current_token = IDENTIFIER;
    check_for_keyword();
    return;
}

int
look_ahead( int _token ) {
    return current_token == _token;
}

int
required( int found ) {
    if ( found == 1 ) return 1;
    printf( "CURRENT TOKEN: %s\n", report_token(current_token) );
    parse_error("required");
    return 0;
}

int
token( int _token ) {
    // printf( "CHECK TOKEN current %s == %s\n", report_token(current_token), report_token(_token) );
    if ( current_token != _token )  return 0;
    get_next_token();
    // printf( "NEXT TOKEN: %s\n", report_token(current_token) );
    return 1;
}

/* vim: set autoindent expandtab sw=4 : */
