/*-*- c++ -*-********************************************************
 * wxMAppl class: all GUI specific application stuff                *
 *                                                                  *
 * (C) 1997 by Karsten Ball�der (Ballueder@usa.net)                 *
 *                                                                  *
 * $Id$                 *
 *******************************************************************/
#ifndef WXMAPP_H
#define WXMAPP_H

#ifndef USE_PCH
#  define   Uses_wxApp
#  include  <wx/wx.h>
#  include  <MApplication.h>
#endif  //USE_PCH

// fwd decl
class wxLog;
class wxIconManager;

// ----------------------------------------------------------------------------
// wxMApp
// ----------------------------------------------------------------------------

/**
  * A wxWindows implementation of MApplication class.
  */

class wxMApp : public wxApp, public MAppBase
{
public:
   /// Constructor
   wxMApp();

   /// create the main application window
   virtual MFrame *CreateTopLevelFrame();

   // wxWin calls these functions on application init/termination
   virtual bool OnInit();
   virtual int  OnExit();

   /// return a pointer to the IconManager:
   wxIconManager *GetIconManager(void) { return m_IconManager; }

   /// Destructor
   ~wxMApp();
private:
   /// an iconmanager instance
   wxIconManager *m_IconManager;
};

// ----------------------------------------------------------------------------
// global application object
// ----------------------------------------------------------------------------
#ifdef  USE_WXWINDOWS2
   // created dynamically by wxWindows
   DECLARE_APP(wxMApp);

#  define mApplication (wxGetApp())
#else
   // global variable
   extern MApplication mApplication;
#endif

#endif
