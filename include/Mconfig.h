/*-*- c++ -*-********************************************************
 * Mconfig.h: configuration for M                                   *
 *                                                                  *
 * (C) 1997 by Karsten Ball�der (Ballueder@usa.net)                 *
 *                                                                  *
 * $Id$                                                             *
 ********************************************************************
 * $Log$
 * Revision 1.3  1998/03/26 23:05:36  VZ
 * Necessary changes to make it compile under Windows (VC++ only)
 * Header reorganization to be able to use precompiled headers
 *
 * Revision 1.2  1998/03/22 20:44:46  KB
 * fixed global profile bug in MApplication.cc
 * adopted configure/makeopts to Python 1.5
 *
 * Revision 1.1  1998/03/14 12:21:11  karsten
 * first try at a complete archive
 *
 *******************************************************************/
#ifndef MCONFIG_H
#define	MCONFIG_H

#include	"config.h"

#ifdef unix
#	define	OS_UNIX		1
#	define	OS_TYPE		"unix"
#elif defined(__WIN__) || defined (__WIN32__)
#	define	OS_WIN		1
#	define	OS_TYPE		"windows"
# ifndef  __WINDOWS__
#   define  __WINDOWS__     // for wxWindows 2.x
# endif
#else
  // this reminder is important, it won't compile without it anyhow...
# error   "Unknown platform (forgot to #define unix?)"
#endif

#ifdef  USE_WXWINDOWS2
  #define wxTextWindow  wxTextCtrl
  #define wxText        wxTextCtrl
#endif  // wxWin 2

/// use one common base class
#define	USE_COMMONBASE		1

/// do some memory allocation debugging
#define	USE_MEMDEBUG		1

/// derive common base from wxObject
#define	USE_WXOBJECT		0

/// debug allocator
#define	USE_DEBUGNEW		0

/// are we using precompiled headers?
#ifndef USE_PCH
# define USE_PCH        1
#endif

#if USE_DEBUGNEW
#	define	GLOBAL_NEW	  WXDEBUG_NEW
#	define	GLOBAL_DELETE	delete
#else
#	define	GLOBAL_NEW	  new
#	define	GLOBAL_DELETE	delete
#endif

/// use simple dynamic class information
#define	USE_CLASSINFO		1

/// name of the application
#define	M_APPLICATIONNAME	"M"


#if	USE_BASECLASS
#	define	BASECLASS	CommonBase
#endif

#ifdef	HAVE_COMPFACE_H
#	define	HAVE_XFACES
#endif

#define	M_STRBUFLEN		1024

#endif
