
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

/** \file traps.c
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

/**
 */
static void
segv( int sig, siginfo_t *info, void *data ) {
    int i;
    int frame_count = 0;
    void *pointers[256];

    switch ( info->si_code ) {
    case SEGV_MAPERR: syslog( LOG_ERR, "SEGV: address not mapped to object (%p)", info->si_addr ); break;
    case SEGV_ACCERR: syslog( LOG_ERR, "SEGV: invalid permissions for mapped object (%p)", info->si_addr ); break;
    default: syslog( LOG_ERR, "segmentation violation at address %p\n", info->si_addr );
    }

    frame_count = backtrace( pointers, sizeof(pointers) );
    char **frames = backtrace_symbols( pointers, frame_count );
    for ( i = 0 ; i < frame_count ; ++i ) {
        syslog( LOG_ERR, "frame(%03d): %s", i, frames[i] );
    }
    abort();
}

/**
 */
static void
bus( int sig, siginfo_t *info, void *data ) {
    int i;
    int frame_count = 0;
    void *pointers[256];

    switch ( info->si_code ) {
    case BUS_ADRALN: syslog( LOG_ERR, "BUS: invalid address alignment (%p)", info->si_addr ); break;
    case BUS_ADRERR: syslog( LOG_ERR, "BUS: non-existent physical address (%p)", info->si_addr ); break;
    case BUS_OBJERR: syslog( LOG_ERR, "BUS: object specific hardware error (%p)", info->si_addr ); break;
    default: syslog( LOG_ERR, "bus error at address %p\n", info->si_addr );
    }

    frame_count = backtrace( pointers, sizeof(pointers) );
    char **frames = backtrace_symbols( pointers, frame_count );
    for ( i = 0 ; i < frame_count ; ++i ) {
        syslog( LOG_ERR, "frame(%03d): %s", i, frames[i] );
    }
    abort();
}

/**
 */
static void
fpe( int sig, siginfo_t *info, void *data ) {
    int i;
    int frame_count = 0;
    void *pointers[256];

    switch ( info->si_code ) {
    case FPE_INTDIV: syslog( LOG_ERR, "FPE: integer divide by zero" ); break;
    case FPE_INTOVF: syslog( LOG_ERR, "FPE: integer overflow" ); break;
    case FPE_FLTDIV: syslog( LOG_ERR, "FPE: floating point divide by zero" ); break;
    case FPE_FLTOVF: syslog( LOG_ERR, "FPE: floating point overflow" ); break;
    case FPE_FLTUND: syslog( LOG_ERR, "FPE: floating point underflow" ); break;
    case FPE_FLTRES: syslog( LOG_ERR, "FPE: floating point inexact result" ); break;
    case FPE_FLTINV: syslog( LOG_ERR, "FPE: floating point invalid operation" ); break;
    case FPE_FLTSUB: syslog( LOG_ERR, "FPE: floating point subscript out of range" ); break;
    default: syslog( LOG_ERR, "FPE at address %p\n", info->si_addr ); break;
    }

    frame_count = backtrace( pointers, sizeof(pointers) );
    char **frames = backtrace_symbols( pointers, frame_count );
    for ( i = 0 ; i < frame_count ; ++i ) {
        syslog( LOG_ERR, "frame(%03d): %s", i, frames[i] );
    }
    abort();
}

/**
 */
static void
ill( int sig, siginfo_t *info, void *data ) {
    int i;
    int frame_count = 0;
    void *pointers[256];

    switch ( info->si_code ) {
    case ILL_ILLOPC: syslog( LOG_ERR, "ILL: illegal opcode (%p)", info->si_addr ); break;
    case ILL_ILLOPN: syslog( LOG_ERR, "ILL: illegal operand (%p)", info->si_addr ); break;
    case ILL_ILLADR: syslog( LOG_ERR, "ILL: illegal addressing mode (%p)", info->si_addr ); break;
    case ILL_ILLTRP: syslog( LOG_ERR, "ILL: illegal trap (%p)", info->si_addr ); break;
    case ILL_PRVOPC: syslog( LOG_ERR, "ILL: privileged opcode (%p)", info->si_addr ); break;
    case ILL_PRVREG: syslog( LOG_ERR, "ILL: privilieged register (%p)", info->si_addr ); break;
    case ILL_COPROC: syslog( LOG_ERR, "ILL: coprocessor error (%p)", info->si_addr ); break;
    case ILL_BADSTK: syslog( LOG_ERR, "ILL: internal stack error (%p)", info->si_addr ); break;
    default: syslog( LOG_ERR, "illegal instruction at address %p\n", info->si_addr );
    }

    frame_count = backtrace( pointers, sizeof(pointers) );
    char **frames = backtrace_symbols( pointers, frame_count );
    for ( i = 0 ; i < frame_count ; ++i ) {
        syslog( LOG_ERR, "frame(%03d): %s", i, frames[i] );
    }
    abort();
}

/**
 */
void
trap_error_signals() {
    struct sigaction action;

    action.sa_sigaction = segv;
    sigemptyset( &(action.sa_mask) );
    action.sa_flags = SA_SIGINFO;
    if ( sigaction(SIGSEGV, &action, 0) != 0 ) {
        // printf( "could not trap SIGSEGV\n" );
	// return errno;
    }

    action.sa_sigaction = bus;
    sigemptyset( &(action.sa_mask) );
    action.sa_flags = SA_SIGINFO;
    if ( sigaction(SIGBUS, &action, 0) != 0 ) {
        // printf( "could not trap SIGSEGV\n" );
	// return errno;
    }

    action.sa_sigaction = fpe;
    sigemptyset( &(action.sa_mask) );
    action.sa_flags = SA_SIGINFO;
    if ( sigaction(SIGFPE, &action, 0) != 0 ) {
        // printf( "could not trap SIGSEGV\n" );
	// return errno;
    }

    action.sa_sigaction = ill;
    sigemptyset( &(action.sa_mask) );
    action.sa_flags = SA_SIGINFO;
    if ( sigaction(SIGILL, &action, 0) != 0 ) {
        // printf( "could not trap SIGSEGV\n" );
	// return errno;
    }

}

/* vim: set autoindent expandtab sw=4 : */
