/*-*- c++ -*-********************************************************
 * MFrame.h : GUI independent frame/window representation           *
 *                                                                  *
 * (C) 1997 by Karsten Ball�der (Ballueder@usa.net)                 *
 *                                                                  *
 * $Id$                                                             *
 ********************************************************************
 * $Log$
 * Revision 1.1  1998/03/14 12:21:11  karsten
 * first try at a complete archive
 *
 *******************************************************************/

#ifndef MFRAME_H
#define MFRAME_H

//#ifdef __GNUG__
//#pragma interface "MFrame.h"
//#endif

#include	<Mcommon.h>

/**
   MFrameBase virtual base class, defining the interface for a window.
   Every window should have a unique name associated with it for use
   in the configuration file. E.g. "FolderView" or "ComposeWindow".
*/

class MFrameBase
{   
private:
   /// used to set the name of the window class
   virtual void	SetName(String const & name = String("MFrame")) = 0;
public:
   /// virtual destructor
   virtual ~MFrameBase() {};

   /// used to set the title of the window class
   virtual void	SetTitle(String const & name = String("M")) = 0;
   /// make it visible or invisible
   virtual void Show(bool visible = true) = 0;
};

#ifdef	USE_WXWINDOWS
#	define	MFrame	wxMFrame
#	include	<wxMFrame.h>
#endif

#endif
