///////////////////////////////////////////////////////////////////////////////
// Project:     M
// File name:   modules/NetscapeImporter.cpp
// Purpose:     import all the mail, news and adb relevant properties from
//              Netscape to Mahogany
// Author:      Michele Ravani ( based on Vadim Zeitlin Pine importer)
// Modified by:
// Created:     23.03.01
// CVS-ID:
// Copyright:   (c) 2000 Michele ravani <michele.ravani@bigfoot.com>
// Licence:     M license
///////////////////////////////////////////////////////////////////////////////

//
// This code borrows heavily, very heavily
// from the Pine Import module implemented by Vadim Zeitlin.
//

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// TODO
//  - check superfluos includes

#include "Mpch.h"

#ifndef USE_PCH
   #include "Mcommon.h"

   #include "MFolder.h"

   #include <wx/log.h>
   #include <wx/file.h>       // for wxFile::Exists
   #include <wx/textfile.h>
#endif // USE_PCH

#include <string.h>

#include <wx/dir.h>
#include <wx/confbase.h>      // for wxExpandEnvVars
#include <wx/tokenzr.h>
#include <wx/dynarray.h>
#include <wx/hash.h>

#include "adb/AdbImport.h"
#include "adb/AdbImpExp.h"
#include "MImport.h"

// User/Dev Comments TODO
// - create an outbox folder setting [DONE]
//
// - use flags in folder creation
// The important parameter is "flags" which is a combination of masks defined
// in MImport.h:
//    enum
//    {
//       ImportFolder_SystemImport    = 0x0001, // import system folders
//       ImportFolder_SystemUseParent = 0x0003, // put them under parent
//       ImportFolder_AllUseParent    = 0x0006  // put all folders under parent
//    };

// i.e. you should use the provided parent folder for the normal folders only if
// AllUseParent flag is given (be careful to test for equality, not just for !=
// 0) and the system folders (such as inbox, drafts, ...) may be:

// 1. not imported at all (if !SystemImport)
// 2. imported under root (if !SystemUseParent)
// 3. imported under parent

//  These flags are really confusing, I know, but this is the price to pay for
// the flexiblity Nerijus requested to have for XFMail importer - have a look
// at it, BTW, you will see how it handles system folders (Pine code doesn't do
// it although it probably should as well).
//


// GENERAL TODO
// - set up makefile stuff to create the module out of many
//   header and source files. I like to separate interfaces from
//   implementation and to put each class in a file by the same name.
//   It decreased the number of things one has to know and makes the
//   code browsing simpler.
//
// - find the default location of netscape files on other OS
//
// - import the addressbook. Netscape uses a LDIF(right?), the generic communication
//   formar for LDAP server and clients. The import of the adb could be a side effect
//   of an LDAP integration module ('import from file')
//
// - implement the choice between 'sharing' the mail folders or physically move
//   them to the .M directory.
//
// - using '/' explicitly in the code is not portable [DONE]
//
// - check that the config files is backed up or implement it!
//
// - option to move mail in the Netscape's Inbox to New Mail (others? e.g. drafts, templates)
//
// - import filters
//
// - import newsgroups and status
//
// - check error detection and treatment.
//   (If I hold my breath until I turn blue, can I have exceptions?)
//
// - Misc folders, i.e. foo in foo.sbd
//    + create only if they are not empty
//    + if the group folder with the same name is empty, create only a mailbox folder
//    + rename to 'AAA Misc' to make it appear in the first slot in the folder [ASK]
//
// - implement importing of IMAP server stuff [ASK]




#define NR_SYS_FLD 6
static const wxString sysFolderList[] = {"Drafts",
                                             "Inbox",
                                             "Sent",
                                             "Templates",
                                             "Trash",
                                             "Unsent Messages"};

//------------------------------------------
// Map struct
//------------------------------------------


// this is quite a crude solution, but I guess it'll do for a start,
// as it simplifies the mapping a bit.
//
// TODO:
// - ask for help in mapping those settings I've missed
// - find the complete set of Netscape keys
// - investigate a config file based solution
//   + config.in file with M #defines, preprocessed to create
//     a proper config file, possibly in XML format.
//     maps should be built in the given Import<type>Settings method
// - set up the makefile stuff to do it


#define NM_NONE 0
#define NM_IS_BOOL 1
#define NM_IS_INT 2
#define NM_IS_STRING 3          // empty strings not allowed
#define NM_IS_NEGATE_BOOL 4
#define NM_IS_STRNIL 5          // empty strings allowed

struct PrefMap {
  wxString npKey;     // the key in the Netscape pref file
  wxString mpKey;     // the key in the Mahogany congif file
  wxString desc;      // short description
  unsigned int type;  // type of the Mahogany pref
  bool procd;         // TRUE if entry has been processed.
};





// IDENTITY Preferences
static  PrefMap g_IdentityPrefMap[] = {
  {"mail.identity.username" , MP_PERSONALNAME , "user's full name", NM_IS_STRING, FALSE },
  {"mail.identity.defaultdomain" , MP_HOSTNAME , "default domain", NM_IS_STRING, FALSE },
  {"mail.identity.useremail" , MP_FROM_ADDRESS , "e-mail address", NM_IS_STRING, FALSE },
  {"mail.identity.organization" , "Not mapped", "No descr", NM_NONE, FALSE },
  {"mail.identity.reply_to" , MP_REPLY_ADDRESS , "reply address", NM_IS_STRING, FALSE },
  {"mail.attach_vcard" , MP_USEVCARD , "attach vCard to outgoing messages", NM_IS_BOOL, FALSE },
  {"END", "Ignored", "No descrEond of list record", NM_NONE }   // DO NOT REMOVE, hack to find the end
};

// IDENTITY Preferences
static  PrefMap g_NetworkPrefMap[] = {

 {"mail.smtp_name" , MP_SMTPHOST_LOGIN, "SMTP login name", NM_IS_STRING, FALSE },
 // my guess that netscape uses the same name
 {"mail.smtp_name" , MP_NNTPHOST_LOGIN, "NNTP login name", NM_IS_STRING, FALSE },
 {"mail.pop_name" , MP_USERNAME, "POP username", NM_IS_STRING, FALSE },
 {"mail.pop_password" , "Ignored", "Password for the POP server", NM_IS_STRING, FALSE },
 {"network.hosts.smtp_server" , MP_SMTPHOST, "SMTP Server Name", NM_IS_STRING, FALSE },
 {"network.hosts.nntp_server" , MP_NNTPHOST, "NNTP Server Name", NM_IS_STRING, FALSE },
 {"network.hosts.pop_server" , MP_POPHOST, "POP Server Name", NM_IS_STRING, FALSE },
   // imap stuff is not there yet. It is a bit more complex: a bunch of keys
   // like mail.imap.<servername>.<property>. I don't know enough to make sense
   // out of it at the moment. I may set the imap to nil.
 {"mail.imap.server_sub_directory" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.imap.root_dir" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.imap.local_copies" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.imap.server_ssl" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.imap.delete_is_move_to_trash" , "Not mapped", "No descr", NM_NONE, FALSE },
 // something to use instead of send/fetchmail here?
 {"mail.use_movemail" , MP_USE_SENDMAIL, "use mail moving program", NM_IS_BOOL, FALSE },
 {"mail.use_builtin_movemail" , "Ignored", "No descr", NM_NONE, FALSE },
 {"mail.movemail_program" , MP_SENDMAILCMD, "mail moving command", NM_IS_STRING, FALSE },
 {"mail.movemail_warn" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"END", "Ignored", "End of list record", NM_NONE }   // DO NOT REMOVE, hack to find the end
};


// COMPOSE Preferences
static  PrefMap g_ComposePrefMap[] = {
 {"mail.wrap_long_lines" , MP_AUTOMATIC_WORDWRAP, "automatic line wrap", NM_IS_BOOL, FALSE },
 {"mailnews.wraplength" , MP_WRAPMARGIN, "wrap lenght", NM_IS_INT, FALSE },
   // additional bcc addresses: add to MP_COMPOSE_BCC if this true
 {"mail.default_cc" , "Special", "No descr", NM_NONE, FALSE },
 {"mail.use_default_cc" , "Special", "No descr", NM_NONE, FALSE },
   // directory where sent mail goes
 {"mail.default_fcc" , MP_OUTGOINGFOLDER, "sent mail folder", NM_IS_STRNIL, FALSE }, //where copied
   // if true copy mail to def fcc
 {"mail.use_fcc" , MP_USEOUTGOINGFOLDER, "keep copies of sent mail", NM_IS_BOOL, FALSE },
   // if set, put email addresse in BCC: MP_COMPOSE_BCC
 {"mail.cc_self" , "Special", "No descr", NM_NONE, FALSE },
 {"mail.auto_quote" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.html_compose" , "Ignored", "No descr", NM_NONE, FALSE },
   // set MP_COMPOSE_USE_SIGNATURE if not empty
 {"mail.signature_file" , MP_COMPOSE_SIGNATURE , "filename of signature file", NM_IS_STRNIL, FALSE },
 {"END", "Ignored", "End of list record", NM_NONE }   // DO NOT REMOVE, hack to find the end
};



// FOLDER Preferences
static  PrefMap g_FolderPrefMap[] = {
  // no pref for the name of the outbox
  {"mail.deliver_immediately" , MP_USE_OUTBOX, "send messages later", NM_IS_NEGATE_BOOL, FALSE },
   // map this to a very large number if "mail.check_new_mail" false
  {"mail.check_time" , MP_POLLINCOMINGDELAY, "interval between checks for incoming mail", NM_IS_INT, FALSE },
  // AFAIK cannot switch off polling in M. Could set the interval to a veeery large number
  {"mail.check_new_mail" , "Special", "check mail at intervals", NM_IS_BOOL, FALSE },
   // not sure about this one. I mean ... even less sure than for the others
  {"mail.max_size" , MP_MAX_MESSAGE_SIZE, "max size for downloaded message", NM_IS_INT, FALSE },
  {"END", "Ignored", "End of list record", NM_NONE }   // DO NOT REMOVE, hack to find the end
};

// VIEWER Preferences
static  PrefMap g_ViewerPrefMap[] = {
 {"mail.quoted_style" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.quoted_size" , "Not mapped", "No descr", NM_NONE, FALSE },
   // color for quoted mails: MP_MVIEW_QUOTED_COLOUR1
   // if not "" set MP_MVIEW_QUOTED_COLOURIZE to true
 {"mail.citation_color" , MP_MVIEW_QUOTED_COLOUR1, "color for quoted mails", NM_IS_STRING, FALSE },

 {"mail.wrap_long_lines" , MP_VIEW_AUTOMATIC_WORDWRAP, "automatic line wrap", NM_IS_BOOL, FALSE },
 {"mailnews.wraplength" , MP_VIEW_WRAPMARGIN, "wrap lenght", NM_IS_INT, FALSE },
 {"mail.thread_mail" , MP_MSGS_USE_THREADING, "display mail threads", NM_IS_BOOL, FALSE },

  {"END", "Ignored", "End of list record", NM_NONE }   // DO NOT REMOVE, hack to find the end
};


static  PrefMap g_RestPrefMap[] = {
  // very specially treated
 {"mail.directory" , "Special", "mail directory", NM_IS_STRING, FALSE },

 {"helpers.global_mime_types_file" , MP_MIMETYPES, "global mime types file", NM_IS_STRING, FALSE },
 {"helpers.private_mime_types_file" , MP_MIMETYPES, "private mime types file", NM_IS_STRING, FALSE },

 {"helpers.global_mailcap_file" , MP_MAILCAP, "global mailcap file", NM_IS_STRING, FALSE },
 {"helpers.private_mailcap_file" , MP_MAILCAP, "private mailcap file", NM_IS_STRING, FALSE },

 {"print.print_command" , MP_PRINT_COMMAND, "print command", NM_IS_STRING, FALSE },
 {"print.print_reversed" , "Ignored", "No descr", NM_NONE, FALSE },
 {"print.print_color" , MP_PRINT_COLOUR, "print color", NM_IS_BOOL, FALSE },
 {"print.print_landscape" , MP_PRINT_ORIENTATION, "print orientation", NM_IS_NEGATE_BOOL, FALSE },
 // see what is used in Netscape
 {"print.print_paper_size" , MP_PRINT_PAPER, "paper size", NM_IS_STRING, FALSE },

 {"intl.character_set" , "Ignored", "No descr", NM_NONE, FALSE },
 {"intl.font_charset" , "Ignored", "No descr", NM_NONE, FALSE },
 {"intl.font_spec_list" , "Ignored", "No descr", NM_NONE, FALSE },
 {"intl.accept_languages" , "Ignored", "No descr", NM_NONE, FALSE },

 {"mail.play_sound" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.strictly_mime" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.file_attach_binary" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.addr_book.lastnamefirst" , "Not mapped", "No descr", NM_NONE, FALSE },

 {"mail.signature_date" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.leave_on_server" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.limit_message_size" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.prompt_purge_threshhold" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.purge_threshhold" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.use_mapi_server" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.server_type" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.fixed_width_messages" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.empty_trash" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.remember_password" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.support_skey" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.pane_config" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.sort_by" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mail.default_html_action" , "Not mapped", "No descr", NM_NONE, FALSE },

 {"mailnews.reuse_message_window" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mailnews.reuse_thread_window" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mailnews.message_in_thread_window" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mailnews.nicknames_only" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mailnews.reply_on_top" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"mailnews.reply_with_extra_lines" , "Not mapped", "No descr", NM_NONE, FALSE },

 {"network.ftp.passive" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"network.max_connections" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"network.tcpbufsize" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"network.hosts.socks_server" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"network.hosts.socks_serverport" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"network.proxy.ftp" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"network.proxy.ftp_port" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"network.proxy.http" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"network.proxy.http_port" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"network.proxy.gopher" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"network.proxy.gopher_port" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"network.proxy.wais" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"network.proxy.wais_port" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"network.proxy.ssl" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"network.proxy.ssl_port" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"network.proxy.no_proxies_on" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"network.proxy.type" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"network.proxy.autoconfig_url" , "Not mapped", "No descr", NM_NONE, FALSE },

 {"news.default_cc" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"news.default_fcc" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"news.cc_self" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"news.use_fcc" , "Not mapped", "No descr", NM_NONE, FALSE },

 {"news.directory" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"news.notify.on" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"news.max_articles" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"news.cache_xover" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"news.show_first_unread" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"news.sash_geometry" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"news.thread_news" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"news.pane_config" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"news.sort_by" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"news.keep.method" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"news.keep.days" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"news.keep.count" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"news.keep.only_unread" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"news.remove_bodies.by_age" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"news.remove_bodies.days" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"news.server_port" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"news.server_is_secure" , "Not mapped", "No descr", NM_NONE, FALSE },

 {"offline.startup_mode" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"offline.news.download.unread_only" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"offline.news.download.by_date" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"offline.news.download.use_days" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"offline.news.download.days" , "Not mapped", "No descr", NM_NONE, FALSE },
 {"offline.news.download.increments" , "Not mapped", "No descr", NM_NONE, FALSE },

 {"security.email_as_ftp_password" , "Ignored", "No descr", NM_NONE, FALSE },
 {"security.submit_email_forms" , "Ignored", "No descr", NM_NONE, FALSE },
 {"security.warn_entering_secure" , "Ignored", "No descr", NM_NONE, FALSE },
 {"security.warn_leaving_secure" , "Ignored", "No descr", NM_NONE, FALSE },
 {"security.warn_viewing_mixed" , "Ignored", "No descr", NM_NONE, FALSE },
 {"security.warn_submit_insecure" , "Ignored", "No descr", NM_NONE, FALSE },
 {"security.enable_java" , "Ignored", "No descr", NM_NONE, FALSE },
 {"javascript.enabled" , "Ignored", "No descr", NM_NONE, FALSE },
 {"security.enable_ssl2" , "Ignored", "No descr", NM_NONE, FALSE },
 {"security.enable_ssl3" , "Ignored", "No descr", NM_NONE, FALSE },
 {"security.ciphers" , "Ignored", "No descr", NM_NONE, FALSE },
 {"security.default_personal_cert" , "Ignored", "No descr", NM_NONE, FALSE },
 {"security.use_password" , "Ignored", "No descr", NM_NONE, FALSE },
 {"security.ask_for_password" , "Ignored", "No descr", NM_NONE, FALSE },
 {"security.password_lifetime" , "Ignored", "No descr", NM_NONE, FALSE },
 {"custtoolbar.has_toolbar_folder" , "Ignored", "No descr", NM_NONE, FALSE },
 {"custtoolbar.personal_toolbar_folder" , "Ignored", "No descr", NM_NONE, FALSE },
 {"editor.author" , "Ignored", "No descr", NM_NONE, FALSE },
 {"editor.html_editor" , "Ignored", "No descr", NM_NONE, FALSE },
 {"editor.image_editor" , "Ignored", "No descr", NM_NONE, FALSE },
 {"editor.template_location" , "Ignored", "No descr", NM_NONE, FALSE },
 {"editor.auto_save_delay" , "Ignored", "No descr", NM_NONE, FALSE },
 {"editor.use_custom_colors" , "Ignored", "No descr", NM_NONE, FALSE },
 {"editor.background_color" , "Ignored", "No descr", NM_NONE, FALSE },
 {"editor.text_color" , "Ignored", "No descr", NM_NONE, FALSE },
 {"editor.link_color" , "Ignored", "No descr", NM_NONE, FALSE },
 {"editor.active_link_color" , "Ignored", "No descr", NM_NONE, FALSE },
 {"editor.followed_link_color" , "Ignored", "No descr", NM_NONE, FALSE },
 {"editor.background_image" , "Ignored", "No descr", NM_NONE, FALSE },
 {"editor.publish_keep_links" , "Ignored", "No descr", NM_NONE, FALSE },
 {"editor.publish_keep_images" , "Ignored", "No descr", NM_NONE, FALSE },
 {"editor.publish_location" , "Ignored", "No descr", NM_NONE, FALSE },
 {"editor.publish_username" , "Ignored", "No descr", NM_NONE, FALSE },
 {"editor.publish_password" , "Ignored", "No descr", NM_NONE, FALSE },
 {"editor.publish_save_password" , "Ignored", "No descr", NM_NONE, FALSE },
 {"editor.publish_browse_location" , "Ignored", "No descr", NM_NONE, FALSE },
 {"editor.show_copyright" , "Ignored", "No descr", NM_NONE, FALSE },

 {"fortezza.toggle" , "Ignored", "No descr", NM_NONE, FALSE },
 {"fortezza.timeout" , "Ignored", "No descr", NM_NONE, FALSE },

 {"general.startup.browser" , "Ignored", "No descr", NM_NONE, FALSE },
 {"general.startup.mail" , "Ignored", "No descr", NM_NONE, FALSE },
 {"general.startup.news" , "Ignored", "No descr", NM_NONE, FALSE },
 {"general.startup.editor" , "Ignored", "No descr", NM_NONE, FALSE },
 {"general.startup.conference" , "Ignored", "No descr", NM_NONE, FALSE },
 {"general.startup.netcaster" , "Ignored", "No descr", NM_NONE, FALSE },
 {"general.startup.calendar" , "Ignored", "No descr", NM_NONE, FALSE },
 {"general.always_load_images" , "Ignored", "No descr", NM_NONE, FALSE },
 {"general.help_source.site" , "Ignored", "No descr", NM_NONE, FALSE },
 {"general.help_source.url" , "Ignored", "No descr", NM_NONE, FALSE },
 {"images.dither" , "Ignored", "No descr", NM_NONE, FALSE },
 {"images.incremental_display" , "Ignored", "No descr", NM_NONE, FALSE },

 {"END", "Ignored", "No descr", NM_NONE }   // DO NOT REMOVE, hack to find the end
 };


// this should be ok on unix
static const wxString g_VarIdent      = "user_pref";   // identifies a key-value entry
static const wxString g_GlobalPrefDir = "/usr/lib/netscape";
static const wxString g_PrefFile      = "preferences.js";
static const wxString g_PrefFile2      = "liprefs.js";
static const wxString g_DefNetscapePrefDir = "$HOME/.netscape";
static const wxString g_DefNetscapeMailDir = "$HOME/nsmail";

static const wxChar   g_CommentChar   = '/';

//---------------------------------------------
// UTILITY CLASSES
//---------------------------------------------

// TODO
// - pile this stuff into header and impl classes in  a modules subdir
//   nicely one class per file.
// - makefile infrastructure to create .so and .a


// ----------------------------------------------------------------------------
// class MyFolderArray
//   simply a wxArray of MFolder pointers that calls DecRef for the items
//   in the array when destroyed. Simplifies cleanup
// ----------------------------------------------------------------------------

WX_DEFINE_ARRAY(MFolder *, FolderArray);

class MyFolderArray: public FolderArray
{
public:

  ~MyFolderArray()
   {
     for (unsigned k = 0; k < GetCount(); k++ )
      Item(k)->DecRef();
   }
};


// ----------------------------------------------------------------------------
// class MyHashTable
//   utility class to get the primitives from the hashtable
// ----------------------------------------------------------------------------

class MyHashTable
{
public:

  MyHashTable();
  ~MyHashTable();

  bool Exist(const wxString& key) const;
  bool GetValue(const wxString& key, bool& value) const;
  bool GetValue(const wxString& key, wxString& value ) const;
  bool GetValue(const wxString& key, unsigned long& value ) const;

  void Put(const wxString& key, const wxString& val);
  void Delete(const wxString& key);

private:
  wxHashTable m_tbl;

};

MyHashTable::MyHashTable()
  : m_tbl(wxKEY_STRING)
{}

// I'm not too sure about this stuff ...
// It is getting down to details I can't remember
// why isn't a wxString a wxObject? What about hashes of strings?
MyHashTable::~MyHashTable()
{
  // should delete the strings hier;
  m_tbl.BeginFind();
  wxString *tmp = NULL;
  wxNode* node = NULL;
  while ( (node = m_tbl.Next()) != NULL )
   if ((tmp = (wxString*)node->Data()))
     delete tmp;

  //  m_tbl.DeleteContents(FALSE);  // just ot make sure, they are deleted

}

void MyHashTable::Put(const wxString& key, const wxString& val)
{
  wxString* tmp = new wxString(val);
  m_tbl.Put(key,(wxObject*)tmp);
}

void MyHashTable::Delete(const wxString& key)
{
  wxString* tmp = (wxString*) m_tbl.Delete(key);
  delete tmp;
}

bool MyHashTable::Exist(const wxString& key) const
{
  wxString* tmp = (wxString *)m_tbl.Get(key.c_str());
  return ( tmp != NULL );
}

bool MyHashTable::GetValue(const wxString& key, bool& value) const
{
  value = FALSE;
  wxString* tmp = (wxString *)m_tbl.Get(key.c_str());
  if ( tmp )
   {
     value = (( *tmp == "true" ) || ( *tmp == "TRUE" ) || ( *tmp == "1"));
     return TRUE;
   }
  else
   return FALSE;
}

bool MyHashTable::GetValue(const wxString& key, wxString& value ) const
{
  value.Empty();
  wxString* tmp = (wxString *)m_tbl.Get(key.c_str());
  if ( tmp )
   {
     value = *tmp;
     return TRUE;
   }
  else
   return FALSE;
}

bool MyHashTable::GetValue(const wxString& key, unsigned long& value ) const
{
  wxString* tmp = (wxString *)m_tbl.Get(key.c_str());

  if ( tmp && tmp->ToULong(&value) )
   return TRUE;

  value = (unsigned long)-1; // FIXME: is this really needed?

  return FALSE;
}


//---------------------------------------------
// CLASS MNetscapeImporter
//---------------------------------------------

class MNetscapeImporter : public MImporter
{
public:

  MNetscapeImporter();

  virtual bool Applies() const;
  virtual int  GetFeatures() const;

  virtual bool ImportADB();
  virtual bool ImportFolders(MFolder *folderParent, int flags);
  virtual bool ImportSettings();
  virtual bool ImportFilters();

  DECLARE_M_IMPORTER();

private:

  // call ImportSettingsFromFile() if the file exists, do nothing (and return
  // TRUE) otherwise
  bool ImportSettingsFromFileIfExists( const wxString& prefs );

  // parses the preferences file to find key-value pairs
  bool ImportSettingsFromFile( const wxString& prefs );
  bool ImportIdentitySettings ( MyHashTable& tbl );
  bool ImportNetworkSettings ( MyHashTable& tbl );
  bool ImportComposeSettings ( MyHashTable& tbl );
  bool ImportFolderSettings ( MyHashTable& tbl );
  bool ImportViewerSettings ( MyHashTable& tbl );
  bool ImportRestSettings ( MyHashTable& tbl );

  bool ImportSettingList( PrefMap* map, const MyHashTable& tbl);

  bool WriteProfileEntry(const wxString& key, const wxString& val, const wxString& desc );
  bool WriteProfileEntry(const wxString& key, const int val, const wxString& desc );
  bool WriteProfileEntry(const wxString& key, const bool val, const wxString& desc );

  bool CreateFolders(MFolder *parent, const wxString& dir, int flags);

  wxString m_MailDir;    // it is a free user choice, stored in preferences
  wxString m_PrefDir;    // it is a free user choice


};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// ctor & dtor
// ----------------------------------------------------------------------------

MNetscapeImporter::MNetscapeImporter()
  : m_MailDir(wxExpandEnvVars(g_DefNetscapeMailDir)),
    m_PrefDir(wxExpandEnvVars(g_DefNetscapePrefDir))
{}


// ----------------------------------------------------------------------------
// generic
// ----------------------------------------------------------------------------

// TODO: mention with which Netscape versions it works (i.e. should it be
//       "Import settings from Netscape 4.x"? "6.x"?)
IMPLEMENT_M_IMPORTER(MNetscapeImporter, "Netscape",
                     gettext_noop("Import settings from Netscape"));

int MNetscapeImporter::GetFeatures() const
{
  // return Import_Folders | Import_Settings | Import_ADB;
  return Import_Folders | Import_Settings;
}

// ----------------------------------------------------------------------------
// determine if Netscape is installed
// ----------------------------------------------------------------------------

bool MNetscapeImporter::Applies() const
{
  // just check for ~/.netscape directory
  bool b = wxDir::Exists(m_PrefDir.c_str());
  return b;
}

// ----------------------------------------------------------------------------
// import the Netscape ADB
// ----------------------------------------------------------------------------

bool MNetscapeImporter::ImportADB()
{
#if 0
   // import the Netscape address book from ~/.addressbook
   AdbImporter *importer = AdbImporter::GetImporterByName("AdbNetscapeImporter");
   if ( !importer )
   {
      wxLogError(_("%s address book import module not found."), "Netscape");

      return FALSE;
   }

   wxString filename = importer->GetDefaultFilename();
   wxLogMessage(_("Starting importing %s address book '%s'..."),
                "Netscape", filename.c_str());
   bool ok = AdbImport(filename, "Netscape.adb", "Netscape Address Book", importer);

   importer->DecRef();

   if ( ok )
      wxLogMessage(_("Successfully imported %s address book."), "Netscape");
   else
      wxLogError(_("Failed to import %s address book."), "Netscape");

   return ok;
#endif // 0

  return FALSE;
}

// ----------------------------------------------------------------------------
// import the Netscape folders
// ----------------------------------------------------------------------------

bool MNetscapeImporter::ImportFolders(MFolder *folderParent, int flags)
{
  // create a folder for each mbox file

  // if the mail dir was found in the preferences, it will be used
  // otherwise the default ($HOME/nsmail) will have to do.

  if (! wxDir::Exists(m_MailDir.c_str()) )
   {
     // TODO
     // - ask the user for his Netscape mail dir
     //   On the other hand it should ahve been read in prefs
     wxLogMessage(_("Cannot import folders, directory '%s' doesn't exist"),
                  m_MailDir.c_str());
     return FALSE;
   }

  wxDir dir(m_MailDir);
  if ( ! dir.IsOpened() )    // if it can't be opened bail out
   return FALSE;          // looks like the isOpened already logs a message

  // the parent for all folders: use the given one, don't create an extra level
  // of indirection if the user doesn't want it
  if ( CreateFolders(folderParent, m_MailDir, flags) )
   {
     MEventManager::Send
      (
       new MEventFolderTreeChangeData
       (
        folderParent ? folderParent->GetFullName() : String(""),
        MEventFolderTreeChangeData::CreateUnder
        )
       );
   }
  // then is ok
  else
   // TODO
   // - remove the created folders, something went wrong
   return FALSE;

  return TRUE;
}


// Recursively called method which creates the FoFs and mail folders
// as found in Netscape.
// Netscape allows to store messages in the FoFs. It implements this
// functionality by creating a file with the name of the folder and
// at the same time a directory (same name + '.sbd') where the subfolders
// will be stored.
// M lacks this functionality, therefore messages in the Netscape FoF will
// be stored in a subfolder called "Misc"

bool MNetscapeImporter::CreateFolders(MFolder *parent,
                                      const wxString& dir,
                                      int flags )
{
  // based on Pine and XFMail Importer by Vadim and Nerijus
  static int level = 0;

  wxDir currDir(dir);

  if ( ! currDir.IsOpened() )    // if it can't be opened bail out
   return FALSE;          // looks like the isOpened already logs a message

  // find the folders
  wxString filename;
  wxArrayString fileList;
  bool cont = currDir.GetFirst(&filename, "*", wxDIR_FILES);
  while ( cont )
   {
     fileList.Add(filename);
     cont = currDir.GetNext(&filename);
   }

  // find the Folders of Folders (FoF)
  wxArrayString dirList;
  cont = currDir.GetFirst(&filename, "*.sbd", wxDIR_DIRS); // FoF in Netscape have .sbd extension
  while ( cont )
   {
     dirList.Add(filename);
     cont = currDir.GetNext(&filename);
   }

  // not really necessary
  fileList.Sort();
  dirList.Sort();

  if ( !fileList.GetCount() && !dirList.GetCount() )
   {
    wxLogMessage(_("No folders found in '%s'."), dir.c_str());

    // we can consider the operation successful
    return TRUE;
   }

  // as far as I know there isn't a flag in Netscape to mark
  // system folders, but I don't know much. So, if you know
  // please let me know

  // Usually, system folders are at root level and have given names
  // Eliminate them from the file list, if user doesn't want to import them

  if (level == 0) {                             // only at root level
    if (!(flags & ImportFolder_SystemImport) )      // don't want to import them
      {
        for (int i=0; i<NR_SYS_FLD; i++)
          fileList.Remove(sysFolderList[i]); 
        wxLogMessage("NOTE: >>>>>>> Netscape system folders not imported");
      }
  }    

  MFolder *folder = NULL;
  MFolder *subFolder = NULL;
  MyFolderArray folderList;
  wxString dirFldName;

  folderList.Alloc(25);

  // loop through the found directories
  // for each one,
  // - create a folder
  // - check if a file with the 'same' name exists
  // - if yes, create a subfolder named "Misc"
  // - call this method for the directory
  // - create all the 'real' mail folders

  // TODO
  // - in the case of failure, remove the created folders
  //   (essentially before each 'return FALSE;'
  // - [DONE] find out the type (MF_?) of FoFs [DONE]

  // in the next for loop no system folders will be treated anyway
  MFolder *tmpParent = NULL;
  if (level == 0) {
    if ( (flags & ImportFolder_AllUseParent)
         == ImportFolder_AllUseParent )
      tmpParent = parent;
  }
  else 
    tmpParent = parent;   // at lower levels, the dad is known
  
  for ( size_t n = 0; n < dirList.GetCount(); n++ )
   {
     const wxString& name = dirList[n];
     wxString path;
     path << dir << DIR_SEPARATOR << name;
     dirFldName = name.BeforeLast('.');

     // TODO: determine if this is a system folder (probably just by name, i.e.
     //       compare with Inbox, Outbox, Drafts, ...) and process it
     //       accordingly to the flags [DONE?]
     folder = CreateFolderTreeEntry (tmpParent,    // parent may be NULL
                                     dirFldName,      // the folder name
                                     MF_GROUP,   //            type
                                     0,         //            flags
                                     path,      //            path
                                     FALSE      // don't notify
                                     );

     if ( folder )
      {
        folderList.Add(folder);
        wxLogMessage(_("Imported group folder: %s."),dirFldName.c_str());
      }
     else
      return FALSE;

     // check if there is a file matching (without .sbd)
     int i = fileList.Index( dirFldName.c_str() );
     if ( i != wxNOT_FOUND)
      {
        // TODO
        // - ask the user what name does he want to give to the folder

        wxString tmpPath;
        tmpPath << dir << DIR_SEPARATOR << fileList[i];
        subFolder = CreateFolderTreeEntry ( folder,    // parent may be NULL
                                   "AAA Misc",      // the folder name
                                   MF_FILE,   //            type
                                   0,         //            flags
                                   tmpPath,      //            path
                                   FALSE      // don't notify
                                   );


        if ( subFolder )
         {
           folderList.Add(subFolder);
           fileList.Remove(i);      // this one has been created, remove from filelist
           wxLogMessage(_("NOTE: >>>>>> Created 'AAA Misc' folder to contain the msgs currently in group folder %s."),
                        dirFldName.c_str());
         }
        else
         return FALSE;
      }

     // crude way to know if we are at the root mail dir level
     // another would be to check if the current dir is m_MailDir
     level++; 
     // recursive call
     if ( ! CreateFolders( folder, path, flags ) )
      return FALSE;
     level--;
   }

  // we have created all the directories, etc
  // now create the normal folders

  for ( size_t n = 0; n < fileList.GetCount(); n++ )
  {
    const wxString& name = fileList[n];
    wxString path;
    path << dir << DIR_SEPARATOR << name;

     // find out the folder type (system or not) by walking the list
     // to know how to set the parent folder (accordig to flags)
    tmpParent = NULL;
     
    if (level == 0) {
      // look mum, I'm making fire with two stones!
      bool found = FALSE;
      for (int i=0; i<NR_SYS_FLD; i++)
        if (name == sysFolderList[i]) {
          found = TRUE;
          break;
        }
      
      if ( ! found )  {    // it isn't a system folder
        if ( (flags & ImportFolder_AllUseParent)
             == ImportFolder_AllUseParent )
          tmpParent = parent;
      }
      else                        // it is, it is
        if ( (flags & ImportFolder_SystemUseParent) 
             == ImportFolder_SystemUseParent )
          tmpParent = parent;
    }
    else 
      tmpParent = parent;   // at lower levels, the dad is known
    
    folder = CreateFolderTreeEntry ( tmpParent,    // parent may be NULL
                                     name,      // the folder name
                                     MF_FILE,   //            type
                                     0,         //            flags
                                     path,      //            path
                                     FALSE      // don't notify
                                     );
    if ( folder )
      {
        folderList.Add(folder);
        wxLogMessage(_("Imported mail folder: %s "), name.c_str());
      }
    else
      return FALSE;
  }
  
  return TRUE;
  }



// ----------------------------------------------------------------------------
// import the Netscape settings
// ----------------------------------------------------------------------------

bool MNetscapeImporter::ImportSettings()
{

   // parse different netscape config files and pick the settings of interest to
   // us from it. Try the global preference file first

  wxString filename = g_GlobalPrefDir + DIR_SEPARATOR + g_PrefFile;

  if ( ! ImportSettingsFromFileIfExists(filename) )
   wxLogMessage(_("Couldn't import the global preferences file: %s."),
             filename.c_str());

  // user own preference file
  // entries here will override the global settings
  // TODO
  // - check which one is more recent
  filename = m_PrefDir + DIR_SEPARATOR + g_PrefFile;

  if (! wxFile::Exists(filename.c_str()) )
   {
     // TODO
     // - ask user if he knows where the prefs file is

     return FALSE;
   }


  // TODO
  // - check if g_PrefFile2 is younger or older than g_PrefFile
  //   (use wxFileModificationTime)
  bool status = ImportSettingsFromFileIfExists(filename);
  if ( !status )
  {
     wxLogMessage(_("Couldn't import the user preferences file: %s."),
                  filename.c_str());
  }

  filename = m_PrefDir + DIR_SEPARATOR + g_PrefFile2;
  if (status)
   status = ImportSettingsFromFileIfExists(filename);
  //else: hmm, what message should we give here?

  return status;
}

bool
MNetscapeImporter::ImportSettingsFromFileIfExists(const wxString& filename)
{
   if ( !wxFile::Exists(filename) )
   {
      // pretend everything is ok
      return TRUE;
   }

   return ImportSettingsFromFile(filename);
}


bool MNetscapeImporter::ImportSettingsFromFile(const wxString& filename)
{
  wxTextFile file(filename);
  if ( !file.Open() )
      return FALSE;

  wxString token;
  wxStringTokenizer tkz;
  wxString varName;
  wxString value;
  wxString msg;
  size_t nLines = file.GetLineCount();



  MyHashTable keyval;   // to keep the key-value pairs

  for ( size_t nLine = 0; nLine < nLines; nLine++ )
   {
      const wxString& line = file[nLine];

      // skip empty lines and the comments
      if ( !line || line[0] == g_CommentChar)
      continue;

      // extract variable name and its value
      int nEq = line.Find(g_VarIdent);

     // lines which do not contain a key-value pair will log a message
      if ( nEq == wxNOT_FOUND )
      {
         wxLogDebug("%s(%u): missing variable identifier ('%s').", filename.c_str(), nLine + 1,g_VarIdent.c_str());

         // skip line
         continue;
      }

     // Very, Very primitive parsing of the prefs line
     // TODO
     //  - check out alternatives
     //    + reuse mozilla code
     //    + write a small parser
     //    + use regex

     tkz.SetString(line, "(,)");
     token = tkz.GetNextToken();  // get rid of the first part
     varName = tkz.GetNextToken();  // the variable name
     value = tkz.GetNextToken();

     // get rid of white space
     value.Trim(); value.Trim(FALSE);
     varName.Trim(); varName.Trim(FALSE);

     // clean away eventual quotes
     if (varName[0] == '"' && varName[varName.Len()-1] == '"')
      varName = varName(1,varName.Len()-2);

     if (value[0] == '"' && value[value.Len()-1] == '"')
      value = value(1,value.Len()-2);

     // and now the white space again, e.g. " \" the value \""
     value.Trim(); value.Trim(FALSE);
     varName.Trim(); varName.Trim(FALSE);

     // key-value found (hopefully), add to hashtable
     keyval.Put(varName,value);
   }

  // a special case
  if ( keyval.Exist("mail.directory") )
   keyval.GetValue("mail.directory",m_MailDir);

  ImportIdentitySettings( keyval );
  ImportNetworkSettings ( keyval );
  ImportComposeSettings ( keyval );
  ImportFolderSettings ( keyval );
  ImportViewerSettings ( keyval );
  ImportRestSettings ( keyval );


  return TRUE;
}

bool MNetscapeImporter::ImportIdentitySettings ( MyHashTable& tbl )
{
  wxLogMessage(">>>>>>>>>> Import identity settings");

  PrefMap* map = g_IdentityPrefMap;

  if ( ! ImportSettingList(map, tbl) )
   return FALSE;


  // do the special stuff (derived settings etc)

  for (int i=0; map[i].npKey != "END"; i++)
   {
     // set default domain flag if name has been imported
     if ( map[i].npKey == "mail.identity.defaultdomain")
      WriteProfileEntry(MP_ADD_DEFAULT_HOSTNAME,map[i].procd,"use default domain");
   }

  return TRUE;
}


bool MNetscapeImporter::ImportNetworkSettings ( MyHashTable& tbl )
{
  wxLogMessage(">>>>>>>>>> Import network settings");

  PrefMap* map = g_NetworkPrefMap;

  if ( ! ImportSettingList(map, tbl) )
   return FALSE;


  // do the special stuff (derived settings etc)

  // set the imap host to nil (at least until implemented!)
  // Silly, but to insure that the right WriteProfileEntry
  // is called, not the bool one
  wxString tmp = "";
  WriteProfileEntry(MP_IMAPHOST,tmp,"imap server name");

  // for (int i=0; map[i].npKey != "END"; i++)
  //    {
  //    }

  return TRUE;
}

bool MNetscapeImporter::ImportComposeSettings ( MyHashTable& tbl )
{
  wxLogMessage(">>>>>>>>>> Import compose settings");

  wxString lstr;

  // Netscape uses the complete path to the folder instead of the name
  if ( tbl.GetValue("mail.default_fcc",lstr) && !lstr.IsEmpty())
   {
     // if I'm correct, Netscape names the files and the folders the same
     // therefore taking the last of the path should be sufficient
     lstr = lstr.AfterLast(DIR_SEPARATOR);
     tbl.Delete("mail.default_fcc");     // it isn't a dictionary, multiple entries ok
     tbl.Put("mail.default_fcc",lstr);   // change the value in the hashtable
   }                                     // insertion will be done by ImportSettingList

  PrefMap* map = g_ComposePrefMap;

  if ( ! ImportSettingList(map, tbl) )
   return FALSE;

  // add additional bcc addresses

  bool tmpBool = FALSE;

  if ( tbl.GetValue("mail.use_default_cc",tmpBool) && tmpBool) // BCC to others
   tbl.GetValue("mail.default_cc",lstr);

  wxString lstr2;
  if ( tbl.GetValue("mail.cc_self",tmpBool) && tmpBool )    // BCC to self
   tbl.GetValue("mail.identity.useremail",lstr2);

  lstr = lstr2 + lstr;

  if (! lstr.IsEmpty() )
   WriteProfileEntry(MP_COMPOSE_BCC,lstr,"blind copy addresses");

  // use the fact that these variables are set to infer that they are also
  // used is weak, but it is all I have at the moment
  if ( tbl.GetValue("mail.signature_file",lstr) && !lstr.IsEmpty())
   WriteProfileEntry(MP_COMPOSE_USE_SIGNATURE,TRUE,"use signature file");

  return TRUE;
}


bool MNetscapeImporter::ImportFolderSettings ( MyHashTable& tbl )
{
  wxLogMessage(">>>>>>>>>> Import folder settings");

  PrefMap* map = g_FolderPrefMap;

  if ( ! ImportSettingList(map, tbl) )
   return FALSE;

  bool tmpBool = FALSE;
  wxString lstr;

  // pref says not to check for new mail, then set to a very large number
  // otherwise leave it as set.
  if ( tbl.GetValue("mail.check_new_mail",tmpBool) && ! tmpBool )
   WriteProfileEntry(MP_POLLINCOMINGDELAY,30000,"new mail polling delay");

  // if Not deliver immediately, then create an Outbox to be used
  if ( tbl.GetValue("mail.deliver_immediately",tmpBool) && ! tmpBool )
	WriteProfileEntry(MP_OUTBOX_NAME, "Outbox","Outgoing mail folder");

  return TRUE;
}

bool MNetscapeImporter::ImportViewerSettings ( MyHashTable& tbl )
{

  wxLogMessage(">>>>>>>>>> Import viewer settings");

  PrefMap* map = g_ViewerPrefMap;

  if ( ! ImportSettingList(map, tbl) )
   return FALSE;

  wxString lstr;
  // use the fact that these variables are set to infer that they are also
  // used is weak, but it is all I have at the moment
  if ( tbl.GetValue("mail.citation_color",lstr) && !lstr.IsEmpty())
   WriteProfileEntry(MP_MVIEW_QUOTED_COLOURIZE,TRUE,"use color for quoted messages");

  return TRUE;
}

bool MNetscapeImporter::ImportRestSettings ( MyHashTable& tbl )
{
  // just to run through the rest of the prefs

  wxLogMessage(">>>>>>>>>> Import remaining settings");

  PrefMap* map = g_RestPrefMap;

  if ( ! ImportSettingList(map, tbl) )
   return FALSE;

  return TRUE;
}

bool MNetscapeImporter::ImportSettingList( PrefMap* map, const MyHashTable& tbl)
{
  wxString value;
  bool tmp = FALSE;
  unsigned long lval = (unsigned long)-1;

  for (int i=0; map[i].npKey != "END"; i++)
   {
     if ( ! tbl.Exist(map[i].npKey) )  // no key
      {
        continue; // next
      }

     else if (map[i].mpKey == "Not mapped")
      {
        wxLogMessage(_("Key '%s' hasn't been mapped yet"), map[i].npKey.c_str());
        map[i].procd = TRUE; // mark to find out which ones in the file are also in the maps
        continue;
      }

     else if (( map[i].mpKey == "Ignored") || ( map[i].mpKey == "Special" ))
      {
        map[i].procd = TRUE;
        continue;
      }

     switch (map[i].type)
      {
      case NM_IS_BOOL:
      case NM_IS_NEGATE_BOOL:
        {
         if ( tbl.GetValue(map[i].npKey,tmp) )
           {
            if (map[i].type == NM_IS_NEGATE_BOOL)
              tmp = !tmp;
            map[i].procd = WriteProfileEntry(map[i].mpKey, tmp, map[i].desc);
           }
         else
           wxLogMessage(_("Parsing error for key '%s'"),
                     map[i].npKey.c_str());
         break;
        }
      case NM_IS_STRING:
      case NM_IS_STRNIL:
         {
          if ( tbl.GetValue(map[i].npKey,value) )
            {
             if ((map[i].type == NM_IS_STRING) && value.IsEmpty() )
               {
                wxLogMessage(_("Bad value for key '%s': cannot be empty"),
                          map[i].npKey.c_str());
                break;
               }
             map[i].procd = WriteProfileEntry(map[i].mpKey, value, map[i].desc);
            }
         else
           wxLogMessage(_("Parsing error for key '%s'"),
                     map[i].npKey.c_str());
         break;
        }
      case NM_IS_INT:
        {
         if ( tbl.GetValue(map[i].npKey,lval) )
           {
            map[i].procd = WriteProfileEntry(map[i].mpKey, (int)lval, map[i].desc);
           }
         else
           wxLogMessage(_("Type mismatch for key '%s' ulong expected.'"),
                     map[i].npKey.c_str());
         break;
        }
      default:
        wxLogMessage(_("Bad type key '%s'"), map[i].npKey.c_str());
      }
     if ( ! map[i].procd )
      return FALSE;
   }
  return TRUE;
}




bool MNetscapeImporter::WriteProfileEntry(const wxString& key, const wxString& val, const wxString& desc )
{
  bool status = FALSE;

  // let's make sure that if there are environment variable
  // they are expanded
  wxString tmpVal = wxExpandEnvVars(val);

  Profile* l_Profile = mApplication->GetProfile();

  if ( status = l_Profile->writeEntry( key, tmpVal) )
   wxLogMessage(_("Imported '%s' setting from %s: %s."),
             desc.c_str(),"Netscape",tmpVal.c_str());
  else
   wxLogWarning(_("Failed to write '%s' entry to profile"), desc.c_str());

  return status;
}

bool MNetscapeImporter::WriteProfileEntry(const wxString& key, const int val, const wxString& desc )
{
  bool status = FALSE;

  Profile* l_Profile = mApplication->GetProfile();

  if ( status = l_Profile->writeEntry( key, val) )
   wxLogMessage(_("Imported '%s' setting from %s: %u."),
             desc.c_str(),"Netscape",val);
  else
   wxLogWarning(_("Failed to write '%s' entry to profile"), desc.c_str());

  return status;
}

bool MNetscapeImporter::WriteProfileEntry(const wxString& key, const bool val, const wxString& desc )
{
  bool status = FALSE;

  Profile* l_Profile = mApplication->GetProfile();

  if ( val )
   status = l_Profile->writeEntry( key.c_str(), 1L);
  else
   status = l_Profile->writeEntry( key.c_str(), 0L);

  if ( status )
   wxLogMessage(_("Imported '%s' setting from %s: %u."), desc.c_str(),"Netscape",val);
  else
   wxLogWarning(_("Failed to write '%s' entry to profile"), desc.c_str());

  return status;
}




// ----------------------------------------------------------------------------
// import the Netscape filters
// ----------------------------------------------------------------------------

bool MNetscapeImporter::ImportFilters()
{
   return FALSE;
}


// ----------------------------------------------------------------------------
// Netscape Keys, not yet used
// ----------------------------------------------------------------------------



 // this is windows positioning stuff, I guess we really need to import it
/*  {"taskbar.floating" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"taskbar.horizontal" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"taskbar.ontop" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"taskbar.x" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"taskbar.y" , "Ignored", "No descr", NM_NONE, FALSE }, */

/*  {"custtoolbar.Browser.Navigation_Toolbar.position" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Browser.Navigation_Toolbar.showing" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Browser.Navigation_Toolbar.open" , "Ignored", "No descr", NM_NONE, FALSE }, */

/*  {"custtoolbar.Browser.Location_Toolbar.position" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Browser.Location_Toolbar.showing" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Browser.Location_Toolbar.open" , "Ignored", "No descr", NM_NONE, FALSE }, */

/*  {"custtoolbar.Browser.Personal_Toolbar.position" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Browser.Personal_Toolbar.showing" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Browser.Personal_Toolbar.open" , "Ignored", "No descr", NM_NONE, FALSE }, */

/*  {"custtoolbar.Messenger.Navigation_Toolbar.position" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Messenger.Navigation_Toolbar.showing" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Messenger.Navigation_Toolbar.open" , "Ignored", "No descr", NM_NONE, FALSE }, */

/*  {"custtoolbar.Messenger.Location_Toolbar.position" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Messenger.Location_Toolbar.showing" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Messenger.Location_Toolbar.open" , "Ignored", "No descr", NM_NONE, FALSE }, */

/*  {"custtoolbar.Messages.Navigation_Toolbar.position" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Messages.Navigation_Toolbar.showing" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Messages.Navigation_Toolbar.open" , "Ignored", "No descr", NM_NONE, FALSE }, */

/*  {"custtoolbar.Messages.Location_Toolbar.position" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Messages.Location_Toolbar.showing" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Messages.Location_Toolbar.open" , "Ignored", "No descr", NM_NONE, FALSE }, */

/*  {"custtoolbar.Folders.Navigation_Toolbar.position" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Folders.Navigation_Toolbar.showing" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Folders.Navigation_Toolbar.open" , "Ignored", "No descr", NM_NONE, FALSE }, */

/*  {"custtoolbar.Folders.Location_Toolbar.position" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Folders.Location_Toolbar.showing" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Folders.Location_Toolbar.open" , "Ignored", "No descr", NM_NONE, FALSE }, */

/*  {"custtoolbar.Address_Book.Address_Book_Toolbar.position" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Address_Book.Address_Book_Toolbar.showing" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Address_Book.Address_Book_Toolbar.open" , "Ignored", "No descr", NM_NONE, FALSE }, */

/*  {"custtoolbar.Compose_Message.Message_Toolbar.position" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Compose_Message.Message_Toolbar.showing" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Compose_Message.Message_Toolbar.open" , "Ignored", "No descr", NM_NONE, FALSE }, */

/*  {"custtoolbar.Composer.Composition_Toolbar.position" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Composer.Composition_Toolbar.showing" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Composer.Composition_Toolbar.open" , "Ignored", "No descr", NM_NONE, FALSE }, */

/*  {"custtoolbar.Composer.Formatting_Toolbar.position" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Composer.Formatting_Toolbar.showing" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"custtoolbar.Composer.Formatting_Toolbar.open" , "Ignored", "No descr", NM_NONE, FALSE }, */

/*  {"browser.win_width" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"browser.win_height" , "Ignored", "No descr", NM_NONE, FALSE }, */

/*  {"mail.compose.win_width" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"mail.compose.win_height" , "Ignored", "No descr", NM_NONE, FALSE }, */


/*  {"editor.win_width" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"editor.win_height" , "Ignored", "No descr", NM_NONE, FALSE }, */

/*  {"mail.folder.win_width" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"mail.folder.win_height" , "Ignored", "No descr", NM_NONE, FALSE }, */

/*  {"mail.msg.win_width" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"mail.msg.win_height" , "Ignored", "No descr", NM_NONE, FALSE }, */

/*  {"mail.thread.win_width" , "Ignored", "No descr", NM_NONE, FALSE }, */
/*  {"mail.thread.win_height" , "Ignored", "No descr", NM_NONE, FALSE }, */

