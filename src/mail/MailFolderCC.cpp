//////////////////////////////////////////////////////////////////////////////
// Project:     M - cross platform e-mail GUI client
// File name:   mail/MailFolderCC.cpp: implements MailFolderCC class
// Purpose:     handling of mail folders with c-client lib
// Author:      Karsten Ball�der
// Modified by: Vadim Zeitlin at 24.01.01: complete rewrite of update logic
// Created:     1997
// CVS-ID:      $Id$
// Copyright:   (C) 1997-2000 by Karsten Ball�der (ballueder@gmx.net)
// Licence:     M license
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
#   pragma implementation "MailFolderCC.h"
#endif

#include  "Mpch.h"

#ifndef  USE_PCH
#   include "strutil.h"
#   include "MApplication.h"
#   include "Profile.h"
#endif // USE_PCH

#include "MPython.h"

#include "Mdefaults.h"
#include "MDialogs.h"
#include "FolderView.h"

#include "UIdArray.h"

#include "MFStatus.h"
#include "MSearch.h"
#include "LogCircle.h"
#include "MFui.h"                      // for SizeToString

#include "AddressCC.h"
#include "MailFolderCC.h"
#include "MessageCC.h"

#include "MEvent.h"
#include "ASMailFolder.h"
#include "Mpers.h"

// we need "Impl" for ArrayHeaderInfo declaration
#include "HeaderInfoImpl.h"

#include "modules/Filters.h"

#include "MFCache.h"
#include "Sequence.h"

#include "MFPrivate.h"
#include "mail/Driver.h"
#include "mail/FolderPool.h"
#include "mail/ServerInfo.h"

// just to use wxFindFirstFile()/wxFindNextFile() for lockfile checking and
// wxFile::Exists() too
#include <wx/filefn.h>
#include <wx/file.h>

// DecodeHeader() uses CharsetToEncoding()
#include <wx/fontmap.h>

#include <ctype.h>      // isspace()
#include <sys/stat.h>   // struct stat

#ifdef OS_UNIX
   #include <signal.h>

   // wxTYPE_SA_HANDLER is defined by wxWindows configure script in its setup.h
   extern "C" void sigpipe_handler(wxTYPE_SA_HANDLER)
   {
      // do nothing
   }
#endif // OS_UNIX

extern "C"
{
   #undef LOCAL         // before including imap4r1.h which defines it too

   #define namespace cc__namespace
   #include <imap4r1.h> // for LEVELSORT/THREAD in CanSort()/Thread()
   #undef namespace
}

// this is #define'd in windows.h included by one of cclient headers
#undef CreateDialog

#define USE_READ_PROGRESS
//#define USE_BLOCK_NOTIFY

#ifdef USE_BLOCK_NOTIFY
   #include <wx/datetime.h>
#endif

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

/**
  @name checking folder macros

  check if the folder is still available and give an error message if it isn't
  (the message must contain a %s which is replaced by the folder name)
 */

//@{

/// and return
#define CHECK_DEAD(msg) \
   if ( m_MailStream == NIL ) \
   { \
      ERRORMESSAGE((_(msg), GetName().c_str()));\
      return;\
   }

/// and return the given value
#define CHECK_DEAD_RC(msg, rc) \
   if ( m_MailStream == NIL )\
   { \
      ERRORMESSAGE((_(msg), GetName().c_str()));\
      return rc; \
   }

//@}

// ----------------------------------------------------------------------------
// options we use here
// ----------------------------------------------------------------------------

extern const MOption MP_CONN_CLOSE_DELAY;
extern const MOption MP_DEBUG_CCLIENT;
extern const MOption MP_FOLDERPROGRESS_THRESHOLD;
extern const MOption MP_FOLDER_FILE_DRIVER;
extern const MOption MP_FOLDER_TRY_CREATE;
extern const MOption MP_IMAP_LOOKAHEAD;
extern const MOption MP_MESSAGEPROGRESS_THRESHOLD_SIZE;
extern const MOption MP_MESSAGEPROGRESS_THRESHOLD_TIME;
extern const MOption MP_MSGS_SERVER_SORT;
extern const MOption MP_MSGS_SERVER_THREAD;
extern const MOption MP_MSGS_SERVER_THREAD_REF_ONLY;
extern const MOption MP_NEWS_SPOOL_DIR;
extern const MOption MP_RSH_PATH;
extern const MOption MP_SSH_PATH;
extern const MOption MP_TCP_CLOSETIMEOUT;
extern const MOption MP_TCP_OPENTIMEOUT;
extern const MOption MP_TCP_READTIMEOUT;
extern const MOption MP_TCP_WRITETIMEOUT;
extern const MOption MP_TCP_RSHTIMEOUT;
extern const MOption MP_TCP_SSHTIMEOUT;

// ----------------------------------------------------------------------------
// persistent msgboxes we use here
// ----------------------------------------------------------------------------

extern const MPersMsgBox *M_MSGBOX_NO_NET_PING_ANYWAY;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

/// invalid value for MailFolderCC::m_chDelimiter
#define ILLEGAL_DELIMITER ((char)-1)

// ----------------------------------------------------------------------------
// trace masks used (you have to wxLog::AddTraceMask() to enable the
// correpsonding kind of messages)
// ----------------------------------------------------------------------------

// turn on to log [almost] all cclient callbacks
#define TRACE_MF_CALLBACK "cccback"

// turn on logging of MailFolderCC operations
#define TRACE_MF_CALLS "mfcall"

// ----------------------------------------------------------------------------
// global functions prototypes
// ----------------------------------------------------------------------------

extern void Pop3_SaveFlags(const String& folderName, MAILSTREAM *stream);
extern void Pop3_RestoreFlags(const String& folderName, MAILSTREAM *stream);

// ----------------------------------------------------------------------------
// typedefs
// ----------------------------------------------------------------------------

typedef void (*mm_list_handler)(MAILSTREAM *stream,
                                char delim,
                                String name,
                                long  attrib);

typedef void (*mm_status_handler)(MAILSTREAM *stream,
                                  const char *mailbox,
                                  MAILSTATUS *status);

// ----------------------------------------------------------------------------
// globals
// ----------------------------------------------------------------------------

/// object used to reflect some events back to MailFolderCC
static class CCEventReflector *gs_CCEventReflector = NULL;

#ifdef USE_DIALUP
/// object used to close the streams if it can't be done when closing folder
static class CCStreamCleaner *gs_CCStreamCleaner = NULL;
#endif // USE_DIALUP

/// handler for temporarily redirected mm_list calls
static mm_list_handler gs_mmListRedirect = NULL;

/// handler for temporarily redirected mm_status calls
static mm_status_handler gs_mmStatusRedirect = NULL;

// a variable telling c-client to shut up
static bool mm_ignore_errors = false;

/// a variable disabling most events (not mm_list or mm_lsub though)
static bool mm_disable_callbacks = false;

/// show cclient debug output
#ifdef DEBUG
   static bool mm_show_debug = true;
#else
   static bool mm_show_debug = false;
#endif

/// loglevel for cclient error messages:
static int cc_loglevel = wxLOG_Error;

/// the cclient mail folder driver name
static const char *CCLIENT_DRIVER_NAME = "cclient";

/// our mail folder factory object
static MFDriver gs_driverCC
(
   CCLIENT_DRIVER_NAME,
   MailFolderCCInit,
   MailFolderCC::OpenFolder,
   MailFolderCC::CheckStatus,
   MailFolderCC::DeleteFolder,
   MailFolderCC::Rename,
   MailFolderCC::ClearFolder,
   MailFolderCC::GetFullImapSpec,
   MailFolderCCCleanup
);

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

extern "C"
{
#ifdef USE_READ_PROGRESS
   void mahogany_read_progress(GETS_DATA *md, unsigned long count);
#endif

#ifdef USE_BLOCK_NOTIFY
   void *mahogany_block_notify(int reason, void *data);
#endif
};

/// return the c-client "{...}" string for the given folder
static wxString GetImapSpec(const MFolder *folder);

/// trivial wrappers around mail_open() which wants a non const "char *" (ugh)
static inline
MAILSTREAM *MailOpen(MAILSTREAM *stream, const char *mailbox, long options = 0)
{
   if ( mm_show_debug )
      options |= OP_DEBUG;

   return mail_open(stream, (char *)mailbox, options);
}

// we want to specify the stream as the first argument as c-client does so we
// can't use just one function with default NIL parameter for the stream but
// instead we have this second version for the most common case
static inline
MAILSTREAM *MailOpen(const char *mailbox, long options = 0)
{
   return MailOpen(NIL, mailbox, options);
}

// ============================================================================
// private classes
// ============================================================================

/// a list of MAILSTREAM pointers
M_LIST_PTR(StreamList, MAILSTREAM);

// we extend ServerInfoEntry with connection caching
class ServerInfoEntryCC : public ServerInfoEntry
{
public:
   // replace the base class functions with our versions returning
   // ServerInfoEntryCC and not ServerInfoEntry so that we can use GetStream()
   // and KeepStream() on the returned pointers
   //
   // the casts are ugly but valid because all the folders we have here are of
   // "cclient" class and hence their server info entries must be of
   // ServerInfoEntryCC class

   static ServerInfoEntryCC *Get(const MFolder *folder)
      { return (ServerInfoEntryCC *)ServerInfoEntry::Get(folder); }

   static ServerInfoEntryCC *GetOrCreate(const MFolder *folder)
   {
      ServerInfoEntryCC *serverInfo = Get(folder);
      if ( !serverInfo )
      {
         serverInfo = new ServerInfoEntryCC(folder);
         ms_servers.push_back(serverInfo);
      }

      return serverInfo;
   }

   /// return true if this server can be used with the given folder
   bool CanBeUsedFor(const MFolder *folder) const;

   /// check for the timed out connections for all servers
   static void CheckTimeoutAll();

   /// delete all server entries
   static void DeleteAll();

   /**
     @name Connection pooling
    */
   //@{

   /// return an existing connection to this server or NULL if none
   MAILSTREAM *GetStream();

   /// give us a stream to reuse later (or close if nobody wants it)
   void KeepStream(MAILSTREAM *stream, const MFolder *folder);

   /// close those of our connections which have timed out
   virtual bool CheckTimeout();

   //@}

   // dtor must be public in order to use M_LIST_OWN() but nobody should delete
   // us directly!
   virtual ~ServerInfoEntryCC();

private:
   // ctor is private, nobody except GetOrCreate() can create us
   ServerInfoEntryCC(const MFolder *folder);

   // parse the folder to ms_lastParsed: this function caches the last folder
   // used thus avoiding calling mail_valid_net_parse() unnecessarily
   static bool Parse(const MFolder *folder);

   // this struct contains the info about the last folder parsed by Parse()
   static /* FIXME-MT */ struct LastParsedFolder
   {
      NETMBX netmbx;
      const MFolder *folder;
   } ms_lastParsed;

   // this struct contains the server hostname, port, username &c for this
   // folder
   NETMBX m_netmbx;

   // the pool of connections to this server which may be reused (may be empty,
   // of course)
   //
   // this list is used as a FIFO queue, in fact
   StreamList m_connections;

   // the timeouts for each of the connections, i.e. the moment when we should
   // close them
   M_LIST(TimeList, time_t) m_timeouts;

   /**
     A small class to close the cached connections periodically.
    */
   static class ConnCloseTimer : public wxTimer
   {
   public:
      virtual void Notify() { ServerInfoEntryCC::CheckTimeoutAll(); }
   } *ms_connCloseTimer;

   // it creates us in CreateServerInfo
   friend class MailFolderCC;
};

/**
  Close a stream: these functions either really close it or put it to the
  server connection pool for those folders for which it makes sense, i.e. IMAP
  and NNTP (POP connections cannot and shouldn't be reused).

  Having two versions is only useful because it avoids calling
  ServerInfoEntry::Get() again unnecessarily if the caller already has the
  server.
*/

/// is this a reusable folder?
static bool IsReusableFolder(const MFolder *folder)
{
   CHECK( folder, false, "NULL folder in IsReusableFolder" );

   MFolderType folderType = folder->GetType();
   return folderType == MF_IMAP || folderType == MF_NNTP;
}

/// close the stream associated with the given folder and server
static void CloseOrKeepStream(MAILSTREAM *stream,
                              const MFolder *folder,
                              ServerInfoEntryCC *server)
{
   // don't cache the connections if we're going to shutdown (and hence close
   // them all) soon anyhow
   if ( server && IsReusableFolder(folder) && !mApplication->IsShuttingDown() )
   {
      server->KeepStream(stream, folder);
   }
   else
   {
      wxLogTrace(TRACE_MF_CALLS, "Closing connection to '%s'",
                 folder->GetFullName().c_str());

      mail_close(stream);
   }
}

/// close the stream associated with the given folder
static void CloseOrKeepStream(MAILSTREAM *stream,
                              const MFolder *folder)
{
   ServerInfoEntryCC *server;
   if ( IsReusableFolder(folder) )
   {
      // do look for the server
      server = ServerInfoEntryCC::GetOrCreate(folder);
   }
   else // no need
   {
      // this will just call mail_close() in CloseOrKeepStream() above
      server = NULL;
   }

   CloseOrKeepStream(stream, folder, server);
}

/**
   The idea behind CCEventReflector is to allow postponing some actions in
   MailFolderCC code, i.e. instead of doing something immediately after getting
   c-client notification, we generate and send a helper event which is
   processed by gs_CCEventReflector later (== during the next idle handler
   iteration) and which calls back the corresponding MailFolderCC method on the
   object which got the c-client notification in the first place.

   This is necessary in some cases and very convenient in the others: necessary
   for new mail handling as we want to filter it which can (and will) result in
   more c-client calls but this can't be done from inside c-client handler
   (such as mm_exists()) because c-client is not reentrant. Convenient for
   batching together several notifications: for example, we often receive a lot
   of mm_flags at once (when the status of a selection consisting of many
   messages is changed) and forwarding all of them to the GUI immediately
   would result in horrible flicker - so, instead, we just accumulate them in
   internal arrays and generate a single MsgStatus event for them when
   OnMsgStatusChanged() is called.

   The same technique should be used to batch process other notifications as
   well if it's worth it.
 */

// ----------------------------------------------------------------------------
// private event classes used together with CCEventReflector to reflect
// some events back to MailFolderCC
// ----------------------------------------------------------------------------

/// MEventFolderOnNewMailData - used by MailFolderCC to update itself
class MEventFolderOnNewMailData : public MEventWithFolderData
{
public:
   MEventFolderOnNewMailData(MailFolder *folder)
      : MEventWithFolderData(MEventId_MailFolder_OnNewMail, folder) { }
};

/// MEventFolderOnMsgStatusData - used by MailFolderCC to send MsgStatus events
class MEventFolderOnMsgStatusData : public MEventWithFolderData
{
public:
   MEventFolderOnMsgStatusData(MailFolder *folder)
      : MEventWithFolderData(MEventId_MailFolder_OnMsgStatus, folder) { }
};

// ----------------------------------------------------------------------------
// CCEventReflector
// ----------------------------------------------------------------------------

// this class processes MEventFolderOnNewMail events by just calling
// MailFolderCC::OnNewMail()
class CCEventReflector : public MEventReceiver
{
public:
   CCEventReflector()
   {
      MEventManager::RegisterAll
      (
         this,
         MEventId_MailFolder_OnNewMail,   &m_cookieNewMail,
         MEventId_MailFolder_OnMsgStatus, &m_cookieMsgStatus,
         MEventId_Null
      );
   }

   virtual bool OnMEvent(MEventData& ev)
   {
      MEventWithFolderData& event = (MEventWithFolderData &)ev;
      MailFolderCC *mfCC = (MailFolderCC *)event.GetFolder();

      switch ( ev.GetId() )
      {
         case MEventId_MailFolder_OnNewMail:
            mfCC->OnNewMail();
            break;

         case MEventId_MailFolder_OnMsgStatus:
            mfCC->OnMsgStatusChanged();
            break;

         default:
            FAIL_MSG( "unexpected event in CCEventReflector!" );
            return true;
      }

      // no need to search further, only we process these events
      return false;
   }

   virtual ~CCEventReflector()
   {
      MEventManager::DeregisterAll
      (
         &m_cookieNewMail,
         &m_cookieMsgStatus,
         NULL
      );
   }

private:
   void *m_cookieNewMail,
        *m_cookieMsgStatus;
};

#ifdef USE_DIALUP

// ----------------------------------------------------------------------------
// CCStreamCleaner
// ----------------------------------------------------------------------------

/** This is a list of all mailstreams which were left open because the
    dialup connection broke before we could close them.
    We remember them and try to close them at program exit. */
class CCStreamCleaner
{
public:
   void Add(MAILSTREAM *stream)
   {
      CHECK_RET( stream, "NULL stream in CCStreamCleaner::Add()" );

      m_List.push_back(stream);
   }

   ~CCStreamCleaner();

private:
   StreamList m_List;
};

#endif // USE_DIALUP

// ----------------------------------------------------------------------------
// OverviewData: this class is just used to share parameters between
// GetHeaderInfo() and OverviewHeaderEntry() which is called from it
// indirectly
// ----------------------------------------------------------------------------

class OverviewData
{
public:
   // ctor takes the pointer to the start of HeaderInfo buffer and the number
   // of slots in it and the msgno we are going to start retrieving headers
   // from
   OverviewData(const Sequence& seq, ArrayHeaderInfo& headers, size_t nTotal)
      : m_seq(seq), m_headers(headers)
   {
      m_nTotal = nTotal;
      m_nRetrieved = 0;
      m_progdlg = NULL;

      m_current = m_seq.GetFirst(m_seqCookie);
   }

   // dtor deletes the progress dialog if it had been shown
   ~OverviewData()
   {
      if ( m_progdlg )
      {
         MGuiLocker locker;
         delete m_progdlg;

         // ok, it's really ugly to put it here, but it's the only way to do it
         // for now, unfortunately (FIXME)
         //
         // we don't want to leave a big "hole" on the parent window but this
         // may happen as we are not returning to the main loop immediately -
         // instead we risk to spend some (long) time applying filters and the
         // window won't be repainted before we finish, so force window redraw
         // right now
         wxFrame *frame = mApplication->TopLevelFrame();
         if ( frame )
         {
            frame->Update();
         }
      }
   }

   // get the current HeaderInfo object
   HeaderInfo *GetCurrent() const { return m_headers[m_current - 1]; }

   // get the number of messages retrieved so far
   size_t GetRetrievedCount() const { return m_nRetrieved; }

   // go to the next message
   void Next()
   {
      m_nRetrieved++;

      m_current = m_seq.GetNext(m_current, m_seqCookie);
   }

   // return true if we retrieved all the messages
   bool Done() const
   {
      ASSERT_MSG( m_nRetrieved <= m_nTotal, "too many messages" );

      return m_nRetrieved == m_nTotal;
   }

   // tell us to use the progress dialog
   void SetProgressDialog(const String& name)
   {
      ASSERT_MSG( !m_progdlg, "SetProgressDialog() can only be called once" );

      MGuiLocker locker;

      m_msgProgress.Printf(_("Retrieving %lu message headers..."), m_nTotal);

      m_progdlg = new MProgressDialog(name, m_msgProgress + "\n\n", m_nTotal,
                                      NULL, false, true);
   }

   // get the progress dialog we use (may be NULL)
   MProgressDialog *GetProgressDialog() const { return m_progdlg; }

   // update the progress dialog and return false if it was cancelled
   bool UpdateProgress(const HeaderInfo& entry) const
   {
      if ( m_progdlg )
      {
         MGuiLocker locker;

         // construct the label
         String label;
         label << m_msgProgress << '\n'
               << _("From: ") << entry.GetFrom() << '\n'
               << _("Subject: ") << entry.GetSubject();

         if ( !m_progdlg->Update(m_nRetrieved, label) )
         {
            // cancelled by user
            return false;
         }
      }

      return true;
   }

private:
   // the total number of messages we are going to retrieve
   size_t m_nTotal;

   // the number of headers retrieved so far, the final value of this is the
   // GetHeaderInfo() return value
   size_t m_nRetrieved;

   // the current msgno in the sequence
   UIdType m_current;

   // the sequence cookie used for iterating over it
   size_t m_seqCookie;

   // the sequence we're overviewing
   const Sequence& m_seq;

   // the array where we read the info
   ArrayHeaderInfo& m_headers;

   // the progress dialog if we use one, NULL otherwise
   MProgressDialog *m_progdlg;

   // the base label of the progress dialog
   String m_msgProgress;
};

// ----------------------------------------------------------------------------
// Various locker classes
// ----------------------------------------------------------------------------

// lock the folder in ctor, unlock in dtor
class MailFolderLocker
{
public:
   MailFolderLocker(MailFolderCC *mf) : m_mf(mf) { m_mf->Lock(); }
   ~MailFolderLocker() { m_mf->UnLock(); }

   operator bool() const { return m_mf->IsLocked(); }

private:
   MailFolderCC *m_mf;
};

// temporarily disable (most) cclient callbacks
class CCCallbackDisabler
{
public:
   CCCallbackDisabler()
   {
      m_old = mm_disable_callbacks;
      mm_disable_callbacks = true;
   }

   ~CCCallbackDisabler() { mm_disable_callbacks = m_old; }

private:
   bool m_old;
};

// temporarily disable error messages from cclient
class CCErrorDisabler
{
public:
   CCErrorDisabler()
   {
      m_old = mm_ignore_errors;
      mm_ignore_errors = true;
   }

   ~CCErrorDisabler() { mm_ignore_errors = m_old; }

private:
   bool m_old;
};

// temporarily disable callbacks and errors from cclient
class CCAllDisabler : public CCCallbackDisabler, public CCErrorDisabler
{
};

// temporarily change cclient log level for all messages
class CCLogLevelSetter
{
public:
   CCLogLevelSetter(int level)
   {
      m_level = cc_loglevel;
      cc_loglevel = level;
   }

   ~CCLogLevelSetter() { cc_loglevel = m_level; }

private:
   int m_level;
};

// temporarily set the given as the default folder to use for cclient
// callbacks
class CCDefaultFolder
{
public:
   CCDefaultFolder(MailFolderCC *mf)
   {
      m_mfOld = MailFolderCC::ms_StreamListDefaultObj;
      MailFolderCC::ms_StreamListDefaultObj = mf;
   }

   ~CCDefaultFolder() { MailFolderCC::ms_StreamListDefaultObj = m_mfOld; }

private:
   MailFolderCC *m_mfOld;
};

// temporarily pause a global timer
class MTimerSuspender
{
public:
   MTimerSuspender(MAppBase::Timer timer)
   {
      m_restart = mApplication->StopTimer(m_timer = timer);
   }

   ~MTimerSuspender()
   {
      if ( m_restart )
      {
         if ( !mApplication->StartTimer(m_timer) )
         {
            wxFAIL_MSG("Failed to restart suspended timer");
         }
      }
   }

private:
   MAppBase::Timer m_timer;
   bool m_restart;
};

// ----------------------------------------------------------------------------
// redirector classes: temporarily redirect c-client callbacks
// ----------------------------------------------------------------------------

// temporarily redirect mm_list callbacks to the given function
class MMListRedirector
{
public:
   MMListRedirector(mm_list_handler handler)
   {
      // this would be a mess and mm_list() calls would surely go to a wrong
      // handler!
      ASSERT_MSG( !gs_mmListRedirect, "not supposed to be used recursively" );

      gs_mmListRedirect = handler;
   }

   ~MMListRedirector()
   {
      gs_mmListRedirect = NULL;
   }
};

// temporarily redirect mm_status callbacks and only remember the info they
// provide in the given MAILSTATUS object instead of performing the usual
// processing
class MMStatusRedirector
{
public:
   MMStatusRedirector(const String& mailbox, MAILSTATUS *mailstatus)
   {
      ms_mailstatus = mailstatus;
      memset(ms_mailstatus, 0, sizeof(*ms_mailstatus));

      if ( !CanonicalizeMailbox(mailbox, &ms_mbx, &ms_filename) )
      {
         FAIL_MSG( "c-client failed to parse our spec?" );
      }

      gs_mmStatusRedirect = MMStatusRedirectorHandler;
   }

   ~MMStatusRedirector()
   {
      gs_mmStatusRedirect = NULL;

      ms_mailstatus = NULL;
   }

private:
   // we have either the filename for the local folders or the parsed struct
   // for the remote ones
   static String ms_filename;
   static NETMBX ms_mbx;

   // we store the result of mm_status() here
   static MAILSTATUS *ms_mailstatus;

   // bring the folder name to the canonical form: for the filenames this means
   // making it absolute, for the remote folders - parsing it into a NETMBX
   // which can be compared with another NETMBX later
   //
   // returns false if we failed to parse the mailbox spec
   static bool CanonicalizeMailbox(const char *mailbox,
                                   NETMBX *mbx,
                                   String *filename)
   {
      CHECK( mailbox && mbx && filename, false,
             "CanonicalizeMailbox: NULL parameter" );

      if ( *mailbox == '{' )
      {
         // remote spec
         if ( !mail_valid_net_parse((char *)mailbox, mbx) )
         {
            return false;
         }
      }
      else // local file
      {
         *filename = strutil_expandfoldername(mailbox);
      }

      return true;
   }

   static void MMStatusRedirectorHandler(MAILSTREAM * /* stream */,
                                         const char *name,
                                         MAILSTATUS *mailstatus)
   {
      // we can get the status events from other mailboxes, ignore them
      //
      // NB: note that simply comparing the names doesn't work because the
      //     name we have might not be in the canonical form while c-client's
      //     name already is
      String filename;
      NETMBX mbx;
      if ( !CanonicalizeMailbox(name, &mbx, &filename) )
      {
         FAIL_MSG( "c-client failed to parse its own spec?" );
      }
      else // parsed ok
      {
         // what have we got?
         if ( filename.empty() )
         {
            // FIXME: how to compare the hosts? many servers are multi homed so
            //        neither string nor IP address comparison works!
            //
            // the way to fix it is probably use mail_open() before
            // mail_status() and compare MAILSTREAMs instead of NETMBXs

            // remote spec
            if ( // strcmp(ms_mbx.host, mbx.host) ||
                 strcmp(ms_mbx.user, mbx.user) ||
                 strcmp(ms_mbx.mailbox, mbx.mailbox) ||
                 strcmp(ms_mbx.service, mbx.service) ||
                 (ms_mbx.port && (ms_mbx.port != mbx.port)) )
            {
               // skip the assignment below
               return;
            }
         }
         else // local file
         {
            if ( !strutil_compare_filenames(filename, ms_filename) )
            {
               // skip the assignment below
               return;
            }
         }

         memcpy(ms_mailstatus, mailstatus, sizeof(*ms_mailstatus));
      }
   }
};

String MMStatusRedirector::ms_filename;
NETMBX MMStatusRedirector::ms_mbx;
MAILSTATUS *MMStatusRedirector::ms_mailstatus = NULL;

// ----------------------------------------------------------------------------
// ReadProgressInfo: create an instance of this object to start showing progress
// info, delete it to make it disappear
// ----------------------------------------------------------------------------

#ifdef USE_READ_PROGRESS

class ReadProgressInfo
{
public:
   // totalToRead is the amount of data to be read, in byts, while secondsToWait
   // is the time we wait before popping up the dialog at all
   ReadProgressInfo(unsigned long totalToRead, int secondsToWait)
   {
      // never set m_readTotal to 0
      m_readTotal = totalToRead ? totalToRead : 1;
      m_readSoFar = 0ul;

      if ( secondsToWait )
      {
         // don't show the dialog immediately, wait a little
         m_dlgProgress = NULL;

         m_timeStart = time(NULL) + secondsToWait;
      }
      else // show the dialog from the beginning
      {
         CreateDialog();
      }
   }

   ~ReadProgressInfo()
   {
      delete m_dlgProgress;
   }

   void OnProgress(unsigned long count)
   {
      // CreateDialog() and Update() may call wxYield() and result in
      // generation of the idle events and calls to MEventManager::
      // DispatchPending() which may lead to a disaster if any event handler
      // decides to call back to c-client which is currently locked by our
      // caller
      //
      // So disable the event processing temporarily to avoid all these
      // complications
      MEventManagerSuspender suspendEvent;

      m_readSoFar = count;

      if ( !m_dlgProgress )
      {
         if ( time(NULL) > m_timeStart )
         {
            CreateDialog();
         }
         else // too early to show it yet
         {
            // skip Update() below (which would dereference NULL pointer)
            return;
         }
      }

      int percentDone = (m_readSoFar * 100) / m_readTotal;
      if ( percentDone > 100 )
      {
         // I don't know if this can happen, but if, by chance, it does, don't
         // look silly - cheat the user instead
         percentDone = 99;
      }

      m_dlgProgress->Update(percentDone);
   }

private:
   void CreateDialog()
   {
      String msg;
      msg.Printf
          (
            _("Reading %s..."),
            SizeToString
            (
               m_readTotal,
               0,
               MessageSize_AutoBytes,
               SizeToString_Verbose
            ).c_str()
          );

      m_dlgProgress = new MProgressDialog
                          (
                           _("Retrieving data from server"),
                           msg
                          );
   }

   MProgressDialog *m_dlgProgress;

   unsigned long m_readSoFar,
                 m_readTotal;

   // the moment when we started reading
   time_t m_timeStart;
} *gs_readProgressInfo = NULL;

#endif // USE_READ_PROGRESS

// ----------------------------------------------------------------------------
// LastNewUIDList: stores the UID of the last seen new message permanently,
//                 i.e. keeps it even after the folder was closed
// ----------------------------------------------------------------------------

struct LastNewUIDEntry
{
   LastNewUIDEntry(const String& folderName,
                   UIdType uidLastNew,
                   UIdType uidValidity)
      : m_folderName(folderName),
        m_uidLastNew(uidLastNew),
        m_uidValidity(uidValidity)
   {
   }

   // the full folder name in the tree
   String m_folderName;

   // the UID of the last processed new message in this folder
   UIdType m_uidLastNew;

   // the UID validity for this folder: if it changes, m_uidLastNew should be
   // discarded as well as all the UIDs in the folder could have changed
   UIdType m_uidValidity;
};

M_LIST_OWN(LastNewUIDList, LastNewUIDEntry);

static LastNewUIDList gs_lastNewUIDList;

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// various MailFolderCC statics
// ----------------------------------------------------------------------------

String MailFolderCC::ms_LastCriticalFolder = "";

// ----------------------------------------------------------------------------
// functions working with IMAP specs and flags
// ----------------------------------------------------------------------------

static String GetImapFlags(int flag)
{
   String flags;
   if(flag & MailFolder::MSG_STAT_SEEN)
      flags <<"\\SEEN ";

   // RECENT flag can't be set, it is handled entirely by server in IMAP

   if(flag & MailFolder::MSG_STAT_ANSWERED)
      flags <<"\\ANSWERED ";
   if(flag & MailFolder::MSG_STAT_DELETED)
      flags <<"\\DELETED ";
   if(flag & MailFolder::MSG_STAT_FLAGGED)
      flags <<"\\FLAGGED ";

   if(flags.Length() > 0) // chop off trailing space
     flags.RemoveLast();

   return flags;
}

// small GetImapSpec() helper
static String GetSSLOptions(int flags)
{
   String specSsl;

#ifdef USE_SSL
   if ( flags & MF_FLAGS_SSLAUTH )
   {
      specSsl << "/ssl";
      if ( flags & MF_FLAGS_SSLUNSIGNED )
         specSsl << "/novalidate-cert";
   }
#endif // USE_SSL

   return specSsl;
}

/* static */
String MailFolder::GetImapSpec(int typeOrig,
                               int flags,
                               const String &name,
                               const String &iserver,
                               const String &login)
{
   String mboxpath;

   String server = iserver;
   strutil_tolower(server);

   MFolderType type = (MFolderType)typeOrig;

#ifdef USE_SSL
   // SSL only for NNTP/IMAP/POP:
   if( (flags & MF_FLAGS_SSLAUTH) && !FolderTypeSupportsSSL(type) )
   {
      flags ^= MF_FLAGS_SSLAUTH;
      wxLogWarning(_("Ignoring SSL authentication for folder '%s'"),
                   name.c_str());
   }
#endif // USE_SSL

   if ( flags & MF_FLAGS_SSLAUTH )
   {
      extern bool InitSSL(void);

      if ( !InitSSL() )
      {
         flags ^= MF_FLAGS_SSLAUTH;
      }
   }

   switch( type )
   {
      case MF_INBOX:
         mboxpath = "INBOX";
         break;

      case MF_FILE:
         mboxpath = strutil_expandfoldername(name, type);
         break;

      case MF_MH:
         {
            // accept either absolute or relative filenames here, but the absolute
            // ones should start with MHROOT
            wxString mhRoot = MailFolderCC::InitializeMH();

            const char *p;
            wxString nameReal;
            if ( name.StartsWith(mhRoot, &nameReal) )
            {
               p = nameReal.c_str();
            }
            else
            {
               p = name.c_str();

               if ( *p == '/' )
               {
                  wxLogError(_("Invalid MH folder name '%s' not under the "
                               "root MH directory '%s'."),
                            p, mhRoot.c_str());

                  return "";
               }
            }

            // newly created MH folders won't have leading slash, but we may still
            // have to deal with the old entries in config, so eat both here, but
            // be sure to remove the leading backslash: we want #mh/inbox, not
            // #mh//inbox
            while ( *p == '/' )
               p++;

            mboxpath << "#mh/" << p;
         }
         break;

      case MF_POP:
         mboxpath << '{' << server << "/pop3" << GetSSLOptions(flags) << '}';
         break;

      case MF_IMAP:
         mboxpath << '{' << server;
         if ( flags & MF_FLAGS_ANON )
         {
            mboxpath << "/anonymous";
         }
         else
         {
            if( !login.empty() )
               mboxpath << "/user=" << login ;
            //else: we get asked  later
         }

         mboxpath << GetSSLOptions(flags) << '}' << name;
         break;

      case MF_NEWS:
         mboxpath << "#news." << name;
         break;

      case MF_NNTP:
         mboxpath << '{' << server << "/nntp"
                  << GetSSLOptions(flags) << '}' << name;
         break;

      default:
         FAIL_MSG("Unsupported folder type.");
   }

   return mboxpath;
}

static
wxString GetImapSpec(const MFolder *folder)
{
   return MailFolder::GetImapSpec(folder->GetType(),
                                  folder->GetFlags(),
                                  folder->GetPath(),
                                  folder->GetServer(),
                                  folder->GetLogin());
}

/* static */
String MailFolderCC::GetFullImapSpec(const MFolder *folder, const String& login)
{
   return MailFolder::GetImapSpec(folder->GetType(),
                                  folder->GetFlags(),
                                  folder->GetPath(),
                                  folder->GetServer(),
                                  login);
}

bool MailFolder::SpecToFolderName(const String& specification,
                                  MFolderType folderType,
                                  String *pName)
{
   CHECK( pName, FALSE, "NULL name in MailFolderCC::SpecToFolderName" );

   String& name = *pName;
   switch ( folderType )
   {
   case MF_MH:
   {
      static const int lenPrefix = 4;  // strlen("#mh/")
      if ( strncmp(specification, "#mh/", lenPrefix) != 0 )
      {
         FAIL_MSG("invalid MH folder specification - no #mh/ prefix");

         return FALSE;
      }

      // make sure that the folder name does not start with s;ash
      const char *start = specification.c_str() + lenPrefix;
      while ( wxIsPathSeparator(*start) )
      {
         start++;
      }

      name = start;
   }
   break;

   case MF_NNTP:
   case MF_NEWS:
   {
      int startIndex = wxNOT_FOUND;

      // Why this code? The spec is {hostname/nntp} not
      // {nntp/hostname}, I leave it in for now, but add correct(?)
      // code underneath:
      if ( specification[0u] == '{' )
      {
         wxString protocol(specification.c_str() + 1, 4);
         protocol.MakeLower();
         if ( protocol == "nntp" || protocol == "news" )
         {
            startIndex = specification.Find('}');
         }
         //else: leave it to be wxNOT_FOUND
      }
      if ( startIndex == wxNOT_FOUND )
      {
         wxString lowercase = specification;
         lowercase.MakeLower();
         if( lowercase.Contains("/nntp") ||
             lowercase.Contains("/news") )
         {
            startIndex = specification.Find('}');
         }
      }

      if ( startIndex == wxNOT_FOUND )
      {
         FAIL_MSG("invalid folder specification - no {nntp/...}");

         return FALSE;
      }

      name = specification.c_str() + (size_t)startIndex + 1;
   }
   break;
   case MF_IMAP:
      name = specification.AfterFirst('}');
      return TRUE;
   default:
      FAIL_MSG("not done yet");
      return FALSE;
   }

   return TRUE;
}

// This does not return driver prefixes such as #mh/ which are
// considered part of the pathname for now.
static
String GetFirstPartFromImapSpec(const String &imap)
{
   if(imap[0] != '{') return "";
   String first;
   const char *cptr = imap.c_str()+1;
   while(*cptr && *cptr != '}')
      first << *cptr++;
   first << *cptr;
   return first;
}

static
String GetPathFromImapSpec(const String &imap)
{
   if(imap[0] != '{') return imap;

   const char *cptr = imap.c_str()+1;
   while(*cptr && *cptr != '}')
      cptr++;
   if(*cptr == '}')
      return cptr+1;
   else
      return ""; // error
}

// ----------------------------------------------------------------------------
// misc helpers
// ----------------------------------------------------------------------------

// Small function to translate c-client status flags into ours.
//
// NB: not static as also used by Pop3.cpp
int GetMsgStatus(const MESSAGECACHE *elt)
{
   int status = 0;
   if (elt->recent)
      status |= MailFolder::MSG_STAT_RECENT;
   if (elt->seen)
      status |= MailFolder::MSG_STAT_SEEN;
   if (elt->flagged)
      status |= MailFolder::MSG_STAT_FLAGGED;
   if (elt->answered)
      status |= MailFolder::MSG_STAT_ANSWERED;
   if (elt->deleted)
      status |= MailFolder::MSG_STAT_DELETED;

   // TODO: what about custom/user flags?

   return status;
}

// msg status info
//
// NB: name MessageStatus is already taken by MailFolder
struct MsgStatus
{
   bool recent;
   bool unread;
   bool newmsgs;
   bool flagged;
   bool searched;
};

// is this message recent/new/unread/...?
static MsgStatus AnalyzeStatus(int stat)
{
   MsgStatus status;

   // deal with recent and new (== recent and !seen)
   status.recent = (stat & MailFolder::MSG_STAT_RECENT) != 0;
   status.unread = !(stat & MailFolder::MSG_STAT_SEEN);
   status.newmsgs = status.recent && status.unread;

   // and also count flagged and searched messages
   status.flagged = (stat & MailFolder::MSG_STAT_FLAGGED) != 0;
   status.searched = (stat & MailFolder::MSG_STAT_SEARCHED ) != 0;

   return status;
}

// ----------------------------------------------------------------------------
// header decoding
// ----------------------------------------------------------------------------

/*
   See RFC 2047 for the description of the encodings used in the mail headers.
   Briefly, "encoded words" can be inserted which have the form of

      encoded-word = "=?" charset "?" encoding "?" encoded-text "?="

   where charset and encoding can't contain space, control chars or "specials"
   and encoded-text can't contain spaces nor "?".

   NB: don't be confused by 2 meanings of encoding here: it is both the
       charset encoding for us and also QP/Base64 encoding for RFC 2047
 */
static
String DecodeHeaderOnce(const String& in, wxFontEncoding *pEncoding)
{
   // we don't enforce the sanity checks on charset and encoding - should we?
   // const char *specials = "()<>@,;:\\\"[].?=";

   wxFontEncoding encoding = wxFONTENCODING_SYSTEM;

   String out;
   out.reserve(in.length());
   for ( const char *p = in.c_str(); *p; p++ )
   {
      if ( *p == '=' && *(p + 1) == '?' )
      {
         // found encoded word

         // save the start of it
         const char *pEncWordStart = p++;

         // get the charset
         String csName;
         for ( p++; *p && *p != '?'; p++ ) // initial "++" to skip '?'
         {
            csName += *p;
         }

         if ( !*p )
         {
            wxLogDebug("Invalid encoded word syntax in '%s': missing charset.",
                       pEncWordStart);
            out += pEncWordStart;

            break;
         }

         if ( encoding == wxFONTENCODING_SYSTEM )
         {
            encoding = wxFontMapper::Get()->CharsetToEncoding(csName);
         }

         // get the encoding in RFC 2047 sense
         enum
         {
            Encoding_Unknown,
            Encoding_Base64,
            Encoding_QuotedPrintable
         } enc2047 = Encoding_Unknown;

         p++; // skip '?'
         if ( *(p + 1) == '?' )
         {
            if ( *p == 'B' || *p == 'b' )
               enc2047 = Encoding_Base64;
            else if ( *p == 'Q' || *p == 'q' )
               enc2047 = Encoding_QuotedPrintable;
         }
         //else: multi letter encoding unrecognized

         if ( enc2047 == Encoding_Unknown )
         {
            wxLogDebug("Unrecognized header encoding in '%s'.",
                       pEncWordStart);

            // scan until the end of the encoded word
            const char *pEncWordEnd = strstr(p, "?=");
            if ( !pEncWordEnd )
            {
               wxLogDebug("Missing encoded word end marker in '%s'.",
                          pEncWordStart);
               out += pEncWordStart;

               break;
            }
            else
            {
               out += String(pEncWordStart, pEncWordEnd);

               p = pEncWordEnd;

               continue;
            }
         }

         p += 2; // skip "Q?" or "B?"

         // get the encoded text
         bool hasUnderscore = false;
         const char *pEncTextStart = p;
         while ( *p && (p[0] != '?' || p[1] != '=') )
         {
            // this is needed for QP hack below
            if ( *p == '_' )
               hasUnderscore = true;

            p++;
         }

         unsigned long lenEncWord = p - pEncTextStart;

         if ( !*p )
         {
            wxLogDebug("Missing encoded word end marker in '%s'.",
                       pEncWordStart);
            out += pEncWordStart;

            continue;
         }

         // skip '=' following '?'
         p++;

         // now decode the text using cclient functions
         unsigned long len;
         unsigned char *start = (unsigned char *)pEncTextStart; // for cclient
         char *text;
         if ( enc2047 == Encoding_Base64 )
         {
            text = (char *)rfc822_base64(start, lenEncWord, &len);
         }
         else // QP
         {
            // cclient rfc822_qprint() behaves correctly and leaves '_' in the
            // QP encoded text because this is what RFC says, however many
            // broken clients replace spaces with underscores and so we undo it
            // here - better user-friendly than standard conformant
            String strWithoutUnderscores;
            if ( hasUnderscore )
            {
               strWithoutUnderscores = String(pEncTextStart, lenEncWord);
               strWithoutUnderscores.Replace("_", " ");
               start = (unsigned char *)strWithoutUnderscores.c_str();
            }

            text = (char *)rfc822_qprint(start, lenEncWord, &len);
         }

         out += String(text, (size_t)len);

         fs_give((void **)&text);
      }
      else
      {
         // just another normal char
         out += *p;
      }
   }

   if ( pEncoding )
      *pEncoding = encoding;

   return out;
}

/* static */
String MailFolder::DecodeHeader(const String& in, wxFontEncoding *pEncoding)
{
   if ( pEncoding )
      *pEncoding = wxFONTENCODING_SYSTEM;

   // some brain dead mailer encode the already encoded headers so to obtain
   // the real header we keep decoding it until it stabilizes
   String header,
          headerOrig = in;
   for ( ;; )
   {
      wxFontEncoding encoding;
      header = DecodeHeaderOnce(headerOrig, &encoding);
      if ( header == headerOrig )
         break;

      if ( pEncoding )
         *pEncoding = encoding;

      headerOrig = header;
   }

   return header;
}

// ----------------------------------------------------------------------------
// MailFolderCC auth info
// ----------------------------------------------------------------------------

// login data
String MailFolderCC::MF_user;
String MailFolderCC::MF_pwd;

/* static */
void
MailFolderCC::SetLoginData(const String &user, const String &pw)
{
   MailFolderCC::MF_user = user;
   MailFolderCC::MF_pwd = pw;
}

// ----------------------------------------------------------------------------
// MailFolderCC construction/opening
// ----------------------------------------------------------------------------

MailFolderCC::MailFolderCC(const MFolder *mfolder, wxFrame *frame)
{
   m_mfolder = (MFolder *)mfolder; // const_cast needed for IncRef()/DecRef()
   m_mfolder->IncRef();

   m_Profile = mfolder->GetProfile();

   (void)SetInteractiveFrame(frame);

   Create(mfolder->GetType(), mfolder->GetFlags());

   m_ImapSpec = ::GetImapSpec(mfolder);

   // do this at the very end as it calls RequestUpdate() and so anything may
   // happen from now on
   UpdateConfig();
}

void
MailFolderCC::Create(MFolderType type, int flags)
{
   m_MailStream = NIL;
   m_nMessages = 0;

   UpdateTimeoutValues();

   Init();

   m_ASMailFolder = NULL;

   m_SearchMessagesFound = NULL;
}

void MailFolderCC::Init()
{
   m_uidLast =
   m_uidLastNew =
   m_uidValidity = UID_ILLEGAL;

   // maybe we had stored the UID of the last new message for this folder?
   for ( LastNewUIDList::iterator i = gs_lastNewUIDList.begin();
         i != gs_lastNewUIDList.end();
         ++i )
   {
      if ( i->m_folderName == GetName() )
      {
         m_uidLastNew = i->m_uidLastNew;
         m_uidValidity = i->m_uidValidity;

         break;
      }
   }

   m_expungedMsgnos =
   m_expungedPositions =
   m_statusChangedMsgnos =
   m_statusChangedOld =
   m_statusChangedNew = NULL;

   m_gotUnprocessedNewMail =
   m_InCritical = false;

   m_chDelimiter = ILLEGAL_DELIMITER;
}

MailFolderCC::~MailFolderCC()
{
   // we don't want to call Close() if we had been closed by outside code
   // before (this happens if the user chooses "close folder" manually)
   if ( m_MailStream )
   {
      Close();
   }

   // check that our temporary data isn't hanging around
   if ( m_SearchMessagesFound )
   {
      FAIL_MSG( "m_SearchMessagesFound unexpectedly != NULL" );

      delete m_SearchMessagesFound;
   }

   // normally this one should be deleted as well but POP3 server never sends
   // us mm_expunged() (well, because it never expunges the messages until we
   // close the folder but by this time the notifications are blocked) so in
   // this case it may be left lying around
   if ( m_expungedMsgnos )
   {
      FAIL_MSG( "m_expungedMsgnos unexpectedly != NULL" );

      delete m_expungedMsgnos;
      delete m_expungedPositions;
   }

   // these arrays must have been cleared by OnMsgStatusChanged() earlier
   if ( m_statusChangedMsgnos )
   {
      FAIL_MSG( "m_statusChangedMsgnos unexpectedly != NULL" );

      delete m_statusChangedMsgnos;
      delete m_statusChangedOld;
      delete m_statusChangedNew;
   }

   m_Profile->DecRef();
   m_mfolder->DecRef();
}

// ----------------------------------------------------------------------------
// MailFolderCC: Open() and related functions
// ----------------------------------------------------------------------------

/* static */
bool MailFolderCC::CreateIfNeeded(const MFolder *folder, MAILSTREAM **pStream)
{
   if ( pStream )
      *pStream = NULL;

   Profile_obj profile(folder->GetProfile());

   if ( !profile->readEntryFromHere(MP_FOLDER_TRY_CREATE, false) )
   {
      // don't even try
      return true;
   }

   String imapspec = ::GetImapSpec(folder);

   String login, password;
   if ( !GetAuthInfoForFolder(folder, login, password) )
   {
      // can't continue without login/password
      return false;
   }

   if ( !login.empty() )
   {
      SetLoginData(login, password);
   }

   // disable callbacks if we don't have the folder to redirect them to: assume
   // that we do have the folder if we want to get the stream back
   CCAllDisabler *noCallbacks = pStream ? NULL : new CCAllDisabler;

   MAILSTREAM *stream;

   // try to open the mailbox - maybe it already exists?
   //
   // disable the errors during the first try as it's normal for it to not
   // exist
   {
      wxLogTrace(TRACE_MF_CALLS, "Trying to open MailFolderCC '%s' first.",
                 imapspec.c_str());

      CCErrorDisabler noErrs;
      stream = MailOpen(imapspec);
   }

   // login data was reset by mm_login() called from mail_open(), set it once
   // again
   if ( !login.empty() )
   {
      SetLoginData(login, password);
   }

   // if failed, try to create the mailbox
   //
   // note that for IMAP folders, c-client returns a non NULL but not opened
   // stream to allow us to reuse the connection here
   if ( !stream || stream->halfopen )
   {
      wxLogTrace(TRACE_MF_CALLS, "Creating MailFolderCC '%s'.",
                 imapspec.c_str());

      // stream may be NIL or not here
      mail_create(stream, (char *)imapspec.c_str());

      // restore the auth info once again
      if ( !login.empty() )
      {
         SetLoginData(login, password);
      }

      // and try to open it again now
      wxLogTrace(TRACE_MF_CALLS, "Opening MailFolderCC '%s' after creating it.",
                 imapspec.c_str());

      stream = MailOpen(stream, imapspec);
   }

   if ( stream )
   {
      if ( stream->halfopen )
      {
         // still failed
         mail_close(stream);
         stream = NULL;
      }
      else // successfully opened
      {
         // either keep the stream for the caller or close it if we don't need
         // it any more
         if ( pStream )
            *pStream = stream;
         else
            mail_close(stream);
      }
   }

   // don't try any more, whatever the result was
   profile->DeleteEntry(MP_FOLDER_TRY_CREATE);

   // restore callbacks
   delete noCallbacks;

   return stream != NULL;
}

/* static */
MailFolder *
MailFolderCC::OpenFolder(const MFolder *folder,
                         const String& login,
                         const String& password,
                         OpenMode openmode,
                         wxFrame *frame)
{
   // create the new mail folder for it and init its members
   MailFolderCC *mf = new MailFolderCC(folder, frame);

   // use the provided auth info, if any
   if ( !login.empty() )
   {
      mf->m_login = login;
      mf->m_password = password;
   }

   // try to really open the folder

   // reset the last error
   mApplication->SetLastError(M_ERROR_OK);

   if ( mf->Open(openmode) )
   {
      // ok!
      return mf;
   }

   // an error occured, set the last error code
   if ( !mApplication->GetLastError() )
   {
      // catch all error code
      mApplication->SetLastError(M_ERROR_CCLIENT);
   }

   mf->DecRef();

   return NULL;
}

void MailFolderCC::CreateFileFolder()
{
   MFolderType folderType = GetType();

   CHECK_RET( folderType == MF_FILE || folderType == MF_MH,
              "shouldn't be called for non file folders" );

   bool exists;
   if ( folderType == MF_FILE )
   {
      exists = wxFileExists(m_ImapSpec);
   }
   else // if ( folderType == MF_MH )
   {
      // construct the filename from MH folder name
      String path;
      path << InitializeMH()
           << m_ImapSpec.c_str() + 4; // 4 == strlen("#mh/")
      exists = wxDirExists(path);
   }

   // if the file folder doesn't exist, we should create it first
   if ( !exists )
   {
      // This little hack makes it root (uid 0) safe and allows us
      // to choose the file format, too:
      String tmp;
      if ( folderType == MF_FILE )
      {
         static const char *cclient_drivers[] =
         {
            "mbx",
            "unix",
            "mmdf",
            "tenex"
         };

         ASSERT_MSG( WXSIZEOF(cclient_drivers) == FileMbox_Max,
                     "forgot to update something" );

         long format = READ_CONFIG(m_Profile, MP_FOLDER_FILE_DRIVER);
         if ( format < 0  || (size_t)format > FileMbox_Max )
         {
            FAIL_MSG( "invalid mailbox format" );
            format = 0;
         }

         tmp = "#driver.";
         tmp << cclient_drivers[format] << '/';

         wxLogDebug("Trying to create folder '%s' in %s format.",
                    GetName().c_str(), cclient_drivers[format]);
      }
      else // MF_MH folder
      {
         // the only one still not handled
         ASSERT_MSG( folderType == MF_MH, "unexpected folder type" );

         tmp = "#driver.mh/";
      }

      tmp += m_ImapSpec;
      mail_create(NIL, (char *)tmp.c_str());
   }

   // we either already tried to create it once or it had existed even before
   Profile_obj profile(m_mfolder->GetProfile());
   if ( profile->HasEntry(MP_FOLDER_TRY_CREATE) )
   {
      profile->DeleteEntry(MP_FOLDER_TRY_CREATE);
   }
}

static String
GetOperationName(MailFolder::OpenMode openmode)
{
   String name;
   switch ( openmode )
   {
      case MailFolder::Normal:
         name = _("Opening");
         break;

      case MailFolder::ReadOnly:
         name = _("Opening in read only mode");
         break;

      case MailFolder::HalfOpen:
         name = _("Connecting to");
         break;

      default:
         FAIL_MSG( "unknown open mode" );
         name = "???";
   }

   return name;
}

bool
MailFolderCC::CheckForFileLock()
{
   // this code doesn't work under Windows anyhow because lockname() doesn't
   // use ".lock" suffix there
#ifdef OS_UNIX
   String file;

   MFolderType folderType = GetType();
   if( folderType == MF_FILE )
      file = m_ImapSpec;
#ifdef OS_UNIX
   else if ( folderType == MF_INBOX )
   {
      // get INBOX path name
      MCclientLocker lock;
      file = (char *) mail_parameters (NIL,GET_SYSINBOX,NULL);
      if(file.empty()) // another c-client stupidity
         file = (char *) sysinbox();
   }
#endif // OS_UNIX
   else
   {
      FAIL_MSG( "shouldn't be called for not file based folders!" );
      return true;
   }

   // TODO: we should be using c-client's lockname() here
   String lockfile = wxFindFirstFile(file + ".lock*", wxFILE);
   while ( !lockfile.empty() )
   {
      // outch, someone has a lock, opening the mailbox will fail as c-client
      // checks for it -- propose to the user to remove the lock first
      bool shouldRemove = MDialog_YesNoDialog
                          (
                           String::Format(
                              _("Found lock-file '%s' for the mailbox '%s'.\n"
                                "\n"
                                "Some other process may be using the folder.\n"
                                "Shall I forcefully override the lock?"),
                              lockfile.c_str(), file.c_str()
                           ),
                           NULL,
                           MDIALOG_YESNOTITLE,
                           M_DLG_YES_DEFAULT
                          );

      if ( shouldRemove )
      {
         // ask extra confirmation if the file is not empty to avoid deleting
         // the important files erroneously
         wxStructStat stBuf;
         if ( wxStat(lockfile, &stBuf) != 0 || stBuf.st_size != 0 )
         {
            shouldRemove = MDialog_YesNoDialog
                           (
                              String::Format(
                                 _("The file '%s' is not empty, still remove it?"),
                                 lockfile.c_str()),
                              NULL,
                              MDIALOG_YESNOTITLE,
                              M_DLG_NO_DEFAULT
                             );
         }
      }

      if ( shouldRemove )
      {
         // remove the file but be prepared to the fact that it could have
         // already disappeared by itself
         if ( remove(lockfile) != 0 && wxFile::Exists(lockfile) )
         {
            wxLogSysError(_("Failed to remove the lock file"));

            return false;
         }
      }
      else if ( wxFile::Exists(lockfile) )
      {
         wxLogWarning(_("Lock file is present, opening the folder might fail."));
      }

      lockfile = wxFindNextFile();
   }
#endif // OS_UNIX

   return true;
}

bool
MailFolderCC::Open(OpenMode openmode)
{
   wxLogTrace(TRACE_MF_CALLS, "%s \"'%s'\"",
              GetOperationName(openmode).c_str(), GetName().c_str());

   wxFrame *frame = GetInteractiveFrame();
   if ( frame )
   {
      STATUSMESSAGE((frame, _("%s mailbox \"%s\"..."),
                     GetOperationName(openmode).c_str(), GetName().c_str()));
   }

   // Now, we apply the very latest c-client timeout values, in case they have
   // changed.
   UpdateTimeoutValues();

   MFolderType folderType = GetType();

   // for files, check whether mailbox is locked, c-client library is
   // to dumb to handle this properly
   if ( openmode == Normal &&
        (
         folderType == MF_FILE
#ifdef OS_UNIX
         || folderType == MF_INBOX
#endif
        )
      )
   {
      if ( !CheckForFileLock() )
      {
         // folder is locked, can't open
         return false;
      }
   }
   //else: not a file based folder, no locking problems

   // get the login/password to use (if we don't have them yet)
   if ( NeedsAuthInfo() &&
        !GetAuthInfoForFolder(m_mfolder, m_login, m_password) )
   {
      // can't open without login/password
      return false;
   }

   if ( HasLogin() )
   {
      SetLoginData(m_login, m_password);
   }

   // we need to dispatch the pending MEventFolderExpungeData events because
   // they may keep the other folder(s) on the server we're going to open
   // artificially opened thus preventing us from reusing the connection to it
   // and forcing to open a new one even if the old one is going to be closed
   // during the next main loop iteration
   //
   // ideally we should just dispatch MEventWithFolderData events, not all of
   // the pending ones as this might cause problems but for now let's keep it
   // simple and fix it when/if we find the problems with the current approach
   MEventManager::DispatchPending();

   // lock cclient inside this block
   {
      MCclientLocker lock;

      // Make sure that all events to unknown folder go to us
      CCDefaultFolder def(this);

      long ccOptions = 0;
      bool tryOpen = true;
      switch ( openmode )
      {
         case Normal:
            // create the mailbox if it doesn't exist yet: this must be done
            // separately for the file folders to specify the format we want
            // this mailbox to be in
            if ( folderType == MF_FILE || folderType == MF_MH )
            {
               CreateFileFolder();
            }
            else // and for all the others
            {
               // check if this is the first time we're opening this folder: in
               // this case, try creating it first
               if ( !CreateIfNeeded(m_mfolder, &m_MailStream) )
               {
                  // CreateIfNeeded() returns false only if the folder couldn't
                  // be opened at all, no need to retry again
                  tryOpen = false;
               }
            }
            break;

         case ReadOnly:
            ccOptions |= OP_READONLY;
            break;

         case HalfOpen:
            ccOptions |= OP_HALFOPEN;
            break;

         default:
            FAIL_MSG( "unknown open mode" );
            break;
      }

      // the stream could have been already opened by CreateIfNeeded(), but if
      // it wasn't, open it here
      if ( !m_MailStream && tryOpen )
      {
         // try to reuse an existing stream if possible
         MAILSTREAM *stream = NIL;
         if ( IsReusableFolder(m_mfolder) )
         {
            ServerInfoEntryCC *server = ServerInfoEntryCC::Get(m_mfolder);
            if ( server )
            {
               stream = server->GetStream();
            }
         }

         wxLogTrace(TRACE_MF_CALLS, "Opening MailFolderCC '%s'.",
                    m_ImapSpec.c_str());

         m_MailStream = MailOpen(stream, m_ImapSpec, ccOptions);
      }
   } // end of cclient lock block

   // for some folders (notably the IMAP ones), mail_open() will return a not
   // NULL pointer but set halfopen flag if it couldn't be SELECTed - as we
   // really want to open it here and not halfopen, this is an error for us
   if ( m_MailStream && m_MailStream->halfopen && openmode != HalfOpen )
   {
      // unfortunately, at least the IMAP folder will have halfopen flag
      // set even in some situations when the folder can't be opened at all -
      // for example, if the INBOX is locked by another session (yes, this
      // goes against the IMAP RFC, but some servers just break the connection
      // if the mailbox is locked by another session...), so we need to try to
      // half open it now to know if this is just a folder with NOSELECT flag
      // or if we really can't open it

      // reuse the same login/password again
      if ( HasLogin() )
      {
         SetLoginData(m_login, m_password);
      }

      wxLogTrace(TRACE_MF_CALLS, "Half opening MailFolderCC '%s'.",
                 m_ImapSpec.c_str());

      // redirect all notifications to us again
      CCDefaultFolder def(this);
      MAILSTREAM *
         msHalfOpened = MailOpen(m_MailStream, m_ImapSpec, OP_HALFOPEN);
      if ( msHalfOpened )
      {
         mail_close(msHalfOpened);

         MFolder_obj folder(m_Profile);
         if ( folder )
         {
            // remember that we can't open it
            folder->AddFlags(MF_FLAGS_NOSELECT);
         }
         else
         {
            // were we deleted without noticing?
            FAIL_MSG( "associated folder somehow disappeared?" );
         }

         mApplication->SetLastError(M_ERROR_HALFOPENED_ONLY);
      }
      //else: it can't be opened at all

      m_MailStream = NIL;
   }

   // so did we eventually succeed in opening it or not?
   if ( m_MailStream == NIL )
   {
      // can we give the user a hint as to why did it fail?
      GetLogCircle().GuessError();

      // give the general error message anyhow
      wxLogError(_("Could not open mailbox '%s'."), GetName().c_str());

      return false;
   }

   // folder really opened!

   // we have to invalidate m_uidLastNew if the UID validity changed
   //
   // NB: this happens each time we open the folder for the types which
   //     don't support the real UIDs (i.e. POP3)
   if ( m_uidLastNew != UID_ILLEGAL )
   {
      ASSERT_MSG( m_uidValidity != UID_ILLEGAL,
                  "UID validity must be set if we deal with UIDs!" );

      if ( m_MailStream->uid_validity != m_uidValidity )
      {
         m_uidLastNew = UID_ILLEGAL;
      }
   }

   // and update UID validity for the next time
   m_uidValidity = m_MailStream->uid_validity;

   // update the flags for POP3 (which doesn't keep them itself) from our cache
   if ( GetType() == MF_POP )
   {
      Pop3_RestoreFlags(GetName(), m_MailStream);
   }

   if ( frame )
   {
      String msg;
      switch ( openmode )
      {
         case Normal:
            msg = _("Mailbox \"%s\" opened.");
            break;

         case ReadOnly:
            msg = _("Mailbox \"%s\" opened in read only mode.");
            break;

         case HalfOpen:
            msg = _("Successfully connected to \"%s\"");
            break;

         default:
            FAIL_MSG( "unknown open mode" );
            break;
      }

      if ( !msg.empty() )
      {
         STATUSMESSAGE((frame, msg, GetName().c_str()));
      }
   }

   PY_CALLBACK(MCB_FOLDEROPEN, 0, GetProfile());

   return true;   // success
}

// ----------------------------------------------------------------------------
// MailFolderCC closing
// ----------------------------------------------------------------------------

void MailFolderCC::DiscardExpungeData()
{
   if ( m_expungedMsgnos )
   {
      // harmless but not supposed to happen
      ASSERT_MSG( m_expungedPositions, "can't be NULL if m_expungedMsgnos!" );

      delete m_expungedMsgnos;
      delete m_expungedPositions;

      m_expungedMsgnos = NULL;
      m_expungedPositions = NULL;
   }
}

void
MailFolderCC::Close()
{
   wxLogTrace(TRACE_MF_CALLS, "Closing folder '%s'", GetName().c_str());

   MailFolderCmn::Close();

   if ( m_MailStream )
   {
      /*
         DO NOT SEND EVENTS FROM HERE, ITS CALLED FROM THE DESTRUCTOR AND
         THE OBJECT *WILL* DISAPPEAR!
       */
      CCAllDisabler no;

      if ( GetType() == MF_POP )
      {
         Pop3_SaveFlags(GetName(), m_MailStream);
      }

#ifdef USE_DIALUP
      if ( NeedsNetwork() && !mApplication->IsOnline() )
      {
         // a remote folder but we're not connected: delay closing as we can't
         // do it properly right now
         gs_CCStreamCleaner->Add(m_MailStream);
      }
      else
#endif // USE_DIALUP
      {
         // it is wrong to do this as it may result in mm_exists() callbacks
         // which we ignore (per above), so we miss new mail
#if 0
         if ( !m_MailStream->halfopen )
         {
            // update flags, etc, .newsrc
            mail_check(m_MailStream);
         }
#endif // 0

         // don't close the stream immediately, reuse it later
         CloseOrKeepStream(m_MailStream, m_mfolder);
      }

      m_MailStream = NIL;
   }

   // we could be closing before we had time to process all events
   //
   // FIXME: is this really true, i.e. does it ever happen?
   DiscardExpungeData();

   if ( m_statusChangedMsgnos )
   {
      delete m_statusChangedMsgnos;
      delete m_statusChangedOld;
      delete m_statusChangedNew;

      m_statusChangedMsgnos =
      m_statusChangedOld =
      m_statusChangedNew = NULL;
   }

   // normally the folder won't be reused any more but reset them just in case
   m_uidLast = UID_ILLEGAL;
   m_nMessages = 0;

   // save our last new UID in case this folder is going to be reopened later
   String folderName = GetName();

   LastNewUIDList::iterator i;
   for ( i = gs_lastNewUIDList.begin(); i != gs_lastNewUIDList.end(); ++i )
   {
      if ( i->m_folderName == folderName )
      {
         i->m_uidLastNew = m_uidLastNew;
         i->m_uidValidity = m_uidValidity;

         break;
      }
   }

   if ( i == gs_lastNewUIDList.end() )
   {
      // note that the lists take ownership of the entry
      gs_lastNewUIDList.push_front(new LastNewUIDEntry
                                       (
                                          folderName,
                                          m_uidLastNew,
                                          m_uidValidity
                                       ));
   }
}

void
MailFolderCC::ForceClose()
{
   // now folder is dead
   m_MailStream = NIL;

   Close();

   // notify any opened folder views
   MEventManager::Send(new MEventFolderClosedData(this));
}

// ----------------------------------------------------------------------------
// MailFolderCC timeouts
// ----------------------------------------------------------------------------

void
MailFolderCC::ApplyTimeoutValues(void)
{
   (void) mail_parameters(NIL, SET_OPENTIMEOUT, (void *) m_TcpOpenTimeout);
   (void) mail_parameters(NIL, SET_READTIMEOUT, (void *) m_TcpReadTimeout);
   (void) mail_parameters(NIL, SET_WRITETIMEOUT, (void *) m_TcpWriteTimeout);
   (void) mail_parameters(NIL, SET_CLOSETIMEOUT, (void *) m_TcpCloseTimeout);
   (void) mail_parameters(NIL, SET_RSHTIMEOUT, (void *) m_TcpRshTimeout);
   (void) mail_parameters(NIL, SET_SSHTIMEOUT, (void *) m_TcpSshTimeout);

   (void) mail_parameters(NIL, SET_LOOKAHEAD, (void *) m_LookAhead);

   // only set the paths if we do use rsh/ssh
   if ( m_TcpRshTimeout )
      (void) mail_parameters(NIL, SET_RSHPATH, (char *)m_RshPath.c_str());
   if ( m_TcpSshTimeout )
      (void) mail_parameters(NIL, SET_SSHPATH, (char *)m_SshPath.c_str());
}



void
MailFolderCC::UpdateTimeoutValues(void)
{
   Profile *p = GetProfile();

   // We now use only one common config setting for all TCP timeouts
   m_TcpOpenTimeout = READ_CONFIG(p, MP_TCP_OPENTIMEOUT);
   m_TcpReadTimeout = READ_CONFIG(p, MP_TCP_READTIMEOUT);
   m_TcpWriteTimeout = READ_CONFIG(p, MP_TCP_WRITETIMEOUT);
   m_TcpCloseTimeout = READ_CONFIG(p, MP_TCP_CLOSETIMEOUT);
   m_LookAhead = READ_CONFIG(p, MP_IMAP_LOOKAHEAD);

   // but a separate one for rsh timeout to allow enabling/disabling rsh
   // independently of TCP timeout
   m_TcpRshTimeout = READ_CONFIG(p, MP_TCP_RSHTIMEOUT);

   // and another one for SSH
   m_TcpSshTimeout = READ_CONFIG(p, MP_TCP_SSHTIMEOUT);

   // also read the paths for the commands if we use them
   if ( m_TcpRshTimeout )
      m_RshPath = READ_CONFIG_TEXT(p, MP_RSH_PATH);
   if ( m_TcpSshTimeout )
      m_SshPath = READ_CONFIG_TEXT(p, MP_SSH_PATH);

   ApplyTimeoutValues();
}

// ----------------------------------------------------------------------------
// Working with the list of all open mailboxes
// ----------------------------------------------------------------------------

bool MailFolderCC::ms_CClientInitialisedFlag = false;

MailFolderCC *MailFolderCC::ms_StreamListDefaultObj = NULL;

// this function should normally always return a non NULL folder
/* static */
MailFolderCC *
MailFolderCC::LookupObject(const MAILSTREAM *stream)
{
   // try to match the stream
   String driverName;
   MFPool::Cookie cookie;
   for ( MailFolder *mf = MFPool::GetFirst(cookie, &driverName);
         mf;
         mf = MFPool::GetNext(cookie, &driverName) )
   {
      mf->DecRef();

      if ( driverName != CCLIENT_DRIVER_NAME )
         continue;

      // the cast is valid because of the check above
      MailFolderCC *mfCC = ((MailFolderCC *)mf);
      if ( mfCC->Stream() == stream )
      {
         return mfCC;
      }
   }

   if ( ms_StreamListDefaultObj )
   {
      return ms_StreamListDefaultObj;
   }

   // unfortunately, c-client seems to allocate the temp private streams fairly
   // liberally and it doesn't seem possible to detect here all cases when it
   // does it (some examples: from mail_status() for some folders, from
   // mail_open() for the file based one, ...) so don't always assert here as
   // we used to do
   //
   // NB: gs_mmStatusRedirect is !NULL only while we're inside mail_status()
#if 0
   ASSERT_MSG( gs_mmStatusRedirect,
               "No mailfolder for c-client callback?" );
#endif

   return NULL;
}

/* static */
MailFolderCC *
MailFolderCC::FindFolder(const MFolder* folder)
{
   // FIXME: using GetLogin() here is wrong for the folders which don't
   //        store the login in config
   return
       (MailFolderCC *)MFPool::Find(&gs_driverCC, folder, folder->GetLogin());
}

// ----------------------------------------------------------------------------
// MailFolderCC options
// ----------------------------------------------------------------------------

void MailFolderCC::ReadConfig(MailFolderCmn::MFCmnOptions& config)
{
   // the command line option overrides the config value
   mm_show_debug = mApplication->IsMailDebuggingEnabled()
                     ? true
                     : READ_APPCONFIG_BOOL(MP_DEBUG_CCLIENT);

   MailFolderCmn::ReadConfig(config);
}

// ----------------------------------------------------------------------------
// MailFolderCC pinging
// ----------------------------------------------------------------------------

void
MailFolderCC::Checkpoint(void)
{
   if ( m_MailStream == NULL )
     return; // nothing we can do anymore

   // nothing to do for halfopened folders (and doing CHECK on a half opened
   // IMAP folder is a protocol error, too)
   if ( m_MailStream->halfopen )
      return;

   // an MBOX file created by wxMessageView::MimeHandle() is deleted
   // immediately afterwards, so don't try to access it from here
   if ( GetType() == MF_FILE )
   {
      if ( !wxFile::Exists(m_MailStream->mailbox) )
      {
         DBGMESSAGE(("MBOX folder '%s' already deleted.", m_MailStream->mailbox));
         return;
      }
   }

#ifdef USE_DIALUP
   if ( NeedsNetwork() && ! mApplication->IsOnline() )
   {
      ERRORMESSAGE((_("System is offline, cannot access mailbox �%s�"),
                   GetName().c_str()));
      return;
   }
#endif // USE_DIALUP

   MailFolderLocker lock(this);
   if ( lock )
   {
      wxLogTrace(TRACE_MF_CALLS, "MailFolderCC::Checkpoint() on %s.",
                 GetName().c_str());

      mail_check(m_MailStream); // update flags, etc, .newsrc
   }
}

bool
MailFolderCC::Ping(void)
{
   // we don't want to reopen the folder from here, this leads to inifinite
   // loops if the network connection goes down because we are called from a
   // timer event and so if folder pinging taks too long, we will be called
   // again and again and again ... before we get the chance to do anything
   if ( !IsOpened() )
   {
      return false;
   }

   bool rc;

   MailFolderLocker lockFolder(this);
   if ( lockFolder )
   {
       // c-client 4.7-bug: MH folders don't notice new messages without
      // reopening them: check if this is still needed with imap2000 (TODO)
      if ( GetType() == MF_POP || GetType() == MF_MH )
      {
         wxLogTrace(TRACE_MF_CALLS,
                    "MailFolderCC::Ping() forcing close on folder %s.",
                    GetName().c_str());

         Close();
      }

#ifdef USE_DIALUP
      if ( NeedsNetwork() && !mApplication->IsOnline() &&
           !MDialog_YesNoDialog
           (
            wxString::Format
            (
               _("Dial-Up network is down.\n"
                 "Do you want to try to check folder '%s' anyway?"),
               GetName().c_str()
            ),
            NULL,
            MDIALOG_YESNOTITLE,
            M_DLG_NO_DEFAULT,
            M_MSGBOX_NO_NET_PING_ANYWAY,
            GetName()
           )
        )
      {
         ForceClose();

         return false;
      }
#endif // USE_DIALUP

      // try to reopen the folder if it is closed
      if ( !m_MailStream )
      {
         rc = Open();
      }

      if ( m_MailStream )
      {
         rc = PingOpenedFolder();
      }
      else
      {
         rc = false;
      }
   }
   else // failed to lock folder?
   {
      rc = false;
   }

   return rc;
}

bool
MailFolderCC::PingOpenedFolder()
{
   // caller must check for this
   CHECK( m_MailStream, false, "PingOpenedFolder() called for closed folder" );

   wxLogTrace(TRACE_MF_CALLS, "MailFolderCC::Ping(%s)", GetName().c_str());

   return mail_ping(m_MailStream) != NIL;
}

/* static */
bool
MailFolderCC::DoCheckStatus(const MFolder *folder, MAILSTATUS *mailstatus)
{
   static const int STATUS_FLAGS = SA_MESSAGES | SA_RECENT | SA_UNSEEN;

   wxBusyCursor busyCursor;

   // instead of calling mail_status() with NIL stream we always open the
   // connection to the folder manually before as this gives us a
   // possibility to reuse it for the subsequent operations: this is
   // especially important when the user chooses to update the status of the
   // entire subtree as then CheckStatus() is called many times in quick
   // succession and opening a new connection each time is very inefficient

   // reuse an existing connection to the server if we have one
   ServerInfoEntryCC *server;
   if ( IsReusableFolder(folder) )
   {
      // look for the existing entry for this server
      server = ServerInfoEntryCC::GetOrCreate(folder);

      if ( !server )
      {
         return false;
      }
   }
   else
   {
      server = NULL;
   }

   MAILSTREAM *stream;
   if ( server )
   {
      stream = server->GetStream();
   }
   else // no connection to reuse
   {
      stream = NULL;
   }

   // find out the login and password we need: first see if we don't already
   // have them
   String login, password;
   bool hasAuthInfo = server && server->GetAuthInfo(login, password);
   if ( !hasAuthInfo )
   {
      login = folder->GetLogin();
      password = folder->GetPassword();

      if ( !GetAuthInfoForFolder(folder, login, password ) )
      {
         return false;
      }
   }

   SetLoginData(login, password);

   // preopen the stream if it may be reused, as explained above
   String spec = MailFolder::GetImapSpec(folder->GetType(),
                                         folder->GetFlags(),
                                         folder->GetPath(),
                                         folder->GetServer(),
                                         login);

   if ( server && !stream )
   {
      // we're not interested in mm_exists() and what not
      CCCallbackDisabler noCallbacks;

      stream = MailOpen(spec, OP_HALFOPEN | OP_READONLY);
      if ( !stream )
      {
         // if we failed to open it, checking its status won't work neither
         return false;
      }
   }

   // finally call mail_status()
   MMStatusRedirector statusRedir(spec, mailstatus);

   wxLogTrace(TRACE_MF_CALLS, "MailFolderCC::CheckStatus() on %s.",
              spec.c_str());

   mail_status(stream, (char *)spec.c_str(), STATUS_FLAGS);

   // keep the stream alive to be reused in the next call, if any
   if ( server )
   {
      server->KeepStream(stream, folder);

      // and also remember the auth params if we hadn't had them before
      if ( !hasAuthInfo )
      {
         server->SetAuthInfo(login, password);
      }
   }

   // we succeed only if we managed to get the values of all flags included in
   // STATUS_FLAGS
   return (mailstatus->flags & STATUS_FLAGS) == STATUS_FLAGS;
}

/* static */
bool MailFolderCC::CheckStatus(const MFolder *folder)
{
   CHECK( folder, false, "MailFolderCC::CheckStatus(): NULL folder" );

   // doing mail_status() for POP3 is a bad idea for several reasons:
   //
   // 1. first, our code in LookupObject() asserts because c-client allocs
   //    a temp stream for POP3 folder and sends us mm_notify() when it closes
   //    it and we panic
   //
   // 2. second, mail_status() thinks all messages on the POP3 server are
   //    always unread while we know better because we cache their status
   //
   // 3. it's not more efficient to do mail_status() than mail_open() in this
   //    case because this is what mail_status() does anyhow
   if ( folder->GetType() == MF_POP )
   {
      MailFolder *mf = MailFolder::OpenFolder(folder);
      if ( !mf )
         return false;

      mf->DecRef();

      return true;
   }

   // remember the old status of the folder
   MfStatusCache *mfStatusCache = MfStatusCache::Get();
   MailFolderStatus status;
   (void)mfStatusCache->GetStatus(folder->GetFullName(), &status);

   // and check for the new one
   MAILSTATUS mailstatus;
   if ( !DoCheckStatus(folder, &mailstatus) )
   {
      ERRORMESSAGE(( _("Failed to check status of the folder '%s'"),
                     folder->GetFullName().c_str() ));
      return false;
   }

   // has anything changed?
   if ( mailstatus.messages != status.total ||
        mailstatus.recent != status.recent ||
        mailstatus.unseen != status.unread )
   {
      // do we have any new mail? there is no way to [efficiently] test for new
      // messages but assume that if there are unread and recent ones, then
      // there are new ones as well - and also take this as the guess for their
      // number
      MsgnoType newmsgs = mailstatus.recent < mailstatus.unseen
                                 ? mailstatus.recent
                                 : mailstatus.unseen;

      if ( newmsgs )
      {
         ProcessNewMail(folder, newmsgs);
      }

      // update the status shown in the tree anyhow
      status.total = mailstatus.messages;
      status.recent = mailstatus.recent;
      status.unread = mailstatus.unseen;

      mfStatusCache->UpdateStatus(folder->GetFullName(), status);
   }

   return true;
}

// ----------------------------------------------------------------------------
// MailFolderCC locking
// ----------------------------------------------------------------------------

bool
MailFolderCC::Lock(void) const
{
   ((MailFolderCC *)this)->m_mutexExclLock.Lock();

   return true;
}

bool
MailFolderCC::IsLocked(void) const
{
   return m_mutexExclLock.IsLocked() || m_mutexNewMail.IsLocked();
}

void
MailFolderCC::UnLock(void) const
{
   ((MailFolderCC *)this)->m_mutexExclLock.Unlock();
}

// ----------------------------------------------------------------------------
// Adding and removing (expunging) messages
// ----------------------------------------------------------------------------

// this is the common part of both AppendMessage() functions
void
MailFolderCC::UpdateAfterAppend()
{
   CHECK_RET( IsOpened(), "how did we manage to append to a closed folder?" );

   // this mail will be stored as uid_last+1 which isn't updated
   // yet at this point
   m_LastNewMsgUId = m_MailStream->uid_last + 1;

   // don't update if we don't have listing - it will be rebuilt the next time
   // it is needed
   if ( !IsUpdateSuspended() )
   {
      // make the folder notice the new messages
      PingOpenedFolder();
   }
}

bool
MailFolderCC::AppendMessage(const String& msg)
{
   wxLogTrace(TRACE_MF_CALLS, "MailFolderCC(%s)::AppendMessage(string)",
              GetName().c_str());

   CHECK_DEAD_RC("Appending to closed folder '%s' failed.", false);

   STRING str;
   INIT(&str, mail_string, (void *) msg.c_str(), msg.Length());

   if ( !mail_append(m_MailStream, (char *)m_ImapSpec.c_str(), &str) )
   {
      wxLogError(_("Failed to save message to the folder '%s'"),
                 GetName().c_str());

      return false;
   }

   UpdateAfterAppend();

   return true;
}

bool
MailFolderCC::AppendMessage(const Message& msg)
{
   wxLogTrace(TRACE_MF_CALLS, "MailFolderCC(%s)::AppendMessage(Message)",
              GetName().c_str());

   String date;
   msg.GetHeaderLine("Date", date);

   char *dateptr = NULL;
   char datebuf[128];
   MESSAGECACHE mc;
   if ( mail_parse_date(&mc, (char *) date.c_str()) == T )
   {
     mail_date(datebuf, &mc);
     dateptr = datebuf;
   }

   // append message text to folder
   String tmp;
   if ( !msg.WriteToString(tmp) )
   {
      wxLogError(_("Failed to retrieve the message text."));

      return false;
   }

   String flags = GetImapFlags(msg.GetStatus());

   STRING str;
   INIT(&str, mail_string, (void *) tmp.c_str(), tmp.Length());

   CHECK_DEAD_RC("Appending to closed folder '%s' failed.", false);

   if ( !mail_append_full(m_MailStream,
                          (char *)m_ImapSpec.c_str(),
                          (char *)flags.c_str(),
                          dateptr,
                          &str) )
   {
      // useful to know which message exactly we failed to copy
      wxLogError(_("Message details: subject '%s', from '%s'"),
                 msg.Subject().c_str(), msg.From().c_str());

      wxLogError(_("Failed to save message to the folder '%s'"),
                 GetName().c_str());

      return false;
   }

   UpdateAfterAppend();

   return true;
}

bool
MailFolderCC::SaveMessages(const UIdArray *selections, MFolder *folder)
{
   CHECK( folder, false, "SaveMessages() needs a valid folder pointer" );
   CHECK_DEAD_RC( "SaveMessages(): folder is closed", false );

   size_t count = selections->Count();
   CHECK( count, true, "SaveMessages(): nothing to save" );

   wxLogTrace(TRACE_MF_CALLS, "MailFolderCC(%s)::SaveMessages(%s)",
              GetName().c_str(), folder->GetFullName().c_str());

   /*
      This is an optimisation: if both mailfolders are IMAP and on the same
      server, we ask the server to copy the message, which is more efficient
      (think of a modem!).

      If that is not the case, or it fails, then we fall back to the base class
      version which simply retrieves the message and appends it to the second
      folder.
   */
   bool didServerSideCopy = false;
   if ( GetType() == MF_IMAP && folder->GetType() == MF_IMAP )
   {
      // they're both IMAP but do they live on the same server?
      // check if host, protocol, user all identical:
      String specDst = ::GetImapSpec(folder);
      String serverSrc = GetFirstPartFromImapSpec(specDst);
      String serverDst = GetFirstPartFromImapSpec(GetImapSpec());

      if ( serverSrc == serverDst )
      {
         // before trying to copy messages to this folder, create it if hadn't
         // been done yet
         if ( CreateIfNeeded(folder) )
         {
            String sequence = BuildSequence(*selections);
            String pathDst = GetPathFromImapSpec(specDst);
            if ( mail_copy_full(m_MailStream,
                                (char *)sequence.c_str(),
                                (char *)pathDst.c_str(),
                                CP_UID) )
            {
               didServerSideCopy = true;
            }
            else
            {
               // don't give an error as it is not fatal and there is no way to
               // disable it, but still log it
               wxLogMessage(_("Server side copy from '%s' to '%s' failed, "
                              "trying inefficient append now."),
                            GetName().c_str(),
                            folder->GetName().c_str());
            }
         }
      }
   }

   if ( !didServerSideCopy )
   {
      // use the inefficient retrieve-append way
      return MailFolderCmn::SaveMessages(selections, folder);
   }

   String nameDst = folder->GetFullName();

   // copying to ourselves?
   if ( nameDst == GetName() )
   {
      // yes, no need to update status as it will happen anyhow
      return true;
   }

   MailFolder *mfDst = FindFolder(folder);

   // update status of the target folder
   MfStatusCache *mfStatusCache = MfStatusCache::Get();
   MailFolderStatus status;
   if ( !mfStatusCache->GetStatus(nameDst, &status) )
   {
      // if the folder is already opened, recount the number of messages in it
      // now, otherwise assume it was empty... this is, of course, false, but
      // it allows us to show the folder as having unseen/new/... messages in
      // the tree without having to open it (slow!)
      if ( !mfDst || !mfDst->CountAllMessages(&status) )
      {
         status.total = 0;
      }
   }

   UIdArray uidsNew;

   // examine flags of all messages we copied
   HeaderInfoList_obj headers = GetHeaders();
   for ( size_t n = 0; n < count; n++ )
   {
      MsgnoType msgno = GetMsgnoFromUID(selections->Item(n));
      if ( msgno == MSGNO_ILLEGAL )
      {
         FAIL_MSG("inexistent message was copied??");

         continue;
      }

      size_t idx = headers->GetIdxFromMsgno(msgno);
      HeaderInfo *hi = headers->GetItemByIndex(idx);
      if ( !hi )
      {
         FAIL_MSG( "UpdateMessageStatus: no header info for the given msgno?" );

         continue;
      }

      int flags = hi->GetStatus();

      // deleted messages don't count, except for the total number
      if ( !(flags & MailFolder::MSG_STAT_DELETED) )
      {
         // note that the messages will always be recent in the destination
         // folder!
         //
         //bool isRecent = (flags & MailFolder::MSG_STAT_RECENT) != 0;
         static const bool isRecent = true;

         if ( !(flags & MailFolder::MSG_STAT_SEEN) )
         {
            if ( isRecent )
            {
               uidsNew.Add(selections->Item(n));

               status.newmsgs++;
            }

            status.unread++;
         }
         else if ( isRecent )
         {
            status.recent++;
         }

         if ( flags & MailFolder::MSG_STAT_FLAGGED )
            status.flagged++;
         if ( flags & MailFolder::MSG_STAT_SEARCHED )
            status.searched++;
      }

      status.total++;
   }

   // always send this one to update the number of messages in the tree
   mfStatusCache->UpdateStatus(nameDst, status);

   // if the folder is opened, we must also update the display
   if ( mfDst )
   {
      mfDst->Ping();
      mfDst->DecRef();
   }
   else // dst folder not opened
   {
      // if we have copied any new messages to the dst folder, we must process
      // them (i.e. filter, report, ...)
      if ( !uidsNew.IsEmpty() )
      {
         ProcessNewMail(uidsNew, folder);
      }
      //else: no need to open it
   }

   return true;
}

void
MailFolderCC::ExpungeMessages(void)
{
   CHECK_DEAD("Cannot expunge messages from closed folder '%s'.");

   if ( PY_CALLBACK(MCB_FOLDEREXPUNGE,1,GetProfile()) )
   {
      wxLogTrace(TRACE_MF_CALLS, "MailFolderCC(%s)::ExpungeMessages()",
                 GetName().c_str());

      CHECK_DEAD("ExpungeMessages(): folder closed");

      mail_expunge(m_MailStream);

      // for some types of folders (IMAP) mm_exists() is called from
      // mail_expunge() but for the others (POP) it isn't and we have to call
      // it ourselves: check is based on the fact that m_expungedMsgnos is
      // reset in mm_exists handler
      if ( m_expungedMsgnos )
      {
         RequestUpdateAfterExpunge();
      }
   }
}

// ----------------------------------------------------------------------------
// Counting messages
// ----------------------------------------------------------------------------

unsigned long MailFolderCC::GetMessageCount() const
{
   CHECK( m_MailStream, 0, "GetMessageCount: folder is closed" );

   return m_MailStream->nmsgs;
}

unsigned long MailFolderCC::CountNewMessages() const
{
   MsgnoArray *messages = SearchByFlag(MSG_STAT_NEW);
   if ( !messages )
      return 0;

   unsigned long newmsgs = messages->GetCount();
   delete messages;

   return newmsgs;
}

unsigned long MailFolderCC::CountRecentMessages() const
{
   CHECK( m_MailStream, 0, "CountRecentMessages: folder is closed" );

   return m_MailStream->recent;
}

unsigned long MailFolderCC::CountUnseenMessages() const
{
   // we can't use mail_status here because the mailbox is opened and IMAP
   // doesn't like STATUS on SELECTed mailbox

   SEARCHPGM *pgm = mail_newsearchpgm();
   pgm->unseen = 1;
   pgm->undeleted = 1;

   return SearchAndCountResults(pgm);
}

unsigned long MailFolderCC::CountDeletedMessages() const
{
   SEARCHPGM *pgm = mail_newsearchpgm();
   pgm->deleted = 1;

   return SearchAndCountResults(pgm);
}

bool MailFolderCC::DoCountMessages(MailFolderStatus *status) const
{
   CHECK( status, false, "DoCountMessages: NULL pointer" );

   CHECK( m_MailStream, false, "DoCountMessages: folder is closed" );

   wxLogTrace(TRACE_MF_CALLS, "MailFolderCC(%s)::DoCountMessages()",
              GetName().c_str());

   status->Init();

   status->total = m_MailStream->nmsgs;
   status->recent = m_MailStream->recent;

   status->unread = CountUnseenMessages();

   if ( status->recent || status->unread )
   {
      status->newmsgs = CountNewMessages();
   }
   else // no recent or no unseen
   {
      // hence no new
      status->newmsgs = 0;
   }

   // then get the number of flagged messages
   SEARCHPGM *pgm = mail_newsearchpgm();
   pgm->flagged = 1;
   pgm->undeleted = 1;

   status->flagged = SearchAndCountResults(pgm);

   // TODO: get the number of searched ones
   status->searched = 0;

   return true;
}

// ----------------------------------------------------------------------------
// MailFolderCC accessing the messages
// ----------------------------------------------------------------------------

MsgnoType
MailFolderCC::GetMsgnoFromUID(UIdType uid) const
{
   // garbage in, garbage out
   CHECK( uid != UID_ILLEGAL, MSGNO_ILLEGAL, "GetMsgnoFromUID: bad uid" );

   CHECK( m_MailStream, MSGNO_ILLEGAL, "GetMsgnoFromUID: folder closed" );

   // mail_msgno() is a slow function because it iterates over entire c-client
   // internal cache but I don't see how to make it faster
   return mail_msgno(m_MailStream, uid);
}

Message *
MailFolderCC::GetMessage(unsigned long uid)
{
   CHECK_DEAD_RC("Cannot access closed folder '%s'.", NULL);

   HeaderInfoList_obj headers = GetHeaders();
   CHECK( headers, NULL, "GetMessage: failed to get headers" );

   UIdType idx = headers->GetIdxFromUId(uid);
   CHECK( idx != UID_ILLEGAL, NULL, "GetMessage: no UID with this message" );

   HeaderInfo *hi = headers->GetItemByIndex((size_t)idx);
   CHECK( hi, NULL, "invalid UID in GetMessage" );

   return MessageCC::Create(this, *hi);
}

// ----------------------------------------------------------------------------
// MailFolderCC: searching
// ----------------------------------------------------------------------------

MsgnoArray *
MailFolderCC::DoSearch(struct search_program *pgm, int flags) const
{
   ASSERT_MSG( flags == SEARCH_UID || flags == SEARCH_MSGNO,
               "DoSearch(): invalid flags value" );

   CHECK( m_MailStream, NULL, "DoSearch(): folder is closed" );

   // at best we're going to have a memory leak, at worse c-client is locked
   // and we will just crash
   ASSERT_MSG( !m_SearchMessagesFound, "MailFolderCC::DoSearch() reentrancy" );

   MailFolderCC *self = (MailFolderCC *)this; // const_cast
   self->m_SearchMessagesFound = new UIdArray;

   // never prefetch the results as we often want to just count them and
   // prefetching messages may also result in unwanted reentrancies so it's
   // safer to avoid it
   //
   // also, as we never reuse the program, we always free it immediately
   mail_search_full
   (
      m_MailStream,
      NIL,   // charset: use the default US-ASCII
      pgm,
      (flags & SEARCH_UID ? SE_UID : 0) | SE_FREE | SE_NOPREFETCH
   );

   CHECK( m_SearchMessagesFound, NULL, "who deleted m_SearchMessagesFound?" );

   MsgnoArray *searchMessagesFound = m_SearchMessagesFound;
   self->m_SearchMessagesFound = NULL;

   return searchMessagesFound;
}

unsigned long
MailFolderCC::SearchAndCountResults(struct search_program *pgm) const
{
   MsgnoArray *searchResults = DoSearch(pgm);

   unsigned long count;
   if ( searchResults )
   {
      count = searchResults->Count();

      delete searchResults;
   }
   else
   {
      count = 0;
   }

   return count;
}

MsgnoArray *MailFolderCC::SearchByFlag(MessageStatus flag,
                                       int flags,
                                       MsgnoType last) const
{
   SEARCHPGM *pgm = mail_newsearchpgm();

   // do we look for messages with this flag or without?
   bool set = !(flags & SEARCH_UNSET);

   // construct the corresponding search program
   switch ( flag )
   {
      case MSG_STAT_SEEN:
         if ( set )
            pgm->seen = 1;
         else
            pgm->unseen = 1;
         break;

      case MSG_STAT_DELETED:
         if ( set )
         {
            // we don't risk finding many messages like this!
            ASSERT_MSG( !(flags & SEARCH_UNDELETED),
                        "deleted and undeleted at once?" );

            pgm->deleted = 1;
         }
         else
            pgm->undeleted = 1;
         break;

      case MSG_STAT_ANSWERED:
         if ( set )
            pgm->answered = 1;
         else
            pgm->unanswered = 1;
         break;

      case MSG_STAT_RECENT:
         if ( set )
            pgm->recent = 1;
         else
            pgm->old = 1;
         break;

      case MSG_STAT_FLAGGED:
         if ( set )
            pgm->flagged = 1;
         else
            pgm->unflagged = 1;
         break;

      case MSG_STAT_NEW:
         if ( set )
         {
            pgm->recent = 1;
            pgm->unseen = 1;
         }
         else
         {
            // although it is formally possible, this must be an error
            FAIL_MSG( "do you really want to search for unnew messages??" );
         }
         break;

      case MSG_STAT_SEARCHED: // unsupported and unneeded
      default:
         FAIL_MSG( "unexpected flag in SearchByFlag" );

         mail_free_searchpgm(&pgm);

         return NULL;
   }

   if ( flags & SEARCH_UNDELETED )
   {
      pgm->undeleted = 1;
   }

   if ( last )
   {
      // only search among the messages after this one
      SEARCHSET *set = mail_newsearchset();
      set->first = last + 1;

      CHECK( m_MailStream, 0, "SearchByFlag: folder is closed" );


      if ( flags & SEARCH_UID )
      {
         set->last = mail_uid(m_MailStream, m_nMessages);
         pgm->uid = set;
      }
      else // msgno search
      {
         set->last = m_nMessages;
         pgm->msgno = set;
      }
   }

   return DoSearch(pgm, flags & (SEARCH_UID | SEARCH_MSGNO));
}

UIdArray *
MailFolderCC::SearchMessages(const SearchCriterium *crit, int flags)
{
   CHECK( crit, NULL, "no criterium in SearchMessages" );

   // server side searching doesn't support all possible search criteria,
   // check if it can do this search first
   SEARCHPGM *pgm = mail_newsearchpgm();
   STRINGLIST **slistMatch;

   switch ( crit->m_What )
   {
      case SearchCriterium::SC_FULL:
         slistMatch = &pgm->text;
         break;

      case SearchCriterium::SC_BODY:
         slistMatch = &pgm->body;
         break;

      case SearchCriterium::SC_SUBJECT:
         slistMatch = &pgm->subject;
         break;

      case SearchCriterium::SC_TO:
         slistMatch = &pgm->to;
         break;

      case SearchCriterium::SC_FROM:
         slistMatch = &pgm->from;
         break;

      case SearchCriterium::SC_CC:
         slistMatch = &pgm->cc;
         break;

      default:
         slistMatch = NULL;
         mail_free_searchpgm(&pgm);
   }

   if ( !slistMatch )
   {
      // can't do server side search, fall back to the generic version
      return MailFolderCmn::SearchMessages(crit, flags);
   }

   *slistMatch = mail_newstringlist();

   (*slistMatch)->text.data = (unsigned char *)strdup(crit->m_Key);
   (*slistMatch)->text.size = crit->m_Key.length();

   if ( crit->m_Invert )
   {
      // represent the inverted search as "TRUE && !pgm"
      SEARCHPGM *pgmReal = pgm;

      pgm = mail_newsearchpgm();
      pgm->msgno = mail_newsearchset();
      pgm->msgno->first = 1;
      pgm->msgno->last = GetMessageCount();

      pgm->cc_not = mail_newsearchpgmlist();
      pgm->cc_not->pgm = pgmReal;
   }

   return DoSearch(pgm, flags);
}

// ----------------------------------------------------------------------------
// Message flags
// ----------------------------------------------------------------------------

/* static */
String MailFolderCC::BuildSequence(const UIdArray& messages)
{
   Sequence seq;

   size_t count = messages.GetCount();
   for ( size_t n = 0; n < count; n++ )
   {
      seq.Add(messages[n]);
   }

   return seq.GetString();
}

bool
MailFolderCC::SetMessageFlag(unsigned long uid,
                             int flag,
                             bool set)
{
   Sequence seq;
   seq.Add(uid);

   return SetSequenceFlag(SEQ_UID, seq, flag, set);
}

bool
MailFolderCC::SetSequenceFlag(SequenceKind kind,
                              const Sequence& seq,
                              int flag,
                              bool set)
{
   CHECK_DEAD_RC("Cannot access closed folder '%s'.", false);

   if ( !CanSetFlag(flag) )
   {
      ERRORMESSAGE((_("Impossible to set this flag for the folder '%s'."),
                    GetName().c_str()));

      return false;
   }

   String flags = GetImapFlags(flag);

   const String sequence = seq.GetString();

   if(PY_CALLBACKVA((set ? MCB_FOLDERSETMSGFLAG : MCB_FOLDERCLEARMSGFLAG,
                     1, this, this->GetClassName(),
                     GetProfile(), "ss", sequence.c_str(), flags.c_str()),1)  )
   {
      wxBusyCursor busyCursor;

      int opFlags = 0;
      if ( set )
         opFlags |= ST_SET;
      if ( kind == SEQ_UID )
         opFlags |= ST_UID;

      CHECK( m_MailStream, 0, "SetSequenceFlag: folder is closed" );

      wxLogTrace(TRACE_MF_CALLS, "MailFolderCC(%s)::SetFlags(%s) = %s",
                 GetName().c_str(), sequence.c_str(), flags.c_str());

      mail_flag(m_MailStream,
                (char *)sequence.c_str(),
                (char *)flags.c_str(),
                opFlags);
   }
   //else: blocked by python callback

   return true;
}

void
MailFolderCC::OnMsgStatusChanged()
{
   // we only send MEventId_MailFolder_OnMsgStatus events if something really
   // changed, so this is not suposed to happen
   CHECK_RET( m_statusChangedMsgnos, "unexpected OnMsgStatusChanged() call" );

   // first update the cached status
   MailFolderStatus status;
   MfStatusCache *mfStatusCache = MfStatusCache::Get();
   if ( mfStatusCache->GetStatus(GetName(), &status) )
   {
      size_t count = m_statusChangedMsgnos->GetCount();
      for ( size_t n = 0; n < count; n++ )
      {
         int statusNew = m_statusChangedNew->Item(n),
             statusOld = m_statusChangedOld->Item(n);

         bool wasDeleted = (statusOld & MSG_STAT_DELETED) != 0,
              isDeleted = (statusNew & MSG_STAT_DELETED) != 0;

         MsgStatus msgStatusOld = AnalyzeStatus(statusOld),
                   msgStatusNew = AnalyzeStatus(statusNew);

         // we consider that a message has some flag only if it is not deleted
         // (which is discussable at least for flagged and searched flags
         // although, OTOH, why flag a deleted message?)

         #define UPDATE_NUM_OF(what)   \
            if ( (!isDeleted && msgStatusNew.what) && \
                 (wasDeleted || !msgStatusOld.what) ) \
            { \
               wxLogTrace(M_TRACE_MFSTATUS, "%s: " #what "++ (now %lu)", \
                          GetName().c_str(), status.what + 1); \
               status.what++; \
            } \
            else if ( (!wasDeleted && msgStatusOld.what) && \
                      (isDeleted || !msgStatusNew.what) ) \
               if ( status.what > 0 ) \
               { \
                  wxLogTrace(M_TRACE_MFSTATUS, "%s: " #what "-- (now %lu)", \
                             GetName().c_str(), status.what - 1); \
                  status.what--; \
               } \
               else \
                  FAIL_MSG( "error in msg status change logic" )

         UPDATE_NUM_OF(recent);
         UPDATE_NUM_OF(unread);
         UPDATE_NUM_OF(newmsgs);
         UPDATE_NUM_OF(flagged);
         UPDATE_NUM_OF(searched);

         #undef UPDATE_NUM_OF
      }

      mfStatusCache->UpdateStatus(GetName(), status);
   }

   // next notify everyone else about the status change
   wxLogTrace(TRACE_MF_EVENTS,
              "Sending MsgStatus event for %u msgs in folder '%s'",
              m_statusChangedMsgnos->GetCount(), GetName().c_str());

   MEventManager::Send(new MEventMsgStatusData(this,
                                               m_statusChangedMsgnos,
                                               m_statusChangedOld,
                                               m_statusChangedNew));

   // MEventMsgStatusData will delete them
   m_statusChangedMsgnos =
   m_statusChangedOld =
   m_statusChangedNew = NULL;
}

void
MailFolderCC::UpdateMessageStatus(unsigned long msgno)
{
   // if we're retrieving the headers right now, we can't use
   // headers->GetItemByIndex() below as this risks to reenter c-client which
   // is a fatal error
   //
   // besides, we don't need to do it neither as mm_exists() will follow soon
   // which will invalidate the current status anyhow
   if ( IsLocked() )
      return;

   CHECK_RET( m_MailStream, "UpdateMessageStatus: folder is closed" );

   MESSAGECACHE *elt = mail_elt(m_MailStream, msgno);
   CHECK_RET( elt, "UpdateMessageStatus: no elt for the given msgno?" );

   HeaderInfoList_obj headers = GetHeaders();
   CHECK_RET( headers, "UpdateMessageStatus: couldn't get headers" );

   HeaderInfo *hi = headers->GetItemByMsgno(msgno);
   CHECK_RET( hi, "UpdateMessageStatus: no header info for the given msgno?" );

   int statusNew = GetMsgStatus(elt),
       statusOld = hi->GetStatus();
   if ( statusNew != statusOld )
   {
      hi->m_Status = statusNew;

      if ( MfStatusCache::Get()->GetStatus(GetName()) )
      {
         // we send the event telling us that we have some messages with the
         // changed status only once, when we get the first notification - and
         // also when we allocate the arrays for msg status change data
         bool sendEvent;
         if ( !m_statusChangedMsgnos )
         {
            m_statusChangedMsgnos = new wxArrayInt;
            m_statusChangedOld = new wxArrayInt;
            m_statusChangedNew = new wxArrayInt;

            sendEvent = true;
         }
         else
         {
            sendEvent = false;
         }

         // remember who changed and how
         m_statusChangedMsgnos->Add(msgno);
         m_statusChangedOld->Add(statusOld);
         m_statusChangedNew->Add(statusNew);

         // and schedule call to our OnMsgStatusChanged() if not done yet
         if ( sendEvent )
         {
            MEventManager::Send(new MEventFolderOnMsgStatusData(this));
         }
      }
      //else: don't update the status because we don't have anything to update
      //      and, worse, we might obtain the new message status before we get
      //      to OnMsgStatusChanged() and then we'd apply the updates
      //      pertaining to the old status to the new values which would make
      //      them out of sync with the real values
   }
   //else: flags didn't really change
}

bool MailFolderCC::IsReadOnly(void) const
{
   CHECK( m_MailStream, true, "MailFolderCC::IsReadOnly(): folder is closed" );

   return m_MailStream->rdonly != 0;
}

bool MailFolderCC::CanSetFlag(int flags) const
{
   CHECK( m_MailStream, false, "MailFolderCC::CanSetFlag(): folder is closed" );

   if ( !IsReadOnly() )
   {
      // assume we can set all the flags in a folder opened read/write
      return true;
   }

   int canSet = 1;
   if ( flags & MSG_STAT_SEEN )
   {
      canSet &= m_MailStream->perm_seen;
   }

   if ( flags & MSG_STAT_DELETED )
   {
      canSet &= m_MailStream->perm_deleted;
   }

   if ( flags & MSG_STAT_ANSWERED )
   {
      canSet &= m_MailStream->perm_answered;
   }

   if ( flags & MSG_STAT_FLAGGED )
   {
      canSet &= m_MailStream->perm_flagged;
   }

   return canSet != 0;
}

// ----------------------------------------------------------------------------
// Debug only helpers
// ----------------------------------------------------------------------------

#ifdef DEBUG

void
MailFolderCC::Debug(void) const
{
}

String
MailFolderCC::DebugDump() const
{
   String str = MObjectRC::DebugDump();
   str << "mailbox '" << m_ImapSpec << "' of type " << (int) GetType();

   return str;
}

#endif // DEBUG

// ----------------------------------------------------------------------------
// MailFolderCC sorting/threading
// ----------------------------------------------------------------------------

bool
MailFolderCC::SortMessages(MsgnoType *msgnos, const SortParams& sortParams)
{
   CHECK( m_MailStream, false, "can't sort closed folder" );

   /*
      if we want to do server side sorting and the server supports it
         if it can sort with the criteria from sortParams
            construct the sort program
            call mail_sort() to do server side sorting
            if ok
               return

      call base class version to do local sorting
    */

   // does the server support sorting? and if so, do we want to use it?
   if ( GetType() == MF_IMAP && LEVELSORT(m_MailStream) &&
        READ_CONFIG(m_Profile, MP_MSGS_SERVER_SORT) )
   {
      // construct the sort program checking that all sort criteria are
      // supported by the server side sort
      SORTPGM *pgmSort = NULL;
      SORTPGM **ppgmStep = &pgmSort;

      // iterate over all individual sort criteriums
      for ( long sortOrder = sortParams.sortOrder;
            sortOrder;
            sortOrder = GetSortNextCriterium(sortOrder) )
      {
         // c-client sort function
         int sortFunction;

         MessageSortOrder crit = GetSortCritDirect(sortOrder);
         switch ( crit )
         {
            case MSO_DATE:
               sortFunction = SORTDATE;
               break;

            case MSO_SUBJECT:
               sortFunction = SORTSUBJECT;
               break;

            case MSO_SIZE:
               sortFunction = SORTSIZE;
               break;

            case MSO_SENDER:
               if ( !sortParams.detectOwnAddresses )
               {
                  sortFunction = SORTFROM;
                  break;
               }
               //else: server side sorting doesn't support this, fall through

            case MSO_STATUS:
            case MSO_SCORE:
               sortFunction = -1;
               break;

            case MSO_NONE:
               // MSO_NONE is not supposed to occur in sortOrder except as the
               // first (and only) criterium in which case we wouldn't get
               // here because of the loop test
               //
               // but as the user can currently create such sort orders using
               // the controls in the sort dialog (there are no checks there,
               // just ignore it here instead of giving an error)
               wxLogDebug("Unexpected MSO_NONE in the sort order");
               continue;

            default:
               FAIL_MSG("unexpected sort criterium");

               sortFunction = -1;
         }

         if ( sortFunction == -1 )
         {
            // criterium not supported, don't do server side sorting
            mail_free_sortpgm(&pgmSort);

            break;
         }

         // either start of continue the program
         *ppgmStep = mail_newsortpgm();

         // fill in the sort struct
         (*ppgmStep)->reverse = IsSortCritReversed(sortOrder);
         (*ppgmStep)->function = sortFunction;
         ppgmStep = &(*ppgmStep)->next;
      }

      // call mail_sort() to do server side sorting if we can
      if ( pgmSort )
      {
         wxLogTrace(TRACE_MF_CALLS, "MailFolderCC(%s)::SortMessages()",
                    GetName().c_str());

         // if any new messages appear in the folder during sorting, nmsgs is
         // going to change but we only have enough place for the current value
         // of it in the msgnos array
         MsgnoType numMessagesSorted = m_MailStream->nmsgs;

         // we need to provide a search program, otherwise c-client doesn't
         // sort anything - but if we give it an uninitialized SEARCHPGM, it
         // sorts all the messages [it's just a pleasure to use this library]
         SEARCHPGM *pgmSrch = mail_newsearchpgm();
         MsgnoType *results = mail_sort(m_MailStream,
                                        "US-ASCII",
                                        pgmSrch,
                                        pgmSort,
                                        SE_FREE | SO_FREE);
         if ( !results )
         {
            wxLogWarning(_("Server side sorting failed, trying to sort "
                           "messages locally."));
         }
         else // sorted ok
         {
            // we need to copy the results as we can't just reuse this buffer
            // because c-client has its own memory allocation function which
            // could be incompatible with our malloc/free
            for ( MsgnoType n = 0; n < numMessagesSorted; n++ )
            {
               *msgnos++ = *results++;
            }

            // do we have to free them or not??
            //fs_give((void **)&results);

            return true;
         }
      }
   }

   // call base class version to do local sorting
   return MailFolderCmn::SortMessages(msgnos, sortParams);
}

static void ThreadMessagesHelper(THREADNODE *thr,
                                 size_t level,
                                 MsgnoType& n,
                                 ThreadData *thrData,
                                 const ThreadParams& params)
{
   while ( thr )
   {
      MsgnoType msgno = thr->num;

      size_t indentChildren;
      if ( msgno == 0 )
      {
         // dummy message, representing the thread root which is not in the
         // mailbox: ignore it but don't forget to indent messages under it
         // if the corresponding option is set
         indentChildren = params.indentIfDummyNode;
      }
      else // valid message
      {
         // save this message details
         thrData->m_tableThread[n++] = msgno;
         thrData->m_indents[msgno - 1] = level;   // -1 to convert to index

         // always indent children below
         indentChildren = 1;
      }

      // do we have subtree?
      MsgnoType numChildren;
      if ( thr->next )
      {
         // process it and count the number of children
         MsgnoType nOld = n;
         ThreadMessagesHelper(thr->next, level + indentChildren, n,
                              thrData, params);

         ASSERT_MSG( n > nOld, "no children in a subthread?" );

         numChildren = n - nOld;
      }
      else
      {
         numChildren = 0;
      }

      if ( msgno )
      {
         // remember the number of children under this message, used later
         thrData->m_children[msgno - 1] = numChildren;
      }

      // pass to the next sibling
      thr = thr->branch;
   }
}

static bool MailStreamHasThreader(MAILSTREAM *stream, const char *thrName)
{
   IMAPLOCAL *imapLocal = (IMAPLOCAL *)stream->local;
   THREADER *thr;
   for ( thr = imapLocal->threader;
         thr && mail_compare_cstring(thr->name, (char *)thrName);
         thr = thr->next )
      ;

   return thr != NULL;
}


//
// Copy the tree given to us to some 'known' memory
//
static THREADNODE* CopyTree(THREADNODE* th) {
   if (th == NULL) return NULL;
   THREADNODE* thrNode = new THREADNODE;
   thrNode->num = th->num;
   thrNode->next = CopyTree(th->next);
   thrNode->branch = CopyTree(th->branch);
   return thrNode;
}


bool MailFolderCC::ThreadMessages(const ThreadParams& thrParams,
                                  ThreadData *thrData)
{
   CHECK( m_MailStream, false, "can't thread closed folder" );

   // does the server support threading at all?
   if ( GetType() == MF_IMAP && LEVELSORT(m_MailStream) &&
        READ_CONFIG(m_Profile, MP_MSGS_SERVER_THREAD) )
   {
      const char *threadingAlgo;

      // it does, but maybe we want only threading by references (best) and it
      // only provides dumb threading by subject?
      if ( !MailStreamHasThreader(m_MailStream, "REFERENCES") )
      {
         if ( READ_CONFIG(m_Profile, MP_MSGS_SERVER_THREAD_REF_ONLY) )
         {
            // don't use server side threading at all if it only provides the
            // dumb way to do it
            threadingAlgo = NULL;
         }
         else // we are allowed to fall back to subject threading
         {
            if ( MailStreamHasThreader(m_MailStream, "ORDEREDSUBJECT") )
            {
               threadingAlgo = "ORDEREDSUBJECT";
            }
            else // no ORDEREDSUBJECT neither?
            {
               // well, it does have to support something if it announced
               // threading support in its CAPABILITY reply, so just use the
               // first threading method available
               IMAPLOCAL *imapLocal = (IMAPLOCAL *)m_MailStream->local;
               threadingAlgo = imapLocal->threader->name;
            }
         }
      }
      else // have REFERENCES threading
      {
         // this is the best method available, just use it
         threadingAlgo = "REFERENCES";
      }

      if ( threadingAlgo )
      {
         // do server side threading

         wxLogTrace(TRACE_MF_CALLS, "MailFolderCC(%s)::ThreadMessages()",
                    GetName().c_str());

         THREADNODE *thrRoot = mail_thread
                               (
                                 m_MailStream,
                                 (char *)threadingAlgo,
                                 NULL,                // default charset
                                 mail_newsearchpgm(), // thread all messages
                                 SE_FREE
                               );

         if ( thrRoot )
         {
            // We copy the tree so that we can release the c-client
            // memory right now. After that, we know that whatever
            // the algo that was used, we can destroy the tree the
            // same way.
            thrData->m_root = CopyTree(thrRoot);

            mail_free_threadnode(&thrRoot);

            // everything done
            return true;
         }
         else if ( IsOpened() )
         {
            // it was really an error with threading, how strange
            wxLogWarning(_("Server side threading failed, trying to thread "
                           "messages locally."));
         }
         else // folder closed
         {
            // we lost connection while threading
            wxLogWarning(_("Connection lost while threading messages."));

            return false;
         }
      }
      //else: no appropriate thread method
   }

   // call base class version to do local sorting
   return MailFolderCmn::ThreadMessages(thrParams, thrData);
}

// ----------------------------------------------------------------------------
// MailFolderCC working with the headers
// ----------------------------------------------------------------------------

MsgnoType MailFolderCC::GetHeaderInfo(ArrayHeaderInfo& headers,
                                      const Sequence& seq)
{
   CHECK( m_MailStream, 0, "GetHeaderInfo: folder is closed" );

   MailFolderLocker lockFolder(this);

   String sequence = seq.GetString();
   wxLogTrace(TRACE_MF_CALLS, "Retrieving headers %s for '%s'...",
              sequence.c_str(), GetName().c_str());

   // prepare overviewData to be used by OverviewHeaderEntry()
   // --------------------------------------------------------

   MsgnoType nMessages = seq.GetCount();
   OverviewData overviewData(seq, headers, nMessages);

   // don't show the progress dialog if we're not in interactive mode
   if ( GetInteractiveFrame() && !mApplication->IsInAwayMode() )
   {
      // show the progress dialog if there are many messages to retrieve,
      // the check for threshold > 0 allows to disable the progress dialog
      // completely if the user wants
      long threshold = READ_CONFIG(m_Profile, MP_FOLDERPROGRESS_THRESHOLD);
      if ( threshold > 0 && nMessages > (unsigned long)threshold )
      {
         // create the progress meter which we will just have to Update() from
         // OverviewHeaderEntry() later
         overviewData.SetProgressDialog(GetName());
      }
   }
   //else: no progress dialog

   // do fill the listing
   size_t n;
   for ( UIdType i = seq.GetFirst(n); i != UID_ILLEGAL; i = seq.GetNext(i, n) )
   {
      MESSAGECACHE *elt = mail_elt(m_MailStream, i);
      if ( !elt )
      {
         if ( !m_MailStream )
         {
            // the stream is dead, abort
            break;
         }

         FAIL_MSG( "failed to get sequence element?" );

         continue;
      }

      ENVELOPE *env = mail_fetch_structure(m_MailStream, i, NIL, NIL);
      if ( !env )
      {
         if ( !m_MailStream )
         {
            // connection was broken, don't try to get the others
            break;
         }

         FAIL_MSG( "failed to get sequence element envelope?" );

         continue;
      }

      if ( !OverviewHeaderEntry(&overviewData, elt, env) )
      {
         // cancelled
         break;
      }
   }

   // notify the user if the connection was broken unexpectedly
   if ( !m_MailStream )
   {
      ERRORMESSAGE((_("Error retrieving the message headers from folder '%s'"),
                    GetName().c_str()));
   }

   return overviewData.GetRetrievedCount();
}

/* static */
String
MailFolderCC::ParseAddress(ADDRESS *adr)
{
   AddressList *addrList = AddressListCC::Create(adr);
   String address = addrList->GetAddresses();
   addrList->DecRef();

   return address;
}

bool
MailFolderCC::OverviewHeaderEntry(OverviewData *overviewData,
                                  MESSAGECACHE *elt,
                                  ENVELOPE *env)
{
   // overviewData must have been created in GetHeaderInfo()
   CHECK( overviewData, false, "OverviewHeaderEntry: no overview data?" );

   // it is possible that new messages have arrived in the meantime, ignore
   // them (FIXME: is it really normal?)
   if ( overviewData->Done() )
   {
      wxLogDebug("New message(s) appeared in folder while overviewing it, "
                 "ignored.");

      // stop overviewing
      return false;
   }

   // store what we've got
   HeaderInfo& entry = *overviewData->GetCurrent();

   // status
   entry.m_Status = GetMsgStatus(elt);

   // date
   MESSAGECACHE selt;
   mail_parse_date(&selt, env->date);
   entry.m_Date = (time_t) mail_longdate(&selt);

   // from and to
   entry.m_From = ParseAddress(env->from);

   MFolderType folderType = GetType();
   if ( folderType == MF_NNTP || folderType == MF_NEWS )
   {
      entry.m_NewsGroups = env->newsgroups;
   }
   else
   {
      entry.m_To = ParseAddress(env->to);
   }

   // deal with encodings for the text header fields
   wxFontEncoding encoding;
   if ( !entry.m_To.empty() )
   {
      entry.m_To = DecodeHeader(entry.m_To, &encoding);
   }
   else
   {
      encoding = wxFONTENCODING_SYSTEM;
   }

   wxFontEncoding encodingMsg = encoding;

   entry.m_From = DecodeHeader(entry.m_From, &encoding);
   if ( (encoding != wxFONTENCODING_SYSTEM) &&
        (encoding != encodingMsg) )
   {
      if ( encodingMsg != wxFONTENCODING_SYSTEM )
      {
         // VZ: I just want to know if this happens, we can't do anything
         //     smart about this so far anyhow as we can't display the
         //     different fields in different encodings
         wxLogDebug("Different encodings for From and To fields, "
                    "From may be displayed incorrectly.");
      }
      else
      {
         encodingMsg = encoding;
      }
   }

   // subject

   entry.m_Subject = DecodeHeader(env->subject, &encoding);
   if ( (encoding != wxFONTENCODING_SYSTEM) &&
        (encoding != encodingMsg) )
   {
      if ( encodingMsg != wxFONTENCODING_SYSTEM )
      {
         wxLogDebug("Different encodings for From and Subject fields, "
                    "subject may be displayed incorrectly.");
      }
      else
      {
         encodingMsg = encoding;
      }
   }

   // all the other fields
   entry.m_Size = elt->rfc822_size;
   entry.m_Lines = 0;   // TODO: calculate them?
   entry.m_Id = env->message_id;
   entry.m_References = env->references;
   entry.m_InReplyTo = env->in_reply_to;
   entry.m_UId = mail_uid(m_MailStream, elt->msgno);

   // set the font encoding to be used for displaying this entry
   entry.m_Encoding = encodingMsg;

   // update the progress dialog and also check if it wasn't cancelled by the
   // user in the meantime
   if ( !overviewData->UpdateProgress(entry) )
   {
      // cancelled by user
      return false;
   }

   overviewData->Next();

   // continue
   return true;
}

// ----------------------------------------------------------------------------
// MailFolderCC notification sending
// ----------------------------------------------------------------------------

void MailFolderCC::RequestUpdateAfterExpunge()
{
   CHECK_RET( m_expungedMsgnos, "shouldn't be called if we didn't expunge" );

   // FIXME: we ignore IsUpdateSuspended() here, should we? and if not,
   //        what to do?

   // we can update the status faster here as we have decremented the
   // number of recent/unseen/... when the messages were deleted (before
   // being expunged), so we just have to update the total now
   //
   // NB: although this has all chances to break down with manual expunge
   //     or even with automatic one due to race condition (if the
   //     message status changed from outside...) - but this is so much
   //     faster and the problem is not really fatal that I still prefer
   //     to do it like this
   MailFolderStatus status;
   MfStatusCache *mfStatusCache = MfStatusCache::Get();
   if ( mfStatusCache->GetStatus(GetName(), &status) )
   {
      // caller is supposed to check for this!
      CHECK_RET( m_MailStream, "UpdateStatusAfterExpunge(): dead stream" );

      status.total = m_MailStream->nmsgs;

      mfStatusCache->UpdateStatus(GetName(), status);
   }

   // tell GUI to update
   wxLogTrace(TRACE_MF_EVENTS, "Sending FolderExpunged event for folder '%s'",
              GetName().c_str());

   MEventManager::Send(new MEventFolderExpungeData(this,
                                                   m_expungedMsgnos,
                                                   m_expungedPositions));

   // MEventFolderExpungeData() will delete them
   m_expungedMsgnos =
   m_expungedPositions = NULL;
}

void MailFolderCC::OnMailExists(struct mail_stream *stream, MsgnoType msgnoMax)
{
   // NB: use stream and not m_MailStream because the latter might not be set
   //     yet if we're called from mail_open()

   // ignore any callbacks for closed or half opened folders: normally we
   // shouldn't get them at all but somehow we do (ask c-client why...)
   if ( !stream || stream->halfopen )
   {
      wxLogDebug("mm_exists() for not opened folder '%s' ignored.",
                 GetName().c_str());

      return;
   }

   // special case: when we get the first mm_exists() for an empty folder,
   // msgnoMax will be equal to m_nMessages (both will be 0), so catch this
   // with an additional check for m_uidLast
   if ( msgnoMax != m_nMessages || m_uidLast == UID_ILLEGAL )
   {
      ASSERT_MSG( msgnoMax >= m_nMessages, "where have they gone?" );

      // update the number of headers in the listing
      if ( m_headers )
      {
         wxLogTrace(TRACE_MF_EVENTS, "Adding msgno %u to headers", (unsigned int)msgnoMax);

         m_headers->OnAdd(msgnoMax);
      }

      // update the number of messages: it should always be in sync with the
      // real folder
      m_nMessages = msgnoMax;

      // we don't have to do anything for empty folders except updating their
      // status
      if ( msgnoMax )
      {
         // we often receive two "* EXISTS" in a row (this seems to be a bug in
         // the IMAP server I use but it does happen all the time) and in this
         // case we don't want to send two MEventFolderOnNewMails
         if ( !m_gotUnprocessedNewMail )
         {
            // set a flag to avoid sending expunge events until we process the
            // new mail event: this is unnecessary anyhow but, even worse, it
            // may result in the c-client reentrancy as new mail invalidates
            // the msgno -> position mapping in m_headers and so a call to
            // GetPosFromIdx() in OnMailExpunge() below could result in another
            // call to c-client if we use server side sorting and/or threading
            m_gotUnprocessedNewMail = true;

            // NB: if we have m_expungedMsgnos, leave them for now and don't
            //     discard them as we used to do because if all new mail is
            //     filtered away, we'd still have to notify the GUI about these
            //     expunged messages

            // we want to apply the filtering code but we can't do it from here
            // because we're inside c-client now and it is not reentrant, so we
            // send an event to ourselves to do it slightly later
            MEventManager::Send(new MEventFolderOnNewMailData(this));

            // the number of unread/marked/... messages may have changed (there
            // could be some more of them among the new ones), so forget the
            // old values
            //
            // NB: we can't assume that the status hasn't changed just because
            //     all new mail was filtered away because we might have some
            //     non new messages as well and the flags of the existing ones
            //     could have been changed from the outside
            MfStatusCache::Get()->InvalidateStatus(GetName());
         }
      }
      else // empty folder
      {
         // do update status in this case as OnNewMail() won't be called

         MailFolderStatus status;
         status.total = 0; // all the other fields are already 0

         MfStatusCache::Get()->UpdateStatus(GetName(), status);
      }
   }
   else // same number of messages
   {
      // anything expunged?
      if ( m_expungedMsgnos )
      {
         RequestUpdateAfterExpunge();
      }
      //else: nothing at all (this does happen although, again, shouldn't)
   }

   // update it so that it won't be UID_ILLEGAL the next time and the test
   // above works as expected
   m_uidLast = stream->uid_last;
}

void MailFolderCC::OnMailExpunge(MsgnoType msgno)
{
   // remove the message from the header info list if we have headers
   // (it would be wasteful to create them just to do it)
   if ( m_headers )
   {
      size_t idx = msgno - 1;
      if ( idx < m_headers->Count() )
      {
         // if we'll send a FolderUpdate later, there is no need to send a
         // FolderExpunge one
         //
         // otherwise prepare for sending it from RequestUpdateAfterExpunge()
         // a bit later
         if ( !m_gotUnprocessedNewMail )
         {
            // let all the others know that some messages were expunged: we
            // don't do it right now but wait until mm_exists() which is sent
            // after all mm_expunged() as it is much faster to delete all
            // messages at once instead of one at a time
            if ( !m_expungedMsgnos )
            {
               // create new arrays
               m_expungedMsgnos = new wxArrayInt;
               m_expungedPositions = new wxArrayInt;
            }

            // add the msgno to the list of expunged messages
            m_expungedMsgnos->Add(msgno);

            // and remember its position as well
            m_expungedPositions->Add(m_headers->GetPosFromIdx(idx));
         }

         wxLogTrace(TRACE_MF_EVENTS, "Removing msgno %u from headers", (unsigned int)msgno);

         m_headers->OnRemove(idx);
      }
      else // expunged message not in m_headers
      {
         FAIL_MSG( "invalid msgno in mm_expunged" );
      }
   }
   //else: no headers, nothing to do

   // update the total number of messages
   ASSERT_MSG( m_nMessages > 0, "expunged message from an empty folder?" );

   m_nMessages--;

   // we don't change the cached status here as it will be done in
   // RequestUpdateAfterExpunge() later
}

// ----------------------------------------------------------------------------
// MailFolderCC new mail handling
// ----------------------------------------------------------------------------

void MailFolderCC::OnNewMail()
{
   CHECK_RET( m_MailStream, "OnNewMail: folder is closed" );

   // c-client is not reentrant, this is why we have to call this function
   // when we are not inside any c-client call!
   CHECK_RET( !m_MailStream->lock, "OnNewMail: folder is locked" );

   // OnMailExists() sets this before calling us, so how could it have been
   // reset?
   ASSERT_MSG( m_gotUnprocessedNewMail, "OnNewMail: why are we called?" );

   wxLogTrace(TRACE_MF_EVENTS, "Got new mail notification for '%s'",
              GetName().c_str());

   // should we notify the GUI about the new mail in the folder? initially we
   // don't need to do it as we are not even sure we actually have any new mail
   bool shouldNotify = false;

   // see if we have any new messages
   if ( m_MailStream->recent )
   {
      // we don't want to process any events while we're filtering mail as
      // there can be a lot of them and all of them can be processed later
      MEventManagerSuspender suspendEvents;

      // don't allow changing the folder while we're filtering it
      MLocker filterLock(m_mutexNewMail);

      // only find the new new messages, i.e. the ones which we hadn't reported
      // yet
      UIdArray *uidsNew = SearchByFlag
                          (
                           MSG_STAT_NEW,
                           SEARCH_SET | SEARCH_UNDELETED | SEARCH_UID,
                           m_uidLastNew == UID_ILLEGAL ? 0 : m_uidLastNew
                          );

      if ( uidsNew )
      {
         size_t count = uidsNew->GetCount();
         if ( count )
         {
            // use "%ld" to print UID_ILLEGAL as -1 although it's really
            // unsigned
            wxLogTrace(TRACE_MF_NEWMAIL, "Folder %s: last new UID %ld -> %ld",
                       GetName().c_str(), m_uidLastNew, uidsNew->Last());

            // update m_uidLastNew to avoid finding the same messages again the
            // next time
            m_uidLastNew = uidsNew->Last();

            HeaderInfoList_obj hil = GetHeaders();
            if ( hil )
            {
               // process the new mail, whatever it means (collecting,
               // filtering, just reporting, ...)
               if ( ProcessNewMail(*uidsNew) && !uidsNew->IsEmpty() )
               {
                  // ProcessNewMail() removes the messages removed by filters
                  // or whatever else it does from uidsNew - if something is
                  // left there after it finished, we have some new mail to
                  // notify the GUI about (this has nothing to do with
                  // notifying the user about new mail!)
                  shouldNotify = true;
               }
               //else: nothing changed for this folder
            }
            else
            {
               FAIL_MSG( "no headers in OnNewMail()?" );
            }
         }

         delete uidsNew;
      }
      //else: this can possibly happen if they were deleted by another client
   }
   else // no recent messages
   {
      // although normally any new message appearing in the folder must be
      // recent, there are a few exceptions to this rule:
      //
      // 1. if another session has this folder opened, only it might see the
      //    message as recent whereas we won't
      //
      // 2. the IMAP server I use is horribly broken for the moment and the
      //    new messages don't have the recent flag _at_all_ which is, of
      //    course, clearly a server bug but Mahogany should still be ready to
      //    handle such madness
      shouldNotify = true;
   }

   // no more
   m_gotUnprocessedNewMail = false;

   // we delayed sending the update notification in OnMailExists() because we
   // wanted to filter the new messages first - now it is done and we can notify
   // the GUI about the changes, if there were any
   if ( shouldNotify )
   {
      // we got some new mail: ignore the previously expunged messages (if we
      // have any) as the listing is going to be updated anyhow
      DiscardExpungeData();

      // and update everything
      RequestUpdate();
   }
   else // no new mail
   {
      // we might want to notify about the delayed message expunging
      if ( m_expungedMsgnos )
      {
         RequestUpdateAfterExpunge();
      }
   }
}

// ----------------------------------------------------------------------------
// CClient initialization and clean up
// ----------------------------------------------------------------------------

void
MailFolderCC::CClientInit(void)
{
   if(ms_CClientInitialisedFlag == true)
      return;

   // link in all drivers (SSL is initialized later, when it is needed for the
   // first time)
#ifdef OS_WIN
#  include "linkage.c"
#else
#  include "linkage_no_ssl.c"
#endif

   // this triggers c-client initialisation via env_init()
#ifdef OS_UNIX
   if(geteuid() != 0) // running as root, skip init:
#endif
      (void *) myusername();

#ifdef OS_WIN
   // cclient extra initialization under Windows: use the home directory of the
   // user (the programs directory by default) as HOME
   wxString home;
   mail_parameters(NULL, SET_HOMEDIR, (void *)wxGetHomeDir(&home));
#endif // OS_WIN

   // 1 try is enough, the default (3) is too slow: notice that this only sets
   // the number of trials for SMTP and not for the mailbox drivers for which
   // we have to set this separately!
   mail_parameters(NULL, SET_MAXLOGINTRIALS, (void *)1);

   (*imapdriver.parameters)(SET_MAXLOGINTRIALS, (void *)1);
   (*pop3driver.parameters)(SET_MAXLOGINTRIALS, (void *)1);

#ifdef USE_BLOCK_NOTIFY
   mail_parameters(NULL, SET_BLOCKNOTIFY, (void *)mahogany_block_notify);
#endif // USE_BLOCK_NOTIFY

#ifdef USE_READ_PROGRESS
   mail_parameters(NULL, SET_READPROGRESS, (void *)mahogany_read_progress);
#endif // USE_READ_PROGRESS

#ifdef OS_UNIX
   // install our own sigpipe handler to ignore (and not die) if a SIGPIPE
   // happens
   struct sigaction sa;
   sigset_t sst;
   sigemptyset(&sst);
   sa.sa_handler = sigpipe_handler;
   sa.sa_mask = sst;
   sa.sa_flags = SA_RESTART;
   if( sigaction(SIGPIPE,  &sa, NULL) != 0)
   {
      wxLogError(_("Cannot set signal handler for SIGPIPE."));
   }
#endif // OS_UNIX

   ms_CClientInitialisedFlag = true;

#ifdef USE_DIALUP
   ASSERT(gs_CCStreamCleaner == NULL);
   gs_CCStreamCleaner = new CCStreamCleaner();
#endif // USE_DIALUP

   ASSERT_MSG( !gs_CCEventReflector, "couldn't be created yet" );
   gs_CCEventReflector = new CCEventReflector;
}

#ifdef USE_DIALUP

CCStreamCleaner::~CCStreamCleaner()
{
   wxLogTrace(TRACE_MF_CALLS, "CCStreamCleaner: checking for left-over streams");

   // don't send us any notifications any more
   delete gs_CCEventReflector;

   bool isOnline = mApplication->IsOnline();

   for ( StreamList::iterator i = m_List.begin(); i != m_List.end(); ++i )
   {
      MAILSTREAM *stream = *i;

      if ( isOnline )
      {
         // we are online, so we can close it properly:
         DBGMESSAGE(("CCStreamCleaner: closing stream"));

         mail_close(stream);
      }
      else // !online
      {
         // brutally free all memory without closing stream:
         DBGMESSAGE(("CCStreamCleaner: freeing stream offline"));

         // copied from c-client mail.c:
         if (stream->mailbox) fs_give ((void **) &stream->mailbox);
         stream->sequence++;  /* invalidate sequence */
         /* flush user flags */
         for (int n = 0; n < NUSERFLAGS; n++)
            if (stream->user_flags[n]) fs_give ((void **) &stream->user_flags[n]);
         mail_free_cache (stream);  /* finally free the stream's storage */
         /*if (!stream->use)*/ fs_give ((void **) &stream);
      }
   }
}

#endif // USE_DIALUP

// ----------------------------------------------------------------------------
// functions used by MailFolder initialization/shutdown code
// ----------------------------------------------------------------------------

bool MailFolderCCInit(void)
{
   MailFolderCC::CClientInit();

   return true;
}

void MailFolderCCCleanup(void)
{
   ServerInfoEntryCC::DeleteAll();

   // as c-client lib doesn't seem to think that deallocating memory is
   // something good to do, do it at it's place...
   //
   // NB: do not reverse the ordering: GET_NEWSRC may need GET_HOMEDIR
   free(mail_parameters((MAILSTREAM *)NULL, GET_NEWSRC, NULL));
   free(mail_parameters((MAILSTREAM *)NULL, GET_HOMEDIR, NULL));

#ifdef USE_DIALUP
   if ( gs_CCStreamCleaner )
   {
      delete gs_CCStreamCleaner;
      gs_CCStreamCleaner = NULL;
   }
#endif // USE_DIALUP
}

static MFSubSystem gs_subsysCC(MailFolderCCInit, MailFolderCCCleanup);

// ----------------------------------------------------------------------------
// Some news spool support (TODO: move out from here)
// ----------------------------------------------------------------------------

static String gs_NewsSpoolDir;

/// return the directory of the newsspool:
/* static */
String
MailFolderCC::GetNewsSpool(void)
{
   CClientInit();
   return (const char *)mail_parameters (NIL,GET_NEWSSPOOL,NULL);
}

const String& MailFolder::InitializeNewsSpool()
{
   if ( !gs_NewsSpoolDir )
   {
      // first, init cclient
      MailFolderCCInit();

      gs_NewsSpoolDir = (char *)mail_parameters(NULL, GET_NEWSSPOOL, NULL);
      if ( !gs_NewsSpoolDir )
      {
         gs_NewsSpoolDir = READ_APPCONFIG_TEXT(MP_NEWS_SPOOL_DIR);
         mail_parameters(NULL, SET_NEWSSPOOL, (char *)gs_NewsSpoolDir.c_str());
      }
   }

   return gs_NewsSpoolDir;
}

// ----------------------------------------------------------------------------
// callbacks called by cclient
// ----------------------------------------------------------------------------

/// tells program that there are this many messages in the mailbox
void
MailFolderCC::mm_exists(MAILSTREAM *stream, unsigned long msgnoMax)
{
   MailFolderCC *mf = LookupObject(stream);
   CHECK_RET( mf, "number of messages changed in unknown mail folder" );

   mf->OnMailExists(stream, msgnoMax);
}

/* static */
void
MailFolderCC::mm_expunged(MAILSTREAM *stream, unsigned long msgno)
{
   MailFolderCC *mf = LookupObject(stream);
   CHECK_RET(mf, "mm_expunged for non existent folder");

   mf->OnMailExpunge(msgno);
}

/// this message matches a search
void
MailFolderCC::mm_searched(MAILSTREAM * stream,
                          unsigned long msgno)
{
   MailFolderCC *mf = LookupObject(stream);
   CHECK_RET( mf, "messages found in unknown mail folder" );

   // this must have been allocated before starting the search
   CHECK_RET( mf->m_SearchMessagesFound, "logic error in search code" );

   mf->m_SearchMessagesFound->Add(msgno);
}

/** this mailbox name matches a listing request
       @param stream mailstream
       @param delim  character that separates hierarchies
       @param name   mailbox name
       @param attrib mailbox attributes
 */
void
MailFolderCC::mm_list(MAILSTREAM * stream,
                      char delim,
                      String  name,
                      long  attrib)
{
   if(gs_mmListRedirect)
   {
      gs_mmListRedirect(stream, delim, name, attrib);
      return;
   }

   MailFolderCC *mf = LookupObject(stream);
   CHECK_RET(mf,"NULL mailfolder");

   // create the event corresponding to the folder
   ASMailFolder::ResultFolderExists *result =
      ASMailFolder::ResultFolderExists::Create
      (
         mf->m_ASMailFolder,
         mf->m_Ticket,
         name,
         delim,
         attrib,
         mf->m_UserData
      );

   // and send it
   MEventManager::Send(new MEventASFolderResultData (result) );

   // don't forget to free the result - MEventASFolderResultData makes a copy
   // (i.e. calls IncRef) of it
   result->DecRef();

   // wxYield() is needed to send the events resulting from (previous) calls
   // to MEventManager::Send(), but don't forget to prevent any other calls to
   // c-client from happening - this will result in a fatal error as it is not
   // reentrant
#if wxCHECK_VERSION(2, 2, 6)
   wxYieldIfNeeded();
#else // wxWin <= 2.2.5
   wxYield();
#endif // wxWin 2.2.5/2.2.6
}


/** matches a subscribed mailbox listing request
       @param stream mailstream
       @param delim  character that separates hierarchies
       @param name   mailbox name
       @param attrib mailbox attributes
       */
void
MailFolderCC::mm_lsub(MAILSTREAM * stream,
                      char delim ,
                      String  name,
                      long  attrib)
{
   MailFolderCC *mf = LookupObject(stream);
   CHECK_RET(mf, "NULL mailfolder");

   wxFAIL_MSG( "TODO" );
}

/** status of mailbox has changed
    @param stream mailstream
    @param mailbox   mailbox name for this status
    @param status structure with new mailbox status
*/
void
MailFolderCC::mm_status(MAILSTREAM *stream,
                        String mailbox,
                        MAILSTATUS *status)
{
   MailFolderCC *mf = LookupObject(stream);
   CHECK_RET(mf, "mm_status for non existent folder");

   wxLogTrace(TRACE_MF_CALLBACK, "mm_status: folder '%s', %lu messages",
              mf->m_ImapSpec.c_str(), status->messages);

   // do nothing here for now
}

/** log a verbose message
       @param stream mailstream
       @param str message str
       @param errflg error level
 */
void
MailFolderCC::mm_notify(MAILSTREAM * stream, String str, long errflg)
{
   MailFolderCC *mf = MailFolderCC::LookupObject(stream);

   // detect the notifications about stream being closed
   if ( mf && mf->IsOpened() && errflg == BYE )
   {
      mf->ForceClose();

      wxLogWarning(_("Connection to the folder '%s' lost unexpectedly."),
                   mf->GetName().c_str());
   }

   mm_log(str, errflg, mf);
}

/** log a message
    @param str message str
    @param errflg error level
*/
void
MailFolderCC::mm_log(const String& str, long errflg, MailFolderCC *mf)
{
   GetLogCircle().Add(str);

   if ( str.StartsWith("SELECT failed") )
   {
      // send this to the debug window anyhow, but don't show it to the user
      // as this message can only result from trying to open a folder which
      // has a \Noselect flag and we handle this ourselves
      mm_dlog(str);

      return;
   }

   if ( errflg == 0 && !mm_show_debug )
   {
      // this is a verbose information message and we're not interested in
      // them normally (there are tons of them...)
      return;
   }

   String msg = _("Mail log");
   if( mf )
      msg << " (" << mf->GetName() << ')';
   msg << ": " << str
#ifdef DEBUG
       << _(", error level: ") << strutil_ultoa(errflg)
#endif
      ;

   wxLogLevel loglevel;
   switch ( errflg )
   {
      case 0: // a.k.a INFO
         loglevel = wxLOG_User;
         break;

      case BYE:
      case WARN:
         loglevel = wxLOG_Status; // do not WARN user, it's annoying
         break;

      default:
         FAIL_MSG( "unknown cclient log level" );
         // fall through

      case ERROR:
         // usually just wxLOG_Error but may be set to something else to
         // temporarily suppress the errors
         loglevel = cc_loglevel;
         break;

      case PARSE:
         loglevel = wxLOG_Status; // goes to window anyway
         break;
   }

   wxLogGeneric(loglevel, msg);
}

/** log a debugging message
    @param str    message str
*/
void
MailFolderCC::mm_dlog(const String& str)
{
   if ( str.empty() )
   {
      // avoid calling GetWriteBuf(0) below, this is invalid
      return;
   }

   GetLogCircle().Add(str);

   // replace the passwords in the log window output (which can be seen by
   // others or be cut-and-pasted to show some problems) with stars if plain
   // text authentification is used: PASS for POP or LOGIN for IMAP
   if ( mm_show_debug )
   {
      String msg, username, password;
      unsigned long seq;

      // the max len of username/password
      size_t len = str.length();

      bool isPopLogin = sscanf(str, "PASS %s", password.GetWriteBuf(len)) == 1;
      password.UngetWriteBuf();

      if ( isPopLogin )
      {
         msg << "PASS " << wxString('*', password.length());
      }
      else
      {
         bool isImapLogin = sscanf(str, "%08lx LOGIN %s %s",
                                   &seq,
                                   username.GetWriteBuf(len),
                                   password.GetWriteBuf(len)) == 3;
         username.UngetWriteBuf();
         password.UngetWriteBuf();

         if ( isImapLogin )
         {
            msg.Printf("%08lx LOGIN %s %s",
                       seq,
                       username.c_str(),
                       wxString('*', password.length()).c_str());
         }
         else
         {
            msg = str;
         }
      }

      // send it to the window
      wxLogGeneric(M_LOG_WINONLY, _("Mail log: %s"), msg.c_str());
   }
}

/** get user name and password
       @param  mb parsed mailbox specification
       @param  user    pointer where to return username
       @param  pwd   pointer where to return password
       @param  trial number of prior login attempts
*/
void
MailFolderCC::mm_login(NETMBX * /* mb */,
                       char *user,
                       char *pwd,
                       long /* trial */)
{
   // normally this shouldn't happen
   ASSERT_MSG( !MF_user.empty(), "no username in mm_login()?" );

   strcpy(user, MF_user.c_str());
   strcpy(pwd, MF_pwd.c_str());

   // they are used once only, don't keep them or they could be reused for
   // another folder somehow
   MF_user.clear();
   MF_pwd.clear();
}

/* static */
void
MailFolderCC::mm_flags(MAILSTREAM *stream, unsigned long msgno)
{
   MailFolderCC *mf = LookupObject(stream);
   CHECK_RET(mf, "mm_flags for non existent folder");

   // message flags really changed, cclient checks for it
   mf->UpdateMessageStatus(msgno);
}

/** alert that c-client will run critical code
    @param  stream   mailstream
 */
void
MailFolderCC::mm_critical(MAILSTREAM *stream)
{
   if ( stream )
   {
      ms_LastCriticalFolder = stream->mailbox;
      MailFolderCC *mf = LookupObject(stream);
      if(mf)
      {
         mf->m_InCritical = true;
      }
   }
   else
   {
      ms_LastCriticalFolder = "";
   }
}

/**   no longer running critical code
   @param   stream mailstream
     */
void
MailFolderCC::mm_nocritical(MAILSTREAM *stream)
{
   ms_LastCriticalFolder = "";
   if ( stream )
   {
      MailFolderCC *mf = LookupObject(stream);
      if(mf)
      {
         mf->m_InCritical = false;
      }
   }
}

/** unrecoverable write error on mail file
       @param  stream   mailstream
       @param  errcode  OS error code
       @param  serious  non-zero: c-client cannot undo error
       @return abort flag: if serious error and abort non-zero: abort, else retry
       */
long
MailFolderCC::mm_diskerror(MAILSTREAM *stream,
                           long /* errcode */,
                           long /* serious */)
{
   return 1;   // abort!
}

/** program is about to crash!
    @param  str   message str
*/
void
MailFolderCC::mm_fatal(char *str)
{
   GetLogCircle().Add(str);
   wxLogError(_("Fatal error: %s"), str);

   String msg2 = str;
   if(ms_LastCriticalFolder.length())
      msg2 << _("\nLast folder in a critical section was: ")
           << ms_LastCriticalFolder;
   FatalError(msg2);
}

// ----------------------------------------------------------------------------
// Listing/subscribing/deleting the folders
// ----------------------------------------------------------------------------

/* static */
bool
MailFolderCC::Subscribe(const String &host,
                        MFolderType protocol,
                        const String &mailboxname,
                        bool subscribe)
{
   String spec = MailFolder::GetImapSpec(protocol, 0, mailboxname, host, "");
   return (subscribe ? mail_subscribe (NIL, (char *)spec.c_str())
                     : mail_unsubscribe (NIL, (char *)spec.c_str())) != NIL;
}

void
MailFolderCC::ListFolders(ASMailFolder *asmf,
                          const String &pattern,
                          bool subscribedOnly,
                          const String &reference,
                          UserData ud,
                          Ticket ticket)
{
   CHECK_DEAD("Cannot list subfolder of the closed folder '%s'.");

   String spec = m_ImapSpec;

   // make sure that there is a folder name delimiter before pattern - this
   // only makes sense for non empty spec
   if ( !spec.empty() )
   {
      char ch = spec.Last();

      if ( ch != '}' )
      {
         char chDelim = GetFolderDelimiter();

         if ( ch != chDelim )
            spec += chDelim;
      }
   }

   spec << reference << pattern;

   ASSERT(asmf);

   // set user data (retrieved by mm_list)
   m_UserData = ud;
   m_Ticket = ticket;
   m_ASMailFolder = asmf;
   m_ASMailFolder->IncRef();

   if ( subscribedOnly )
   {
      mail_lsub (m_MailStream, NULL, (char *) spec.c_str());
   }
   else
   {
      mail_list (m_MailStream, NULL, (char *) spec.c_str());
   }

   // Send event telling about end of listing:
   ASMailFolder::ResultFolderExists *result =
      ASMailFolder::ResultFolderExists::Create
      (
         m_ASMailFolder,
         m_Ticket,
         "",  // empty name == no more entries
         0,   // no delim
         0,   // no flags
         m_UserData
      );

   // and send it
   MEventManager::Send(new MEventASFolderResultData (result) );

   result->DecRef();
   m_ASMailFolder->DecRef();
   m_ASMailFolder = NULL;
}

// ----------------------------------------------------------------------------
// gettting the folder delimiter separator
// ----------------------------------------------------------------------------

static char gs_delimiter = '\0';

static void GetDelimiterMMList(MAILSTREAM *stream,
                               char delim,
                               String name,
                               long attrib)
{
   if ( delim )
      gs_delimiter = delim;
}

char MailFolderCC::GetFolderDelimiter() const
{
   // may be we already have it cached?
   if ( m_chDelimiter == ILLEGAL_DELIMITER )
   {
      MailFolderCC *self = (MailFolderCC *)this; // const_cast

      // the only really interesting case is IMAP as we need to query the
      // server for this one: to do it we issue 'LIST "" ""' command which is
      // guaranteed by RFC 2060 to return the delimiter
      if ( GetType() == MF_IMAP )
      {
         CHECK( m_MailStream, '\0', "folder closed in GetFolderDelimiter" );

         MMListRedirector redirect(GetDelimiterMMList);

         // pass the spec for the IMAP server itself to mail_list()
         String spec = m_ImapSpec.BeforeFirst('}');
         if ( !spec.empty() )
            spec += '}';

         gs_delimiter = '\0';
         mail_list(m_MailStream, NULL, (char *)spec.c_str());

         // well, except that in practice some IMAP servers do *not* return
         // anything in reply to this command! try working around this bug
         if ( !gs_delimiter )
         {
            CHECK( m_MailStream, '\0', "folder closed in GetFolderDelimiter" );

            spec += '%';
            mail_list(m_MailStream, NULL, (char *)spec.c_str());
         }

         // we must have got something!
         ASSERT_MSG( gs_delimiter,
                     "broken IMAP server returned no folder name delimiter" );

         self->m_chDelimiter = gs_delimiter;
      }
      else
      {
         // not IMAP, let the base class version do it
         self->m_chDelimiter = MailFolder::GetFolderDelimiter();
      }
   }

   ASSERT_MSG( m_chDelimiter != ILLEGAL_DELIMITER, "should have delimiter" );

   return m_chDelimiter;
}

// ----------------------------------------------------------------------------
// delete folder
// ----------------------------------------------------------------------------

/* static */
bool
MailFolderCC::Rename(const MFolder *mfolder, const String& name)
{
   wxLogTrace(TRACE_MF_CALLS, "MailFolderCC::Rename(): %s -> %s",
              mfolder->GetPath().c_str(), name.c_str());

   // I'm unsure if this is needed but I suppose we're going to have problems
   // if we rename the folder being used - or maybe not?
   (void)MailFolderCC::CloseFolder(mfolder);

   if ( !mail_rename(NULL,
                     (char *)::GetImapSpec(mfolder).c_str(),
                     (char *)name.c_str()) )
   {
      wxLogError(_("Failed to rename the mailbox for folder '%s' "
                   "from '%s' to '%s'."),
                 mfolder->GetFullName().c_str(),
                 mfolder->GetPath().c_str(),
                 name.c_str());

      return false;
   }

   return true;
}

// ----------------------------------------------------------------------------
// clear folder
// ----------------------------------------------------------------------------

/* static */
long
MailFolderCC::ClearFolder(const MFolder *mfolder)
{
   MFolderType type = mfolder->GetType();
   int flags = mfolder->GetFlags();
   String login = mfolder->GetLogin(),
          password = mfolder->GetPassword(),
          fullname = mfolder->GetFullName();
   String mboxpath = MailFolder::GetImapSpec(type,
                                             flags,
                                             mfolder->GetPath(),
                                             mfolder->GetServer(),
                                             login);

   wxLogTrace(TRACE_MF_CALLS, "Clearing folder '%s'", fullname.c_str());

   // if this folder is opened, use its stream - and also notify about message
   // deletion
   MAILSTREAM *stream;
   unsigned long nmsgs;
   CCCallbackDisabler *noCCC;
   ServerInfoEntryCC *server;

   MailFolderCC *mf = FindFolder(mfolder);
   if ( mf )
   {
      stream = mf->m_MailStream;

      CHECK( stream, false, "Closed folder in the opened folders map?" );

      nmsgs = mf->GetMessageCount();

      noCCC = NULL;
      server = NULL;
   }
   else // this folder is not opened
   {
      if ( !GetAuthInfoForFolder(mfolder, login, password ) )
      {
         return false;
      }

      SetLoginData(login, password);

      // we don't want any notifications
      noCCC = new CCCallbackDisabler;

      // try to reuse an existing connection, if any
      server = ServerInfoEntryCC::Get(mfolder);
      stream = server ? server->GetStream() : NIL;

      // open the folder: although we don't need to do it to get its status, we
      // have to do it anyhow below, so better do it right now
      stream = MailOpen(stream, mboxpath);

      if ( !stream )
      {
         wxLogError(_("Impossible to open folder '%s'"), fullname.c_str());

         delete noCCC;

         return false;
      }

      // get the number of messages (only)
      MAILSTATUS mailstatus;
      MMStatusRedirector statusRedir(stream->mailbox, &mailstatus);
      mail_status(stream, stream->mailbox, SA_MESSAGES);
      nmsgs = mailstatus.messages;
   }

   if ( nmsgs > 0 )
   {
      wxBusyCursor busyCursor;

      String seq;
      seq << "1:" << nmsgs;

      // now mark them all as deleted (we don't need notifications about the
      // status change so save a *lot* of bandwidth by using ST_SILENT)
      mail_flag(stream, (char *)seq.c_str(), "\\DELETED", ST_SET | ST_SILENT);

      // and expunge
      if ( mf )
      {
         mf->ExpungeMessages();
      }
      else // folder is not opened, just expunge quietly
      {
         mail_expunge(stream);

         // we need to update the status manually in this case
         MailFolderStatus status;

         // all other members are already set to 0
         status.total = 0;
         MfStatusCache::Get()->UpdateStatus(fullname, status);
      }
   }
   //else: no messages to delete

   if ( stream )
   {
      CloseOrKeepStream(stream, mfolder, server);
   }

   if ( mf )
      mf->DecRef();

   delete noCCC;

   return nmsgs;
}

// ----------------------------------------------------------------------------
// delete folder
// ----------------------------------------------------------------------------

/* static */
bool
MailFolderCC::DeleteFolder(const MFolder *mfolder)
{
   String login, password;
   if ( !GetAuthInfoForFolder(mfolder, login, password ) )
   {
      return false;
   }

   String mboxpath = MailFolder::GetImapSpec(mfolder->GetType(),
                                             mfolder->GetFlags(),
                                             mfolder->GetPath(),
                                             mfolder->GetServer(),
                                             login);

   MCclientLocker lock;
   SetLoginData(login, password);

   wxLogTrace(TRACE_MF_CALLS,
              "MailFolderCC::DeleteFolder(%s)", mboxpath.c_str());

   return mail_delete(NIL, (char *) mboxpath.c_str()) != NIL;
}

// ----------------------------------------------------------------------------
// HasInferiors() function: check if the folder has subfolders
// ----------------------------------------------------------------------------

// currently unused
#if 0

/*
  Small function to check if the mailstream has \inferiors flag set or
  not, i.e. if it is a mailbox or a directory on the server.
*/

static int gs_HasInferiorsFlag = -1;

static void HasInferiorsMMList(MAILSTREAM *stream,
                               char delim,
                               String name,
                               long attrib)
{
   gs_HasInferiorsFlag = !(attrib & LATT_NOINFERIORS);
}

/* static */
bool
MailFolderCC::HasInferiors(const String& imapSpec,
                           const String& login,
                           const String& passwd)
{
   // We lock the complete c-client code as we need to immediately
   // redirect the mm_list() callback to tell us what we want to know.
   MCclientLocker lock;

   // we want to disable all the usual callbacks as we might not have the
   // folder corresponding to imapSpec yet (as this function is called from
   // the folder creation dialog) and, even if we did, we are not interested
   // in them for now
   CCCallbackDisabler noCallbacks;

   SetLoginData(login, passwd);
   gs_HasInferiorsFlag = -1;

   ServerInfoEntryCC *server = ServerInfoEntryCC::Get(mfolder);
   MAILSTREAM *stream = server ? server->GetStream() : NIL;

   stream = MailOpen(stream, imapSpec, OP_HALFOPEN);

   if ( stream != NIL )
   {
      MMListRedirector redirect(HasInferiorsMMList);
      mail_list (stream, NULL, (char *) imapSpec.c_str());

      /* This does happen for for folders where the server does not know
         if they have inferiors, i.e. if they don't exist yet.
         I.e. -1 is an unknown/undefined status
         ASSERT(gs_HasInferiorsFlag != -1);
       */

      CloseOrKeepStream(stream, mfolder, server);
   }

   return gs_HasInferiorsFlag == 1;
}

#endif // 0

// ----------------------------------------------------------------------------
// network operations progress
// ----------------------------------------------------------------------------

void MailFolderCC::StartReading(unsigned long total)
{
#ifdef USE_READ_PROGRESS
   ASSERT_MSG( !gs_readProgressInfo, "can't start another read operation" );

   // don't show the progress dialogs for the local folders - in practice, it
   // will never take really long time to read from them
   if ( IsLocalQuickFolder(GetType()) )
      return;

   // here is the idea: if the message is big, we show the progress dialog
   // immediately, otherwise we only do it if retrieving it takes longer than
   // the specified time
   long size = READ_CONFIG(m_Profile, MP_MESSAGEPROGRESS_THRESHOLD_SIZE);
   if ( size > 0 )
   {
      long delay =
         total > (unsigned long)size * 1024
            ? 0
            : (long)READ_CONFIG(m_Profile, MP_MESSAGEPROGRESS_THRESHOLD_TIME);

      gs_readProgressInfo = new ReadProgressInfo(total, delay);
   }
#endif // USE_READ_PROGRESS
}

void MailFolderCC::EndReading()
{
#ifdef USE_READ_PROGRESS
   if ( gs_readProgressInfo )
   {
      delete gs_readProgressInfo;
      gs_readProgressInfo = NULL;
   }
   //else: we didn't create it in StartReading()
#endif // USE_READ_PROGRESS
}

// ----------------------------------------------------------------------------
// C-Client callbacks: the ones which must be implemented
// ----------------------------------------------------------------------------

// define a macro TRACE_CALLBACKn() for tracing a callback with n + 1 params
#ifdef DEBUG // EXPERIMENTAL_log_callbacks
   static const char *GetErrorLevel(long errflg)
   {
      return errflg == ERROR ? "error"
                             : errflg == WARN ? "warn"
                                              : errflg == PARSE ? "parse"
                                                                : "other";
   }

   static inline const char *GetStreamMailbox(MAILSTREAM *stream)
   {
      return stream ? stream->mailbox : "<no stream>";
   }

   #define TRACE_CALLBACK0(name) \
      wxLogTrace(TRACE_MF_CALLBACK, "%s(%s)%s", \
                 #name, GetStreamMailbox(stream), \
                 mm_disable_callbacks ? " [disabled]" : "")

   #define TRACE_CALLBACK1(name, fmt, parm) \
      wxLogTrace(TRACE_MF_CALLBACK, "%s(%s, " fmt ")%s", \
                 #name, GetStreamMailbox(stream), parm, \
                 mm_disable_callbacks ? " [disabled]" : "")

   #define TRACE_CALLBACK2(name, fmt, parm1, parm2) \
      wxLogTrace(TRACE_MF_CALLBACK, "%s(%s, " fmt ")%s", \
                 #name, GetStreamMailbox(stream), parm1, parm2, \
                 mm_disable_callbacks ? " [disabled]" : "")

   #define TRACE_CALLBACK3(name, fmt, parm1, parm2, parm3) \
      wxLogTrace(TRACE_MF_CALLBACK, "%s(%s, " fmt ")%s", \
                 #name, GetStreamMailbox(stream), parm1, parm2, parm3, \
                 mm_disable_callbacks ? " [disabled]" : "")

   #define TRACE_CALLBACK_NOSTREAM_1(name, fmt, parm1) \
      wxLogTrace(TRACE_MF_CALLBACK, "%s(" fmt ")%s", \
                 #name, parm1, \
                 mm_disable_callbacks ? " [disabled]" : "")

   #define TRACE_CALLBACK_NOSTREAM_2(name, fmt, parm1, parm2) \
      wxLogTrace(TRACE_MF_CALLBACK, "%s(" fmt ")%s", \
                 #name, parm1, parm2, \
                 mm_disable_callbacks ? " [disabled]" : "")

   #define TRACE_CALLBACK_NOSTREAM_5(name, fmt, parm1, parm2, parm3, parm4, parm5) \
      wxLogTrace(TRACE_MF_CALLBACK, "%s(" fmt ")%s", \
                 #name, parm1, parm2, parm3, parm4, parm5, \
                 mm_disable_callbacks ? " [disabled]" : "")
#else // !EXPERIMENTAL_log_callbacks
   #define TRACE_CALLBACK0(name)
   #define TRACE_CALLBACK0(name)
   #define TRACE_CALLBACK1(name, fmt, p1)
   #define TRACE_CALLBACK2(name, fmt, p1, p2)
   #define TRACE_CALLBACK3(name, fmt, p1, p2, p3)

   #define TRACE_CALLBACK_NOSTREAM_1(name, fmt, p1)
   #define TRACE_CALLBACK_NOSTREAM_2(name, fmt, p1, p2)
   #define TRACE_CALLBACK_NOSTREAM_5(name, fmt, p1, p2, p3, p4, p5)
#endif

// they're called from C code
extern "C"
{

void
mm_searched(MAILSTREAM *stream, unsigned long msgno)
{
   TRACE_CALLBACK1(mm_searched, "%lu", msgno);

   if ( !mm_disable_callbacks )
   {
      MailFolderCC::mm_searched(stream,  msgno);
   }
}

void
mm_expunged(MAILSTREAM *stream, unsigned long msgno)
{
   TRACE_CALLBACK1(mm_expunged, "%lu", msgno);

   if ( !mm_disable_callbacks )
   {
      MailFolderCC::mm_expunged(stream, msgno);
   }
}

void
mm_flags(MAILSTREAM *stream, unsigned long msgno)
{
   TRACE_CALLBACK1(mm_flags, "%lu", msgno);

   if ( !mm_disable_callbacks )
   {
      MailFolderCC::mm_flags(stream, msgno);
   }
}

void
mm_notify(MAILSTREAM *stream, char *str, long errflg)
{
   TRACE_CALLBACK2(mm_notify, "%s (%s)", str, GetErrorLevel(errflg));

   if ( !mm_disable_callbacks )
   {
      MailFolderCC::mm_notify(stream, str, errflg);
   }
}

void
mm_list(MAILSTREAM *stream, int delim, char *name, long attrib)
{
   TRACE_CALLBACK3(mm_list, "%d, `%s', %ld", delim, name, attrib);

   MailFolderCC::mm_list(stream, delim, name, attrib);
}

void
mm_lsub(MAILSTREAM *stream, int delim, char *name, long attrib)
{
   TRACE_CALLBACK3(mm_lsub, "%d, `%s', %ld", delim, name, attrib);

   MailFolderCC::mm_lsub(stream, delim, name, attrib);
}

void
mm_exists(MAILSTREAM *stream, unsigned long msgno)
{
   TRACE_CALLBACK1(mm_exists, "%lu", msgno);

   if ( !mm_disable_callbacks )
   {
      MailFolderCC::mm_exists(stream, msgno);
   }
}

void
mm_status(MAILSTREAM *stream, char *mailbox, MAILSTATUS *status)
{
   TRACE_CALLBACK1(mm_status, "%s", mailbox);

   // allow redirected callbacks even while all others are disabled
   if ( gs_mmStatusRedirect )
   {
      gs_mmStatusRedirect(stream, mailbox, status);
   }
   else if ( !mm_disable_callbacks )
   {
      MailFolderCC::mm_status(stream, mailbox, status);
   }
}

void
mm_log(char *str, long errflg)
{
   TRACE_CALLBACK_NOSTREAM_2(mm_log, "%s (%s)", str, GetErrorLevel(errflg));

   if ( mm_disable_callbacks || mm_ignore_errors )
      return;

   String msg(str);

   // TODO: what's going on here?
   if(errflg >= 4) // fatal imap error, reopen-mailbox
   {
      if ( !MailFolderCC::PingAllOpened() )
         msg << _("\nAttempt to re-open all folders failed.");
   }

   MailFolderCC::mm_log(msg, errflg);
}

void
mm_dlog(char *str)
{
   TRACE_CALLBACK_NOSTREAM_1(mm_dlog, "%s", str);

   // always show debug logs, even if other callbacks are disabled - this
   // makes it easier to understand what's going on

   // if ( !mm_disable_callbacks )
   {
      MailFolderCC::mm_dlog(str);
   }
}

void
mm_login(NETMBX *mb, char *user, char *pwd, long trial)
{
   // don't test for mm_disable_callbacks here, we can be called even while
   // other callbacks are disabled (example is call to mail_open() in
   // ClearFolder())
   TRACE_CALLBACK_NOSTREAM_5(mm_login, "%s %s@%s:%ld, try #%ld",
                             mb->service, mb->user, mb->host, mb->port,
                             trial);

   MailFolderCC::mm_login(mb, user, pwd, trial);
}

void
mm_critical(MAILSTREAM *stream)
{
   TRACE_CALLBACK0(mm_critical);

   MailFolderCC::mm_critical(stream);
}

void
mm_nocritical(MAILSTREAM *stream)
{
   TRACE_CALLBACK0(mm_nocritical);

   MailFolderCC::mm_nocritical(stream);
}

long
mm_diskerror(MAILSTREAM *stream, long int errorcode, long int serious)
{
   if ( mm_disable_callbacks )
      return 1;

   return MailFolderCC::mm_diskerror(stream,  errorcode, serious);
}

void
mm_fatal(char *str)
{
   MailFolderCC::mm_fatal(str);
}

// ----------------------------------------------------------------------------
// some more cclient callbacks: these are optional but we set them up to have
// more control over cclient operation
// ----------------------------------------------------------------------------

#ifdef USE_BLOCK_NOTIFY

void *mahogany_block_notify(int reason, void *data)
{
   #define LOG_BLOCK_REASON(what) \
      if ( reason == BLOCK_##what ) \
         printf("%s: mm_blocknotify(%s, %p)\n", \
                wxDateTime::Now().FormatTime().c_str(), #what, data); \
      else \

   LOG_BLOCK_REASON(NONE)
   LOG_BLOCK_REASON(SENSITIVE)
   LOG_BLOCK_REASON(NONSENSITIVE)
   LOG_BLOCK_REASON(DNSLOOKUP)
   LOG_BLOCK_REASON(TCPOPEN)
   LOG_BLOCK_REASON(TCPREAD)
   LOG_BLOCK_REASON(TCPWRITE)
   LOG_BLOCK_REASON(TCPCLOSE)
   LOG_BLOCK_REASON(FILELOCK)
   printf("mm_blocknotify(UNKNOWN, %p)\n", data);

   return NULL;
}

#endif // USE_BLOCK_NOTIFY

#ifdef USE_READ_PROGRESS

void mahogany_read_progress(GETS_DATA *md, unsigned long count)
{
   if ( gs_readProgressInfo )
      gs_readProgressInfo->OnProgress(count);
}

#endif // USE_READ_PROGRESS

} // extern "C"

// ============================================================================
// ServerInfoEntryCC implementation
// ============================================================================

ServerInfoEntryCC::ConnCloseTimer *ServerInfoEntryCC::ms_connCloseTimer = NULL;

ServerInfoEntryCC::LastParsedFolder ServerInfoEntryCC::ms_lastParsed;

// ----------------------------------------------------------------------------
// creation
// ----------------------------------------------------------------------------

static inline bool Folder2NETMBX(const MFolder *folder, NETMBX *netmbx)
{
   if ( !mail_valid_net_parse((char *)GetImapSpec(folder).c_str(), netmbx) )
   {
      FAIL_MSG( "invalid remote folder spec in ServerInfoEntryCC?" );

      return false;
   }

   return true;
}

ServerInfoEntryCC::ServerInfoEntryCC(const MFolder *folder)
                 : ServerInfoEntry(folder)
{
   Folder2NETMBX(folder, &m_netmbx);
}

ServerInfoEntryCC::~ServerInfoEntryCC()
{
   wxLogTrace(TRACE_SERVER_CACHE,
              "Deleting server entry for %s(%s).",
              m_netmbx.host, m_netmbx.user);

   // close all connections we may still have
   for ( StreamList::iterator i = m_connections.begin();
         i != m_connections.end();
         ++i )
   {
      mail_close(*i);
   }
}

/* static */
void ServerInfoEntryCC::DeleteAll()
{
   ServerInfoEntry::DeleteAll();

   if ( ms_connCloseTimer )
   {
      delete ms_connCloseTimer;
      ms_connCloseTimer = NULL;
   }
}

// this function is a method of MailFolderCC but logically makes part of
// ServerInfoEntryCC -- it is used to allow creating us from a static (and
// hence non polymorphic) GetOrCreate()
ServerInfoEntry *MailFolderCC::CreateServerInfo(const MFolder *folder) const
{
   return new ServerInfoEntryCC(folder);
}

// ----------------------------------------------------------------------------
// ServerInfoEntryCC comparison
// ----------------------------------------------------------------------------

/* static */
bool ServerInfoEntryCC::Parse(const MFolder *folder)
{
   if ( folder == ms_lastParsed.folder )
   {
      // already got it
      return true;
   }

   ms_lastParsed.folder = folder;

   return Folder2NETMBX(folder, &ms_lastParsed.netmbx);
}

bool ServerInfoEntryCC::CanBeUsedFor(const MFolder *folder) const
{
   return Parse(folder) &&
          strcmp(m_netmbx.host, ms_lastParsed.netmbx.host) == 0 &&
          strcmp(m_netmbx.service, ms_lastParsed.netmbx.service) == 0 &&
          (!m_netmbx.port || (m_netmbx.port == ms_lastParsed.netmbx.port)) &&
          (m_netmbx.anoflag == ms_lastParsed.netmbx.anoflag) &&
          (!m_netmbx.user[0] ||
            (strcmp(m_netmbx.user, ms_lastParsed.netmbx.user) == 0));
}

// ----------------------------------------------------------------------------
// ServerInfoEntryCC connection caching
// ----------------------------------------------------------------------------

MAILSTREAM *ServerInfoEntryCC::GetStream()
{
   if ( m_connections.empty() )
      return NULL;

   MAILSTREAM *stream = *m_connections.begin();
   m_connections.pop_front();
   m_timeouts.pop_front();

   return stream;
}

void ServerInfoEntryCC::KeepStream(MAILSTREAM *stream, const MFolder *folder)
{
   m_connections.push_back(stream);

   Profile_obj profile(folder->GetProfile());
   time_t t = time(NULL);
   time_t delay = READ_CONFIG(profile, MP_CONN_CLOSE_DELAY);

   wxLogTrace(TRACE_SERVER_CACHE,
              "Keeping connection to %s alive for %d seconds.",
              stream->mailbox, (int)delay);

   m_timeouts.push_back(t + delay);

   if ( !ms_connCloseTimer )
   {
      ms_connCloseTimer = new ConnCloseTimer;
   }

   if ( !ms_connCloseTimer->IsRunning() ||
           (ms_connCloseTimer->GetInterval() / 1000 > delay) )
   {
      wxLogTrace(TRACE_SERVER_CACHE,
                 "Starting connection clean up timer (delay = %ds)", (int)delay);

      // we want to use a smaller interval
      ms_connCloseTimer->Start(delay * 1000);
   }
}

bool ServerInfoEntryCC::CheckTimeout()
{
   // micro optimization
   if ( m_connections.empty() )
      return false;

   // close the connection even if the timeout hasn't expired yet but expires
   // in less than 1 second: this helps when we have a really big timeout (i.e.
   // 30 minutes) and if the timer comes up slightly before the moment *j
   // (which does happen in practice): we don't want to wait for another 30
   // minutes before closing the connection
   time_t t = time(NULL) + 1;

   // iterate in parallel over both lists
   StreamList::iterator i = m_connections.begin();
   TimeList::iterator j = m_timeouts.begin();
   while ( i != m_connections.end() )
   {
      if ( *j <= t )
      {
         // timed out
         MAILSTREAM *stream = *i;

         wxLogTrace(TRACE_SERVER_CACHE,
                    "Connection to %s timed out, closing.", stream->mailbox);

         // we're not interested in getting mail_close() babble
         CCCallbackDisabler cc;

         mail_close(stream);

         i = m_connections.erase(i);
         j = m_timeouts.erase(j);
      }
      else
      {
         ++i;
         ++j;
      }
   }

   // any connections left?
   return !m_connections.empty();
}

/* static */
void ServerInfoEntryCC::CheckTimeoutAll()
{
   bool hasAnyConns = false;

   for ( ServerInfoList::iterator i = ms_servers.begin();
         i != ms_servers.end();
         ++i )
   {
      if ( i->CheckTimeout() )
      {
         hasAnyConns = true;
      }
   }

   if ( !hasAnyConns )
   {
      // timer will be restarted in KeepStream() when/if neecessary
      wxLogTrace(TRACE_SERVER_CACHE, "Stopping connection clean up timer");

      ms_connCloseTimer->Stop();
   }
}

