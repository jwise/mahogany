/*-*- c++ -*-********************************************************
 * PersistentList.cc - a persistent STL compatible list class       *
 *                                                                  *
 * (C) 1997 by Karsten Ball�der (Ballueder@usa.net)                 *
 *                                                                  *
 * $Id$                                                             *
 ********************************************************************
 * $Log$
 * Revision 1.2  1998/03/26 23:05:39  VZ
 * Necessary changes to make it compile under Windows (VC++ only)
 * Header reorganization to be able to use precompiled headers
 *
 * Revision 1.1  1998/03/14 12:21:19  karsten
 * first try at a complete archive
 *
 *******************************************************************/

#ifdef __GNUG__
#pragma implementation "PersistentList.h"
#endif

#include <PersistentList.h>

#ifdef  USE_IOSTREAMH
  #include <fstream>
#else
  #include <fstream.h>
#endif

PersistentList::PersistentList(String const &ifilename)
   : list<PLEntry *>()
{
   filename = ifilename;

   ifstream	istr(filename.c_str());
   
}

PersistentList::~PersistentList()
{
}
