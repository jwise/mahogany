/*-*- c++ -*-********************************************************
 * MDialogs.h : GUI independent dialog boxes                        *
 *                                                                  *
 * (C) 1998 by Karsten Ball�der (Ballueder@usa.net)                 *
 *                                                                  *
 * $Id$
 *******************************************************************/

#ifndef MDIALOGS_H
#define MDIALOGS_H

#ifdef __GNUG__
#pragma interface "MDialogs.h"
#endif

/**@name Strings for default titles */
//@{
/// for error message
#define	MDIALOG_ERRTITLE	_("Error")
/// for error message
#define	MDIALOG_SYSERRTITLE	_("System Error")
/// for error message
#define	MDIALOG_FATALERRTITLE	_("Fatal Error")
/// for error message
#define	MDIALOG_MSGTITLE	_("Information")
/// for error message
#define	MDIALOG_YESNOTITLE	_("Please choose")
//@}

#ifdef USE_WXWINDOWS
#	include	"gui/wxMDialogs.h"
#endif

#endif
