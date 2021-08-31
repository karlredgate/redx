
#include <stdio.h>
#include <stdlib.h>

#include "errors.h"
#include "scanner.h"

int JSONValue();

int
JSONNumber() {
    return token(INTEGER);
}

int
JSONString() {
    return token(STRING);
}

int
JSONList() {
    if ( JSONValue() == 0 ) return 0;
    while ( look_ahead(',') ) {
        token(',');
        if ( JSONValue() == 0 ) parse_error("trailing ,");
    }
    return 1;
}

int
JSONMember() {
    return JSONString() && required( token(':') ) && required( JSONValue() );
}

int
JSONMemberList() {
    if ( JSONMember() == 0 ) return 0;
    while ( look_ahead(',') ) {
        token(',');
        if ( JSONMember() == 0 ) parse_error("trailing ,");
    }
    return 1;
}

int
JSONObjectMembers() {
    return JSONMemberList() || 1;
}

int
JSONArrayMembers() {
    return JSONList() || 1;
}

int
recognized_JSONObject() {
    /* printf( "JSONObject\n" ); */
    return 1;
}

int
JSONObject() {
    return token('{') && JSONObjectMembers() && required( token('}') ) && recognized_JSONObject();
    return token('{') && JSONObjectMembers() && required( token('}') );
}

int
JSONArray() {
    return token('[') && JSONArrayMembers() && required( token(']') );
}

int
JSONBoolean() {
    return token(KEYWORD_TRUE) || token(KEYWORD_FALSE);
}

int
JSONValue() {
    return JSONObject() ||
           JSONArray() ||
           JSONNumber() ||
           JSONBoolean() ||
           JSONString() ||
           token(KEYWORD_NULL);
}

int
JSONText() {
    return JSONValue() || parse_error("invalid value");
}

void
parse() {
    init_scanner();
    get_next_token();
    JSONText() && required( token(EOI) );
}

int
main( int argc, char **argv ) {
    parse();
    return 0;
}

/* vim: set autoindent expandtab sw=4 : */
