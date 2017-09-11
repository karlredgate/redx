
/*
 * Copyright (c) 2017 Karl N. Redgate
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

#include <stddef.h>

#ifndef _LIST_H_
#define _LIST_H_

/**
 */
class ListElement {
public:
    ListElement *next;
    ListElement( ListElement *next );
    virtual ~ListElement() { if ( next != NULL ) delete next; }
};

/**
 */
class ListAdapter {
public:
    ListAdapter() {}
    virtual ~ListAdapter() {}
    virtual void process( ListElement * ) = 0;
    virtual ListElement *transform( ListElement * ) = 0;
};

class List {
private:
    ListElement *elements;
public:
    List();
    virtual ~List();
    void insert( ListElement *element );
    void append( ListElement *element );
    void search( ListElement *element );
    void each( ListAdapter *adapter );
};

#endif

/* vim: set autoindent expandtab sw=4 : */
