/*-*- c++ -*-********************************************************
 * wxMApp class: do all GUI specific  application stuff             *
 *                                                                  *
 * (C) 1997,1998 by Karsten Ball�der (Ballueder@usa.net)            *
 *                                                                  *
 * $Id$                *
 *
 *******************************************************************/

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------
#include "Mpch.h"

#ifndef  USE_PCH
#  include "Mcommon.h"
#  include "guidef.h"
#  include "MFrame.h"
#  include "gui/wxMFrame.h"

#  include "kbList.h"
#  include "PathFinder.h"
#  include "MimeList.h"
#  include "MimeTypes.h"
#  include "Profile.h"
#  include "Mdefaults.h"
#  include "MApplication.h"
#  include "gui/wxMApp.h"

#  include <wx/log.h>
#endif

#include "MObject.h"

#include "gui/wxMainFrame.h"
#include "gui/wxIconManager.h"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// the name of config section where the log window position is saved
#define  LOG_FRAME_SECTION    "LogWindow"

// ----------------------------------------------------------------------------
// classes
// ----------------------------------------------------------------------------

// a small class implementing saving/restoring position for the log frame
class wxMLogWindow : public wxLogWindow
{
public:
   // ctor (creates the frame hidden)
   wxMLogWindow(wxFrame *pParent, const char *szTitle);

   // override base class virtual to implement saving the frame position
   virtual void OnFrameDelete(wxFrame *frame);
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxMLogWindow
// ----------------------------------------------------------------------------
wxMLogWindow::wxMLogWindow(wxFrame *pParent, const char *szTitle)
            : wxLogWindow(pParent, szTitle, FALSE)
{
  int x, y, w, h;
  wxMFrame::RestorePosition(LOG_FRAME_SECTION, &x, &y, &w, &h);
  GetFrame()->SetSize(x, y, w, h);
  Show();
}

void wxMLogWindow::OnFrameDelete(wxFrame *frame)
{
   wxMFrame::SavePosition(LOG_FRAME_SECTION, frame);

   wxLogWindow::OnFrameDelete(frame);
}

// ----------------------------------------------------------------------------
// wxMApp
// ----------------------------------------------------------------------------
wxMApp::wxMApp(void)
{
   m_IconManager = NULL;
}

wxMApp::~wxMApp()
{
   if(m_IconManager) delete m_IconManager;
}

// app initilization
#ifdef  USE_WXWINDOWS2
bool
#else //wxWin1
wxFrame *
#endif
wxMApp::OnInit()
{
   m_IconManager = new wxIconManager();

   if ( OnStartup() ) {
      // now we can create the log window
      wxLogWindow *log = new wxMLogWindow(m_topLevelFrame,
                                          _("M Activity Log"));

      // we want it to be above the log frame
#ifdef OS_WIN 
     m_topLevelFrame->Raise();
#endif

#     ifdef  USE_WXWINDOWS2
         return true;
#     else
         return m_topLevelFrame;
#     endif
   }
   else {
      ERRORMESSAGE(("Can't initialize application, terminating."));

      return 0; // either false or (wxFrame *)NULL
   }
}

MFrame *wxMApp::CreateTopLevelFrame()
{
   m_topLevelFrame = GLOBAL_NEW wxMainFrame();
   m_topLevelFrame->SetTitle(M_TOPLEVELFRAME_TITLE);
   m_topLevelFrame->Show(true);

   return m_topLevelFrame;
}

int wxMApp::OnExit()
{
   MObject::CheckLeaks();

   wxLogWindow *log = (wxLogWindow *)wxLog::GetActiveTarget();
   wxLog::SetActiveTarget(log->GetOldLog());
   delete log;
 
   // as c-client lib doesn't seem to think that deallocating memory is
   // something good to do, do it at it's place...
//FIXME
#ifdef __WXMSW__
   free(sysinbox());
   free(myhomedir());
   free(myusername());
#endif
   return 0;
}

// ----------------------------------------------------------------------------
// global vars
// ----------------------------------------------------------------------------

// this creates the one and only application object
#ifdef  USE_WXWINDOWS2
   IMPLEMENT_APP(wxMApp);
#else   // wxWin1
   wxMApp mApplication;
#endif  // wxWin ver
