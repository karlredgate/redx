
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

/** \file Bridge.h
 * \brief Bridge interface abstraction.
 *
 */

#ifndef _BRIDGE_H_
#define _BRIDGE_H_

namespace ICMPv6 { class Socket; }

/**
 */
namespace Network {

    class Interface;

    /**
     */
    class Bridge {
    private:
        char *_name;
        Interface *interface;
        Interface *captured[];
    public:
        Bridge( const char * );
        ~Bridge();
        const char * create();
        const char * destroy();
        const char * add( Interface * );
        const char * capture( Interface * );
        const char * remove( Interface * );
        const char * set_mac_address( unsigned char * );
        const char *name() const { return _name; }
        bool lock_address();
        bool unlock_address();

        unsigned long forward_delay();
        bool forward_delay( unsigned long );

        bool is_tunnelled() const;
    };

}

#endif

/* vim: set autoindent expandtab sw=4 : */
