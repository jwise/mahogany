///////////////////////////////////////////////////////////////////////////////
// Project:     M - cross platform e-mail GUI client
// File name:   gui/wxMApp.cpp - GUI-specific part of application logic
// Purpose:     program application, startup, non-portable hooks to implement
//              the functionality needed by MApplication, also log window impl
// Author:      Karsten Ball�der, Vadim Zeitlin
// Modified by:
// Created:     1997
// CVS-ID:      $Id$
// Copyright:   (c) 1997-2002 M-Team
// Licence:     M license
///////////////////////////////////////////////////////////////////////////////

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
#  include "strutil.h"
#  include "kbList.h"
#  include "PathFinder.h"
#  include "Profile.h"
#  include "MApplication.h"
#  include "gui/wxMApp.h"

#  include "Mcclient.h"

#  include <wx/log.h>
#  include <wx/config.h>
#  include <wx/generic/dcpsg.h>
#  include <wx/thread.h>
#endif

#include <wx/msgdlg.h>   // for wxMessageBox
#include "wx/persctrl.h" // for wxPMessageBoxEnable(d)
#include <wx/ffile.h>
#include <wx/menu.h>
#include <wx/statusbr.h>
#include <wx/fs_mem.h>
#include <wx/cmdline.h>

#ifdef USE_DIALUP
#  include <wx/dialup.h>
#endif // USE_DIALUP

#include <wx/fontmap.h>
#include <wx/encconv.h>
#include <wx/tokenzr.h>

#include <wx/snglinst.h>
#ifdef OS_WIN
   #define wxConnection    wxDDEConnection
   #define wxServer        wxDDEServer
   #define wxClient        wxDDEClient

   #include <wx/dde.h>
#else // !Windows
   #define wxConnection    wxTCPConnection
   #define wxServer        wxTCPServer
   #define wxClient        wxTCPClient

   #include <wx/sckipc.h>
#endif // Windows/!Windows

#include "MObject.h"

#include "Mdefaults.h"
#include "MDialogs.h"

#include "MHelp.h"

#include "gui/wxMainFrame.h"
#include "gui/wxIconManager.h"
#include "FolderMonitor.h"
#include "MModule.h"
#include "MThread.h"
#include "Mpers.h"

#include "Composer.h"       // for SaveAll()

#include "MFCache.h"

#include "CmdLineOpts.h"

#ifdef OS_WIN
#   include <winnls.h>

#   include "wx/msw/helpbest.h"
#endif

// ----------------------------------------------------------------------------
// options we use here
// ----------------------------------------------------------------------------

extern const MOption MP_AFMPATH;
extern const MOption MP_AUTOSAVEDELAY;
extern const MOption MP_AWAY_AUTO_ENTER;
extern const MOption MP_BEACONHOST;
extern const MOption MP_CONFIRMEXIT;
#ifdef USE_DIALUP
extern const MOption MP_DIALUP_SUPPORT;
#endif // USE_DIALUP
extern const MOption MP_HELPBROWSER;
extern const MOption MP_HELPBROWSER_ISNS;
extern const MOption MP_HELPDIR;
extern const MOption MP_HELPFRAME_HEIGHT;
extern const MOption MP_HELPFRAME_WIDTH;
extern const MOption MP_HELPFRAME_XPOS;
extern const MOption MP_HELPFRAME_YPOS;
extern const MOption MP_MODULES;
extern const MOption MP_NET_OFF_COMMAND;
extern const MOption MP_NET_ON_COMMAND;
extern const MOption MP_POLLINCOMINGDELAY;
extern const MOption MP_PRINT_BOTTOMMARGIN_X;
extern const MOption MP_PRINT_BOTTOMMARGIN_Y;
extern const MOption MP_PRINT_COLOUR;
extern const MOption MP_PRINT_COMMAND;
extern const MOption MP_PRINT_FILE;
extern const MOption MP_PRINT_MODE;
extern const MOption MP_PRINT_OPTIONS;
extern const MOption MP_PRINT_ORIENTATION;
extern const MOption MP_PRINT_PAPER;
extern const MOption MP_PRINT_TOPMARGIN_X;
extern const MOption MP_PRINT_TOPMARGIN_Y;
extern const MOption MP_SHOWLOG;
extern const MOption MP_SHOWTIPS;
extern const MOption MP_SMTPHOST;
extern const MOption MP_USE_OUTBOX;

// ----------------------------------------------------------------------------
// persistent msgboxes we use here
// ----------------------------------------------------------------------------

extern const MPersMsgBox *M_MSGBOX_CONFIRM_EXIT;
extern const MPersMsgBox *M_MSGBOX_SHOWLOGWINHINT;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// the name of config section where the log window position is saved
#define  LOG_FRAME_SECTION    "LogWindow"

// trace mask for timer events
#define TRACE_TIMER "timer"

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

/**
  Tries to save all unsaved stuff which the user wouldn't like to lose
 */
static int SaveAll()
{
   int rc = Composer::SaveAll();

   Profile::FlushAll();

   MfStatusCache::Flush();

   return rc;
}

// ----------------------------------------------------------------------------
// classes
// ----------------------------------------------------------------------------

// a small class implementing saving/restoring position for the log frame
class wxMLogWindow : public wxLogWindow
{
public:
   // ctor (creates the frame hidden)
   wxMLogWindow(wxFrame *pParent, const char *szTitle);

   // override base class virtual to implement saving the frame position and
   // to update the MP_SHOWLOG option
   virtual bool OnFrameClose(wxFrame *frame);
   virtual void OnFrameDelete(wxFrame *frame);

   // are we currently shown?
   bool IsShown() const;

private:
   bool m_hasWindow;
};

#ifdef wxHAS_LOG_CHAIN

// a wxLogStderr version which closes the file it logs to
class MLogFile : public wxLogStderr
{
public:
   MLogFile(FILE *fp) : wxLogStderr(fp) { }

   virtual ~MLogFile() { fclose(m_fp); }
};

#endif // wxHAS_LOG_CHAIN

// a timer used to periodically autosave profile settings
class AutoSaveTimer : public wxTimer
{
public:
   AutoSaveTimer() { m_started = FALSE; }

   virtual bool Start( int millisecs = -1, bool oneShot = FALSE )
      { m_started = TRUE; return wxTimer::Start(millisecs, oneShot); }

   virtual void Notify()
   {
      wxLogTrace(TRACE_TIMER, "Autosaving options and folder status.");

      (void)SaveAll();
   }

   virtual void Stop()
      { if ( m_started ) wxTimer::Stop(); }

public:
    bool m_started;
};

// a timer used to periodically check for new mail
class MailCollectionTimer : public wxTimer
{
public:
   virtual void Notify();
};

// a timer used to wake up idle loop to force generation of the idle events
// even if the program is otherwise idle (i.e. no GUI events happen)
class IdleTimer : public wxTimer
{
public:
   IdleTimer() : wxTimer() { Start(100); }

   virtual void Notify() { wxWakeUpIdle(); }
};

// and yet another timer for the away mode
class AwayTimer : public wxTimer
{
public:
   virtual void Notify()
   {
      wxLogTrace(TRACE_TIMER, "Going away on timer");

      mApplication->SetAwayMode(TRUE);
   }
};

// ----------------------------------------------------------------------------
// global vars
// ----------------------------------------------------------------------------

// a (unique) autosave timer instance
static AutoSaveTimer *gs_timerAutoSave = NULL;

// a (unique) timer for polling for new mail
static MailCollectionTimer *gs_timerMailCollection = NULL;

// the timer for auto going into away mode: may be NULL
static AwayTimer *gs_timerAway = NULL;

struct MModuleEntry
{
   MModuleEntry(MModule *m) { m_Module = m; }
   MModule *m_Module;
};

KBLIST_DEFINE(ModulesList, MModuleEntry);

// a list of modules loaded at startup:
static ModulesList gs_GlobalModulesList;

// the mutex we lock to block any background processing temporarily
static MMutex gs_mutexBlockBg;

// ============================================================================
// implementation
// ============================================================================

// this creates the one and only application object
IMPLEMENT_APP(wxMApp);

// ----------------------------------------------------------------------------
// MailCollectionTimer
// ----------------------------------------------------------------------------

void MailCollectionTimer::Notify()
{
   if ( !mApplication->AllowBgProcessing() )
      return;

   wxLogTrace(TRACE_TIMER, "Collection timer expired.");

   mApplication->UpdateOutboxStatus();

   FolderMonitor *collector = mApplication->GetFolderMonitor();
   if ( collector )
      collector->CheckNewMail();
}

// ----------------------------------------------------------------------------
// wxMLogWindow
// ----------------------------------------------------------------------------

wxMLogWindow::wxMLogWindow(wxFrame *pParent, const char *szTitle)
            : wxLogWindow(pParent, szTitle, FALSE)
{
   int x, y, w, h;
   bool i;
   wxMFrame::RestorePosition(LOG_FRAME_SECTION, &x, &y, &w, &h, &i);
   wxFrame *frame = GetFrame();

   frame->SetSize(x, y, w, h);
   frame->SetIcon(ICON("MLogFrame"));
   m_hasWindow = true;

   // normally we want to iconize the frame before showing it to avoid
   // flicker, but it doesn't seem to work under GTK
#ifdef __WXGTK__
   Show();
   frame->Iconize(i);
#else // !GTK
   frame->Iconize(i);
   Show();
#endif // GTK/!GTK
}

bool wxMLogWindow::IsShown() const
{
   if ( !m_hasWindow )
      return false;

   return GetFrame()->IsShown();
}

bool wxMLogWindow::OnFrameClose(wxFrame *frame)
{
   if ( !MDialog_YesNoDialog
        (
         _("Would you like to close the log window only for the rest "
           "of this session or permanently?\n"
           "\n"
           "Note that in either case you may make it appear again by "
           "changing the corresponding\n"
           "setting in the 'Miscellaneous' page of the Preferences dialog.\n"
           "\n"
           "Choose \"Yes\" to close the log window just for this "
           "session, \"No\" - to permanently close it."),
         NULL,
         MDIALOG_MSGTITLE,
         M_DLG_YES_DEFAULT,
         M_MSGBOX_SHOWLOGWINHINT
        ) )
   {
      // disable the log window permanently
      mApplication->GetProfile()->writeEntry(MP_SHOWLOG, 0l);
   }

   return wxLogWindow::OnFrameClose(frame); // TRUE, normally
}

void wxMLogWindow::OnFrameDelete(wxFrame *frame)
{
   wxMFrame::SavePosition(LOG_FRAME_SECTION, frame);

   m_hasWindow = false;

   wxLogWindow::OnFrameDelete(frame);
}

#if 0
// ----------------------------------------------------------------------------
// wxMStatusBar
// ----------------------------------------------------------------------------
class wxMStatusBar : public wxStatusBar
{
public:
   wxMStatusBar(wxWindow *parent) : wxStatusBar(parent, -1)
      {
         m_show = false;
         m_OnlineIcon = new wxIcon;
         m_OfflineIcon = new wxIcon;
         *m_Onli+neIcon = ICON( "online");
         *m_OfflineIcon = ICON( "offline");
      }

   ~wxMStatusBar()
      {
         delete m_OnlineIcon;
         delete m_OfflineIcon;
      }
   void UpdateOnlineStatus(bool show, bool isOnline)
      {
         m_show = show; m_isOnline = isOnline;
//         Refresh();
      }
   virtual void DrawField(wxDC &dc, int i)
      {
         wxStatusBar::DrawField(dc, i);
         if(m_show)
         {
            int field = mApplication->GetStatusField(MAppBase::SF_ONLINE);
            if(i == field)
            {
               wxRect r;
               GetFieldRect(i, r);
               dc.DrawIcon(m_isOnline ?
                           *m_OnlineIcon
                           : *m_OfflineIcon,
                           r.x, r.y+2); // +2: small vertical offset
            }
         }
      }
private:
   wxIcon *m_OnlineIcon;
   wxIcon *m_OfflineIcon;
   /// show online status?
   bool m_show;
   /// are we online?
   bool m_isOnline;
};
#endif

// ----------------------------------------------------------------------------
// wxMApp
// ----------------------------------------------------------------------------


BEGIN_EVENT_TABLE(wxMApp, wxApp)
   EVT_IDLE                (wxMApp::OnIdle)

#ifdef USE_DIALUP
   EVT_DIALUP_CONNECTED    (wxMApp::OnConnected)
   EVT_DIALUP_DISCONNECTED (wxMApp::OnDisconnected)
#endif // USE_DIALUP

   EVT_QUERY_END_SESSION   (wxMApp::OnQueryEndSession)
END_EVENT_TABLE()

// ----------------------------------------------------------------------------
// wxMApp ctor/dtor
// ----------------------------------------------------------------------------

wxMApp::wxMApp(void)
{
   m_IconManager = NULL;
   m_HelpController = NULL;
   m_CanClose = FALSE;
   m_IdleTimer = NULL;

#ifdef USE_DIALUP
   m_OnlineManager = NULL;
   m_DialupSupport = FALSE;
#endif // USE_DIALUP

   m_PrintData = NULL;
   m_PageSetupData = NULL;

   m_logWindow = NULL;
   m_logChain = NULL;

   m_snglInstChecker = NULL;
   m_serverIPC = NULL;

   m_topLevelFrame = NULL;
}

wxMApp::~wxMApp()
{
#ifdef wxHAS_LOG_CHAIN
   delete m_logChain;
#endif // wxHAS_LOG_CHAIN
}

void
wxMApp::OnIdle(wxIdleEvent &event)
{
   if ( AllowBgProcessing() )
   {
      MEventManager::DispatchPending();
   }

   event.Skip();
}

bool
wxMApp::AllowBgProcessing() const
{
   return !gs_mutexBlockBg.IsLocked();
}

#ifdef __WXDEBUG__

void
wxMApp::OnAssert(const wxChar *file, int line, const wxChar *msg)
{
   // don't provoke an assert from inside the assert (which would happen if we
   // tried to lock an already locked mutex below)
   if ( gs_mutexBlockBg.IsLocked() )
   {
      wxTrap();

      return;
   }

   // don't allow any calls to c-client while we're in the assert dialog
   // because we might be called from a c-client callback and another call to
   // it will result in a fatal error
   MLocker lock(gs_mutexBlockBg);

   wxApp::OnAssert(file, line, msg);
}

#endif // __WXDEBUG__

bool
wxMApp::Yield(bool onlyIfNeeded)
{
   // try to not crash...
   if ( gs_mutexBlockBg.IsLocked() )
      return false;

   // don't allow any calls to c-client from inside wxYield() neither as it is
   // implicitly (!) called by wxProgressDialog which is shown from inside some
   // c-client handlers and so calling c-client now would result in a crash
   MLocker lock(gs_mutexBlockBg);

   return wxApp::Yield(onlyIfNeeded);
}

void
wxMApp::OnAbnormalTermination()
{
   MAppBase::OnAbnormalTermination();

   // try to save the unsaved messages and options
   int nSaved = SaveAll();

   wxString msg;
   switch ( nSaved )
   {
      case -1:
         msg = _("Not all composer windows could be saved.");
         break;

      case 0:
         // leave empty
         break;

      default:
         msg.Printf(_("%d unsaved messages were saved,\n"
                      "they will be restored on the next program startup."),
                    nSaved);
   }

   static const char *msgCrash =
      gettext_noop("The application is terminating abnormally.\n"
                   "\n"
                   "Please report the bug via our bugtracker at\n"
                   "http://mahogany.sourceforge.net/bugz\n"
                   "Be sure to mention your OS version and what\n"
                   "exactly you were doing before this message\n"
                   "box appeared.\n"
                   "\n"
                   "Thank you!");
   static const char *title = gettext_noop("Fatal application error");

   if ( !msg.empty() )
   {
      msg += "\n\n";
   }

   msg += msgCrash;

   // using a plain message box is safer in this situation, but under Unix we
   // have no such choice
#ifdef __WXMSW__
   ::MessageBox(NULL, _(msg), _(title), MB_ICONSTOP);
#else // !MSW
   wxMessageBox(_(msg), _(title), wxICON_STOP | wxOK);
#endif // MSW/!MSW
}

// can we close now?
bool
wxMApp::CanClose() const
{
   // If we get called the second time, we just reuse our old value, but only
   // if it was TRUE (if we couldn't close before, may be we can now)
   if ( m_CanClose )
      return m_CanClose;

   // verify that the user didn't accidentally remembered "No" as the answer to
   // the next message box - the problem with this is that in this case there
   // is no way to exit M at all!
   String path = GetPersMsgBoxName(M_MSGBOX_CONFIRM_EXIT);
   if ( wxPMessageBoxIsDisabled(path) )
   {
      if ( !MDialog_YesNoDialog("", NULL, "",
                                M_DLG_NO_DEFAULT, M_MSGBOX_CONFIRM_EXIT) )
      {
         wxLogDebug("Exit confirmation msg box has been disabled on [No], "
                    "reenabling it.");

         wxPMessageBoxEnable(path);
      }
      //else: it was on [Yes], ok
   }

   // ask the user for confirmation
   if ( !MDialog_YesNoDialog(_("Do you really want to exit Mahogany?"),
                              m_topLevelFrame,
                              MDIALOG_YESNOTITLE,
                              M_DLG_YES_DEFAULT | M_DLG_NOT_ON_NO,
                              M_MSGBOX_CONFIRM_EXIT) )
   {
      return false;
   }

   wxWindowList::Node *node = wxTopLevelWindows.GetFirst();
   while ( node )
   {
      wxWindow *win = node->GetData();
      node = node->GetNext();

      // We do not ask the toplevel frame as this would ask us again,
      // leading to some recursion.
      if ( win->IsKindOf(CLASSINFO(wxMFrame)) && win != m_topLevelFrame)
      {
         wxMFrame *frame = (wxMFrame *)win;

         if ( !IsOkToClose(frame) )
         {
            if ( !frame->CanClose() )
               return false;
         }
         //else: had been asked before
      }
      //else: what to do here? if there is something other than a frame
      //      opened, it might be a (non modal) dialog or may be the log
      //      frame?
   }


   // We assume that we can always close the toplevel frame if we make
   // it until here. Attemps to close the toplevel frame will lead to
   // this function being evaluated anyway. The frame itself does not
   // do any tests.
   wxMApp *self = (wxMApp *)this;
   self->AddToFramesOkToClose(m_topLevelFrame);

   self->m_CanClose = MAppBase::CanClose();

   return m_CanClose;
}

void wxMApp::OnQueryEndSession(wxCloseEvent& event)
{
   m_CanClose = false;

   if ( !CanClose() )
      event.Veto();
}

// do close the app by closing all frames
void
wxMApp::DoExit()
{
   // shut down MEvent handling
   if(m_IdleTimer)
   {
      m_IdleTimer->Stop();
      delete m_IdleTimer;
      m_IdleTimer = NULL; // paranoid
   }

   // flush the logs while we can
   wxLog::FlushActive();

   // before deleting the frames, make sure that all dialogs which could be
   // still hanging around don't do it any more
   wxTheApp->DeletePendingObjects();

   // close all windows
   wxWindowList::Node *node = wxTopLevelWindows.GetFirst();
   while ( node )
   {
      wxWindow *win = node->GetData();
      node = node->GetNext();

      // avoid closing the log window because it is so smart that it will not
      // reopen itself the next time we're run if we do it
      if ( (!m_logWindow || (win != m_logWindow->GetFrame()))
               && win->IsKindOf(CLASSINFO(wxFrame)) )
      {
         wxMFrame *frame = (wxMFrame *)win;

         // force closing the frame
         frame->Close(TRUE);
      }
   }
}

// app initilization
bool
wxMApp::OnInit()
{
   // in release build we handle any exceptions we generate (i.e. crashes)
   // ourselves
#ifndef DEBUG
   wxHandleFatalExceptions();
#endif

   // create a semaphore indicating that we're running - or check if another
   // copy already is
   m_snglInstChecker = new wxSingleInstanceChecker;

   // note that we can't create the lock file in ~/.M because the directory
   // might not have been created yet...
   if ( !m_snglInstChecker->Create(".mahogany.lock") )
   {
      // this message is in English because translations were not loaded yet as
      // the locale hadn't been set
      wxLogWarning("Mahogany will not be able to detect if any other "
                   "program instances are running.");
   }

#if wxCHECK_VERSION(2, 3, 2)
   // parse our command line options inside OnInit()
   if ( !wxApp::OnInit() )
      return false;
#endif // wxWin 2.3.2+

#ifdef OS_WIN
   // stupidly enough wxWin resets the default timestamp under Windows :-(
   wxLog::SetTimestamp(_T("%X"));
#endif // OS_WIN

#ifdef USE_I18N
   // Set up locale first, so everything is in the right language.
   bool hasLocale = false;

#ifdef OS_UNIX
   const char *locale = getenv("LANG");
   hasLocale =
      locale &&
      (strcmp(locale, "C") != 0) &&
      (strcmp(locale, "en") != 0) &&
      (strncmp(locale,"en_", 3) != 0) &&
      (strcmp(locale, "us") != 0);
#elif defined(OS_WIN)
   // this variable is not usually set under Windows, but give the user a
   // chance to override our locale detection logic in case it doesn't work
   // (which is well possible because it's totally untested)
   wxString locale = getenv("LANG");
   if ( locale.Length() > 0 )
   {
      // setting LANG to "C" disables all this
      hasLocale = (locale[0] != 'C') || (locale[1] != '\0');
   }
   else
   {
      // try to detect locale ourselves
      LCID lcid = GetThreadLocale(); // or GetUserDefaultLCID()?
      WORD idLang = PRIMARYLANGID(LANGIDFROMLCID(lcid));
      hasLocale = idLang != LANG_ENGLISH;
      if ( hasLocale )
      {
         static char s_countryName[256];
         int rc = GetLocaleInfo(lcid, LOCALE_SABBREVCTRYNAME,
                                s_countryName, WXSIZEOF(s_countryName));
         if ( !rc )
         {
            // this string is intentionally not translated
            wxLogSysError("Can not get current locale setting.");

            hasLocale = FALSE;
         }
         else
         {
            // TODO what if the buffer was too small?
            locale = s_countryName;
         }
      }
   }
#else // Mac?
#   error "don't know how to get the current locale on this platform."
#endif // OS

#ifdef __LINUX__
   /* This is a hack for SuSE Linux which sets LANG="german" instead
      of LANG="de". Once again, they do their own thing...
   */
   if( hasLocale && (wxStricmp(locale, "german") == 0) )
      locale = "de_DE";
#endif // __LINUX__

   // if we fail to load the msg catalog, remember it in this variable because
   // we can't remember this in the profile yet - it is not yet created
   bool failedToLoadMsgs = false,
        failedToSetLocale = false;

   if ( hasLocale )
   {
      // TODO should catch the error messages and save them for later
      wxLogNull noLog;

      m_Locale = new wxLocale(locale, locale, NULL);
      if ( !m_Locale->IsOk() )
      {
         delete m_Locale;
         m_Locale = NULL;

         failedToSetLocale = true;
      }
      else
      {
#ifdef OS_UNIX
         String localePath;
         localePath << M_BASEDIR << "/locale";
#elif defined(OS_WIN)
         // we can't use InitGlobalDir() here as the profile is not created yet,
         // but I don't have time to fix it right now - to do later!!
         #error "this code is broken and will crash"  // FIXME

         InitGlobalDir();
         String localePath;
         localePath << m_globalDir << "/locale";
#else
#   error "don't know where to find message catalogs on this platform"
#endif // OS
         m_Locale->AddCatalogLookupPathPrefix(localePath);

         if ( !m_Locale->AddCatalog(M_APPLICATIONNAME) )
         {
            // better use English messages if msg catalog was not found
            delete m_Locale;
            m_Locale = NULL;

            failedToLoadMsgs = true;
         }
      }
   }
   else
   {
      m_Locale = NULL;
   }
#endif // USE_I18N

   wxInitAllImageHandlers();
   wxFileSystem::AddHandler(new wxMemoryFSHandler);

   // this is necessary to avoid that the app closes automatically when we're
   // run for the first time and show a modal dialog before opening the main
   // frame - if we don't do it, when the dialog (which is the last app window
   // at this moment) disappears, the app will close.
   SetExitOnFrameDelete(FALSE);

   // create timers -- are accessed by OnStartup()
   gs_timerAutoSave = new AutoSaveTimer;
   gs_timerMailCollection = new MailCollectionTimer;

   if ( OnStartup() )
   {
#ifdef USE_I18N
      // only now we can use profiles
      if ( failedToSetLocale || failedToLoadMsgs )
      {
         // if we can't load the msg catalog for the given locale once, don't
         // try it again next time - the flag is stored in the profile in this
         // key
         wxString configPath;
         configPath << "UseLocale_" << locale;

         Profile *profile = GetProfile();
         if ( profile->readEntry(configPath, 1l) != 0l )
         {
            CloseSplash();

            // the strings here are intentionally not translated
            wxString msg;
            if ( failedToSetLocale )
            {
               msg.Printf("Locale '%s' couldn't be set, do you want to "
                          "retry setting it the next time?",
                          locale);
            }
            else // failedToLoadMsgs
            {
               msg.Printf("Impossible to load message catalog(s) for the "
                          "locale '%s', do you want to retry next time?",
                          locale);
            }

            if ( wxMessageBox(msg, "Error",
                              wxICON_STOP | wxYES_NO) != wxYES )
            {
               profile->writeEntry(configPath, 0l);
            }
         }
         //else: the user had previously answered "no" to the question above,
         //      don't complain any more about missing catalogs
      }
#endif // USE_I18N

      // at this moment we're fully initialized, i.e. profile and log
      // subsystems are working and the main window is created

      // start a timer to autosave the profile entries
      StartTimer(Timer_Autosave);

      // the timer to poll for new mail will be started when/if FolderMonitor
      // is created
      //StartTimer(Timer_PollIncoming);

      // start away timer if necessary
      StartTimer(Timer_Away);

      // another timer to do MEvent handling:
      m_IdleTimer = new IdleTimer;

      // restore the normal behaviour (see the comments above)
      SetExitOnFrameDelete(TRUE);

#ifdef USE_DIALUP
      // reflect settings in menu and statusbar:
      UpdateOnlineDisplay();
#endif // USE_DIALUP

      // make sure this is displayed correctly:
      UpdateOutboxStatus();

      // show tip dialog unless disabled
      if ( READ_APPCONFIG(MP_SHOWTIPS) )
      {
         MDialog_ShowTip(GetTopWindow());
      }

      return true;
   }
   else
   {
      if ( GetLastError() != M_ERROR_CANCEL )
      {
         ERRORMESSAGE(("Can't initialize application, terminating."));
      }
      //else: this would be superfluous

      return false;
   }
}

wxMFrame *wxMApp::CreateTopLevelFrame()
{
   m_topLevelFrame = new wxMainFrame();
   m_topLevelFrame->Show(true);
   SetTopWindow(m_topLevelFrame);
   return m_topLevelFrame;
}

int wxMApp::OnRun()
{
#ifdef PROFILE_STARTUP
   return 0;
#else
   return wxApp::OnRun();
#endif
}

int wxMApp::OnExit()
{
   // we won't be able to do anything more (i.e. another program copy can't ask
   // us to execute any actions for it) so it's as if we were already not
   // running at all
   delete m_snglInstChecker;
   m_snglInstChecker = NULL;

   // no more IPC for us neither
   delete m_serverIPC;
   m_serverIPC = NULL;

   // if one timer was created, then all of them were
   if ( gs_timerAutoSave )
   {
      // delete timers
      delete gs_timerAutoSave;
      delete gs_timerMailCollection;
      delete gs_timerAway;

      gs_timerAutoSave = NULL;
      gs_timerMailCollection = NULL;
      gs_timerAway = NULL;
   }

   CleanUpPrintData();

   if(m_HelpController)
   {
      wxSize size;
      wxPoint pos;
      m_HelpController->GetFrameParameters(&size, &pos);
      m_profile->writeEntry(MP_HELPFRAME_WIDTH, size.x),
      m_profile->writeEntry(MP_HELPFRAME_HEIGHT, size.y);
      m_profile->writeEntry(MP_HELPFRAME_XPOS, pos.x);
      m_profile->writeEntry(MP_HELPFRAME_YPOS, pos.y);
      delete m_HelpController;
   }

   UnloadModules();
   MAppBase::OnShutDown();

   MModule_Cleanup();
   delete m_IconManager;

#ifdef USE_I18N
   delete m_Locale;
#endif // USE_I18N

#ifdef USE_DIALUP
   delete m_OnlineManager;
#endif // USE_DIALUP

   // FIXME this is not the best place to do it, but at least we're safe
   //       because we know that by now it's unused any more
   Profile::DeleteGlobalConfig();

   MObjectRC::CheckLeaks();
   MObject::CheckLeaks();

   return 0;
}

// ----------------------------------------------------------------------------
// wxMApp printing
// ----------------------------------------------------------------------------

void wxMApp::CleanUpPrintData()
{
   // save our preferred printer settings
   if ( m_PrintData )
   {
#if wxUSE_POSTSCRIPT
      m_profile->writeEntry(MP_PRINT_COMMAND, m_PrintData->GetPrinterCommand());
      m_profile->writeEntry(MP_PRINT_OPTIONS, m_PrintData->GetPrinterOptions());
      m_profile->writeEntry(MP_PRINT_ORIENTATION, m_PrintData->GetOrientation());
      m_profile->writeEntry(MP_PRINT_MODE, m_PrintData->GetPrintMode());
      m_profile->writeEntry(MP_PRINT_PAPER, m_PrintData->GetPaperId());
      m_profile->writeEntry(MP_PRINT_FILE, m_PrintData->GetFilename());
      m_profile->writeEntry(MP_PRINT_COLOUR, m_PrintData->GetColour());
#endif // wxUSE_POSTSCRIPT

      delete m_PrintData;
      m_PrintData = NULL;
   }

   if ( m_PageSetupData )
   {
#if wxUSE_POSTSCRIPT
      m_profile->writeEntry(MP_PRINT_TOPMARGIN_X,
                            m_PageSetupData->GetMarginTopLeft().x);
      m_profile->writeEntry(MP_PRINT_TOPMARGIN_Y,
                            m_PageSetupData->GetMarginTopLeft().y);
      m_profile->writeEntry(MP_PRINT_BOTTOMMARGIN_X,
                            m_PageSetupData->GetMarginBottomRight().x);
      m_profile->writeEntry(MP_PRINT_BOTTOMMARGIN_Y,
                            m_PageSetupData->GetMarginBottomRight().y);
#endif // wxUSE_POSTSCRIPT

      delete m_PageSetupData;
      m_PageSetupData = NULL;
   }
}

const wxPrintData *wxMApp::GetPrintData()
{
   if ( !m_PrintData )
   {
#ifdef OS_WIN
      wxGetApp().SetPrintMode(wxPRINT_WINDOWS);
#else // Unix
      wxGetApp().SetPrintMode(wxPRINT_POSTSCRIPT);

      // set AFM path
      PathFinder pf(mApplication->GetGlobalDir()+"/afm", false);
      pf.AddPaths(READ_APPCONFIG_TEXT(MP_AFMPATH), false);
      pf.AddPaths(mApplication->GetLocalDir(), true);

      bool found;
      String afmpath = pf.FindDirFile("Cour.afm", &found);
      if(found)
      {
         wxThePrintSetupData->SetAFMPath(afmpath);
      }
#endif // Win/Unix

      m_PrintData = new wxPrintData;

#ifndef OS_WIN
      if ( found )
      {
         m_PrintData->SetFontMetricPath(afmpath);
      }
#endif // !OS_WIN

#if wxUSE_POSTSCRIPT
      *m_PrintData = *wxThePrintSetupData;

      m_PrintData->SetPrinterCommand(READ_APPCONFIG(MP_PRINT_COMMAND));
      m_PrintData->SetPrinterOptions(READ_APPCONFIG(MP_PRINT_OPTIONS));
      m_PrintData->SetOrientation(READ_APPCONFIG(MP_PRINT_ORIENTATION));
      m_PrintData->SetPrintMode((wxPrintMode)(long)READ_APPCONFIG(MP_PRINT_MODE));
      m_PrintData->SetPaperId((wxPaperSize)(long)READ_APPCONFIG(MP_PRINT_PAPER));
      m_PrintData->SetFilename(READ_APPCONFIG(MP_PRINT_FILE));
      m_PrintData->SetColour(READ_APPCONFIG(MP_PRINT_COLOUR));
#endif // wxUSE_POSTSCRIPT
   }

   return m_PrintData;
}

void wxMApp::SetPrintData(const wxPrintData& printData)
{
   CHECK_RET( m_PrintData, "must have called GetPrintData() before!" );

   *m_PrintData = printData;
}

wxPageSetupDialogData *wxMApp::GetPageSetupData()
{
   if ( !m_PageSetupData )
   {
      m_PageSetupData = new wxPageSetupDialogData(*GetPrintData());

#if wxUSE_POSTSCRIPT
      m_PageSetupData->SetMarginTopLeft(wxPoint(
         READ_APPCONFIG(MP_PRINT_TOPMARGIN_X),
         READ_APPCONFIG(MP_PRINT_TOPMARGIN_X)));
      m_PageSetupData->SetMarginBottomRight(wxPoint(
         READ_APPCONFIG(MP_PRINT_BOTTOMMARGIN_X),
         READ_APPCONFIG(MP_PRINT_BOTTOMMARGIN_X)));
#endif // wxUSE_POSTSCRIPT
   }

   return m_PageSetupData;
}

void wxMApp::SetPageSetupData(const wxPageSetupDialogData& data)
{
   CHECK_RET( m_PrintData, "must have called GetPageSetupData() before!" );

   *m_PageSetupData = data;
}

// ----------------------------------------------------------------------------
// Help subsystem
// ----------------------------------------------------------------------------

/* static */
wxString wxMApp::BuildHelpInitString(const wxString& dir)
{
   wxString helpfile = dir;

#ifdef OS_WIN
   helpfile += "\\Manual.chm";
#endif // Windows

   return helpfile;
}

/* static */
wxString wxMApp::GetHelpDir()
{
   wxString helpdir = mApplication->GetGlobalDir();
   if ( !helpdir.empty() )
      helpdir += wxFILE_SEP_PATH;

#ifdef OS_WIN
   helpdir += "help";
#else // !Windows
   helpdir += "doc";
#endif // Windows/!Windows

   return helpdir;
}

bool wxMApp::InitHelp()
{
   wxString helpdir = READ_APPCONFIG(MP_HELPDIR);
   if ( helpdir.empty() )
   {
      helpdir = GetHelpDir();
   }

   while ( !m_HelpController )
   {
      // we hardcode the help controller class we use instead of using the
      // default wxHelpController everywhere as we don't have docs in all
      // possible formats, just HTML and CHM
#ifdef OS_UNIX
      m_HelpController = new wxHelpController;
#else // Windows
      m_HelpController = new wxBestHelpController;
#endif // Unix/Windows

      // try to initialise the help system
      if ( !m_HelpController->Initialize(BuildHelpInitString(helpdir)) )
      {
         // failed
         delete m_HelpController;
         m_HelpController = NULL;

         // ask the user if we want to look elsewhere?
         wxString msg;
         msg << _("Cannot initialise help system:\n")
             << _("Help files not found in the directory '") << helpdir << "'\n"
             << "\n"
             << _("Would you like to specify another help files location?");

         if ( !MDialog_YesNoDialog(msg, NULL, _("Mahogany Help")) )
         {
            // can't do anything more
            return false;
         }

         String dir = MDialog_DirRequester
                      (
                       _("Please choose the directory containing "
                         "the Mahogany help files."),
                       helpdir
                      );

         if ( dir.empty() )
         {
            // dialog cancelled
            return false;
         }

         // try again with another location
         helpdir = dir;
      }
   }

   // set help viewer options

#if defined(OS_UNIX) && !wxUSE_HTML
   ((wxExtHelpController *)m_HelpController)->SetBrowser(
      READ_APPCONFIG(MP_HELPBROWSER),
      READ_APPCONFIG(MP_HELPBROWSER_ISNS));
#endif // using wxExtHelpController

   wxSize size = wxSize(READ_APPCONFIG(MP_HELPFRAME_WIDTH),
                        READ_APPCONFIG(MP_HELPFRAME_HEIGHT));
   wxPoint pos = wxPoint(READ_APPCONFIG(MP_HELPFRAME_XPOS),
                         READ_APPCONFIG(MP_HELPFRAME_YPOS));

   m_HelpController->SetFrameParameters("Mahogany : %s", size, pos);

   // remember the dir where we found the files
   if ( helpdir != GetHelpDir() )
   {
      mApplication->GetProfile()->writeEntry(MP_HELPDIR, helpdir);
   }

   // we do have m_HelpController now
   return true;
}

void
wxMApp::Help(int id, wxWindow *parent)
{
   // first thing: close splash if it's still there
   CloseSplash();

   if ( !m_HelpController && !InitHelp() )
   {
      // help controller couldn't be initialized, so no help available
      return;
   }

   // by now we should have either created it or returned
   CHECK_RET( m_HelpController, "no help controller, but should have one!" );

   switch(id)
   {
         // look up contents:
      case MH_CONTENTS:
         m_HelpController->DisplayContents();
         break;

         // implement a search:
      case MH_SEARCH:
         {
            // store help key used in search
            static wxString s_lastSearchKey;

            if ( MInputBox(&s_lastSearchKey,
                           _("Search for?"),
                           _("Search help for keyword"),
                           parent ? parent : TopLevelFrame())
                 && !s_lastSearchKey.empty() )
            {
               m_HelpController->KeywordSearch(s_lastSearchKey);
            }
         }
         break;

         // all other help ids, just look them up:
      default:
         if ( !m_HelpController->DisplaySection(id) )
         {
            wxLogWarning(_("No help found for current context (%d)."), id);
         }
   }
}

// ----------------------------------------------------------------------------
// Loadable modules
// ----------------------------------------------------------------------------

void
wxMApp::LoadModules(void)
{
   char *modulestring;
   kbStringList modules;
   kbStringList::iterator i;

   modulestring = strutil_strdup(READ_APPCONFIG(MP_MODULES));
   strutil_tokenise(modulestring, ":", modules);
   delete [] modulestring;

   MModule *module;
   for(i = modules.begin(); i != modules.end(); i++)
   {
      module = MModule::LoadModule(**i);
      if(module == NULL)
      {
         ERRORMESSAGE((_("Cannot load module '%s'."),
                       (**i).c_str()));
      }
      else
      {
         // remember it so we can decref it before exiting
         gs_GlobalModulesList.push_back(new MModuleEntry(module));
#ifdef DEBUG
         LOGMESSAGE((M_LOG_WINONLY,
                   "Successfully loaded module:\nName: %s\nDescr: %s\nVersion: %s\n",
                   module->GetName(),
                   module->GetDescription(),
                   module->GetVersion()));

#endif
      }
   }
}

void
wxMApp::InitModules(void)
{
   for ( ModulesList::iterator i = gs_GlobalModulesList.begin();
         i != gs_GlobalModulesList.end();
         i++ )
   {
      MModule *module = (**i).m_Module;
      if ( module->Entry(MMOD_FUNC_INIT) != 0 )
      {
         // TODO: propose to disable this module?
         wxLogWarning(_("Module '%s' failed to initialize."),
                      module->GetName());
      }
   }
}

void
wxMApp::UnloadModules(void)
{
   for (ModulesList::iterator j = gs_GlobalModulesList.begin();
       j != gs_GlobalModulesList.end();)
   {
      ModulesList::iterator i = j;
      ++j;
      (**i).m_Module->DecRef();
   }
}

// ----------------------------------------------------------------------------
// timer stuff
// ----------------------------------------------------------------------------

// just a wrapper for FolderMonitor::GetMinCheckTimeout()
static long GetPollIncomingTimerDelay(void)
{
   FolderMonitor *collector = mApplication->GetFolderMonitor();

   return collector ? collector->GetMinCheckTimeout() : 0;
}

bool wxMApp::StartTimer(Timer timer)
{
   long delay;

   switch ( timer )
   {
      case Timer_Autosave:
         delay = READ_APPCONFIG(MP_AUTOSAVEDELAY);
         if ( delay )
         {
            return gs_timerAutoSave->Start(1000*delay);
         }
         break;

      case Timer_PollIncoming:
         delay = GetPollIncomingTimerDelay();
         if ( delay )
         {
            return gs_timerMailCollection->Start(1000*delay);
         }
         break;

      case Timer_PingFolder:
         // TODO - just sending an event "restart timer" which all folders
         //        would be subscribed too should be enough
         return true;

      case Timer_Away:
         delay = READ_APPCONFIG(MP_AWAY_AUTO_ENTER);
         if ( delay )
         {
            // this timer is not created by default
            if ( !gs_timerAway )
               gs_timerAway = new AwayTimer;

            // this delay is in minutes
            return gs_timerAway->Start(1000*60*delay);
         }
         break;

      case Timer_Max:
      default:
         wxFAIL_MSG("attempt to start an unknown timer");
   }

   // timer not started
   return false;
}

bool wxMApp::StopTimer(Timer timer)
{
   switch ( timer )
   {
      case Timer_Autosave:
         if ( gs_timerAutoSave )
            gs_timerAutoSave->Stop();
         break;

      case Timer_PollIncoming:
         if ( gs_timerMailCollection )
            gs_timerMailCollection->Stop();
         break;

      case Timer_PingFolder:
         // TODO!!!
         break;

      case Timer_Away:
         if ( gs_timerAway )
            gs_timerAway->Stop();
         break;

      case Timer_Max:
      default:
         wxFAIL_MSG("attempt to stop an unknown timer");
         return false;
   }

   return true;
}

#ifndef USE_THREADS

void
wxMApp::ThrEnterLeave(bool /* enter */, SectionId /*what*/ , bool /*testing */)
{
   // nothing to do
}

#else

// this mutex must be acquired before any call to a critical c-client function
static wxMutex gs_CclientMutex;
// this mutex must be acquired before any call to MEvent code
static wxMutex gs_MEventMutex;

void
wxMApp::ThrEnterLeave(bool enter, SectionId what, bool
#ifdef DEBUG
                      testing
#endif
   )
{
   wxMutex *which = NULL;
   switch(what)
   {
   case GUI:
      /* case EVENTS: -- avoid multiple case value warning (EVENTS == GUI) */
      if(enter)
         wxMutexGuiEnter();
      else
         wxMutexGuiLeave();
      return;
      break;
   case MEVENT:
      which = &gs_MEventMutex;
      break;
   case CCLIENT:
      which = &gs_CclientMutex;
      break;
   default:
      FAIL_MSG("unsupported mutex id");
   }
   ASSERT(which);
   if(enter)
   {
      ASSERT(which->IsLocked() == FALSE);
      which->Lock();
   }
   else
   {
      ASSERT(which->IsLocked() || testing);
      if(which->IsLocked())
         which->Unlock();
   }
}

#endif

// ----------------------------------------------------------------------------
// Customize wxApp behaviour
// ----------------------------------------------------------------------------

#if wxCHECK_VERSION(2, 3, 0)

// never return splash frame from here
wxWindow *wxMApp::GetTopWindow() const
{
   wxWindow *win = wxApp::GetTopWindow();
   if ( win == g_pSplashScreen )
      win = NULL;

   return win;
}

#endif // 2.3.0

// return our icons
wxIcon
wxMApp::GetStdIcon(int which) const
{
   // this function may be (and is) called from the persistent controls code
   // which uses the global config object for its own needs, so avoid changing
   // its path here - or rather restore it on funciton exit
   class ConfigPathRestorer
   {
   public:
      ConfigPathRestorer()
         {
            m_profile = mApplication ? mApplication->GetProfile() : NULL;
            if ( m_profile )
               m_path = m_profile->GetConfig()->GetPath();
         }

      ~ConfigPathRestorer()
         {
            if ( m_profile )
               m_profile->GetConfig()->SetPath(m_path);
         }

   private:
      wxString m_path;
      Profile *m_profile;
   } storeConfigPath;

   // Set our icons for the dialogs.

   // This might happen during program shutdown, when the profile has
   // already been deleted or at startup before it is set up.
   if(! GetProfile() || GetGlobalDir().Length() == 0)
      return wxApp::GetStdIcon(which);

   // this ugly "#ifdefs" are needed to silent warning about "switch without
   // any case" warning under Windows
#ifndef OS_WIN
   switch(which)
   {
   case wxICON_HAND:
      return ICON("msg_error"); break;
   case wxICON_EXCLAMATION:
      return ICON("msg_warning"); break;
   case wxICON_QUESTION:
      return ICON("msg_question"); break;
   case wxICON_INFORMATION:
      return ICON("msg_info"); break;
   default:
      return wxApp::GetStdIcon(which);
   }
#else
   return wxApp::GetStdIcon(which);
#endif
}

void
wxMApp::UpdateStatusBar(int nfields, bool isminimum) const
{
   ASSERT(nfields <= SF_MAXIMUM);
   ASSERT(nfields >= 0);
   ASSERT(m_topLevelFrame);
   ASSERT(m_topLevelFrame->GetStatusBar());
   int n = nfields;

   // ugly, but effective:
   //static int statusIconWidth = -1;

   wxStatusBar *sbar = m_topLevelFrame->GetStatusBar();
   if(isminimum && sbar->GetFieldsCount() > nfields)
      n = sbar->GetFieldsCount();

   int widths[SF_MAXIMUM];
   widths[0] = 100; //flexible
   widths[1] = 10; // small empty field

#if 0
   if(m_DialupSupport)
   {
      if(statusIconWidth == -1)
      {
         const wxIcon & icon = ICON("online");
         // make field 1.5 times as wide as icon and add border:
         statusIconWidth = (3*icon.GetWidth())/2 + 2*sbar->GetBorderX();
      }
      widths[GetStatusField(SF_ONLINE)] = statusIconWidth;
   }
#endif

#ifdef USE_DIALUP
   if(m_DialupSupport)
      widths[GetStatusField(SF_ONLINE)] = 70;
#endif // USE_DIALUP

   if(READ_APPCONFIG(MP_USE_OUTBOX))
      widths[GetStatusField(SF_OUTBOX)] = 100;
   //FIXME: wxGTK crashes after calling this repeatedly sbar->SetFieldsCount(n, widths);
}

void
wxMApp::UpdateOutboxStatus(MailFolder *mf) const
{
   if(! m_topLevelFrame) // called when flushing events at program end?
      return;

   UIdType nNNTP, nSMTP;
   bool enable = CheckOutbox(&nSMTP, &nNNTP, mf);

   // only enable menu item if outbox is used and contains messages:
   ASSERT(m_topLevelFrame->GetMenuBar());

   bool useOutbox = READ_APPCONFIG_BOOL(MP_USE_OUTBOX);
   m_topLevelFrame->GetMenuBar()->Enable(
      (int)WXMENU_FILE_SEND_OUTBOX,enable && useOutbox);

   if ( !useOutbox )
         return;

   // update status bar
   String msg;
   if(nNNTP == 0 && nSMTP == 0)
      msg = _("Outbox empty");
   else
      msg.Printf(_("Outbox %lu, %lu"),
                 (unsigned long) nSMTP,
                 (unsigned long) nNNTP);

   int field = GetStatusField(SF_OUTBOX);
   UpdateStatusBar(field+1, TRUE);
   ASSERT(m_topLevelFrame->GetStatusBar());
   m_topLevelFrame->GetStatusBar()->SetStatusText(msg, field);
}

wxIconManager *wxMApp::GetIconManager(void) const
{
    if ( !m_IconManager )
    {
        // const_cast
        ((wxMApp *)this)->m_IconManager = new wxIconManager;
    }

    return m_IconManager;
}

// ----------------------------------------------------------------------------
// dial up support
// ----------------------------------------------------------------------------

#ifdef USE_DIALUP

void
wxMApp::OnConnected(wxDialUpEvent &event)
{
   if(! m_DialupSupport)
      return;
   m_IsOnline = TRUE;
   UpdateOnlineDisplay();
   MDialog_Message(_("Dial-Up network connection established."),
                   m_topLevelFrame,
                   _("Information"),
                   "DialUpConnectedMsg");
}

void
wxMApp::OnDisconnected(wxDialUpEvent &event)
{
   if(! m_DialupSupport)
      return;
   m_IsOnline = FALSE;
   UpdateOnlineDisplay();
   MDialog_Message(_("Dial-Up network shut down."),
                   m_topLevelFrame,
                   _("Information"),
                   "DialUpDisconnectedMsg");
}

void
wxMApp::SetupOnlineManager(void)
{
   // only create dial up manager if needed
   m_DialupSupport = READ_APPCONFIG_BOOL(MP_DIALUP_SUPPORT);

   if ( m_DialupSupport )
   {
      if ( !m_OnlineManager )
      {
         m_OnlineManager = wxDialUpManager::Create();
      }

      if(! m_OnlineManager->EnableAutoCheckOnlineStatus(60))
      {
         wxLogError(_("Cannot activate auto-check for dial-up network status."));
      }

      String beaconhost = READ_APPCONFIG(MP_BEACONHOST);
      strutil_delwhitespace(beaconhost);
      // If no host configured, use smtp host:
      if(beaconhost.length() > 0)
      {
         m_OnlineManager->SetWellKnownHost(beaconhost);
      }
      else // beacon host configured
      {
         beaconhost = READ_APPCONFIG_TEXT(MP_SMTPHOST);
         m_OnlineManager->SetWellKnownHost(beaconhost, 25);
      }
#ifdef OS_UNIX
      m_OnlineManager->SetConnectCommand(
         READ_APPCONFIG(MP_NET_ON_COMMAND),
         READ_APPCONFIG(MP_NET_OFF_COMMAND));
#endif // Unix
      m_IsOnline = m_OnlineManager->IsOnline();
   }
   else // no dialup support
   {
      delete m_OnlineManager;
      m_OnlineManager = NULL; // Cleanup will try to delete it.

      m_IsOnline = TRUE;
   }

   UpdateOnlineDisplay();
}

bool
wxMApp::IsOnline(void) const
{
   if(! m_DialupSupport)
      return TRUE; // no dialup--> always connected

   // make sure we always have the very latest value:
   ((wxMApp*)this)->m_IsOnline = m_OnlineManager->IsOnline();

   return m_IsOnline;
}

void
wxMApp::GoOnline(void) const
{
   CHECK_RET( m_OnlineManager, "can't go online" );

   if(m_OnlineManager->IsOnline())
   {
      ((wxMApp *)this)->m_IsOnline = TRUE;
      ERRORMESSAGE((_("Dial-up network is already online.")));
      return;
   }

   m_OnlineManager->Dial();
}

void
wxMApp::GoOffline(void) const
{
   CHECK_RET( m_OnlineManager, "can't go offline" );

   if(! m_OnlineManager->IsOnline())
   {
      ((wxMApp *)this)->m_IsOnline = FALSE;
      ERRORMESSAGE((_("Dial-up network is already offline.")));
      return;
   }
   m_OnlineManager->HangUp();
   if( m_OnlineManager->IsOnline())
   {
      ERRORMESSAGE((_("Attempt to shut down network seems to have failed.")));
   }
}

void
wxMApp::UpdateOnlineDisplay(void)
{
   // Can be called during  application startup, in this case do
   // nothing:
   if(! m_topLevelFrame)
      return;
   wxStatusBar *sbar = m_topLevelFrame->GetStatusBar();
   wxMenuBar *mbar = m_topLevelFrame->GetMenuBar();
   ASSERT(sbar);
   ASSERT(mbar);

   if(! m_DialupSupport)
   {
      mbar->Enable((int)WXMENU_FILE_NET_ON, FALSE);
      mbar->Enable((int)WXMENU_FILE_NET_OFF, FALSE);
   }
   else
   {
      bool online = IsOnline();
      if(online)
      {
         mbar->Enable((int)WXMENU_FILE_NET_OFF, TRUE);
         mbar->Enable((int)WXMENU_FILE_NET_ON, FALSE);
//    m_topLevelFrame->GetToolBar()->EnableItem(WXMENU_FILE_NET_OFF, m_DialupSupport);
      }
      else
      {
         mbar->Enable((int)WXMENU_FILE_NET_ON, TRUE);
         mbar->Enable((int)WXMENU_FILE_NET_OFF, FALSE);
//    m_topLevelFrame->GetToolBar()->EnableItem(WXMENU_FILE_NET_ON, m_DialupSupport);
      }
      int field = GetStatusField(SF_ONLINE);
      UpdateStatusBar(field+1, TRUE);
      sbar->SetStatusText(online ? _("Online"):_("Offline"), field);
   }
}

#endif // USE_DIALUP

// ----------------------------------------------------------------------------
// miscellaneous
// ----------------------------------------------------------------------------

void
wxMApp::FatalError(const char *message)
{
   OnAbnormalTermination();
}

void
wxMApp::SetAwayMode(bool isAway)
{
   MAppBase::SetAwayMode(isAway);

   // update the frame menu
   if ( m_topLevelFrame )
   {
      wxMenuBar *mbar = m_topLevelFrame->GetMenuBar();
      if ( mbar )
      {
         mbar->Check(WXMENU_FILE_AWAY_MODE, isAway);
      }

      wxLogStatus(m_topLevelFrame,
                  isAway ? _("Mahogany is now in unattended mode")
                         : _("Mahogany is not in unattended mode any more"));
   }

   // also change the mode of the log target: in away mode we only show the
   // messages in the log window and don't popup the dialogs
   if ( m_logWindow )
   {
      // show all old messages normally (including the one logged above)
      m_logWindow->Flush();

      m_logWindow->PassMessages(!isAway);
   }
}

// ----------------------------------------------------------------------------
// log window support (see also wxMLogWindow)
// ----------------------------------------------------------------------------

void wxMApp::SetLogFile(const String& filename)
{
#ifdef wxHAS_LOG_CHAIN
   if ( filename.empty() )
   {
      if ( m_logChain )
      {
         // just disable logging to the old file and only pass messages
         // through to the next log target
         m_logChain->SetLog(NULL);
      }
   }
   else // log to file
   {
      wxFFile file(filename, "w");
      if ( !file.IsOpened() )
      {
         wxLogError(_("Failed to open log file for writing."));
      }
      else
      {
         wxLog *log = new MLogFile(file.fp());

         // leave the file opened
         file.Detach();

         if ( m_logChain )
         {
            m_logChain->SetLog(log);
         }
         else
         {
            m_logChain = new wxLogChain(log);
         }

         wxLogVerbose(_("Started logging to the log file '%s'."),
                      filename.c_str());
      }
   }
#endif // wxHAS_LOG_CHAIN
}

bool wxMApp::IsLogShown() const
{
   return m_logWindow && m_logWindow->IsShown();
}

void wxMApp::ShowLog(bool doShow)
{
   if ( !m_logWindow )
   {
      if ( doShow )
      {
         // before creating the log window, force auto creation of the default
         // GUI logger so that log window would pass messages to it
         (void)wxLog::GetActiveTarget();

         // create the log window
         m_logWindow = new wxMLogWindow(m_topLevelFrame,
                                        _("Mahogany : Activity Log"));
      }
      //else: nothing to do
   }
   else
   {
      // reuse the existing one
      m_logWindow->Show(doShow);
   }
}

// ----------------------------------------------------------------------------
// command line parsing
// ----------------------------------------------------------------------------

#define OPTION_BCC         "bcc"
#define OPTION_BODY        "body"
#define OPTION_CC          "cc"
#define OPTION_CONFIG      "config"
#define OPTION_FOLDER      "folder"
#define OPTION_NEWSGROUP   "newsgroup"
#define OPTION_SAFE        "safe"
#define OPTION_SUBJECT     "subject"

void wxMApp::OnInitCmdLine(wxCmdLineParser& parser)
{
   wxApp::OnInitCmdLine(parser);

   static const wxCmdLineEntryDesc cmdLineDesc[] =
   {
      // -b or --bcc to specify the BCC headers
      {
         wxCMD_LINE_OPTION,
         "b",
         OPTION_BCC,
         gettext_noop("specify the blind carbon-copy (BCC) recipients"),
      },

      // --body to specify the message body
      {
         wxCMD_LINE_OPTION,
         NULL,
         OPTION_BODY,
         gettext_noop("specify the body of the message"),
      },

      // -c or --cc to specify the CC headers
      {
         wxCMD_LINE_OPTION,
         "c",
         OPTION_CC,
         gettext_noop("specify the carbon-copy (CC) recipients"),
      },

      // --config=file to specify an alternative config file to use
      {
         wxCMD_LINE_OPTION,
         NULL,
         OPTION_CONFIG,
         gettext_noop("specify the alternative configuration file to use"),
      },

      // -f or --folder to specify the folder to open in the main frame
      {
         wxCMD_LINE_OPTION,
         "f",
         OPTION_FOLDER,
         gettext_noop("specify the folder to open in the main window"),
      },

      // --newsgroup to specify the newsgroup to post the message to
      {
         wxCMD_LINE_OPTION,
         NULL,
         OPTION_NEWSGROUP,
         gettext_noop("the newsgroup to post the message to"),
      },

      // --safe option to prevent crashes at startup
      {
         wxCMD_LINE_SWITCH,
         NULL,
         OPTION_SAFE,
         gettext_noop("don't perform any operations on startup"),
      },

      // -s or --subject to specify the subject
      {
         wxCMD_LINE_OPTION,
         "s",
         OPTION_SUBJECT,
         gettext_noop("specify the subject"),
      },

      // <address> to write to
      {
         wxCMD_LINE_PARAM,
         NULL,
         NULL,
         "address(es) to start composing the message to",
         wxCMD_LINE_VAL_STRING,
         wxCMD_LINE_PARAM_OPTIONAL | wxCMD_LINE_PARAM_MULTIPLE
      },

      // terminator
      { wxCMD_LINE_NONE }
   };

   parser.SetDesc(cmdLineDesc);
}

bool wxMApp::OnCmdLineParsed(wxCmdLineParser& parser)
{
   if ( !wxApp::OnCmdLineParsed(parser) )
      return false;

   // first get the options values
   bool startComposer = parser.Found(OPTION_BCC, &m_cmdLineOptions->composer.bcc);
   startComposer |= parser.Found(OPTION_BODY, &m_cmdLineOptions->composer.body);
   startComposer |= parser.Found(OPTION_CC, &m_cmdLineOptions->composer.cc);
   startComposer |= parser.Found(OPTION_SUBJECT, &m_cmdLineOptions->composer.subject);

   bool hasRcpt = parser.Found(OPTION_NEWSGROUP, &m_cmdLineOptions->composer.newsgroups);
   size_t count = parser.GetParamCount();
   if ( count > 0 )
   {
      hasRcpt = true;

      String to;
      for ( size_t n = 0; n < count; n++ )
      {
         if ( !to.empty() )
            to += ", ";

         to += parser.GetParam(n);
      }

      m_cmdLineOptions->composer.to = to;
   }

   m_cmdLineOptions->safe = parser.Found(OPTION_SAFE);

   if ( startComposer )
   {
      if ( m_cmdLineOptions->safe )
      {
         wxLogWarning(_("Composer options are ignored in safe mode."));
      }
      else
      {
         // if we have any of the message composing-related options, we must
         // have at least one rcpt as well
         if ( !hasRcpt )
         {
            wxLogError(_("A message recipient or a newsgroup must be specified "
                         "if any of the composer-related options are used."));
            return false;
         }
      }
   }

   parser.Found(OPTION_CONFIG, &m_cmdLineOptions->configFile);
   m_cmdLineOptions->useFolder = parser.Found(OPTION_FOLDER,
                                              &m_cmdLineOptions->folder);

   return true;
}

// ----------------------------------------------------------------------------
// global functions implemented here
// ----------------------------------------------------------------------------

extern bool EnsureAvailableTextEncoding(wxFontEncoding *enc,
                                        wxString *text,
                                        bool mayAskUser)
{
   CHECK( enc, false, "CheckEncodingAvailability: NULL encoding" );

   if ( !wxTheFontMapper->IsEncodingAvailable(*enc) )
   {
      // try to find another encoding
      wxFontEncoding encAlt;
      if ( wxTheFontMapper->GetAltForEncoding(*enc, &encAlt, "", mayAskUser) )
      {
         // translate the text (if any) to the equivalent encoding
         if ( text )
         {
            wxEncodingConverter conv;
            if ( conv.Init(*enc, encAlt) )
            {
               *enc = encAlt;

               *text = conv.Convert(*text);
            }
            else // failed to convert the text
            {
               return false;
            }
         }
      }
      else //no equivalent encoding
      {
         return false;
      }
   }

   // we have either the requested encoding or an equivalent one
   return true;
}

// ============================================================================
// IPC and multiple program instances handling
// ============================================================================

// ----------------------------------------------------------------------------
// IPC constants
// ----------------------------------------------------------------------------

#define IPC_SOCKET "/tmp/.mahogany.ipc"
#define IPC_TOPIC  "MRemote"

// ----------------------------------------------------------------------------
// IPC classes
// ----------------------------------------------------------------------------

class MAppIPCConnection : public wxConnection
{
public:
   MAppIPCConnection()
      : wxConnection(m_buffer, WXSIZEOF(m_buffer))
   {
   }

   virtual bool OnExecute(const wxString& WXUNUSED(topic),
                          char *data,
                          int WXUNUSED(size),
                          wxIPCFormat WXUNUSED(format))
   {
      return wxGetApp().OnRemoteRequest(data);
   }

private:
   char m_buffer[4096];
};

class MAppIPCServer : public wxServer
{
public:
   virtual wxConnectionBase *OnAcceptConnection(const wxString& topic)
   {
      if ( topic != IPC_TOPIC )
         return NULL;

      return new MAppIPCConnection;
   }
};

// ----------------------------------------------------------------------------
// wxMApp IPC methods
// ----------------------------------------------------------------------------

bool wxMApp::OnRemoteRequest(const char *request)
{
   CmdLineOptions cmdLineOpts;
   if ( !cmdLineOpts.FromString(request) )
   {
      wxLogError(_("Ignoring unrecognized remote request '%s'."),
                 request);
      return false;
   }

   if ( !ProcessSendCmdLineOptions(cmdLineOpts) )
   {
      // no composer windows were opened, bring the main frame to top to ensure
      // that we have focus
      if ( m_topLevelFrame )
      {
         m_topLevelFrame->Raise();
      }
   }

   return true;
}

bool wxMApp::IsAnotherRunning() const
{
   // it's too early or too late to call us, normally we shouldn't try to do it
   // then, i.e. this indicates an error in the caller logic
   CHECK( m_snglInstChecker, false, "IsAnotherRunning() shouldn't be called" );

   return m_snglInstChecker->IsAnotherRunning();
}

bool wxMApp::SetupRemoteCallServer()
{
   ASSERT_MSG( !m_serverIPC, "SetupRemoteCallServer() called twice?" );

   m_serverIPC = new MAppIPCServer;
   if ( !m_serverIPC->Create(IPC_SOCKET) )
   {
      delete m_serverIPC;
      m_serverIPC = NULL;

      return false;
   }

   return true;
}

bool wxMApp::CallAnother()
{
   CHECK( IsAnotherRunning(), false,
          "can't call another copy when it doesn't run!" );

   CHECK( m_cmdLineOptions, false,
          "we need to parse the options before doing the remote call" );

   wxClient client;
   wxConnectionBase *conn = client.MakeConnection("", IPC_SOCKET, IPC_TOPIC);
   if ( !conn )
   {
      // failed to connect to server at all
      return false;
   }

   // pass m_cmdLineOptions to the other process
   bool ok = conn->Execute(m_cmdLineOptions->ToString());

   delete conn;

   return ok;
}

// ----------------------------------------------------------------------------
// CmdLineOptions
// ----------------------------------------------------------------------------

// the char to use as separator: don't use NUL as I'm not sure that wxIPC
// code handlers it correctly
#define CMD_LINE_OPTS_SEP '\1'

// the version of the CmdLineOptions::ToString() format
#define CMD_LINE_OPTS_VERSION 1.0

// the positions of the individual members in the string produced by ToString()
enum
{
   CmdLineOptions_Version,
   CmdLineOptions_Config,
   CmdLineOptions_Bcc,
   CmdLineOptions_Body,
   CmdLineOptions_Cc,
   CmdLineOptions_Newsgroups,
   CmdLineOptions_Subject,
   CmdLineOptions_To,
   CmdLineOptions_Max
};

String CmdLineOptions::ToString() const
{
   // only serialize the "interesting" options
   String s;
   s << s.Format("%f", CMD_LINE_OPTS_VERSION) << CMD_LINE_OPTS_SEP
     << configFile << CMD_LINE_OPTS_SEP
     << composer.bcc << CMD_LINE_OPTS_SEP
     << composer.body << CMD_LINE_OPTS_SEP
     << composer.cc << CMD_LINE_OPTS_SEP
     << composer.newsgroups << CMD_LINE_OPTS_SEP
     << composer.subject << CMD_LINE_OPTS_SEP
     << composer.to;

   return s;
}

bool CmdLineOptions::FromString(const String& s)
{
   wxArrayString
      tokens = wxStringTokenize(s, CMD_LINE_OPTS_SEP, wxTOKEN_RET_EMPTY_ALL);

   if ( tokens.GetCount() != CmdLineOptions_Max )
      return false;

   if ( atof(tokens[CmdLineOptions_Version]) != CMD_LINE_OPTS_VERSION )
      return false;

   configFile = tokens[CmdLineOptions_Config];
   composer.bcc = tokens[CmdLineOptions_Bcc];
   composer.body = tokens[CmdLineOptions_Body];
   composer.cc = tokens[CmdLineOptions_Cc];
   composer.newsgroups = tokens[CmdLineOptions_Newsgroups];
   composer.subject = tokens[CmdLineOptions_Subject];
   composer.to = tokens[CmdLineOptions_To];

   return true;
}

