/*-*- c++ -*-********************************************************
 * String class interface for python                                *
 *                                                                  *
 * (C) 1998 by Karsten Ball�der (Ballueder@usa.net)                 *
 *                                                                  *
 * $Id$                 *
 *******************************************************************/

// naming this module String is a very bad idea for systems with
// case-insensitive file system (Windows...) because of standard Python
// module string.
%module 	MString
%{
#include  "Mcommon.h"

#include  <wx/config.h>

// we don't want to export our functions as we don't build a shared library
#undef SWIGEXPORT
#define SWIGEXPORT(a,b) a b
%}

class String 
{
public:
   String(const char *);
   const char *c_str(void);
};

