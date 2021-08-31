
#define ERROR          256
#define EOI            257
#define INTEGER        258
#define STRING         259
#define IDENTIFIER     260
#define KEYWORD_NULL   261
#define KEYWORD_TRUE   262
#define KEYWORD_FALSE  263

extern int current_token;
extern char current_identifier[1024];
extern char current_string[4096];
extern long current_integer;

char * report_token( int token );
void init_scanner();
void get_next_token();
void scanner_report( char * );
int look_ahead( int _token );
int required( int found );
int token( int _token );

/* vim: set autoindent expandtab sw=4 : */
