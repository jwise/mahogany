///////////////////////////////////////////////////////////////////////////////
// Project:     M - cross platform e-mail GUI client
// File name:   classes/MessageView.cpp - non GUI logic of msg viewing
// Purpose:     MessageView class does everything related to the message
//              viewing not related to GUI - the rest is done by a GUI viewer
//              which is just an object implementing MessageViewer interface
//              and which is responsible for the GUI
// Author:      Vadim Zeitlin (based on gui/MessageView.cpp by Karsten)
// Modified by:
// Created:     24.07.01
// CVS-ID:      $Id$
// Copyright:   (c) 2001 Mahogany Team
// Licence:     Mahogany license
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
#   pragma implementation "MessageView.h"
#endif

#include "Mpch.h"

#ifndef USE_PCH
#  include "Mcommon.h"
#  include "strutil.h"

#  include "Profile.h"

#  include "MFrame.h"

#  include "gui/wxMApp.h"      // for PrepareForPrinting()

#  include <wx/menu.h>

#  include "gui/wxOptionsDlg.h"
#endif //USE_PCH

#include "Mdefaults.h"
#include "MHelp.h"
#include "MModule.h"

#include "MessageView.h"
#include "MessageViewer.h"

#include "Message.h"
#include "FolderView.h"
#include "ASMailFolder.h"
#include "MFolder.h"

#include "MDialogs.h"
#include "Mpers.h"
#include "XFace.h"
#include "Collect.h"
#include "sysutil.h"
#include "miscutil.h"         // for GetColourByName()

#include "MessageTemplate.h"
#include "Composer.h"

#include "gui/wxIconManager.h"

#include <wx/dynarray.h>
#include <wx/file.h>
#include <wx/mimetype.h>      // for wxFileType::MessageParameters
#include <wx/process.h>
#include <wx/mstream.h>

#include <ctype.h>  // for isspace
#include <time.h>   // for time stamping autocollected addresses

#ifdef OS_UNIX
   #include <sys/stat.h>

   #include <wx/dcps.h> // for wxThePrintSetupData
#endif

// ----------------------------------------------------------------------------
// options we use here
// ----------------------------------------------------------------------------

extern const MOption MP_AFMPATH;
extern const MOption MP_AUTOCOLLECT;
extern const MOption MP_AUTOCOLLECT_ADB;
extern const MOption MP_AUTOCOLLECT_NAMED;
extern const MOption MP_BROWSER;
extern const MOption MP_BROWSER_INNW;
extern const MOption MP_BROWSER_ISNS;
extern const MOption MP_HIGHLIGHT_URLS;
extern const MOption MP_INCFAX_DOMAINS;
extern const MOption MP_INCFAX_SUPPORT;
extern const MOption MP_INLINE_GFX;
extern const MOption MP_INLINE_GFX_SIZE;
extern const MOption MP_MAX_MESSAGE_SIZE;
extern const MOption MP_MSGVIEW_AUTO_ENCODING;
extern const MOption MP_MSGVIEW_HEADERS;
extern const MOption MP_MSGVIEW_HEADERS_D;
extern const MOption MP_MSGVIEW_VIEWER;
extern const MOption MP_MSGVIEW_VIEWER_D;
extern const MOption MP_MVIEW_TITLE_FMT;
extern const MOption MP_MVIEW_FONT;
extern const MOption MP_MVIEW_FONT_SIZE;
extern const MOption MP_MVIEW_FGCOLOUR;
extern const MOption MP_MVIEW_BGCOLOUR;
extern const MOption MP_MVIEW_URLCOLOUR;
extern const MOption MP_MVIEW_ATTCOLOUR;
extern const MOption MP_MVIEW_QUOTED_COLOURIZE;
extern const MOption MP_MVIEW_QUOTED_CYCLE_COLOURS;
extern const MOption MP_MVIEW_QUOTED_COLOUR1;
extern const MOption MP_MVIEW_QUOTED_COLOUR2;
extern const MOption MP_MVIEW_QUOTED_COLOUR3;
extern const MOption MP_MVIEW_QUOTED_MAXWHITESPACE;
extern const MOption MP_MVIEW_QUOTED_MAXALPHA;
extern const MOption MP_MVIEW_HEADER_NAMES_COLOUR;
extern const MOption MP_MVIEW_HEADER_VALUES_COLOUR;
extern const MOption MP_PLAIN_IS_TEXT;
extern const MOption MP_RFC822_IS_TEXT;
extern const MOption MP_SHOWHEADERS;
extern const MOption MP_SHOW_XFACES;
extern const MOption MP_TIFF2PS;

// ----------------------------------------------------------------------------
// helper functions
// ----------------------------------------------------------------------------

// make the first letter of the string capitalized and all the others of lower
// case, it looks nicer like this when presented to the user
static String NormalizeString(const String& s)
{
   String norm;
   if ( !s.empty() )
   {
      norm << String(s[0]).Upper() << String(s.c_str() + 1).Lower();
   }

   return norm;
}

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// Data about a process (external viewer typically) we launched: these objects
// are created by LaunchProcess() and deleted when the viewer process
// terminates. If it terminates with non 0 exit code, errormsg is given to the
// user. The tempfile is the name of a temp file containing the data passedto
// the viewer (or NULL if none) and will be removed after viewer terminates.
class ProcessInfo
{
public:
   ProcessInfo(wxProcess *process,

               int pid,
               const String& errormsg,
               const String& tempfilename)
      : m_errormsg(errormsg)
   {
      ASSERT_MSG( process && pid, "invalid process in ProcessInfo" );

      m_process = process;
      m_pid = pid;

      if ( !tempfilename.empty() )
         m_tempfile = new MTempFileName(tempfilename);
      else
         m_tempfile = NULL;
   }

   ~ProcessInfo()
   {
      if ( m_process )
         delete m_process;
      if ( m_tempfile )
         delete m_tempfile;
   }

   // get the pid of our process
   int GetPid() const { return m_pid; }

   // get the error message
   const String& GetErrorMsg() const { return m_errormsg; }

   // don't delete wxProcess object (must be called before destroying this
   // object if the external process is still running)
   void Detach() { m_process->Detach(); m_process = NULL; }

   // return the pointer to temp file object (may be NULL)
   MTempFileName *GetTempFile() const { return m_tempfile; }

private:
   String         m_errormsg; // error message to give if launch failed
   wxProcess     *m_process;  // wxWindows process info
   int            m_pid;      // pid of the process
   MTempFileName *m_tempfile; // the temp file (or NULL if none)
};

// a simple event forwarder
class ProcessEvtHandler : public wxEvtHandler
{
public:
   ProcessEvtHandler(MessageView *msgView) { m_msgView = msgView; }

private:
   void OnProcessTermination(wxProcessEvent& event)
   {
      m_msgView->HandleProcessTermination(event.GetPid(), event.GetExitCode());
   }

   MessageView *m_msgView;

   DECLARE_EVENT_TABLE()
};

// the message parameters for the MIME type manager
class MailMessageParameters : public wxFileType::MessageParameters
{
public:
   MailMessageParameters(const wxString& filename,
                         const wxString& mimetype,
                         const MimePart *part)
      : wxFileType::MessageParameters(filename, mimetype)
      {
         m_mimepart = part;
      }

   virtual wxString GetParamValue(const wxString& name) const;

private:
   const MimePart *m_mimepart;
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// helper functions
// ----------------------------------------------------------------------------

static inline
String GetFileNameForMIME(Message *message, int partNo)
{
   return message->GetMimePart(partNo)->GetFilename();
}

// ----------------------------------------------------------------------------
// ProcessEvtHandler
// ----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(ProcessEvtHandler, wxEvtHandler)
   EVT_END_PROCESS(-1, ProcessEvtHandler::OnProcessTermination)
END_EVENT_TABLE()

// ----------------------------------------------------------------------------
// MailMessageParameters
// ----------------------------------------------------------------------------

wxString
MailMessageParameters::GetParamValue(const wxString& name) const
{
   // look in Content-Type parameters
   String value = m_mimepart->GetParam(name);

   if ( value.empty() )
   {
      // try Content-Disposition parameters (should we?)
      value = m_mimepart->GetDispositionParam(name);
   }

   if ( value.empty() )
   {
      // if all else failed, call the base class

      // typedef is needed for VC++ 5.0 - otherwise you get a compile error!
      typedef wxFileType::MessageParameters BaseMessageParameters;
      value = BaseMessageParameters::GetParamValue(name);
   }

   return value;
}

// ----------------------------------------------------------------------------
// MessageView::AllProfileValues
// ----------------------------------------------------------------------------

MessageView::AllProfileValues::AllProfileValues()
{
   // init everything to some default values, even if they're not normally
   // used before ReadAllSettings() is called it is still better not to leave
   // junk in the struct fields
   fontFamily = wxDEFAULT;
   fontSize = 12;

   quotedColourize =
   quotedCycleColours = false;

   quotedMaxWhitespace =
   quotedMaxAlpha = 0;

   showHeaders =
   inlineRFC822 =
   inlinePlainText =
   showFaces =
   highlightURLs = false;

   inlineGFX = -1;

#ifdef OS_UNIX
   browserIsNS =
#endif // Unix
   browserInNewWindow = false;

   autocollect = false;
}

bool
MessageView::AllProfileValues::operator==(const AllProfileValues& other) const
{
   #define CMP(x) (x == other.x)

   return CMP(msgViewer) && CMP(BgCol) && CMP(FgCol) &&
          CMP(UrlCol) && CMP(AttCol) &&
          CMP(QuotedCol[0]) && CMP(QuotedCol[1]) && CMP(QuotedCol[2]) &&
          CMP(quotedColourize) && CMP(quotedCycleColours) &&
          CMP(quotedMaxWhitespace) && CMP(quotedMaxAlpha) &&
          CMP(HeaderNameCol) && CMP(HeaderValueCol) &&
          CMP(fontFamily) && CMP(fontSize) &&
          CMP(showHeaders) && CMP(inlineRFC822) && CMP(inlinePlainText) &&
          CMP(highlightURLs) && CMP(inlineGFX) &&
          CMP(browser) && CMP(browserInNewWindow) &&
          CMP(autocollect) && CMP(autocollectNamed) &&
          CMP(autocollectBookName) &&
#ifdef OS_UNIX
          CMP(browserIsNS) && CMP(afmpath) &&
#endif // Unix
          CMP(showFaces);

   #undef CMP
}

// ============================================================================
// MessageView implementation
// ============================================================================

// ----------------------------------------------------------------------------
// MessageView creation
// ----------------------------------------------------------------------------

MessageView::MessageView()
{
   Init();

   m_viewer = NULL;
}

void
MessageView::Init(wxWindow *parent)
{
   m_viewer = CreateDefaultViewer();
   m_viewer->Create(this, parent);
}

void
MessageView::Init()
{
   m_asyncFolder = NULL;
   m_mailMessage = NULL;
   m_viewer = NULL;

   m_uid = UID_ILLEGAL;
   m_encodingUser =
   m_encodingAuto = wxFONTENCODING_SYSTEM;

   m_evtHandlerProc = NULL;

   RegisterForEvents();
}

// ----------------------------------------------------------------------------
// viewer loading &c
// ----------------------------------------------------------------------------

/* static */
size_t MessageView::GetAllAvailableViewers(wxArrayString *names,
                                           wxArrayString *descs)
{
   CHECK( names && descs, 0, "NULL pointer in GetAllAvailableViewers" );

   MModuleListing *listing =
      MModule::ListAvailableModules(MESSAGE_VIEWER_INTERFACE);
   if ( !listing )
   {
      return 0;
   }

   // found some viewers
   size_t count = listing->Count();
   for ( size_t n = 0; n < count; n++ )
   {
      const MModuleListingEntry& entry = (*listing)[n];

      names->Add(entry.GetName());
      descs->Add(entry.GetShortDescription());
   }

   listing->DecRef();

   return count;
}

void
MessageView::SetViewer(MessageViewer *viewer, wxWindow *parent)
{
   if ( !viewer )
   {
      // having a dummy viewer which simply doesn't do anything is
      // simpler/cleaner than inserting "if ( m_viewer )" checks everywhere
      viewer = CreateDefaultViewer();

      ASSERT_MSG( viewer, "must have default viewer, will crash without it!" );
   }

   viewer->Create(this, parent);

   OnViewerChange(m_viewer, viewer);
   if ( m_viewer )
      delete m_viewer;

   m_viewer = viewer;
}

void
MessageView::CreateViewer(wxWindow *parent)
{
   MessageViewer *viewer = NULL;

   MModuleListing *listing =
      MModule::ListAvailableModules(MESSAGE_VIEWER_INTERFACE);

   if ( !listing  )
   {
      wxLogError(_("No message viewer plugins found. It will be "
                   "impossible to view any messages."));
   }
   else // have at least one viewer, load it
   {
      String name = m_ProfileValues.msgViewer;
      if ( name.empty() )
         name = GetStringDefault(MP_MSGVIEW_VIEWER);

      MModule *viewerFactory = MModule::LoadModule(name);
      if ( !viewerFactory ) // failed to load the configured viewer
      {
         // try any other
         String nameFirst = (*listing)[0].GetName();

         if ( name != nameFirst )
         {
            wxLogWarning(_("Failed to load the configured message viewer '%s'.\n"
                           "\n"
                           "Reverting to the default message viewer."),
                         name.c_str());

            viewerFactory = MModule::LoadModule(nameFirst);

            if ( viewerFactory )
            {
               // remember this one as our real viewer or the code reacting to
               // the options change would break down
               m_ProfileValues.msgViewer = nameFirst;
            }
         }

         if ( !viewerFactory )
         {
            wxLogError(_("Failed to load the default message viewer '%s'.\n"
                         "\n"
                         "Message preview will not work!"), nameFirst.c_str());
         }
      }

      if ( viewerFactory )
      {
         viewer = ((MessageViewerFactory *)viewerFactory)->Create();
         viewerFactory->DecRef();
      }

      listing->DecRef();
   }

   SetViewer(viewer, parent);
}

MessageView::~MessageView()
{
   UnregisterForEvents();

   DetachAllProcesses();
   delete m_evtHandlerProc;

   SafeDecRef(m_mailMessage);
   SafeDecRef(m_asyncFolder);

   delete m_viewer;
}

// ----------------------------------------------------------------------------
// misc
// ----------------------------------------------------------------------------

wxWindow *MessageView::GetWindow() const
{
   return m_viewer->GetWindow();
}

wxFrame *MessageView::GetParentFrame() const
{
   return GetFrame(GetWindow());
}

void
MessageView::Clear()
{
   m_viewer->Clear();
   m_viewer->Update();

   m_uid = UID_ILLEGAL;
   if ( m_mailMessage )
   {
      m_mailMessage->DecRef();
      m_mailMessage = NULL;
   }
}

Profile *MessageView::GetProfile() const
{
   // always return something non NULL
   return m_asyncFolder ? m_asyncFolder->GetProfile()
                        : mApplication->GetProfile();
}

// ----------------------------------------------------------------------------
// MessageView events
// ----------------------------------------------------------------------------

void
MessageView::RegisterForEvents()
{
   // register with the event manager
   m_regCookieOptionsChange = MEventManager::Register(*this, MEventId_OptionsChange);
   ASSERT_MSG( m_regCookieOptionsChange, "can't register for options change event");
   m_regCookieASFolderResult = MEventManager::Register(*this, MEventId_ASFolderResult);
   ASSERT_MSG( m_regCookieASFolderResult, "can't reg folder view with event manager");
}

void MessageView::UnregisterForEvents()
{
   MEventManager::DeregisterAll(&m_regCookieASFolderResult,
                                &m_regCookieOptionsChange,
                                NULL);
}

bool
MessageView::OnMEvent(MEventData& event)
{
   if ( event.GetId() == MEventId_OptionsChange )
   {
      OnOptionsChange((MEventOptionsChangeData &)event);
   }
   else if ( event.GetId() == MEventId_ASFolderResult )
   {
      OnASFolderResultEvent((MEventASFolderResultData &)event);
   }

   return true;
}

void MessageView::OnOptionsChange(MEventOptionsChangeData& event)
{
   // first of all, are we interested in this profile or not?
   Profile *profileChanged = event.GetProfile();
   if ( !profileChanged || !profileChanged->IsAncestor(GetProfile()) )
   {
      // it's some profile which has nothing to do with us
      return;
   }

   switch ( event.GetChangeKind() )
   {
      case MEventOptionsChangeData::Apply:
      case MEventOptionsChangeData::Ok:
      case MEventOptionsChangeData::Cancel:
         UpdateProfileValues();
         break;

      default:
         FAIL_MSG("unknown options change event");
   }

   Update();
}

void
MessageView::OnASFolderResultEvent(MEventASFolderResultData &event)
{
   ASMailFolder::Result *result = event.GetResult();
   if ( result->GetUserData() == this )
   {
      switch(result->GetOperation())
      {
         case ASMailFolder::Op_GetMessage:
         {
            Message *mptr =
               ((ASMailFolder::ResultMessage *)result)->GetMessage();

            if ( mptr )
            {
               // have we got the message we had asked for?
               if ( mptr->GetUId() == m_uid )
               {
                  DoShowMessage(mptr);
               }
               //else: not, it is some other one

               mptr->DecRef();
            }
         }
         break;

         default:
            FAIL_MSG("Unexpected async result event");
      }
   }

   result->DecRef();
}

// ----------------------------------------------------------------------------
// MessageView options
// ----------------------------------------------------------------------------

void
MessageView::UpdateProfileValues()
{
   AllProfileValues settings;
   ReadAllSettings(&settings);
   if ( settings != m_ProfileValues )
   {
      bool recreateViewer = settings.msgViewer != m_ProfileValues.msgViewer;

      // update options first so that CreateViewer() creates the correct
      // viewer
      m_ProfileValues = settings;

      if ( recreateViewer )
      {
         CreateViewer(GetWindow()->GetParent());
      }
   }
   //else: nothing changed
}

void
MessageView::ReadAllSettings(AllProfileValues *settings)
{
   Profile *profile = GetProfile();
   CHECK_RET( profile, "MessageView::ReadAllSettings: no profile" );

   // a macro to make setting many colour options less painful
   #define GET_COLOUR_FROM_PROFILE(col, name) \
      GetColourByName(&col, \
                      READ_CONFIG(profile, MP_MVIEW_##name), \
                      GetStringDefault(MP_MVIEW_##name))

   #define GET_COLOUR_FROM_PROFILE_IF_NOT_FG(which, name) \
      GET_COLOUR_FROM_PROFILE(col, name); \
      if ( col != settings->FgCol ) \
         settings->which = col

   GET_COLOUR_FROM_PROFILE(settings->FgCol, FGCOLOUR);
   GET_COLOUR_FROM_PROFILE(settings->BgCol, BGCOLOUR);

   wxColour col; // used by the macro
   GET_COLOUR_FROM_PROFILE_IF_NOT_FG(UrlCol, URLCOLOUR);
   GET_COLOUR_FROM_PROFILE_IF_NOT_FG(AttCol, ATTCOLOUR);
   GET_COLOUR_FROM_PROFILE_IF_NOT_FG(QuotedCol[0], QUOTED_COLOUR1);
   GET_COLOUR_FROM_PROFILE_IF_NOT_FG(QuotedCol[1], QUOTED_COLOUR2);
   GET_COLOUR_FROM_PROFILE_IF_NOT_FG(QuotedCol[2], QUOTED_COLOUR3);
   GET_COLOUR_FROM_PROFILE_IF_NOT_FG(HeaderNameCol, HEADER_NAMES_COLOUR);
   GET_COLOUR_FROM_PROFILE_IF_NOT_FG(HeaderValueCol, HEADER_VALUES_COLOUR);

   #undef GET_COLOUR_FROM_PROFILE
   #undef GET_COLOUR_FROM_PROFILE_IF_NOT_FG

   settings->quotedColourize =
       READ_CONFIG_BOOL(profile, MP_MVIEW_QUOTED_COLOURIZE);
   settings->quotedCycleColours =
       READ_CONFIG_BOOL(profile, MP_MVIEW_QUOTED_CYCLE_COLOURS);
   settings->quotedMaxWhitespace =
       READ_CONFIG_BOOL(profile, MP_MVIEW_QUOTED_MAXWHITESPACE);
   settings->quotedMaxAlpha = READ_CONFIG(profile,MP_MVIEW_QUOTED_MAXALPHA);

   static const int fontFamilies[] =
   {
      wxDEFAULT,
      wxDECORATIVE,
      wxROMAN,
      wxSCRIPT,
      wxSWISS,
      wxMODERN,
      wxTELETYPE
   };

   long idx = READ_CONFIG(profile, MP_MVIEW_FONT);
   if ( idx < 0 || (size_t)idx >= WXSIZEOF(fontFamilies) )
   {
      FAIL_MSG( "corrupted config data" );

      idx = 0;
   }

   settings->msgViewer = READ_CONFIG_TEXT(profile, MP_MSGVIEW_VIEWER);

   settings->fontFamily = fontFamilies[idx];
   settings->fontSize = READ_CONFIG(profile, MP_MVIEW_FONT_SIZE);
   settings->showHeaders = READ_CONFIG_BOOL(profile, MP_SHOWHEADERS);
   settings->inlinePlainText = READ_CONFIG_BOOL(profile, MP_PLAIN_IS_TEXT);
   settings->inlineRFC822 = READ_CONFIG_BOOL(profile, MP_RFC822_IS_TEXT);
   settings->highlightURLs = READ_CONFIG_BOOL(profile, MP_HIGHLIGHT_URLS);

   // we set inlineGFX to 0 if we don't inline graphics at all and to the
   // max size limit of graphics to show inline otherwise (-1 if no limit)
   settings->inlineGFX = READ_CONFIG(profile, MP_INLINE_GFX);
   if ( settings->inlineGFX )
      settings->inlineGFX = READ_CONFIG(profile, MP_INLINE_GFX_SIZE);

   settings->browser = READ_CONFIG_TEXT(profile, MP_BROWSER);
   settings->browserInNewWindow = READ_CONFIG_BOOL(profile, MP_BROWSER_INNW);
   settings->autocollect =  READ_CONFIG(profile, MP_AUTOCOLLECT);
   settings->autocollectNamed =  READ_CONFIG(profile, MP_AUTOCOLLECT_NAMED);
   settings->autocollectBookName = READ_CONFIG_TEXT(profile, MP_AUTOCOLLECT_ADB);
   settings->showFaces = READ_CONFIG_BOOL(profile, MP_SHOW_XFACES);

   // these settings are used under Unix only
#ifdef OS_UNIX
   settings->browserIsNS = READ_CONFIG_BOOL(profile, MP_BROWSER_ISNS);
   settings->afmpath = READ_APPCONFIG_TEXT(MP_AFMPATH);
#endif // Unix

   // update the parents menu as the show headers option might have changed
   UpdateShowHeadersInMenu();

   m_viewer->UpdateOptions();
}

// ----------------------------------------------------------------------------
// MessageView headers processing
// ----------------------------------------------------------------------------

void
MessageView::ShowHeaders()
{
   m_viewer->StartHeaders();

   // if wanted, display all header lines
   if ( m_ProfileValues.showHeaders )
   {
      m_viewer->ShowRawHeaders(m_mailMessage->GetHeader());
   }

   // retrieve all headers at once instead of calling Message::GetHeaderLine()
   // many times: this is incomparably faster with remote servers (one round
   // trip to server is much less expensive than a dozen of them!)

   // all the headers the user configured
   wxArrayString headersUser =
      strutil_restore_array(':', READ_CONFIG(GetProfile(), MP_MSGVIEW_HEADERS));

   // X-Face is handled separately
#ifdef HAVE_XFACES
   if ( m_ProfileValues.showFaces )
   {
      headersUser.Insert("X-Face", 0);
   }
#endif // HAVE_XFACES

   size_t countHeaders = headersUser.GetCount();

   // stupidly, MP_MSGVIEW_HEADERS_D is terminated with a ':' so there
   // is a dummy empty header at the end - just ignore it for compatibility
   // with existing config files
   if ( countHeaders && headersUser.Last().empty() )
   {
      countHeaders--;
   }

   if ( !countHeaders )
   {
      // no headers at all, don't waste time below
      return;
   }

   // these headers can be taken from the envelope instead of retrieving them
   // from server, so exclude them from headerNames which is the array of
   // headers we're going to retrieve from server
   //
   // the trouble is that we want to keep the ordering of headers correct,
   // hence all the contorsions below: we rmemeber the indices and then inject
   // them back into the code processing headers in the loop
   enum EnvelopHeader
   {
      EnvelopHeader_From,
      EnvelopHeader_To,
      EnvelopHeader_Cc,
      EnvelopHeader_Bcc,
      EnvelopHeader_Subject,
      EnvelopHeader_Date,
      EnvelopHeader_Newsgroups,
      EnvelopHeader_MessageId,
      EnvelopHeader_InReplyTo,
      EnvelopHeader_References,
      EnvelopHeader_Max
   };

   size_t n;

   // put the stuff into the array to be able to use Index() below: a bit
   // silly but not that much as it also gives us index checking in debug
   // builds
   static wxArrayString envelopHeaders;
   if ( envelopHeaders.IsEmpty() )
   {
      // init it on first use
      static const char *envelopHeadersNames[] =
      {
         "From",
         "To",
         "Cc",
         "Bcc",
         "Subject",
         "Date",
         "Newsgroups",
         "Message-Id",
         "In-Reply-To",
         "References",
      };

      ASSERT_MSG( EnvelopHeader_Max == WXSIZEOF(envelopHeadersNames),
                  "forgot to update something - should be kept in sync!" );

      for ( n = 0; n < WXSIZEOF(envelopHeadersNames); n++ )
      {
         envelopHeaders.Add(envelopHeadersNames[n]);
      }
   }

   // the index of the envelop headers if we have to show it, -1 otherwise
   int envelopIndices[EnvelopHeader_Max];
   for ( n = 0; n < EnvelopHeader_Max; n++ )
   {
      envelopIndices[n] = wxNOT_FOUND;
   }

   // a boolean array, in fact
   wxArrayInt headerIsEnv;
   headerIsEnv.Alloc(countHeaders);

   wxArrayString headerNames;

   size_t countNonEnvHeaders = 0;
   for ( n = 0; n < countHeaders; n++ )
   {
      const wxString& h = headersUser[n];

      // we don't need to retrieve envelop headers
      int index = envelopHeaders.Index(h, false /* case insensitive */);
      if ( index == wxNOT_FOUND )
      {
         countNonEnvHeaders++;
      }

      headerIsEnv.Add(index != wxNOT_FOUND);
      headerNames.Add(h);
   }

   // any non envelop headers to retrieve?
   size_t nNonEnv;
   wxArrayInt headerNonEnvEnc;
   wxArrayString headerNonEnvValues;
   if ( countNonEnvHeaders )
   {
      const char **headerPtrs = new const char *[countNonEnvHeaders + 1];

      // have to copy the headers into a temp buffer unfortunately
      for ( nNonEnv = 0, n = 0; n < countHeaders; n++ )
      {
         if ( !headerIsEnv[n] )
         {
            headerPtrs[nNonEnv++] = headerNames[n].c_str();
         }
      }

      // did their number change from just recounting?
      ASSERT_MSG( nNonEnv == countNonEnvHeaders, "logic error" );

      headerPtrs[countNonEnvHeaders] = NULL;

      // get them all at once
      headerNonEnvValues = m_mailMessage->GetHeaderLines(headerPtrs,
                                                         &headerNonEnvEnc);

      delete [] headerPtrs;
   }

   // combine the values of the headers retrieved above with those of the
   // envelop headers into one array
   wxArrayInt headerEncodings;
   wxArrayString headerValues;
   for ( nNonEnv = 0, n = 0; n < countHeaders; n++ )
   {
      if ( headerIsEnv[n] )
      {
         int envhdr = envelopHeaders.Index(headerNames[n]);
         if ( envhdr == wxNOT_FOUND )
         {
            // if headerIsEnv, then it must be in the array
            FAIL_MSG( "logic error" );

            continue;
         }

         // get the raw value
         String value;
         switch ( envhdr )
         {
            case EnvelopHeader_From:
            case EnvelopHeader_To:
            case EnvelopHeader_Cc:
            case EnvelopHeader_Bcc:
               {
                  MessageAddressType mat;
                  switch ( envhdr )
                  {
                     default: FAIL_MSG( "forgot to add header here ");
                     case EnvelopHeader_From: mat = MAT_FROM; break;
                     case EnvelopHeader_To: mat = MAT_TO; break;
                     case EnvelopHeader_Cc: mat = MAT_CC; break;
                     case EnvelopHeader_Bcc: mat = MAT_BCC; break;
                  }

                  value = m_mailMessage->GetAddressesString(mat);
               }
               break;

            case EnvelopHeader_Subject:
               value = m_mailMessage->Subject();
               break;

            case EnvelopHeader_Date:
               // don't read the header line directly because Date() function
               // might return date in some format different from RFC822 one
               value = m_mailMessage->Date();
               break;

            case EnvelopHeader_Newsgroups:
               value = m_mailMessage->GetNewsgroups();
               break;

            case EnvelopHeader_MessageId:
               value = m_mailMessage->GetId();
               break;

            case EnvelopHeader_InReplyTo:
               value = m_mailMessage->GetInReplyTo();
               break;

            case EnvelopHeader_References:
               value = m_mailMessage->GetReferences();
               break;


            default:
               FAIL_MSG( "unknown envelop header" );
         }

         // extract encoding info from it
         wxFontEncoding enc;
         headerValues.Add(MailFolder::DecodeHeader(value, &enc));
         headerEncodings.Add(enc);
      }
      else // non env header
      {
         headerValues.Add(headerNonEnvValues[nNonEnv]);
         headerEncodings.Add(headerNonEnvEnc[nNonEnv]);

         nNonEnv++;
      }
   }

   // for the loop below: we start it at 0 normally but at 1 if we have an
   // X-Face as we don't want to show it verbatim ...
   n = 0;

   // ... instead we show an icon for it
   if ( m_ProfileValues.showFaces )
   {
      wxString xfaceString = headerValues[n++];
      if ( xfaceString.length() > 20 )
      // FIXME it was > 2, i.e. \r\n. Although if(uncompface(data) < 0) in
      // XFace.cpp should catch illegal data, it is not the case. For example,
      // for "X-Face: nope" some nonsense was displayed. So we use 20 for now.
      {
         XFace *xface = new XFace;
         xface->CreateFromXFace(xfaceString.c_str());

         char **xfaceXpm;
         if ( xface->CreateXpm(&xfaceXpm) )
         {
            m_viewer->ShowXFace(wxBitmap(xfaceXpm));

            wxIconManager::FreeImage(xfaceXpm);
         }

         delete xface;
      }
   }

   // show the headers using the correct encoding now
   wxFontEncoding encInHeaders = wxFONTENCODING_SYSTEM;
   for ( ; n < countHeaders; n++ )
   {
      wxFontEncoding encHeader = (wxFontEncoding)headerEncodings[n];

      // remember the encoding that we have found in the headers: some mailers
      // are broken in so weird way that they use correct format for the
      // headers but fail to specify charset parameter in Content-Type (the
      // best of the Web mailers do this - the worst/normal just send 8 bit in
      // the headers too)
      if ( encHeader != wxFONTENCODING_SYSTEM )
      {
         // we deal with them by supposing that the body has the same encoding
         // as the headers by default, so we remember encInHeaders here and
         // use it later when showing the body
         encInHeaders = encHeader;
      }
      else // no encoding in the header
      {
         // use the user specified encoding if none specified in the header
         // itself
         if ( m_encodingUser != wxFONTENCODING_SYSTEM )
            encHeader = m_encodingUser;
      }

      m_viewer->ShowHeader(headerNames[n], headerValues[n], encHeader);
   }

   m_viewer->EndHeaders();

   // NB: some broken mailers don't create correct "Content-Type" header,
   //     but they may yet give us the necessary info in the other headers so
   //     we assume the header encoding as the default encoding for the body
   m_encodingAuto = encInHeaders;
}

// ----------------------------------------------------------------------------
// MessageView text part processing
// ----------------------------------------------------------------------------

size_t
MessageView::GetQuotedLevel(const char *text) const
{
   size_t qlevel = strutil_countquotinglevels
                   (
                     text,
                     m_ProfileValues.quotedMaxWhitespace,
                     m_ProfileValues.quotedMaxAlpha
                   );

   // note that qlevel is counted from 1, really, as 0 means unquoted and that
   // GetQuoteColour() relies on this
   if ( qlevel > QUOTE_LEVEL_MAX )
   {
      if ( m_ProfileValues.quotedCycleColours )
      {
         // cycle through the colours: use 1st level colour for QUOTE_LEVEL_MAX
         qlevel = (qlevel - 1) % QUOTE_LEVEL_MAX + 1;
      }
      else
      {
         // use the same colour for all levels deeper than max
         qlevel = QUOTE_LEVEL_MAX;
      }
   }

   return qlevel;
}

wxColour MessageView::GetQuoteColour(size_t qlevel) const
{
   if ( qlevel-- == 0 )
   {
      return m_ProfileValues.FgCol;
   }

   CHECK( qlevel < QUOTE_LEVEL_MAX, wxNullColour,
          "MessageView::GetQuoteColour(): invalid quoting level" );

   return m_ProfileValues.QuotedCol[qlevel];
}

void MessageView::ShowTextPart(const MimePart *mimepart)
{
   // as we're going to need its contents, we'll have to download it: check if
   // it is not too big before doing this
   if ( !CheckMessagePartSize(mimepart) )
   {
      // don't download this part
      return;
   }

   // cast is ok - it's a text part
   String textPart = (const char *)mimepart->GetContent();

   // get the encoding of the text
   wxFontEncoding encPart;
   if ( m_encodingUser != wxFONTENCODING_SYSTEM )
   {
      // user-specified encoding overrides everything
      encPart = m_encodingUser;
   }
   else if ( READ_CONFIG(GetProfile(), MP_MSGVIEW_AUTO_ENCODING) )
   {
      encPart = mimepart->GetTextEncoding();

      if ( encPart == wxFONTENCODING_UTF8 )
      {
         // convert from UTF-8 to environment's default encoding
         // FIXME it won't be needed when full Unicode support is available
         String textPartOrig = textPart;
         textPart = wxString(textPartOrig.wc_str(wxConvUTF8), wxConvLocal);
         if ( textPart.Length() == 0 )
         {
            // conversion failed - use original text (and display
            // incorrectly, unfortunately)
            textPart = textPartOrig;
            wxLogDebug("conversion from UTF-8 to environment's default encoding failed");
         }

#if wxUSE_INTL
         encPart = wxLocale::GetSystemEncoding();
#else // !wxUSE_INTL
         encPart = wxFONTENCODING_ISO8859_1;
#endif // wxUSE_INTL/!wxUSE_INTL

         // show UTF-8, not env. encoding in Language menu
         m_encodingAuto = wxFONTENCODING_UTF8;
      }
      else if ( encPart == wxFONTENCODING_SYSTEM ||
                encPart == wxFONTENCODING_DEFAULT )
      {
         // use the encoding of the last part which had it
         encPart = m_encodingAuto;
      }
      else if ( m_encodingAuto == wxFONTENCODING_SYSTEM )
      {
         // remember the encoding for the next parts
         m_encodingAuto = encPart;
      }
   }
   else
   {
      // autodetecting encoding is disabled, don't use any
      encPart = wxFONTENCODING_SYSTEM;
   }


   TextStyle style;
   if ( encPart != wxFONTENCODING_SYSTEM )
   {
      wxFont font(
                  m_ProfileValues.fontSize,
                  m_ProfileValues.fontFamily,
                  wxNORMAL,
                  wxNORMAL,
                  FALSE,   // not underlined
                  "",      // no specific face name
                  encPart
                 );
      style.SetFont(font);
   }

   // TODO: detect signature as well and call m_viewer->InsertSignature()
   //       for it
   String url;
   String before;

   do
   {
      if ( m_ProfileValues.highlightURLs )
      {
         // extract the first URL into url string and put all preceding
         // text into before, textPart is updated to contain only the text
         // after the URL
         before = strutil_findurl(textPart, url);
      }
      else
      {
         before = textPart;

         textPart.clear();
      }

      if ( m_ProfileValues.quotedColourize )
      {
         size_t level = GetQuotedLevel(before);
         style.SetTextColour(GetQuoteColour(level));

         // the string shouldn't be shared as only we use it and, although the
         // cast is still ugly and dangerous, it should be used here as it
         // allows us to avoid copying potentially huge strings below but to
         // just insert '\0' as needed
         char *lineCur = (char *)before.c_str();
         char *lineNext = strchr(lineCur, '\n');
         while ( lineNext )
         {
            // skip '\n'
            lineNext++;

            size_t levelNew = GetQuotedLevel(lineNext);
            if ( levelNew != level )
            {
               char chSave = *lineNext;
               *lineNext = '\0';

               m_viewer->InsertText(lineCur, style);

               *lineNext = chSave;

               level = levelNew;
               style.SetTextColour(GetQuoteColour(level));

               lineCur = lineNext;
            }
            //else: same level as the previous line, just continue

            if (*lineNext) 
            {
               lineNext = strchr(lineNext + 1, '\n');
            }
            else
            {
               lineNext = 0;
            }
         }

         if ( lineCur )
         {
            m_viewer->InsertText(lineCur, style);
         }
      }
      else // no quoted text colourizing
      {
         m_viewer->InsertText(before, style);
      }

      if ( !strutil_isempty(url) )
      {
         m_viewer->InsertURL(url);
      }
   }
   while ( !strutil_isempty(textPart) );
}

// ----------------------------------------------------------------------------
// MessageView attachments and images handling
// ----------------------------------------------------------------------------

void MessageView::ShowAttachment(const MimePart *mimepart)
{
   // get the icon for the attachment using its MIME type and filename
   // extension (if any)
   wxString mimeFileName = mimepart->GetFilename();
   wxIcon icon = mApplication->GetIconManager()->
                     GetIconFromMimeType(mimepart->GetType().GetFull(),
                                         mimeFileName.AfterLast('.'));

   m_viewer->InsertAttachment(icon, GetClickableInfo(mimepart));
}

void MessageView::ShowImage(const MimePart *mimepart)
{
   // first of all, can we show it inline at all?
   bool showInline = m_viewer->CanInlineImages();

   if ( showInline )
   {
      switch ( m_ProfileValues.inlineGFX )
      {
         default:
            // check that the size of the image is less than configured
            // maximum
            if ( mimepart->GetSize() > 1024*(size_t)m_ProfileValues.inlineGFX )
            {
               wxString msg;
               msg.Printf
                   (
                     _("An image embedded in this message is bigger "
                       "than the currently configured limit of %luKb.\n"
                       "\n"
                       "Would you still like to see it?\n"
                       "\n"
                       "You can change this setting in the \"Message "
                       "View\" page of the preferences dialog to 0 if "
                       "you want to always show the images inline."),
                     (unsigned long)m_ProfileValues.inlineGFX
                   );

               if ( MDialog_YesNoDialog
                    (
                     msg,
                     GetParentFrame(),
                     MDIALOG_YESNOTITLE,
                     false, // [No] default
                     GetPersMsgBoxName(M_MSGBOX_GFX_NOT_INLINED)
                    ) )
               {
                  // will inline
                  break;
               }

            }
            else
            {
               // will inline
               break;
            }

            // fall through

         case 0:
            // never inline
            showInline = false;
            break;

         case -1:
            // always inline
            break;
      }
   }

   if ( showInline )
   {
      unsigned long len;
      const void *data = mimepart->GetContent(&len);

      if ( !data )
      {
         wxLogError(_("Cannot get attachment content."));

         return;
      }

      wxMemoryInputStream mis(data, len);
      wxImage img(mis);
      if ( !img.Ok() )
      {
#ifdef OS_UNIX
         // try loading via wxIconManager which can use ImageMagik to do the
         // conversion
         MTempFileName tmpFN;
         if ( tmpFN.IsOk() )
         {
            String filename = tmpFN.GetName();
            MimeSave(mimepart, filename);

            bool ok;
            img =  wxIconManager::LoadImage(filename, &ok, true);
            if ( !ok )
               showInline = false;
         }
#else // !OS_UNIX
         showInline = false;
#endif // OS_UNIX/!OS_UNIX
      }

      if ( showInline )
      {
         m_viewer->InsertImage(img, GetClickableInfo(mimepart));
      }
   }

   if ( !showInline )
   {
      // show as an attachment then
      ShowAttachment(mimepart);
   }
}

/* static */
String MessageView::GetLabelFor(const MimePart *mimepart)
{
   wxString label = mimepart->GetFilename();
   if ( !label.empty() )
      label << " : ";

   MimeType type = mimepart->GetType();
   label << type.GetFull();

   // multipart always have size of 0, don't show
   if ( type.GetPrimary() != MimeType::MULTIPART )
   {
      label << ", ";

      size_t lines;
      if ( type.IsText() && (lines = mimepart->GetNumberOfLines()) )
      {
         label << strutil_ultoa(lines) << _(" lines");
      }
      else
      {
         label << strutil_ultoa(mimepart->GetSize()) << _(" bytes");
      }
   }

   return label;
}

ClickableInfo *MessageView::GetClickableInfo(const MimePart *mimepart) const
{
   return new ClickableInfo(mimepart, GetLabelFor(mimepart));
}

// ----------------------------------------------------------------------------
// global MIME structure parsing
// ----------------------------------------------------------------------------

void
MessageView::ShowPart(const MimePart *mimepart)
{
   size_t partSize = mimepart->GetSize();
   if ( partSize == 0 )
   {
      // ignore empty parts but warn user as it might indicate a problem
      wxLogStatus(GetParentFrame(),
                  _("Skipping the empty MIME part #%d."),
                  mimepart->GetPartSpec().c_str());

      return;
   }

   MimeType type = mimepart->GetType();

   String mimeType = type.GetFull();

   String fileName = mimepart->GetFilename();

   MimeType::Primary primaryType = type.GetPrimary();

   // deal with unknown MIME types: some broken mailers use unregistered
   // primary MIME types - try to do something with them here (even though
   // breaking the neck of the author of the software which generated them
   // would be more satisfying)
   if ( primaryType > MimeType::OTHER )
   {
      wxString typeName = type.GetType();

      if ( typeName == "OCTET" )
      {
         // I have messages with "Content-Type: OCTET/STREAM", convert them
         // to "APPLICATION/OCTET-STREAM"
         primaryType = MimeType::APPLICATION;
      }
      else
      {
         wxLogDebug("Invalid MIME type '%s'!", typeName.c_str());
      }
   }

   // let's guess a little if we have generic APPLICATION MIME type, we may
   // know more about this file type from local sources
   if ( primaryType == MimeType::APPLICATION )
   {
      // get the MIME type for the files of this extension
      wxString ext = fileName.AfterLast('.');
      if ( !ext.empty() )
      {
         wxMimeTypesManager& mimeManager = mApplication->GetMimeManager();
         wxFileType *ft = mimeManager.GetFileTypeFromExtension(ext);
         if(ft)
         {
            wxString mt;
            ft->GetMimeType(&mt);
            delete ft;

            if(wxMimeTypesManager::IsOfType(mt,"image/*"))
               primaryType = MimeType::IMAGE;
            else if(wxMimeTypesManager::IsOfType(mt,"audio/*"))
               primaryType = MimeType::AUDIO;
            else if(wxMimeTypesManager::IsOfType(mt,"video/*"))
               primaryType = MimeType::VIDEO;
         }
      }
   }

   m_viewer->StartPart();

   // if the disposition is set to attachment we force the part to be shown
   // as an attachment
   bool isAttachment = mimepart->GetDisposition().IsSameAs("attachment", false);

   // first check for viewer specific formats, next for text, then for
   // images and finally show all the rest as generic attachment

   if ( !isAttachment && m_viewer->CanProcess(mimeType) )
   {
      // as we're going to need its contents, we'll have to download it: check
      // if it is not too big before doing this
      if ( CheckMessagePartSize(mimepart) )
      {
         const void *data = mimepart->GetContent();

         if ( !data )
         {
            wxLogError(_("Cannot get attachment content."));
         }
         else
         {
            String s(data, (const char *)data + partSize);

            m_viewer->InsertRawContents(s);
         }
      }
      //else: skip this part
   }
   else if ( !isAttachment &&
             ((mimeType == "TEXT/PLAIN" &&
               (fileName.empty() || m_ProfileValues.inlinePlainText)) ||
              (primaryType == MimeType::MESSAGE &&
                m_ProfileValues.inlineRFC822)) )
   {
      ShowTextPart(mimepart);
   }
   else if ( primaryType == MimeType::IMAGE )
   {
      ShowImage(mimepart);
   }
   else // attachment
   {
      ShowAttachment(mimepart);
   }

   m_viewer->EndPart();
}

void
MessageView::ProcessAllNestedParts(const MimePart *mimepart)
{
   const MimePart *partChild = mimepart->GetNested();
   while ( partChild )
   {
      ProcessPart(partChild);

      partChild = partChild->GetNext();
   }
}

void
MessageView::ProcessPart(const MimePart *mimepart)
{
   CHECK_RET( mimepart, "MessageView::ProcessPart: NULL mimepart" );

   MimeType type = mimepart->GetType();
   switch ( type.GetPrimary() )
   {
      case MimeType::MULTIPART:
         {
            String subtype = type.GetSubType();

            // TODO: support for DIGEST, RELATED and SIGNED
            if ( subtype == "ALTERNATIVE" )
            {
               // find the best subpart we can show
               //
               // normally we'd have to iterate from end as the best
               // representation (i.e. the most faithful) is the last one
               // according to RFC 2046, but as we only have forward pointers
               // we iterate from the start - not a big deal
               const MimePart *partChild = mimepart->GetNested();

               const MimePart *partBest = partChild;
               while ( partChild )
               {
                  String mimetype = partChild->GetType().GetFull();

                  if ( mimetype == "TEXT/PLAIN" ||
                        m_viewer->CanProcess(mimetype) )
                  {
                     // remember this one as the best so far
                     partBest = partChild;
                  }

                  partChild = partChild->GetNext();
               }

               // show just the best one
               CHECK_RET(partBest != 0, "No part can be displayed !");
               ShowPart(partBest);
            }
            else // assume MIXED for all unknown
            {
               ProcessAllNestedParts(mimepart);
            }
         }
         break;

      case MimeType::MESSAGE:
         if ( m_ProfileValues.inlineRFC822 )
         {
            ProcessAllNestedParts(mimepart);
         }
         //else: fall through and show it as attachment

      case MimeType::TEXT:
      case MimeType::APPLICATION:
      case MimeType::AUDIO:
      case MimeType::IMAGE:
      case MimeType::VIDEO:
      case MimeType::MODEL:
      case MimeType::OTHER:
      case MimeType::CUSTOM1:
      case MimeType::CUSTOM2:
      case MimeType::CUSTOM3:
      case MimeType::CUSTOM4:
      case MimeType::CUSTOM5:
      case MimeType::CUSTOM6:
         // a simple part, show it (ShowPart() decides how exactly)
         ShowPart(mimepart);
         break;

      default:
         FAIL_MSG( "unknown MIME type" );
   }
}

void
MessageView::Update(void)
{
   m_viewer->Clear();

   if( !m_mailMessage )
   {
      // no message to display
      return;
   }

   m_uid = m_mailMessage->GetUId();

   // deal with the headers first
   ShowHeaders();

   m_viewer->StartBody();

   ProcessPart(m_mailMessage->GetTopMimePart());

   m_viewer->EndBody();

   // if user selects the language from the menu, m_encodingUser is set
   wxFontEncoding encoding;
   if ( m_encodingUser != wxFONTENCODING_SYSTEM ) 
   {
      encoding = m_encodingUser;
   }
   else if ( m_encodingAuto != wxFONTENCODING_SYSTEM )
   {
      encoding = m_encodingAuto;
   }
   else encoding = wxFONTENCODING_DEFAULT;

   // update the menu of the frame containing us to show the encoding used
   CheckLanguageInMenu(GetParentFrame(), encoding);
}

// ----------------------------------------------------------------------------
// MIME attachments menu commands
// ----------------------------------------------------------------------------

// show information about an attachment
void
MessageView::MimeInfo(const MimePart *mimepart)
{
   MimeType type = mimepart->GetType();

   String message;
   message << _("MIME type: ") << type.GetFull() << '\n';

   String desc = mimepart->GetDescription();
   if ( !desc.empty() )
      message << '\n' << _("Description: ") << desc << '\n';

   message << _("Size: ");
   size_t lines;
   if ( type.IsText() && (lines = mimepart->GetNumberOfLines()) )
   {
      message << strutil_ltoa(lines) << _(" lines");
   }
   else
   {
      message << strutil_ltoa(mimepart->GetSize()) << _(" bytes");
   }

   message << '\n';

   // param name and value (used in 2 loops below)
   wxString name, value;

   // debug output with all parameters
   const MessageParameterList &plist = mimepart->GetParameters();
   MessageParameterList::iterator plist_it;
   if ( !plist.empty() )
   {
      message += _("\nParameters:\n");
      for ( plist_it = plist.begin(); plist_it != plist.end(); plist_it++ )
      {
         name = plist_it->name;
         message << NormalizeString(name) << ": ";

         // filenames are case-sensitive, don't modify them
         value = plist_it->value;
         if ( name.CmpNoCase("name") != 0 )
         {
            value.MakeLower();
         }

         message << value << '\n';
      }
   }

   // now output disposition parameters too
   String disposition = mimepart->GetDisposition();
   const MessageParameterList& dlist = mimepart->GetDispositionParameters();

   if ( !strutil_isempty(disposition) )
      message << _("\nDisposition: ") << disposition.Lower() << '\n';

   if ( !dlist.empty() )
   {
      message += _("\nDisposition parameters:\n");
      for ( plist_it = dlist.begin(); plist_it != dlist.end(); plist_it++ )
      {
         name = plist_it->name;
         message << NormalizeString(name) << ": ";

         value = plist_it->value;
         if ( name.CmpNoCase("filename") != 0 )
         {
            value.MakeLower();
         }

         message << value << '\n';
      }
   }

   String title;
   title << _("MIME information for attachment #") << mimepart->GetPartSpec();

   MDialog_Message(message, GetParentFrame(), title);
}

// open (execute) a message attachment
void
MessageView::MimeHandle(const MimePart *mimepart)
{
   // we'll need this filename later
   wxString filenameOrig = mimepart->GetFilename();

   MimeType type = mimepart->GetType();

   String mimetype = type.GetFull();
   wxMimeTypesManager& mimeManager = mApplication->GetMimeManager();

   wxFileType *fileType = NULL;
   if ( wxMimeTypesManager::IsOfType(mimetype, "APPLICATION/OCTET-STREAM") )
   {
      // special handling of "APPLICATION/OCTET-STREAM": this is the default
      // MIME type for all binary attachments and many e-mail clients don't
      // use the correct type (e.g. IMAGE/GIF) always leaving this one as
      // default. Try to guess a better MIME type ourselves from the file
      // extension.
      if ( !filenameOrig.empty() )
      {
         wxString ext;
         wxSplitPath(filenameOrig, NULL, NULL, &ext);

         if ( !ext.empty() )
            fileType = mimeManager.GetFileTypeFromExtension(ext);
      }
   }

   if ( !fileType )
   {
      // non default MIME type (so use it) or couldn't get the MIME type from
      // extension
      fileType = mimeManager.GetFileTypeFromMimeType(mimetype);
   }

   // First, we check for those contents that we handle in M itself:

   // handle internally MESSAGE/*
   if ( wxMimeTypesManager::IsOfType(mimetype, "MESSAGE/*") )
   {
#if 0
      // It's a pity, but creating a MessageCC from a string doesn't
      // quite work yet. :-(
      unsigned long len;
      char const *content = m_mailMessage->GetPartContent(mimeDisplayPart, &len);
      if( !content )
      {
         wxLogError(_("Cannot get attachment content."));
         return;
      }
      Message *msg = Message::Create(content, 1);
      ...
      msg->DecRef();
#else // 1
      bool ok = false;
      char *filename = wxGetTempFileName("Mtemp");
      if ( MimeSave(mimepart, filename) )
      {
         wxString name;
         name.Printf(_("Attached message '%s'"),
                     filenameOrig.c_str());

         MFolder_obj mfolder = MFolder::CreateTemp
                               (
                                 name,
                                 MF_FILE, 0,
                                 filename
                               );

         if ( mfolder )
         {
            ASMailFolder *asmf = ASMailFolder::OpenFolder(mfolder);
            if ( asmf )
            {
               // FIXME: assume UID of the first message in new MBX folder is
               //        always 1
               ShowMessageViewFrame(GetParentFrame(), asmf, 1);

               ok = true;

               asmf->DecRef();
            }
         }
      }

      if ( !ok )
      {
         wxLogError(_("Failed to open attached message."));
      }

      wxRemoveFile(filename);
#endif // 0/1

      return;
   }

   String
      filename = wxGetTempFileName("Mtemp"),
      filename2 = "";

   wxString ext;
   wxSplitPath(filenameOrig, NULL, NULL, &ext);
   // get the standard extension for such files if there is no real one
   if ( fileType != NULL && !ext)
   {
      wxArrayString exts;
      if ( fileType->GetExtensions(exts) && exts.GetCount() )
      {
         ext = exts[0u];
      }
   }

   // under Windows some programs will do different things depending on the
   // extensions of the input file (case in point: WinZip), so try to choose a
   // correct one
   wxString path, name, extOld;
   wxSplitPath(filename, &path, &name, &extOld);
   if ( extOld != ext )
   {
      // Windows creates the temp file even if we didn't use it yet
      if ( !wxRemoveFile(filename) )
      {
         wxLogDebug("Warning: stale temp file '%s' will be left.",
                    filename.c_str());
      }

      filename = path + wxFILE_SEP_PATH + name;
      filename << '.' << ext;
   }

   MailMessageParameters params(filename, mimetype, mimepart);

   // We might fake a file, so we need this:
   bool already_saved = false;

   Profile *profile = GetProfile();

#ifdef OS_UNIX
   /* For IMAGE/TIFF content, check whether it comes from one of the
      fax domains. If so, change the mimetype to "IMAGE/TIFF-G3" and
      proceed in the usual fashion. This allows the use of a special
      image/tiff-g3 mailcap entry. */
   if ( READ_CONFIG(profile,MP_INCFAX_SUPPORT) &&
        (wxMimeTypesManager::IsOfType(mimetype, "IMAGE/TIFF")
         || wxMimeTypesManager::IsOfType(mimetype, "APPLICATION/OCTET-STREAM")))
   {
      kbStringList faxdomains;
      char *faxlisting = strutil_strdup(READ_CONFIG(profile,
                                                    MP_INCFAX_DOMAINS));
      strutil_tokenise(faxlisting, ":;,", faxdomains);
      delete [] faxlisting;
      bool isfax = false;
      wxString domain;
      wxString fromline = m_mailMessage->From();
      strutil_tolower(fromline);

      for(kbStringList::iterator i = faxdomains.begin();
          i != faxdomains.end(); i++)
      {
         domain = **i;
         strutil_tolower(domain);
         if(fromline.Find(domain) != -1)
            isfax = true;
      }

      if(isfax
         && MimeSave(mimepart, filename))
      {
         wxLogDebug("Detected image/tiff fax content.");
         // use TIFF2PS command to create a postscript file, open that
         // one with the usual ps viewer
         filename2 = filename.BeforeLast('.') + ".ps";
         String command;
         command.Printf(READ_CONFIG_TEXT(profile,MP_TIFF2PS),
                        filename.c_str(), filename2.c_str());
         // we ignore the return code, because next viewer will fail
         // or succeed depending on this:
         //system(command);  // this produces a postscript file on  success
         RunProcess(command);
         // We cannot use launch process, as it doesn't wait for the
         // program to finish.
         //wxString msg;
         //msg.Printf(_("Running '%s' to create Postscript file failed."), command.c_str());
         //(void)LaunchProcess(command, msg );

         wxRemoveFile(filename);
         filename = filename2;
         mimetype = "application/postscript";
         if(fileType) delete fileType;
         fileType = mimeManager.GetFileTypeFromMimeType(mimetype);

         // proceed as usual
         MailMessageParameters new_params(filename, mimetype, mimepart);
         params = new_params;
         already_saved = true; // use this file instead!
      }
   }
#endif // Unix

   // We must save the file before actually calling GetOpenCommand()
   if( !already_saved )
   {
      MimeSave(mimepart, filename);
      already_saved = TRUE;
   }
   String command;
   if ( (fileType == NULL) || !fileType->GetOpenCommand(&command, params) )
   {
      // unknown MIME type, ask the user for the command to use
      String prompt;
      prompt.Printf(_("Please enter the command to handle '%s' data:"),
                    mimetype.c_str());
      if ( !MInputBox(&command, _("Unknown MIME type"), prompt,
                      GetParentFrame(), "MimeHandler") )
      {
         // cancelled by user
         return;
      }

      if ( command.empty() )
      {
         wxLogWarning(_("Do not know how to handle data of type '%s'."),
                      mimetype.c_str());
      }
      else
      {
         // the command must contain exactly one '%s' format specificator!
         String specs = strutil_extract_formatspec(command);
         if ( specs.empty() )
         {
            // at least the filename should be there!
            command += " %s";
         }

         // do expand it
         command = wxFileType::ExpandCommand(command, params);

         // TODO save this command to mailcap!
      }
      //else: empty command means try to handle it internally
   }

   if ( fileType )
      delete fileType;

   if ( ! command.empty() )
   {
      if(already_saved || MimeSave(mimepart, filename))
      {
         wxString errmsg;
         errmsg.Printf(_("Error opening attachment: command '%s' failed"),
                       command.c_str());
         (void)LaunchProcess(command, errmsg, filename);
      }
   }
}

void
MessageView::MimeOpenWith(const MimePart *mimepart)
{
   // we'll need this filename later
   wxString filenameOrig = mimepart->GetFilename();

   MimeType type = mimepart->GetType();

   String mimetype = type.GetFull();
   wxMimeTypesManager& mimeManager = mApplication->GetMimeManager();

   wxFileType *fileType = NULL;
   fileType = mimeManager.GetFileTypeFromMimeType(mimetype);

   String filename = wxGetTempFileName("Mtemp");

   wxString ext;
   wxSplitPath(filenameOrig, NULL, NULL, &ext);
   // get the standard extension for such files if there is no real one
   if ( fileType != NULL && !ext )
   {
      wxArrayString exts;
      if ( fileType->GetExtensions(exts) && exts.GetCount() )
      {
         ext = exts[0u];
      }
   }

   // under Windows some programs will do different things depending on the
   // extensions of the input file (case in point: WinZip), so try to choose a
   // correct one
   wxString path, name, extOld;
   wxSplitPath(filename, &path, &name, &extOld);
   if ( extOld != ext )
   {
      // Windows creates the temp file even if we didn't use it yet
      if ( !wxRemoveFile(filename) )
      {
         wxLogDebug("Warning: stale temp file '%s' will be left.",
                    filename.c_str());
      }

      filename = path + wxFILE_SEP_PATH + name;
      filename << '.' << ext;
   }

   MailMessageParameters params(filename, mimetype, mimepart);

   String command;
   // ask the user for the command to use
   String prompt;
   prompt.Printf(_("Please enter the command to handle '%s' data:"),
                 mimetype.c_str());
   if ( !MInputBox(&command, _("Open with"), prompt,
                   GetParentFrame(), "MimeHandler") )
   {
      // cancelled by user
      return;
   }

   if ( command.empty() )
   {
      wxLogWarning(_("Do not know how to handle data of type '%s'."),
                   mimetype.c_str());
   }
   else
   {
      // the command must contain exactly one '%s' format specificator!
      String specs = strutil_extract_formatspec(command);
      if ( specs.empty() )
      {
         // at least the filename should be there!
         command += " %s";
      }

      // do expand it
      command = wxFileType::ExpandCommand(command, params);

   }

   if ( ! command.empty() )
   {
      if ( MimeSave(mimepart, filename) )
      {
         wxString errmsg;
         errmsg.Printf(_("Error opening attachment: command '%s' failed"),
                       command.c_str());
         (void)LaunchProcess(command, errmsg, filename);
      }
   }
}

bool
MessageView::MimeSave(const MimePart *mimepart,const char *ifilename)
{
   String filename;

   if ( strutil_isempty(ifilename) )
   {
      filename = mimepart->GetFilename();

      wxString path, name, ext;
      wxSplitPath(filename, &path, &name, &ext);

      filename = wxPFileSelector("MimeSave",_("Save attachment as:"),
                                 NULL, // no default path
                                 name, ext,
                                 NULL,
                                 wxFILEDLG_USE_FILENAME |
                                 wxSAVE |
                                 wxOVERWRITE_PROMPT,
                                 GetParentFrame());
   }
   else
      filename = ifilename;

   if ( !filename )
   {
      // no filename and user cancelled the dialog
      return false;
   }

   unsigned long len;
   const void *content = mimepart->GetContent(&len);
   if( !content )
   {
      wxLogError(_("Cannot get attachment content."));
   }
   else
   {
      wxFile out(filename, wxFile::write);
      if ( out.IsOpened() )
      {
         bool ok = true;

         // when saving messages to a file we need to "From stuff" them to
         // make them readable in a standard mail client (including this one)
         if ( mimepart->GetType().GetPrimary() == MimeType::MESSAGE )
         {
            // standard prefix
            String fromLine = "From ";

            // find the from address
            const char *p = strstr((const char *)content, "From: ");
            if ( !p )
            {
               // this shouldn't normally happen, but if it does just make it
               // up
               wxLogDebug("Couldn't find from header in the message");

               fromLine += "MAHOGANY-DUMMY-SENDER";
            }
            else // take everything until the end of line
            {
               // FIXME: we should extract just the address in angle brackets
               //        instead of taking everything
               while ( *p && *p != '\r' )
               {
                  fromLine += *p++;
               }
            }

            fromLine += ' ';

            // time stamp
            time_t t;
            time(&t);
            fromLine += ctime(&t);

            ok = out.Write(fromLine);
         }

         if ( ok )
         {
            // write the body
            ok = out.Write(content, len) == len;
         }

         if ( ok )
         {
            // only display in interactive mode
            if ( strutil_isempty(ifilename) )
            {
               wxLogStatus(GetParentFrame(), _("Wrote %lu bytes to file '%s'"),
                           len, filename.c_str());
            }

            return true;
         }
      }
   }

   wxLogError(_("Could not save the attachment."));

   return false;
}

void
MessageView::MimeViewText(const MimePart *mimepart)
{
   const void *content = mimepart->GetContent();
   if ( content )
   {
      String title;
      title << _("Attachment #") << mimepart->GetPartSpec();

      // add the filename if any
      String filename = mimepart->GetFilename();
      if ( !filename.empty() )
      {
         title << " ('" << filename << "')";
      }

      MDialog_ShowText(GetParentFrame(), title,
                       (const char *)content, "MimeView");
   }
   else
   {
      wxLogError(_("Failed to view the attachment."));
   }
}

// ----------------------------------------------------------------------------
// URL handling
// ----------------------------------------------------------------------------

void MessageView::OpenURL(const String& url, bool inNewWindow)
{
   wxFrame *frame = GetParentFrame();
   wxLogStatus(frame, _("Opening URL '%s'..."), url.c_str());

   wxBusyCursor bc;

   // the command to execute
   wxString command;

   bool bOk = false;
   if ( m_ProfileValues.browser.empty() )
   {
#ifdef OS_WIN
      // ShellExecute() always opens in the same window,
      // so do it manually for new window
      if ( inNewWindow )
      {
         wxRegKey key(wxRegKey::HKCR, url.BeforeFirst(':') + "\\shell\\open");
         if ( key.Exists() )
         {
            wxRegKey keyDDE(key, "DDEExec");
            if ( keyDDE.Exists() )
            {
               wxString ddeTopic = wxRegKey(keyDDE, "topic");

               // we only know the syntax of WWW_OpenURL DDE request
               if ( ddeTopic == "WWW_OpenURL" )
               {
                  wxString ddeCmd = keyDDE;

                  // this is a bit naive but should work as -1 can't appear
                  // elsewhere in the DDE topic, normally
                  if ( ddeCmd.Replace("-1", "0",
                                      FALSE /* only first occurence */) == 1 )
                  {
                     // and also replace the parameters
                     if ( ddeCmd.Replace("%1", url, FALSE) == 1 )
                     {
                        // magic incantation understood by wxMSW
                        command << "WX_DDE#"
                                << wxRegKey(key, "command") << '#'
                                << wxRegKey(keyDDE, "application") << '#'
                                << ddeTopic << '#'
                                << ddeCmd;
                     }
                  }
               }
            }
         }
      }

      if ( !command.empty() )
      {
         wxString errmsg;
         errmsg.Printf(_("Could not launch browser: '%s' failed."),
                       command.c_str());
         bOk = LaunchProcess(command, errmsg);
      }
      else // easy case: open in the same window
      {
         bOk = (int)ShellExecute(NULL, "open", url,
                                 NULL, NULL, SW_SHOWNORMAL ) > 32;
      }
#else  // Unix
      // propose to choose program for opening URLs
      if (
         MDialog_YesNoDialog
         (
            _("No command configured to view URLs.\n"
              "Would you like to choose one now?"),
            frame,
            MDIALOG_YESNOTITLE,
            true,
            GetPersMsgBoxName(M_MSGBOX_ASK_URL_BROWSER)
            )
         )
         ShowOptionsDialog();

      if ( m_ProfileValues.browser.empty() )
      {
         wxLogError(_("No command configured to view URLs."));
         bOk = false;
      }
#endif // Win/Unix
   }
   else // browser setting non empty, use it
   {
#ifdef OS_UNIX
      if ( m_ProfileValues.browserIsNS ) // try re-loading first
      {
         wxString lockfile;
         wxGetHomeDir(&lockfile);
         if ( !wxEndsWithPathSeparator(lockfile) )
            lockfile += '/';
         lockfile += ".netscape/lock";
         struct stat statbuf;

         if(lstat(lockfile.c_str(), &statbuf) == 0)
         // cannot use wxFileExists, because it's a link pointing to a
         // non-existing location      if(wxFileExists(lockfile))
         {
            command << m_ProfileValues.browser
                    << " -remote openURL(" << url;
            if ( inNewWindow )
            {
               command << ",new-window)";
            }
            else
            {
               command << ")";
            }
            wxString errmsg;
            errmsg.Printf(_("Could not launch browser: '%s' failed."),
                          command.c_str());
            bOk = LaunchProcess(command, errmsg);
         }
      }
#endif // Unix
      // either not netscape or ns isn't running or we have non-UNIX
      if(! bOk)
      {
         command = m_ProfileValues.browser;
         command << ' ' << url;

         wxString errmsg;
         errmsg.Printf(_("Couldn't launch browser: '%s' failed"),
                       command.c_str());

         bOk = LaunchProcess(command, errmsg);
      }
   }

   if ( bOk )
   {
      wxLogStatus(frame, _("Opening URL '%s'... done."), url.c_str());
   }
   else
   {
      wxLogStatus(frame, _("Opening URL '%s' failed."), url.c_str());
   }
}

// ----------------------------------------------------------------------------
// MessageView menu command processing
// ----------------------------------------------------------------------------

bool
MessageView::DoMenuCommand(int id)
{
   if ( m_uid == UID_ILLEGAL )
      return false;

   CHECK( GetFolder(), false, "no folder in message view?" );

   Profile *profile = GetProfile();
   CHECK( profile, false, "no profile in message view?" );

   UIdArray msgs;
   msgs.Add(m_uid);

   switch ( id )
   {
      case WXMENU_EDIT_COPY:
         m_viewer->Copy();
         break;

      case WXMENU_EDIT_FIND:
         {
            String text;
            if ( MInputBox(&text,
                           _("Find text"),
                           _("   Find:"),
                           GetParentFrame(),
                           "MsgViewFindString") )
            {
               if ( !m_viewer->Find(text) )
               {
                  wxLogStatus(GetParentFrame(),
                              _("'%s' not found"),
                              text.c_str());
               }
            }
         }
         break;

      case WXMENU_EDIT_FINDAGAIN:
         if ( !m_viewer->FindAgain() )
         {
            wxLogStatus(GetParentFrame(), _("No more matches"));
         }
         break;

      case WXMENU_HELP_CONTEXT:
         mApplication->Help(MH_MESSAGE_VIEW, GetParentFrame());
         break;

      case WXMENU_MSG_TOGGLEHEADERS:
         m_ProfileValues.showHeaders = !m_ProfileValues.showHeaders;
         profile->writeEntry(MP_SHOWHEADERS, m_ProfileValues.showHeaders);
         UpdateShowHeadersInMenu();
         Update();
         break;

      default:
         if ( WXMENU_CONTAINS(LANG, id) && (id != WXMENU_LANG_SET_DEFAULT) )
         {
            SetLanguage(id);
         }
         else
         {
            // not handled
            return false;
         }
   }

   // message handled
   return true;
}

void
MessageView::DoMouseCommand(int id, const ClickableInfo *ci, const wxPoint& pt)
{
   CHECK_RET( ci, "MessageView::DoMouseCommand(): NULL ClickableInfo" );

   switch ( ci->GetType() )
   {
      case ClickableInfo::CI_ICON:
      {
         switch ( id )
         {
            case WXMENU_LAYOUT_RCLICK:
               PopupMIMEMenu(GetWindow(), ci->GetPart(), pt);
               break;

            case WXMENU_LAYOUT_LCLICK:
               // for now, do the same thing as double click: perhaps the
               // left button behaviour should be configurable?

            case WXMENU_LAYOUT_DBLCLICK:
               // open
               MimeHandle(ci->GetPart());
               break;

            default:
               FAIL_MSG("unknown mouse action");
         }
      }
      break;

      case ClickableInfo::CI_URL:
      {
         wxString url = ci->GetUrl();

         // treat mail urls separately:
         wxString protocol = url.BeforeFirst(':');
         if ( protocol == "mailto" )
         {
            Composer *cv = Composer::CreateNewMessage(GetProfile());

            cv->SetAddresses(ci->GetUrl().Right(ci->GetUrl().Length()-7));
            cv->InitText();

            break;
         }

         if ( id == WXMENU_LAYOUT_RCLICK )
         {
            PopupURLMenu(GetWindow(), url, pt);
         }
         else // left or double click
         {
            OpenURL(url, m_ProfileValues.browserInNewWindow);
         }
      }
      break;

      default:
         FAIL_MSG("unknown embedded object type");
   }
}

void
MessageView::SetLanguage(int id)
{
   wxFontEncoding encoding = GetEncodingFromMenuCommand(id);
   SetEncoding(encoding);
}

void
MessageView::SetEncoding(wxFontEncoding encoding)
{
   m_encodingUser = encoding;

   Update();
}

void MessageView::ResetUserEncoding()
{
   // if the user had manually set the encoding for the old message, we
   // revert back to automatic encoding detection for the new one
   if ( READ_CONFIG(GetProfile(), MP_MSGVIEW_AUTO_ENCODING) )
   {
      // don't keep it for the other messages, just for this one
      m_encodingUser = wxFONTENCODING_SYSTEM;
   }
}

void
MessageView::UpdateShowHeadersInMenu()
{
   wxFrame *frame = GetParentFrame();
   CHECK_RET( frame, "message view without parent frame?" );

   wxMenuBar *mbar = frame->GetMenuBar();
   CHECK_RET( mbar, "message view frame without menu bar?" );

   mbar->Check(WXMENU_MSG_TOGGLEHEADERS, m_ProfileValues.showHeaders);
}

// ----------------------------------------------------------------------------
// MessageView selecting the shown message
// ----------------------------------------------------------------------------

void
MessageView::SetFolder(ASMailFolder *asmf)
{
   if ( asmf == m_asyncFolder )
      return;

   if ( m_asyncFolder )
      m_asyncFolder->DecRef();

   m_asyncFolder = asmf;

   if ( m_asyncFolder )
      m_asyncFolder->IncRef();

   if ( m_asyncFolder )
   {
      // use the settings for this folder now 
      UpdateProfileValues();
   }
   else // no folder
   {
      // on the contrary, revert to the default ones if we don't have any
      // folder any more
      SetViewer(NULL, GetWindow()->GetParent());

      // make sure the viewer will be recreated the next time we are called
      // with a valid folder - if we didn't do it, the UpdateProfileValues()
      // wouldn't do anything if the same folder was reopened, for example
      m_ProfileValues.msgViewer.clear();
   }

   ResetUserEncoding();
}

void
MessageView::ShowMessage(UIdType uid)
{
   if ( m_uid == uid )
      return;

   if ( uid == UID_ILLEGAL || !m_asyncFolder )
   {
      // don't show anything
      Clear();
   }
   else
   {
      ResetUserEncoding();

      // file request, our DoShowMessage() will be called later
      m_uid = uid;
      (void)m_asyncFolder->GetMessage(uid, this);
   }
}

bool
MessageView::CheckMessageOrPartSize(unsigned long size, bool part) const
{
   unsigned long maxSize = (unsigned long)READ_CONFIG(GetProfile(),
                                                      MP_MAX_MESSAGE_SIZE);

   // translate to Kb before comparing with MP_MAX_MESSAGE_SIZE which is in Kb
   size /= 1024;
   if ( size <= maxSize )
   {
      // it's ok, don't ask
      return true;
   }
   //else: big message, ask

   wxString msg;
   msg.Printf(_("The selected message%s is %u Kbytes long which is "
                "more than the current threshold of %d Kbytes.\n"
                "\n"
                "Do you still want to download it?"),
              part ? _(" part") : "", size, maxSize);

   return MDialog_YesNoDialog(msg, GetParentFrame());
}

bool
MessageView::CheckMessagePartSize(const MimePart *mimepart) const
{
   // only check for IMAP here: for POP/NNTP we had already checked it in
   // CheckMessageSize() below and the local folders are fast
   return (m_asyncFolder->GetType() != MF_IMAP) ||
               CheckMessageOrPartSize(mimepart->GetSize(), true);
}

bool
MessageView::CheckMessageSize(const Message *message) const
{
   // we check the size here only for POP3 and NNTP because the local folders
   // are supposed to be fast and for IMAP we don't retrieve the whole message
   // at once so we'd better ask only if we're going to download it
   //
   // for example, if the message has text/plain part of 100 bytes and
   // video/mpeg one of 2Mb, we don't want to ask the user before downloading
   // 100 bytes (and then, second time, before downloading 2b!)
   MFolderType folderType = m_asyncFolder->GetType();
   return (folderType != MF_POP && folderType != MF_NNTP) ||
               CheckMessageOrPartSize(message->GetSize(), false);
}

void
MessageView::DoShowMessage(Message *mailMessage)
{
   CHECK_RET( mailMessage, "no message to show in MessageView" );
   CHECK_RET( m_asyncFolder, "no folder in MessageView::DoShowMessage()" );

   if ( !CheckMessageSize(mailMessage) )
   {
      // too big, don't show it
      return;
   }

   mailMessage->IncRef();

   // ok, make this our new current message
   SafeDecRef(m_mailMessage);

   m_mailMessage = mailMessage;
   m_uid = mailMessage->GetUId();

   // have we not seen the message before?
   if ( !(m_mailMessage->GetStatus() & MailFolder::MSG_STAT_SEEN) )
   {
      // mark it as seen
      m_mailMessage->GetFolder()->
        SetMessageFlag(m_uid, MailFolder::MSG_STAT_SEEN, true);

      // autocollect the addresses from it if configured
      if ( m_ProfileValues.autocollect )
      {
         String folderName = m_mailMessage->GetFolder() ?
            m_mailMessage->GetFolder()->GetName() : String(_("unknown"));

         AutoCollectAddresses(m_mailMessage,
                              m_ProfileValues.autocollect,
                              m_ProfileValues.autocollectNamed != 0,
                              m_ProfileValues.autocollectBookName,
                              folderName,
                              GetParentFrame());
      }
   }

   Update();
}

// ----------------------------------------------------------------------------
// selection
// ----------------------------------------------------------------------------

String MessageView::GetSelection() const
{
   return m_viewer->GetSelection();
}

// ----------------------------------------------------------------------------
// printing
// ----------------------------------------------------------------------------

void
MessageView::PrepareForPrinting()
{
   static bool s_printingPrepared = false;
   if ( s_printingPrepared )
      return;

   s_printingPrepared = true;

#ifdef OS_WIN
   wxGetApp().SetPrintMode(wxPRINT_WINDOWS);
#else // Unix
   wxGetApp().SetPrintMode(wxPRINT_POSTSCRIPT);

   // set AFM path
   PathFinder pf(mApplication->GetGlobalDir()+"/afm", false);
   pf.AddPaths(m_ProfileValues.afmpath, false);
   pf.AddPaths(mApplication->GetLocalDir(), true);

   bool found;
   String afmpath = pf.FindDirFile("Cour.afm", &found);
   if(found)
   {
      ((wxMApp *)mApplication)->GetPrintData()->SetFontMetricPath(afmpath);
      wxThePrintSetupData->SetAFMPath(afmpath);
   }
#endif // Win/Unix
}

bool
MessageView::Print(void)
{
   PrepareForPrinting();

   return m_viewer->Print();
}

void
MessageView::PrintPreview(void)
{
   PrepareForPrinting();

   m_viewer->PrintPreview();
}

// ----------------------------------------------------------------------------
// external processes
// ----------------------------------------------------------------------------

bool
MessageView::RunProcess(const String& command)
{
   wxLogStatus(GetParentFrame(),
               _("Calling external viewer '%s'"),
               command.c_str());
   return wxExecute(command, true) == 0;
}

ProcessEvtHandler *
MessageView::GetEventHandlerForProcess()
{
   if ( !m_evtHandlerProc )
   {
      m_evtHandlerProc = new ProcessEvtHandler(this);
   }

   return m_evtHandlerProc;
}

bool
MessageView::LaunchProcess(const String& command,
                             const String& errormsg,
                             const String& filename)
{
   wxLogStatus(GetParentFrame(),
               _("Calling external viewer '%s'"),
               command.c_str());

   wxProcess *process = new wxProcess(GetEventHandlerForProcess());
   int pid = wxExecute(command, false, process);
   if ( !pid )
   {
      delete process;

      if ( !errormsg.empty() )
         wxLogError("%s.", errormsg.c_str());

      return false;
   }

   if ( pid != -1 )
   {
      ProcessInfo *procInfo = new ProcessInfo(process, pid, errormsg, filename);

      m_processes.Add(procInfo);
   }

   return true;
}

void
MessageView::HandleProcessTermination(int pid, int exitcode)
{
   // find the corresponding entry in m_processes
   size_t n,
          procCount = m_processes.GetCount();
   for ( n = 0; n < procCount; n++ )
   {
      if ( m_processes[n]->GetPid() == pid )
         break;
   }

   CHECK_RET( n != procCount, "unknown process terminated!" );

   ProcessInfo *info = m_processes[n];
   if ( exitcode != 0 )
   {
      wxLogError(_("%s (external viewer exited with non null exit code)"),
                 info->GetErrorMsg().c_str());
   }

   m_processes.RemoveAt(n);
   delete info;
}

void MessageView::DetachAllProcesses()
{
   size_t procCount = m_processes.GetCount();
   for ( size_t n = 0; n < procCount; n++ )
   {
      ProcessInfo *info = m_processes[n];
      info->Detach();

      MTempFileName *tempfile = info->GetTempFile();
      if ( tempfile )
      {
         String tempFileName = tempfile->GetName();
         wxLogWarning(_("Temporary file '%s' left because it is still in "
                        "use by an external process"), tempFileName.c_str());
      }

      delete info;
   }
}

// ----------------------------------------------------------------------------
// MessageView scrolling
// ----------------------------------------------------------------------------

/// scroll down one line
bool
MessageView::LineDown()
{
   return m_viewer->LineDown();
}

/// scroll up one line:
bool
MessageView::LineUp()
{
   return m_viewer->LineUp();
}

/// scroll down one page:
bool
MessageView::PageDown()
{
   return m_viewer->PageDown();
}

/// scroll up one page:
bool
MessageView::PageUp()
{
   return m_viewer->PageUp();
}

