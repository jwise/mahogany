/*-*- c++ -*-********************************************************
 * MFrame.h : GUI independent frame/window representation           *
 *                                                                  *
 * (C) 1997, 1998 by Karsten Ball�der (Ballueder@usa.net)           *
 *                                                                  *
 * $Id$                 *
 *******************************************************************/

#ifndef MFRAME_H
#define MFRAME_H

//#ifdef __GNUG__
//#pragma interface "MFrame.h"
//#endif

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

#ifndef USE_WXWINDOWS2    // wxMFrame shouldn't have 2 virtual Show()
   /// make it visible or invisible
   virtual void Show(bool visible = true) = 0;
#endif  // wxWin 2
};

#endif
