//////////////////////////////////////////////////////////////////////////////
// Project:     M - cross platform e-mail GUI client
// File name:   mail/MailFolder.cpp: generic MailFolder methods which don't
//              use cclient (i.e. don't really work with mail folders)
// Purpose:     handling of mail folders with c-client lib
// Author:      Karsten Ball�der, Vadim Zeitlin
// Modified by:
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
   #pragma implementation "MailFolder.h"
   #pragma implementation "ServerInfo.h"
#endif

#include  "Mpch.h"

#ifndef USE_PCH
#   include "Mcommon.h"
#   include "guidef.h"    // only for high-level functions
#   include "strutil.h"
#   include "Profile.h"
#   include "MEvent.h"
#   include "MModule.h"
#   include <stdlib.h>
#   include <errno.h>
#endif

#include <wx/dynarray.h>
#include <wx/file.h>

#include "MDialogs.h" // for MDialog_FolderChoose

#include "Mdefaults.h"
#include "MFolder.h"

#include "Sequence.h"

#include "Message.h"
#include "MailFolder.h"

#include "UIdArray.h"
#include "MFStatus.h"
#include "LogCircle.h"

#include "MFPrivate.h"
#include "mail/Driver.h"
#include "mail/FolderPool.h"
#include "mail/ServerInfo.h"

#include "Composer.h"
#include "MApplication.h"

// ----------------------------------------------------------------------------
// options we use here
// ----------------------------------------------------------------------------

extern const MOption MP_DEFAULT_REPLY_KIND;
extern const MOption MP_FOLDER_COMMENT;
extern const MOption MP_FOLDER_PATH;
extern const MOption MP_FOLDER_TYPE;
extern const MOption MP_FORWARD_PREFIX;
extern const MOption MP_FROM_REPLACE_ADDRESSES;
extern const MOption MP_IMAPHOST;
extern const MOption MP_LIST_ADDRESSES;
extern const MOption MP_NNTPHOST;
extern const MOption MP_POPHOST;
extern const MOption MP_REPLY_COLLAPSE_PREFIX;
extern const MOption MP_REPLY_PREFIX;
extern const MOption MP_SET_REPLY_FROM_TO;
extern const MOption MP_USERNAME;

// ----------------------------------------------------------------------------
// persistent msgboxes we use here
// ----------------------------------------------------------------------------

extern const MPersMsgBox *M_MSGBOX_DIALUP_ON_OPEN_FOLDER;
extern const MPersMsgBox *M_MSGBOX_KEEP_PWD;
extern const MPersMsgBox *M_MSGBOX_NET_DOWN_OPEN_ANYWAY;
extern const MPersMsgBox *M_MSGBOX_REMEMBER_PWD;

// ----------------------------------------------------------------------------
// global static variables
// ----------------------------------------------------------------------------

ServerInfoEntry::ServerInfoList ServerInfoEntry::ms_servers;

MFSubSystem *MFSubSystem::ms_initilizers = NULL;

// ============================================================================
// MailFolder implementation
// ============================================================================

// ----------------------------------------------------------------------------
// MailFolder initialization and shutdown
// ----------------------------------------------------------------------------

/* static */
bool
MailFolder::Init()
{
   for ( MFSubSystem *subsys = MFSubSystem::GetFirst();
         subsys;
         subsys = subsys->GetNext() )
   {
      if ( !subsys->Init() )
      {
         // any error here is fatal because normally we don't do anything
         // error-prone in the Init() functions
         return false;
      }
   }

   return true;
}

/* static */
void
MailFolder::CleanUp()
{
   ServerInfoEntry::DeleteAll();
   MFPool::DeleteAll();

   for ( MFSubSystem *subsys = MFSubSystem::GetFirst();
         subsys;
         subsys = subsys->GetNext() )
   {
      subsys->CleanUp();
   }
}

// ----------------------------------------------------------------------------
// MailFolder opening
// ----------------------------------------------------------------------------

/**
  initialize all mail subsystems and return the folder driver or NULL if none
  (logging the error message in this case)
 */
static MFDriver *GetFolderDriver(const MFolder *folder)
{
   if ( !MailFolder::Init() )
      return NULL;

   CHECK( folder, NULL, "MailFolder: NULL folder object" );

   const String kind = folder->GetClass();

   // find the creation function for this kind of folders
   MFDriver *driver = MFDriver::Get(kind);
   if ( !driver )
   {
      ERRORMESSAGE((_("Unknown folder kind '%s'"), kind.c_str()));
   }

   return driver;
}

/* static */
MailFolder *
MailFolder::OpenFolder(const MFolder *folder, OpenMode mode, wxFrame *frame)
{
   MFDriver *driver = GetFolderDriver(folder);
   if ( !driver )
      return NULL;

   // ensure that we have the authentification information for this folder
   // before trying to open it
   bool userEnteredPwd = false;
   String login, password;
   if ( !GetAuthInfoForFolder(folder, login, password, &userEnteredPwd) )
   {
      // can't continue without login/password
      return NULL;
   }

   // look if we don't already have it opened
   //
   // NB: if the folder is already opened, the password [possibly] asked for
   //     above is unused and not checked at all and although it's not a
   //     serious problem it still doesn't look right (fixme)
   MailFolder *mf = MFPool::Find(driver, folder, login);
   if ( mf )
   {
      // make sure it's updated
      mf->Ping();

      // the connection could have been lost from inside Ping() so check for it
      // yet again and reopen the folder if this is what happened
      if ( !mf->IsOpened() )
      {
         mf->DecRef();
         mf = NULL;
      }
   }

   if ( !mf )
   {
      // check whether this folder is accessible
      if ( !CheckNetwork(folder, frame) )
      {
         return NULL;
      }

      // and now do [try to] open the folder
      mf = driver->OpenFolder(folder, login, password, mode, frame);

      // if we have succeeded to open the folder ...
      if ( mf )
      {
         // ... add it to the pool
         MFPool::Add(driver, mf, folder, login);

         // ... and propose to the user to save the password so that he doesn't
         // have to enter it again during the subsequent attempts to open it
         if ( userEnteredPwd )
         {
            // const_cast needed, unfortunately
            ProposeSavePassword(mf, (MFolder *)folder, login, password);
         }
      }
   }

   return mf;
}

/* static */
bool MailFolder::CheckNetwork(const MFolder *folder, wxFrame *frame)
{
#ifdef USE_DIALUP
   // check if we need to dial up to open this folder
   if ( folder->NeedsNetwork() && !mApplication->IsOnline() )
   {
      String msg;
      msg.Printf(_("To open the folder '%s', network access is required "
                   "but it is currently not available.\n"
                   "Would you like to connect to the network now?"),
                 folder->GetFullName().c_str());

      if ( MDialog_YesNoDialog(msg,
                               frame,
                               MDIALOG_YESNOTITLE,
                               M_DLG_YES_DEFAULT,
                               M_MSGBOX_DIALUP_ON_OPEN_FOLDER,
                               folder->GetFullName()) )
      {
         mApplication->GoOnline();
      }

      if ( !mApplication->IsOnline() ) // still not?
      {
         if ( !MDialog_YesNoDialog(_("Opening this folder will probably fail!\n"
                                     "Continue anyway?"),
                                   frame,
                                   MDIALOG_YESNOTITLE,
                                   M_DLG_NO_DEFAULT,
                                   M_MSGBOX_NET_DOWN_OPEN_ANYWAY,
                                   folder->GetFullName()) )
         {
             // remember that the user cancelled opening the folder
             mApplication->SetLastError(M_ERROR_CANCEL);

             return false;
         }
      }
   }
#endif // USE_DIALUP

   return true;
}

/* static */
bool
MailFolder::CloseFolder(const MFolder *folder)
{
   CHECK( folder, false, "MailFolder::CloseFolder(): NULL folder" );

   // don't try to close the folder which can't be opened, it's a NOOP
   if ( !folder->CanOpen() )
   {
      return true;
   }

   MFDriver *driver = GetFolderDriver(folder);
   if ( !driver )
      return false;

   // find the associated open folder
   //
   // FIXME: using GetLogin() here is wrong, see comments before MFPool::Find()
   MailFolder_obj mf = MFPool::Find(driver, folder, folder->GetLogin());
   if ( !mf )
   {
      // nothing to close
      return false;
   }

   mf->Close();

   return true;
}

/* static */
int
MailFolder::CloseAll()
{
   // count the number of folders we close
   size_t n = 0;

   MFPool::Cookie cookie;
   for ( MailFolder *mf = MFPool::GetFirst(cookie);
         mf;
         mf = MFPool::GetNext(cookie), n++ )
   {
      mf->Close();

      // notify any opened folder views
      MEventManager::Send(new MEventFolderClosedData(mf));

      mf->DecRef();
   }

   return n;
}


// ----------------------------------------------------------------------------
// MailFolder: other operations
// ----------------------------------------------------------------------------

MailFolder *
MailFolder::GetOpenedFolderFor(const MFolder *folder)
{
   MFDriver *driver = GetFolderDriver(folder);
   if ( !driver )
      return NULL;

   // FIXME: login problem again
   return MFPool::Find(driver, folder, folder->GetLogin());
}

/* static */
bool MailFolder::PingAllOpened(wxFrame *frame)
{
   bool rc = true;

   MFPool::Cookie cookie;
   for ( MailFolder *mf = MFPool::GetFirst(cookie);
         mf;
         mf = MFPool::GetNext(cookie) )
   {
      NonInteractiveLock noInter(mf, frame != NULL);

      if ( !mf->Ping() )
      {
         // failed with at least one folder
         rc = false;
      }

      mf->DecRef();
   }

   return rc;
}

/* static */
bool
MailFolder::CheckFolder(const MFolder *folder, wxFrame *frame)
{
   bool rc;

   MailFolder *mf = MailFolder::GetOpenedFolderFor(folder);
   if ( mf )
   {
      NonInteractiveLock noInter(mf, frame != NULL);

      // just pinging it is enough
      rc = mf->Ping();
      mf->DecRef();
   }
   else // not opened
   {
      MFDriver *driver = GetFolderDriver(folder);

      // check its status without opening it
      rc = driver ? driver->CheckFolder(folder) : false;
   }

   return rc;
}

/* static */
bool
MailFolder::DeleteFolder(const MFolder *folder)
{
   MFDriver *driver = GetFolderDriver(folder);

   return driver ? driver->DeleteFolder(folder) : false;
}

/* static */
bool
MailFolder::Rename(const MFolder *folder, const String& name)
{
   MFDriver *driver = GetFolderDriver(folder);

   return driver ? driver->RenameFolder(folder, name) : false;
}

/* static */
long
MailFolder::ClearFolder(const MFolder *folder)
{
   MFDriver *driver = GetFolderDriver(folder);

   return driver ? driver->ClearFolder(folder) : -1;
}

/* static */
bool MailFolder::CanExit(String *which)
{
   bool rc = true;

   MFPool::Cookie cookie;
   for ( MailFolder *mf = MFPool::GetFirst(cookie);
         mf && rc;
         mf = MFPool::GetNext(cookie) )
   {
      rc = !mf->IsInCriticalSection();

      mf->DecRef();
   }

   return rc;
}

// ----------------------------------------------------------------------------
// reply/forward messages
// ----------------------------------------------------------------------------

// special, invalid, value for Params::msgview member
const MessageView *MailFolder::Params::NO_QUOTE = (MessageView *)-1;

// extract the address string from List-Post header, return an empty string if
// none found
static String
ExtractListPostAddress(const String& listPostHeader)
{
   // the format of List-Post is described in the RFC 2369 but basicly
   // it's an address-like field except that it may also have a special
   // value "NO" if posting to this list is prohibited

   // FIXME: it would be nice to use c-client functions for parsing instead

   for ( const wxChar *p = listPostHeader; ; p++ )
   {
      switch ( *p )
      {
         case '<':
            {
               static const size_t MAILTO_LEN = 7; // strlen("mailto:")

               // start of an URL, get it
               if ( strncmp(++p, "mailto:", MAILTO_LEN) != 0 )
               {
                  wxLogDebug("Unknown URL scheme in List-Post (%s)",
                             listPostHeader.c_str());
                  return "";
               }

               String listPostAddress;
               for ( p += MAILTO_LEN; *p && *p != '>'; p++ )
               {
                  if ( *p == '?' )
                  {
                     // start of URL parameters - ignore them for now (TODO)
                     break;
                  }

                  listPostAddress += *p;
               }

               if ( !*p )
               {
                  // so that p++ in the loop sets it at '\0' during next iteration
                  p--;
               }
               else // successfully extracted an URL
               {
                  return listPostAddress;
               }
            }
            break;

         case '(':
            // start of the comment, skip it
            for ( p++; *p && *p != ')'; p++ )
               ;

            if ( !*p )
            {
               // so that p++ in the loop sets it at '\0' during next iteration
               p--;
            }
            break;

         case 'N':
            // possible "NO"
            if ( p[1] == 'O' )
            {
               // posting is forbidden, hence no list posting address
               return "";
            }
            //else: fall through

         // catches "case '\0':", so the loop always exits
         default:
            // this is just for me, so that I could check for possible bugs in
            // this code
            wxLogDebug("Malformed List-Post header '%s'!",
                       listPostHeader.c_str());
            return "";
      }
   }
}

// find reply kind value replyKind, return true if it was set explicitly
bool
GetReplyKind(const MailFolder::Params& params,
             Profile *profile,
             MailFolder::ReplyKind& replyKind)
{
   // first get the real reply kind
   replyKind = params.replyKind;

   if ( replyKind == MailFolder::REPLY )
   {
      // this means we should use the default reply kind for this folder
      long rk = READ_CONFIG(profile, MP_DEFAULT_REPLY_KIND);

      if ( rk < 0 || rk >= MailFolder::REPLY )
      {
         wxLogDebug("Corrupted config data: invalid MP_DEFAULT_REPLY_KIND!");

         rk = MailFolder::REPLY_SENDER;
      }

      // now cast is safe
      replyKind = (MailFolder::ReplyKind)rk;

      // implicit because we used the default value
      return false;
   }
   else
   {
      return true;
   }
}

// add the recipients addresses extracted from the message being replied to to
// the composer according to the MailFolder::Params::replyKind value
static void
InitRecipients(Composer *cv,
               Message *msg,
               const MailFolder::Params& params,
               Profile *profile)
{
   MailFolder::ReplyKind replyKind;
   bool explicitReplyKind;

   explicitReplyKind = GetReplyKind(params, profile, replyKind);

   // our own addresses - used in the code below
   String returnAddrs = READ_CONFIG(profile, MP_FROM_REPLACE_ADDRESSES);
   wxArrayString ownAddresses = strutil_restore_array(returnAddrs);

   // is this a message from ourselves?
   bool fromMyself = false;

   // is this a follow-up to newsgroup?
   if ( replyKind == MailFolder::FOLLOWUP_TO_NEWSGROUP )
   {
      cv->AddNewsgroup( MailFolder::DecodeHeader(msg->GetNewsgroups()) );
      return;
   }

   // first try Reply-To
   wxSortedArrayString replyToAddresses;
   size_t countReplyTo = msg->GetAddresses(MAT_REPLYTO, replyToAddresses);

   size_t n, count; // loop variables

   // REPLY_LIST overrides any Reply-To in the header
   if ( replyKind != MailFolder::REPLY_LIST )
   {
      // reply to Reply-To address, if any, by default except if REPLY_SENDER
      // had been explicitly chosen - in this case we want to allow using it to
      // make it possible to reply to the sender of the message only even if
      // the Reply-To address had been mangled by the mailing list to point to
      // it instead
      String rcptMain;
      if ( (explicitReplyKind && replyKind == MailFolder::REPLY_SENDER) ||
            !countReplyTo )
      {
         // try from address
         //
         // FIXME: original encoding is lost here
         rcptMain = MailFolder::DecodeHeader(msg->From());
      }
      else // have Reply-To
      {
         rcptMain = MailFolder::DecodeHeader(replyToAddresses[0]);
      }

      // an additional complication: when replying to a message written by
      // oneself you don't usually want to reply to you at all but, instead,
      // use the "To:" address of the original message for your reply as well -
      // the code below catches this particular case
      fromMyself = Message::FindAddress(ownAddresses, rcptMain) != wxNOT_FOUND;
      if ( fromMyself )
      {
         wxArrayString addresses;
         size_t count = msg->GetAddresses(MAT_TO, addresses);
         for ( n = 0; n < count; n++ )
         {
            cv->AddTo(addresses[n]);
         }
      }
      else // not from myself
      {
         cv->AddTo(rcptMain);

         // add the remaining Reply-To addresses (usually there will be none)
         for ( n = 1; n < countReplyTo; n++ )
         {
            // FIXME: same as above
            cv->AddTo(MailFolder::DecodeHeader(replyToAddresses[n]));
         }
      }
   }

   // don't bother extracting the other addresses if we know in advance that
   // we don't need them anyhow
   //
   // this is sort of a hack: if the user really chose "Reply to sender" from
   // the menu, then we won't even add the other addresses to the composer at
   // all because it clearly means that he doesn't need them
   //
   // OTOH, if REPLY_SENDER just happens to be the default reply value (and so
   // could be invoked just by one key press and is the fastest way to reply
   // anyhow), then we'll still add the other recipients to the composer but
   // just disable them by default
   //
   // IMHO this does the right thing in all cases, but maybe we need an
   // additional option to handle this ("add unused addresses to composer")?
   if ( replyKind == MailFolder::REPLY_SENDER && explicitReplyKind )
      return;

   // now get all other addresses
   wxSortedArrayString otherAddresses;

   if ( replyKind == MailFolder::REPLY_LIST )
   {
      // take all address because we didn't use them yet
      WX_APPEND_ARRAY(otherAddresses, replyToAddresses);

      otherAddresses.Add(msg->From());

      // don't want to exclude the Reply-To addresses in the code below
      replyToAddresses.Clear();
   }
   else // we already have used some addresses
   {
      // use From only if not done already above
      if ( countReplyTo > 0 || fromMyself )
         otherAddresses.Add(msg->From());
   }

   wxArrayString ccAddresses;
   msg->GetAddresses(MAT_CC, ccAddresses);

   WX_APPEND_ARRAY(otherAddresses, ccAddresses);

   // for messages from oneself we already used the "To" recipients above
   if ( !fromMyself )
      msg->GetAddresses(MAT_TO, otherAddresses);

   // this is probably an overkill - you never want to reply to the sender
   // address, do you?
#if 0
   msg->GetAddresses(MAT_SENDER, otherAddresses);
#endif // 0

   // decode headers before comparing them
   count = otherAddresses.GetCount();
   for ( n = 0; n < count; n++ )
   {
      // FIXME: as above, we lose the original encoding here
      otherAddresses[n] = MailFolder::DecodeHeader(otherAddresses[n]);
   }

   // remove duplicates
   wxArrayString uniqueAddresses = strutil_uniq_array(otherAddresses);

   // if we're in REPLY_LIST mode, also remember which reply addresses
   // correspond to the mailing lists as we want to reply to them only
   wxArrayString listAddresses;
   if ( replyKind == MailFolder::REPLY_LIST )
   {
      // the manually configured mailing lists
      listAddresses = strutil_restore_array(READ_CONFIG(profile,
                                                        MP_LIST_ADDRESSES));

      // also check if we can't find some in the message itself
      String listPostAddress;
      if ( msg->GetHeaderLine("List-Post", listPostAddress) )
      {
         listPostAddress = ExtractListPostAddress(listPostAddress);
         if ( !listPostAddress.empty() )
         {
            listAddresses.Add(listPostAddress);
         }
      }
   }

   // some addresses may appear also in Reply-To (assuming we used it) and
   // some also may correpond to our own addresses - we exclude them as we
   // don't usually want to reply to ourselves
   //
   // and in the same loop we also detect which addresses correspond to the
   // mailing lists
   wxArrayInt addressesToIgnore,
              addressesList;
   for ( n = 0; n < uniqueAddresses.GetCount(); n++ )
   {
      String addr = uniqueAddresses[n];
      if ( Message::FindAddress(replyToAddresses, addr) != wxNOT_FOUND ||
           Message::FindAddress(ownAddresses, addr) != wxNOT_FOUND )
      {
         addressesToIgnore.Add(n);
      }

      // if we're not in REPLY_LIST mode the listAddresses array is empty
      // anyhow so it doesn't hurt
      if ( Message::FindAddress(listAddresses, addr) != wxNOT_FOUND )
      {
         addressesList.Add(n);
      }
   }

   if ( replyKind == MailFolder::REPLY_LIST )
   {
      // again special case for list reply: we wish to have the list addresses
      // first and all the others later
      size_t countLists = addressesList.GetCount();
      if ( countLists )
      {
         for ( size_t n = 0; n < countLists; n++ )
         {
            cv->AddTo(uniqueAddresses[addressesList[n]]);
         }
      }
      else // no mailing list addresses found
      {
         // again, the same logic: if we explicitly asked to reply to the list
         // than we must think that the message was sent to the list and so we
         // should complain loudly if this is not the case - OTOH, if replying
         // to the list is just the default reply mode, it should work for all
         // messages without errors
         if ( explicitReplyKind )
         {
            wxLogWarning(_("None of the message recipients is a configured "
                           "mailing list, maybe you should configure the "
                           "list of the mailing list addresses?"));
         }

         // in any case fall back to group reply - this seems to be the safest
         // solution
         replyKind = MailFolder::REPLY_ALL;
      }
   }

   count = uniqueAddresses.GetCount();
   for ( n = 0; n < count; n++ )
   {
      if ( addressesToIgnore.Index(n) != wxNOT_FOUND )
      {
         // ignore this one
         continue;
      }

      String address = uniqueAddresses[n];

      // what we do with this address depends on the kind of reply
      Composer::RecipientType rcptType;
      switch ( replyKind )
      {
         default:
            FAIL_MSG( "unexpected reply kind" );
            // fall through

         case MailFolder::REPLY_SENDER:
            // still add addresses  to allow easily adding them to the
            // recipient list - just disable them by default
            rcptType = Composer::Recipient_None;
            break;

         case MailFolder::REPLY_ALL:
            // reply to everyone and their dog
            //
            // but if the dog was only cc'ed, we should keep cc'ing it
            rcptType =
               Message::FindAddress(ccAddresses, address) == wxNOT_FOUND
                  ? Composer::Recipient_To
                  : Composer::Recipient_Cc;
            break;

         case MailFolder::REPLY_LIST:
            // if it is a list address, we already dealt with it above
            if ( addressesList.Index(n) != wxNOT_FOUND )
               continue;

            // do keep the others but disabled by default
            rcptType = Composer::Recipient_None;
            break;
      }

      cv->AddRecipients(uniqueAddresses[n], rcptType);
   }
}

/* static */
Composer *
MailFolder::ReplyMessage(Message *msg,
                         const MailFolder::Params& params,
                         Profile *profile,
                         wxWindow *parent,
                         Composer *cv)
{
   CHECK( msg, NULL, "no message to reply to");

   if(! profile)
      profile = mApplication->GetProfile();

   if ( !cv )
   {
      MailFolder::ReplyKind replyKind;
      GetReplyKind(params, profile, replyKind);

      if ( replyKind == MailFolder::FOLLOWUP_TO_NEWSGROUP )
      {
         cv = Composer::CreateFollowUpArticle(params, profile, msg);
      }
      else
      {
         cv = Composer::CreateReplyMessage(params, profile, msg);
      }

      CHECK( cv, NULL, "failed to create composer" );
   }

   InitRecipients(cv, msg, params, profile);

   // construct the new subject
   String newSubject;
   String replyPrefix = READ_CONFIG(profile, MP_REPLY_PREFIX);

   // FIXME: original subject encoding is lost here
   String subject = DecodeHeader(msg->Subject());

   // we may collapse "Re:"s in the subject if asked for it
   enum CRP // this is an abbreviation of "collapse reply prefix"
   {
      NoCollapse,
      DumbCollapse,
      SmartCollapse
   } collapse = (CRP)(long)READ_CONFIG(profile, MP_REPLY_COLLAPSE_PREFIX);

   if ( collapse != NoCollapse )
   {
      // the default value of the reply prefix is "Re: ", yet we want to
      // match something like "Re[2]: " in replies, so hack it a little
      String replyPrefixWithoutColon(replyPrefix);
      replyPrefixWithoutColon.Trim();
      if ( replyPrefixWithoutColon.Last() == ':' )
      {
         // throw away last character
         replyPrefixWithoutColon.Truncate(replyPrefixWithoutColon.Len() - 1);
      }

      // determine the reply level (level is 0 for first reply, 1 for the
      // reply to the reply &c)
      size_t replyLevel = 0;

            // the search is case insensitive
      wxString subjectLower(subject.Lower()),
         replyPrefixLower(replyPrefixWithoutColon.Lower());
      const char *pStart = subjectLower.c_str();
      for ( ;; )
      {
         // search for the standard string, for the configured string, and
         // for the translation of the standard string (notice that the
         // standard string should be in lower case because we transform
         // everything to lower case)
         static const char *replyPrefixStandard = gettext_noop("re");

         size_t matchLen = 0;
         const char *pMatch = strstr(pStart, replyPrefixLower);
         if ( !pMatch )
            pMatch = strstr(pStart, replyPrefixStandard);
         else if ( !matchLen )
            matchLen = replyPrefixLower.length();
         if ( !pMatch )
            pMatch = strstr(pStart, _(replyPrefixStandard));
         else if ( !matchLen )
            matchLen = strlen(replyPrefixStandard);
         if ( !pMatch
              || (*(pMatch+matchLen) != '[' &&
                  *(pMatch+matchLen) != ':'
                  && *(pMatch+matchLen) != '(')
            )
            break;
         else if ( !matchLen )
            matchLen = strlen(_(replyPrefixStandard));
         pStart = pMatch + matchLen;
         replyLevel++;
      }

      //            if ( replyLevel == 1 )
      //            {
      // try to see if we don't have "Re[N]" string already
      int replyLevelOld;
      if ( sscanf(pStart, "[%d]", &replyLevelOld) == 1 ||
           sscanf(pStart, "(%d)", &replyLevelOld) == 1 )
      {
         replyLevel += replyLevelOld;
         replyLevel --; // one was already counted
         pStart++; // opening [ or (
         while( isdigit(*pStart) )
            pStart ++;
         pStart++; // closing ] or )
      }
      //            }

            // skip spaces
      while ( isspace(*pStart) )
         pStart++;

            // skip also the ":" after "Re" is there was one
      if ( replyLevel > 0 && *pStart == ':' )
      {
         pStart++;

         // ... and the spaces after ':' if any too
         while ( isspace(*pStart) )
            pStart++;
      }

      // this is the start of real subject
      subject = subject.Mid(pStart - subjectLower.c_str());

      if ( collapse == SmartCollapse && replyLevel > 0 )
      {
         // TODO not configurable enough, allow the user to specify the
         //      format string himself and also decide whether we use powers
         //      of 2, just multiply by 2 or nothing at all
         // for now we just increment the replyLevel by one,
         // everything else is funny as it doesn't maintain
         // powers of two :-) KB
         replyLevel ++;
         newSubject.Printf("%s[%d]: %s",
                           replyPrefixWithoutColon.c_str(),
                           replyLevel,
                           subject.c_str());
      }
   }

   // in cases of {No|Dumb}Collapse we fall here
   if ( !newSubject )
   {
      newSubject = replyPrefix + subject;
   }

   cv->SetSubject(newSubject);

   // other headers
   static const char *headers[] =
   {
      "Message-Id",
      "References",
      NULL
   };

   wxArrayString headersOrig = msg->GetHeaderLines(headers);

   String messageid = headersOrig[0].Trim(TRUE).Trim(FALSE);

   // some message don't contain Message-Id at all
   if ( !messageid.empty() )
   {
      String references = headersOrig[1].Trim(TRUE).Trim(FALSE);
      if ( !references.empty() )
      {
         // continue "References" header on the next line
         references += "\015\012 ";
      }
      references += messageid;

      cv->AddHeaderEntry("References", references);
      cv->AddHeaderEntry("In-Reply-To", messageid);
   }

   // if configured, set Reply-To to the same address the message we're
   // replying to was sent
   if ( READ_CONFIG(profile, MP_SET_REPLY_FROM_TO) )
   {
      String rt;
      msg->GetHeaderLine("To", rt);
      cv->AddHeaderEntry("Reply-To", rt);
   }

   cv->InitText(msg, params.msgview);

   return cv;
}

/* static */
Composer *
MailFolder::ForwardMessage(Message *msg,
                           const MailFolder::Params& params,
                           Profile *profile,
                           wxWindow *parent,
                           Composer *cv)
{
   CHECK(msg, NULL, "no message to forward");

   if ( !profile )
      profile = mApplication->GetProfile();

   if ( !cv )
   {
      cv = Composer::CreateFwdMessage(params, profile, msg);
      CHECK( cv, NULL, "failed to create composer" );
   }

   // FIXME: we shouldn't assume that all headers have the same encoding
   //        so we should set the subject text and tell the composer its
   //        encoding as well
   cv->SetSubject(READ_CONFIG(profile, MP_FORWARD_PREFIX) +
                     DecodeHeader(msg->Subject()));
   cv->InitText(msg, params.msgview);

   return cv;
}

// ----------------------------------------------------------------------------
// misc
// ----------------------------------------------------------------------------

char MailFolder::GetFolderDelimiter() const
{
   switch ( GetType() )
   {
      default:
         FAIL_MSG( "Don't call GetFolderDelimiter() for this type" );
         // fall through nevertheless

      case MF_POP:
         // the folders of this type don't have subfolders at all
         return '\0';

      case MF_FILE:
      case MF_MH:
      case MF_MDIR:
      case MF_MFILE:
         // the filenames use slash as separator
         return wxFILE_SEP_PATH;

      case MF_NNTP:
      case MF_NEWS:
         // newsgroups components are separated by periods
         return '.';

      case MF_IMAP:
         // for IMAP this depends on server!
         FAIL_MSG( "shouldn't be called for IMAP, unknown delimiter" );

         // guess :-(
         return '/';
   }
}

// ----------------------------------------------------------------------------
// MailFolder flags
// ----------------------------------------------------------------------------

bool
MailFolder::SetFlagForAll(int flag, bool set)
{
   unsigned long nMessages = GetMessageCount();

   if ( !nMessages )
   {
      // no messages to set the flag for
      return true;
   }

   Sequence sequence;
   sequence.AddRange(1, nMessages);

   return SetSequenceFlag(SEQ_MSGNO, sequence, flag, set);
}

bool MailFolder::SetFlag(const UIdArray *sequence, int flag, bool set)
{
   CHECK( sequence, false, "NULL sequence in MailFolder::SetFlag" );

   Sequence seq;
   seq.AddArray(*sequence);

   return SetSequenceFlag(SEQ_UID, seq, flag, set);
}

// ----------------------------------------------------------------------------
// misc static MailFolder methods
// ----------------------------------------------------------------------------

/* static */
bool MailFolder::SaveMessageAsMBOX(const String& filename, const char *content)
{
   wxFile out(filename, wxFile::write);
   bool ok = out.IsOpened();
   if ( ok )
   {
      // when saving messages to a file we need to "From stuff" them to
      // make them readable in a standard mail client (including this one)

      // standard prefix
      String fromLine = "From ";

      // find the from address
      static const char *FROM_HEADER = "From: ";
      const char *p = strstr(content, FROM_HEADER);
      if ( !p )
      {
         // this shouldn't normally happen, but if it does just make it up
         wxLogDebug("Couldn't find from header in the message");

         fromLine += "MAHOGANY-DUMMY-SENDER";
      }
      else // take everything until the end of line
      {
         // extract just the address in angle brackets
         p += strlen(FROM_HEADER);
         const char *q = strchr(p, '<');
         if ( q )
            p = q + 1;

         while ( *p && *p != '\r' )
         {
            if ( q && *p == '>' )
               break;

            fromLine += *p++;
         }
      }

      fromLine += ' ';

      // time stamp
      time_t t;
      time(&t);
      fromLine += ctime(&t);

      ok = out.Write(fromLine);

      if ( ok )
      {
         // write the body
         size_t len = strlen(content);
         ok = out.Write(content, len) == len;
      }
   }

   return ok;
}

/* static */
MLogCircle& MailFolder::GetLogCircle(void)
{
   static MLogCircle s_LogCircle(10);

   return s_LogCircle;
}

// ----------------------------------------------------------------------------
// MailFolder login/password management
// ----------------------------------------------------------------------------

/* static */
void
MailFolder::ProposeSavePassword(MailFolder *mf,
                                MFolder *folder,
                                const String& login,
                                const String& password)
{
   // do not process the events while we're showing the dialog boxes below:
   // this could lead to the new calls to OpenFolder() which would be bad as
   // we're called from inside OpenFolder()
   MEventManagerSuspender suspendEvents;

   // ask the user if he'd like to remember the password for the future:
   // this is especially useful for the folders created initially by the
   // setup wizard as it doesn't ask for the passwords
   if ( MDialog_YesNoDialog
        (
         String::Format
         (
          _("Would you like to permanently remember the password "
            "for the folder '%s'?\n"
            "(WARNING: don't do it if you are concerned about security)"),
            mf->GetName().c_str()
         ),
         NULL,
         MDIALOG_YESNOTITLE,
         M_DLG_YES_DEFAULT,
         M_MSGBOX_REMEMBER_PWD,
         mf->GetName()
        ) )
   {
      folder->SetAuthInfo(login, password);
   }
   else // don't save password permanently
   {
      // but should we keep it at least during this session?
      if ( MDialog_YesNoDialog
           (
               _("Should the password be kept in memory during this\n"
                 "session only (it won't be saved to a disk file)?\n"
                 "If you answer \"No\", you will be asked for the password\n"
                 "each time when the folder is accessed."),
               NULL,
               MDIALOG_YESNOTITLE,
               M_DLG_YES_DEFAULT,
               M_MSGBOX_KEEP_PWD,
               mf->GetName()
           ) )
      {
         ServerInfoEntry *server = ServerInfoEntry::GetOrCreate(folder, mf);
         CHECK_RET( server, "folder which has login must have server as well!" );

         server->SetAuthInfo(login, password);
      }
      //else: don't keep the login info even in memory
   }
}

/* static */
bool MailFolder::GetAuthInfoForFolder(const MFolder *mfolder,
                                      String& login,
                                      String& password,
                                      bool *userEnteredPwd)
{
   if ( !mfolder->NeedsLogin() )
   {
      // nothing to do then, everything is already fine
      return true;
   }

   login = mfolder->GetLogin();
   password = mfolder->GetPassword();

   if ( login.empty() || password.empty() )
   {
      // do we already have login/password for this folder?
      ServerInfoEntry *server = ServerInfoEntry::Get(mfolder);
      if ( server && server->GetAuthInfo(login, password) )
      {
         return true;
      }

      // we don't have password for this folder, ask the user about it
      if ( !MDialog_GetPassword(mfolder->GetFullName(), &login, &password) )
      {
         ERRORMESSAGE((_("Cannot access this folder without a password.")));

         mApplication->SetLastError(M_ERROR_CANCEL);

         return false;
      }

      // remember that the password was entered interactively and propose to
      // remember it if it really works later
      if ( userEnteredPwd )
         *userEnteredPwd = true;
   }

   return true;
}

