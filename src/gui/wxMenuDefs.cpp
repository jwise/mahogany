///////////////////////////////////////////////////////////////////////////////
// Project:     M
// File name:   gui/wxMenuDefs.cpp - definitions of all application menus
// Purpose:     gathering all the menus in one place makes it easier to find
//              and modify them
// Author:      Vadim Zeitlin
// Modified by:
// Created:     06.08.98
// CVS-ID:      $Id$
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     M license
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "Mpch.h"
#include "Mcommon.h"

#ifndef USE_PCH
#  include "PathFinder.h"

#  include "MApplication.h"
#  include "gui/wxMApp.h"

#  include "guidef.h"

#  include <wx/toolbar.h>
#  include <wx/choice.h>
#endif

#include <wx/menu.h>

#include "Mdefaults.h"

#include "gui/wxIconManager.h"

#include "gui/wxMenuDefs.h"

#include "gui/wxIdentityCombo.h"

// ----------------------------------------------------------------------------
// local data
// ----------------------------------------------------------------------------

// structure describing a menu item
struct MenuItemInfo
{
   int idMenu;
   const char *label;
   const char *helpstring;
   bool isCheckable;
};

struct TbarItemInfo
{
   const char *bmp;      // image file or ressource name
   int         id;       // id of the associated command (-1 => separator)
   const char *tooltip;  // flyby text
};

// array of descriptions of all toolbar buttons, should be in sync with the enum
// in wxMenuDefs.h!
static const TbarItemInfo g_aToolBarData[] =
{
   // separator
   { "", -1, "" },

   // common for all frames
   { "tb_close",       WXMENU_FILE_CLOSE,  gettext_noop("Close this window") },
   { "tb_adrbook",    WXMENU_EDIT_ADB,    gettext_noop("Edit address book") },
   { "tb_preferences", WXMENU_EDIT_PREF,   gettext_noop("Edit preferences")  },

   // main frame
   { "tb_open",          WXMENU_FOLDER_OPEN,     gettext_noop("Open folder")           },
   { "tb_mail_compose",  WXMENU_FILE_COMPOSE,  gettext_noop("Compose a new message") },
   { "tb_help",          WXMENU_HELP_CONTEXT,  gettext_noop("Help")                  },
   { "tb_exit",          WXMENU_FILE_EXIT,     gettext_noop("Exit the program")      },

   // compose frame
   { "tb_print",     WXMENU_COMPOSE_PRINT,     gettext_noop("Print")         },
   { "tb_new",       WXMENU_COMPOSE_CLEAR,     gettext_noop("Clear Window")  },
   { "tb_attach",    WXMENU_COMPOSE_INSERTFILE,gettext_noop("Insert File")   },
   { "tb_editor",    WXMENU_COMPOSE_EXTEDIT,   gettext_noop("Call external editor")   },
   { "tb_mail_send", WXMENU_COMPOSE_SEND,      gettext_noop("Send Message")  },

   // folder and message view frames
   { "tb_next_unread",   WXMENU_MSG_NEXT_UNREAD,gettext_noop("Select next unread message") },
   { "tb_mail",          WXMENU_MSG_OPEN,      gettext_noop("Open message")      },
   { "tb_mail_reply",    WXMENU_MSG_REPLY,     gettext_noop("Reply to message")  },
   { "tb_mail_forward",  WXMENU_MSG_FORWARD,   gettext_noop("Forward message")   },
   { "tb_print",         WXMENU_MSG_PRINT,     gettext_noop("Print message")     },
   { "tb_trash",         WXMENU_MSG_DELETE,    gettext_noop("Delete message")    },

   // ADB edit frame
   //{ "tb_open",     WXMENU_ADBBOOK_OPEN,    gettext_noop("Open address book file")  },
   { "tb_new",      WXMENU_ADBEDIT_NEW,     gettext_noop("Create new entry")        },
   { "tb_delete",   WXMENU_ADBEDIT_DELETE,  gettext_noop("Delete")                  },
   { "tb_undo",     WXMENU_ADBEDIT_UNDO,    gettext_noop("Undo")                    },
   { "tb_lookup",   WXMENU_ADBFIND_NEXT,    gettext_noop("Find next")               },
};

// arrays containing tbar buttons for each frame (must be -1 terminated!)
// the "Close", "Help" and "Exit" icons are added to all frames (except that
// "Close" is not added to the main frame because there it's the same as "Exit")
// FIXME should we also add "Edit adb"/"Preferences" to all frames by default?

static const int g_aMainTbar[] =
{
   WXTBAR_MAIN_OPEN,
   WXTBAR_MAIN_COMPOSE,
   WXTBAR_SEP,
   WXTBAR_MSG_NEXT_UNREAD,
   WXTBAR_MSG_OPEN,
   WXTBAR_MSG_REPLY,
   WXTBAR_MSG_FORWARD,
   WXTBAR_MSG_PRINT,
   WXTBAR_MSG_DELETE,
   WXTBAR_SEP,
   WXTBAR_ADB,
   -1
};

static const int g_aComposeTbar[] =
{
   WXTBAR_COMPOSE_SEND,
   WXTBAR_SEP,
   WXTBAR_COMPOSE_PRINT,
   WXTBAR_COMPOSE_CLEAR,
   WXTBAR_COMPOSE_INSERT,
   WXTBAR_COMPOSE_EXTEDIT,
   WXTBAR_SEP,
   WXTBAR_ADB,
   -1
};

static const int g_aFolderTbar[] =
{
//   WXTBAR_MAIN_OPEN,
   WXTBAR_MSG_OPEN,
   WXTBAR_SEP,
   WXTBAR_MAIN_COMPOSE,
   WXTBAR_SEP,
   WXTBAR_MSG_NEXT_UNREAD,
   WXTBAR_MSG_REPLY,
   WXTBAR_MSG_FORWARD,
   WXTBAR_MSG_PRINT,
   WXTBAR_MSG_DELETE,
   WXTBAR_SEP,
   WXTBAR_ADB,
   -1
};

static const int g_aMsgTbar[] =
{
//   WXTBAR_MAIN_OPEN,
   WXTBAR_MAIN_COMPOSE,
   WXTBAR_MSG_REPLY,
   WXTBAR_MSG_FORWARD,
   WXTBAR_MSG_PRINT,
   WXTBAR_MSG_DELETE,
   WXTBAR_SEP,
   WXTBAR_ADB,
   -1
};

static const int g_aAdbTbar[] =
{
   WXTBAR_SEP,
   WXTBAR_ADB_NEW,
   WXTBAR_ADB_DELETE,
   WXTBAR_ADB_UNDO,
   WXTBAR_SEP,
   WXTBAR_ADB_FINDNEXT,
   -1
};

// this array stores all toolbar buttons for the given frame (the index in it
// is from the previous enum)
static const int *g_aFrameToolbars[WXFRAME_MAX] =
{
   g_aMainTbar,
   g_aComposeTbar,
   g_aFolderTbar,
   g_aMsgTbar,
   g_aAdbTbar
};


// array of descriptions of all menu items
// NB: by convention, if the menu item opens another window (or a dialog), it
//     should end with an ellipsis (`...')
static const MenuItemInfo g_aMenuItems[] =
{
   // ABCDEFGHIJKLMNOPQRSTUVWXYZ (VZ: leave it here, it's a cut-&-paste buffer)

   // filler for WXMENU_LAYOUT_CLICK
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },

   // file
   // available accels: BFGHJKLOQVWZ
   { WXMENU_FILE_COMPOSE,  gettext_noop("Compose &New Message\tCtrl-N"),  gettext_noop("Start a new message")      , FALSE },
   { WXMENU_FILE_COMPOSE_WITH_TEMPLATE,
                           gettext_noop("Compose with &template...\tShift-Ctrl-N"),  gettext_noop("Compose a new message using after choosing a temple for it")      , FALSE },
   { WXMENU_FILE_POST,     gettext_noop("Post News &Article"),   gettext_noop("Write a news article and post it")      , FALSE },
   { WXMENU_FILE_COLLECT,  gettext_noop("&Check mail\tShift-Ctrl-C"), gettext_noop("Check all incoming folders for new mail and download it now") , FALSE },
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_FILE_PRINT_SETUP,    gettext_noop("P&rint Setup"),     gettext_noop("Configure printing")  , FALSE },
   { WXMENU_FILE_PAGE_SETUP,    gettext_noop("P&age Setup"),     gettext_noop("Configure page setup")  , FALSE },
#ifdef USE_PS_PRINTING
   // extra postscript printing
   { WXMENU_FILE_PRINT_SETUP_PS,    gettext_noop("&Print PS Setup"),     gettext_noop("Configure PostScript printing")  , FALSE },
// { WXMENU_FILE_PAGE_SETUP_PS,    gettext_noop("P&age PS Setup"),     gettext_noop("Configure PostScript page setup")  , FALSE },
#endif // USE_PS_PRINTING

#ifdef USE_PYTHON
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_FILE_RUN_PYSCRIPT,   gettext_noop("R&un Python script..."),    gettext_noop("Run a simple python script"), FALSE },
#endif // USE_PYTHON

   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_FILE_SEND_OUTBOX, gettext_noop("&Send messages...\tShift-Ctrl-S"), gettext_noop("Sends messages still in outgoing mailbox"), FALSE },

   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_FILE_NET_ON,    gettext_noop("Conn&ect to Network"), gettext_noop("Activate dial-up networking")        , FALSE },
   { WXMENU_FILE_NET_OFF,   gettext_noop("Shut&down Network"), gettext_noop("Shutdown dial-up networking")        , FALSE },

   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_SUBMENU,       gettext_noop("&Identity"), "", FALSE },
      { WXMENU_FILE_IDENT_ADD, gettext_noop("&Add..."),  gettext_noop("Create a new identity")        , FALSE },
      { WXMENU_FILE_IDENT_CHANGE, gettext_noop("&Change..."), gettext_noop("Change the current identity")        , FALSE },
      { WXMENU_FILE_IDENT_EDIT, gettext_noop("&Edit..."),  gettext_noop("Edit the current identity parameters")        , FALSE },
      { WXMENU_FILE_IDENT_DELETE, gettext_noop("&Delete..."), gettext_noop("Remove an identity")        , FALSE },
   { WXMENU_SUBMENU,       "", "", FALSE },

   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_FILE_IMPORT,   gettext_noop("I&mport..."),  gettext_noop("Import settings from another e-mail program")        , FALSE },

   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_FILE_CLOSE,    gettext_noop("&Close Window\tCtrl-W"),     gettext_noop("Close this window")        , FALSE },
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_FILE_AWAY_MODE,gettext_noop("Awa&y mode\tCtrl-Y"),        gettext_noop("Toggle unattended mode on/off"), TRUE },
   { WXMENU_FILE_EXIT,     gettext_noop("E&xit\tCtrl-Q"),             gettext_noop("Quit the application")     , FALSE },

   // folder
   // available accels: DGHJKMQSTUVWXYZ
   { WXMENU_FOLDER_OPEN,      gettext_noop("&Open...\tCtrl-O"),   gettext_noop("Open an existing message folder")                  , FALSE },
   { WXMENU_FOLDER_CREATE,    gettext_noop("&Create..."), gettext_noop("Create a new folder definition")               , FALSE },
   { WXMENU_FOLDER_RENAME,    gettext_noop("Re&name"), gettext_noop("Rename the selected folder")               , FALSE },
   { WXMENU_FOLDER_REMOVE,    gettext_noop("&Remove from tree"), gettext_noop("Remove the selected folder from the folder tree")               , FALSE },
   { WXMENU_FOLDER_DELETE,    gettext_noop("&Delete"), gettext_noop("Delete all messages in the folder and remove it")               , FALSE },
   { WXMENU_FOLDER_CLEAR,     gettext_noop("C&lear..."), gettext_noop("Delete all messages in the folder")               , FALSE },
   { WXMENU_FOLDER_CLOSE,     gettext_noop("Clos&e"), gettext_noop("Close the current folder")               , FALSE },
   { WXMENU_FOLDER_CLOSEALL,  gettext_noop("Close &all"), gettext_noop("Close all opened folders")               , FALSE },
   { WXMENU_SEPARATOR,        "",                  ""                         , FALSE },
   { WXMENU_FOLDER_IMPORTTREE,gettext_noop("&Import file folders..."),
                              gettext_noop("Create folders for all files in a directory"), FALSE },
   { WXMENU_FOLDER_BROWSESUB, gettext_noop("&Browse..."), gettext_noop("Show subfolders of the current folder")               , FALSE },
   { WXMENU_SEPARATOR,        "",                  ""                         , FALSE },
   { WXMENU_FOLDER_FILTERS,   gettext_noop("&Filters..."), gettext_noop("Edit the filters to use for current folder")               , FALSE },
   { WXMENU_SEPARATOR,        "",                  ""                         , FALSE },
   { WXMENU_FOLDER_PROP,      gettext_noop("&Properties..."), gettext_noop("Show the properties of the current folder")               , FALSE },

   // normal edit
   { WXMENU_EDIT_CUT,  gettext_noop("C&ut\tCtrl-X"), gettext_noop("Cut selection and copy it to clipboard")           , FALSE },
   { WXMENU_EDIT_COPY, gettext_noop("&Copy\tCtrl-C"), gettext_noop("Copy selection to clipboard")           , FALSE },
   { WXMENU_EDIT_PASTE,gettext_noop("&Paste\tCtrl-V"), gettext_noop("Paste from clipboard")           , FALSE },
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_EDIT_ADB,      gettext_noop("&Address books...\tCtrl-D"), gettext_noop("Edit the address book(s)") , FALSE },
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_EDIT_PREF,     gettext_noop("Pr&eferences..."),   gettext_noop("Change options")           , FALSE },
   { WXMENU_EDIT_MODULES,  gettext_noop("&Modules..."), gettext_noop("Choose which extension modules to use")           , FALSE },
   { WXMENU_EDIT_FILTERS,  gettext_noop("&Filter Rules..."), gettext_noop("Edit rules for message filtering")   , FALSE },
   { WXMENU_EDIT_TEMPLATES,gettext_noop("&Templates..."), gettext_noop("Edit templates used for message composition")   , FALSE },
   { WXMENU_EDIT_RESTORE_PREF,
                           gettext_noop("&Restore defaults..."), gettext_noop("Restore default options values") , FALSE },
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_EDIT_SAVE_PREF,gettext_noop("&Save Preferences"), gettext_noop("Save options")             , FALSE },

   // msg

   // the available accelerators for this menu:
   // BJZ

   { WXMENU_MSG_OPEN,      gettext_noop("&Open"),             gettext_noop("View selected message")    , FALSE },
   { WXMENU_MSG_PRINT,     gettext_noop("&Print\tCtrl-P"),            gettext_noop("Print this message")       , FALSE },
   { WXMENU_MSG_PRINT_PREVIEW, gettext_noop("Print Pre&view"),gettext_noop("Preview a printout of this message")       , FALSE },
#ifdef USE_PS_PRINTING
   { WXMENU_MSG_PRINT_PS,     gettext_noop("PS-Prin&t"),      gettext_noop("Print this message as PostScript")       , FALSE },
   { WXMENU_MSG_PRINT_PREVIEW_PS,     gettext_noop("PS&-Print Preview"),      gettext_noop("View PostScript printout")       , FALSE },
#endif // USE_PS_PRINTING
   { WXMENU_MSG_REPLY,     gettext_noop("&Reply\tCtrl-R"),            gettext_noop("Reply to this message")    , FALSE },
   { WXMENU_MSG_REPLY_WITH_TEMPLATE, gettext_noop("Reply with &template...\tShift-Ctrl-R"), gettext_noop("Reply to this message after choosing a template to use ")    , FALSE },
   { WXMENU_MSG_FOLLOWUP,  gettext_noop("Reply to a&ll\tCtrl-G"), gettext_noop("Reply to all recipients of this message")    , FALSE },
   { WXMENU_MSG_FOLLOWUP_WITH_TEMPLATE,
                           gettext_noop("Repl&y to all with template...\tShift-Ctrl-G"),      gettext_noop("Reply to all recipients of this message after choosing a template to use ")    , FALSE },
   { WXMENU_MSG_FORWARD,   gettext_noop("&Forward\tCtrl-F"),          gettext_noop("Forward this message")     , FALSE },
   { WXMENU_MSG_FORWARD_WITH_TEMPLATE,
                           gettext_noop("Forward with te&mplate...\tShift-Ctrl-F"), gettext_noop("Forward this message after choosing a template to use ")     , FALSE },
   { WXMENU_MSG_FILTER,    gettext_noop("&Apply filter rules..."), gettext_noop("Apply filter rules to selected messages")     , FALSE },
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_MSG_NEXT_UNREAD,   gettext_noop("&Next unread\tCtrl-U"), gettext_noop("Select next unread message")     , FALSE },
   { WXMENU_MSG_NEXT_FLAGGED,   gettext_noop("Ne&xt flagged"), gettext_noop("Select next flagged message")     , FALSE },
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_MSG_SAVE_TO_FILE, gettext_noop("Save as f&ile..."), gettext_noop("Export message to a file")   , FALSE },
   { WXMENU_MSG_SAVE_TO_FOLDER, gettext_noop("&Copy to folder..."),gettext_noop("Save message to another folder")   , FALSE },
   { WXMENU_MSG_MOVE_TO_FOLDER, gettext_noop("&Move to folder..."),gettext_noop("Move message to another folder")   , FALSE },
   { WXMENU_MSG_DELETE,    gettext_noop("&Delete"),           gettext_noop("Delete this message")      , FALSE },
   { WXMENU_MSG_UNDELETE,  gettext_noop("&Undelete"),         gettext_noop("Undelete message")         , FALSE },
   { WXMENU_MSG_FLAG,      gettext_noop("Fla&g\tCtrl-I"), gettext_noop("Mark message as flagged/unflagged")         , FALSE },
   { WXMENU_MSG_EXPUNGE,   gettext_noop("Ex&punge"),          gettext_noop("Expunge")                  , FALSE },
#if defined(EXPERIMENTAL_MARK_READ)
   { WXMENU_MSG_MARK_READ,   gettext_noop("Mar&k read"), gettext_noop("Mark message as read"), FALSE },
   { WXMENU_MSG_MARK_UNREAD,   gettext_noop("Mark unread (&q)\tShift-Ctrl-U"), gettext_noop("Mark message as unread"), FALSE },
#endif // EXPERIMENTAL_MARK_READ
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_MSG_SELECTALL, gettext_noop("Select &all\tCtrl-A"),       gettext_noop("Select all messages")      , FALSE },
   { WXMENU_MSG_SELECTUNREAD, gettext_noop("Select all &unread\tShift-Ctrl-A"), gettext_noop("Select all unread messages")      , FALSE },
   { WXMENU_MSG_SELECTFLAGGED, gettext_noop("Select all &flagged"), gettext_noop("Select all flagged messages")      , FALSE },
   { WXMENU_MSG_DESELECTALL,gettext_noop("D&eselect all\tCtrl-B"),    gettext_noop("Deselect all messages")    , FALSE },
   { WXMENU_MSG_SEARCH,  gettext_noop("&Search..."), gettext_noop("Search and select messages") , FALSE },
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_MSG_SAVEADDRESSES, gettext_noop("Extra&ct addresses..."), gettext_noop("Save all or some addresses of the message in an address book"), FALSE },
   { WXMENU_MSG_TOGGLEHEADERS, gettext_noop("Show &headers"), gettext_noop("Toggle display of message header") , TRUE },
   { WXMENU_MSG_SHOWRAWTEXT,  gettext_noop("Show ra&w message\tCtrl-W"), gettext_noop("Show the raw message text") , FALSE },
#ifdef EXPERIMENTAL_show_uid
   { WXMENU_MSG_SHOWUID, "Show message UID&L", "", FALSE },
#endif // EXPERIMENTAL_show_uid
   { WXMENU_MSG_FIND,  gettext_noop("Fi&nd...\tF3"), gettext_noop("Find text in message") , FALSE },
   { WXMENU_MSG_FINDAGAIN, gettext_noop("Find again (&z)\tCtrl-F3"), gettext_noop("Find the same text again") , FALSE },

   // compose
   { WXMENU_COMPOSE_INSERTFILE, gettext_noop("&Insert file...\tCtrl-I"),
                                gettext_noop("Attach a file to the message")            , FALSE },
   { WXMENU_COMPOSE_LOADTEXT,gettext_noop("I&nsert text...\tCtrl-T"), gettext_noop("Insert text file")         , FALSE },
   { WXMENU_COMPOSE_SEND,  gettext_noop("&Send\tShift-Ctrl-X"),             gettext_noop("Send the message now")     , FALSE },
   { WXMENU_COMPOSE_SEND_LATER, gettext_noop("Send &Later\tShift-Ctrl-L"),
                                gettext_noop("Schedule the message to be send at a later time.")     , FALSE },
   { WXMENU_COMPOSE_SEND_KEEP_OPEN, gettext_noop("Send and &keep\tShift-Ctrl-K"),
                                    gettext_noop("Send the message now and keep the editor open")     , FALSE },
   { WXMENU_COMPOSE_PRINT, gettext_noop("&Print\tCtrl-P"),            gettext_noop("Print the message")        , FALSE },
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_COMPOSE_SAVETEXT,gettext_noop("Save &text..."),   gettext_noop("Save (append) message text to file"), FALSE },
   { WXMENU_COMPOSE_CLEAR, gettext_noop("&Clear"),            gettext_noop("Delete message contents")  , FALSE },
   { WXMENU_COMPOSE_EVAL_TEMPLATE, gettext_noop("E&valuate template"), gettext_noop("Use the template to create skeleton of a message")  , FALSE },
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_COMPOSE_EXTEDIT, gettext_noop("&External editor\tCtrl-E"),gettext_noop("Invoke alternative editor"), FALSE },
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_COMPOSE_CUSTOM_HEADERS, gettext_noop("Custom &header...\tCtrl-H"), gettext_noop("Add/edit header fields not shown on the screen"), FALSE },

   // language

   // the available accelerators for this menu:
   // FJMNPQUVXZ
   { WXMENU_LANG_DEFAULT, gettext_noop("De&fault"), gettext_noop("Use the default encoding"), TRUE },
   { WXMENU_SEPARATOR,     "",                  ""                         , TRUE },
   { WXMENU_LANG_ISO8859_1, gettext_noop("&Western European (ISO-8859-1)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_ISO8859_2, gettext_noop("Ce&ntral European (ISO-8859-2)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_ISO8859_3, gettext_noop("Es&peranto (ISO-8859-3)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_ISO8859_4, gettext_noop("Baltic &old (ISO-8859-4)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_ISO8859_5, gettext_noop("&Cyrillic (ISO-8859-5)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_ISO8859_6, gettext_noop("&Arabic (ISO-8859-6)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_ISO8859_7, gettext_noop("&Greek (ISO-8859-7)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_ISO8859_8, gettext_noop("Heb&rew (ISO-8859-8)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_ISO8859_9, gettext_noop("&Turkish (ISO-8859-9)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_ISO8859_10, gettext_noop("Nor&dic (ISO-8859-10)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_ISO8859_11, gettext_noop("T&hai (ISO-8859-11)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_ISO8859_12, gettext_noop("&Indian (ISO-8859-12)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_ISO8859_13, gettext_noop("&Baltic (ISO-8859-13)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_ISO8859_14, gettext_noop("Ce&ltic (ISO-8859-14)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_ISO8859_15, gettext_noop("Western European with &Euro (ISO-8859-15)"), gettext_noop(""), TRUE },
   { WXMENU_SEPARATOR,     "",                  ""                         , TRUE },
   { WXMENU_LANG_CP1250, gettext_noop("Windows Central European (CP 125&0)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_CP1251, gettext_noop("Windows Cyrillic (CP 125&1)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_CP1252, gettext_noop("Windows Western European (CP 125&2)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_CP1253, gettext_noop("Windows Greek (CP 125&3)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_CP1254, gettext_noop("Windows Turkish (CP 125&4)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_CP1255, gettext_noop("Windows Hebrew (CP 125&5)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_CP1256, gettext_noop("Windows Arabic (CP 125&6)"), gettext_noop(""), TRUE },
   { WXMENU_LANG_CP1257, gettext_noop("Windows Baltic (CP 125&7)"), gettext_noop(""), TRUE },
   { WXMENU_SEPARATOR,     "",                  ""                         , TRUE },
   { WXMENU_LANG_KOI8, gettext_noop("Russian (KOI&8-R)"), gettext_noop(""), TRUE },
   { WXMENU_SEPARATOR,     "",                  ""                         , TRUE },
#if wxCHECK_VERSION(2, 3, 0)
   { WXMENU_LANG_UTF8, gettext_noop("&Unicode (UTF-8)"), gettext_noop(""), TRUE },
   { WXMENU_SEPARATOR,     "",                  ""                         , TRUE },
#endif // 2.3.0
   { WXMENU_LANG_SET_DEFAULT, gettext_noop("&Set default encoding..."), ""                         , FALSE },

   // ADB book management
   { WXMENU_ADBBOOK_NEW,   gettext_noop("&New..."),           gettext_noop("Create a new address book"), FALSE },
   { WXMENU_ADBBOOK_OPEN,  gettext_noop("&Open..."),          gettext_noop("Open an address book file"), FALSE },
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_ADBBOOK_EXPORT,gettext_noop("&Export..."),        gettext_noop("Export address book data to another programs format"), FALSE },
   { WXMENU_ADBBOOK_IMPORT,gettext_noop("&Import..."),        gettext_noop("Import data from an address book in another programs format"), FALSE },
   { WXMENU_SUBMENU,       gettext_noop("&vCard"), "", FALSE },
      { WXMENU_ADBBOOK_VCARD_IMPORT, gettext_noop("I&mport vCard file..."),
                                     gettext_noop("Create an entry from vCard file"), FALSE },
      { WXMENU_ADBBOOK_VCARD_EXPORT, gettext_noop("E&xport vCard file..."),
                                     gettext_noop("Export entry to a vCard file"), FALSE },
   { WXMENU_SUBMENU,       "", "", FALSE },
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
#ifdef DEBUG
   { WXMENU_ADBBOOK_FLUSH, "&Flush",                           "Save changes to disk"                         , FALSE },
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
#endif // debug
   { WXMENU_ADBBOOK_PROP,  gettext_noop("&Properties..."), gettext_noop("View properties of the current address book")            , FALSE },

   // ADB edit
   { WXMENU_ADBEDIT_NEW,   gettext_noop("&New entry..."),     gettext_noop("Create new entry/group")   , FALSE },
   { WXMENU_ADBEDIT_DELETE,gettext_noop("&Delete"),           gettext_noop("Delete the selected items"), FALSE },
   { WXMENU_ADBEDIT_RENAME,gettext_noop("&Rename..."),        gettext_noop("Rename the selected items"), FALSE },
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_ADBEDIT_CUT,   gettext_noop("Cu&t"),              gettext_noop("Copy and delete selected items")                    , FALSE },
   { WXMENU_ADBEDIT_COPY,  gettext_noop("&Copy"),             gettext_noop("Copy selected items")      , FALSE },
   { WXMENU_ADBEDIT_PASTE, gettext_noop("&Paste"),            gettext_noop("Paste here")               , FALSE },
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_ADBEDIT_UNDO,  gettext_noop("&Undo changes"),     gettext_noop("Undo all changes to the entry being edited")       , FALSE },

   // ADB search
   { WXMENU_ADBFIND_FIND,  gettext_noop("&Find..."),          gettext_noop("Find entry by name or contents")                 , FALSE },
   { WXMENU_ADBFIND_NEXT,  gettext_noop("Find &next\tCtrl-G"),        gettext_noop("Go to the next match")     , FALSE },
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_ADBFIND_GOTO,  gettext_noop("&Go To..."),         gettext_noop("Go to specified entry")    , FALSE },

   // help
   { WXMENU_HELP_ABOUT,    gettext_noop("&About..."),         gettext_noop("Displays the program information and copyright")  , FALSE },
   { WXMENU_HELP_TIP,      gettext_noop("Show a &tip..."),    gettext_noop("Show a tip about using Mahogany")  , FALSE },
   { WXMENU_HELP_RELEASE_NOTES,    gettext_noop("&Release Notes..."), gettext_noop("Displays notes about the current release.")  , FALSE },
   { WXMENU_HELP_FAQ,    gettext_noop("&FAQ..."),         gettext_noop("Displays the list of Frequently Asked Questions.")  , FALSE },
#ifdef DEBUG
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_HELP_WIZARD,   "Run &wizard",       ""                         , FALSE },
#endif
   { WXMENU_SEPARATOR,     "",                  ""                         , FALSE },
   { WXMENU_HELP_CONTEXT, gettext_noop("&Help\tCtrl-H"),    gettext_noop("Help on current context..."), FALSE },
   { WXMENU_HELP_CONTENTS, gettext_noop("Help &Contents\tF1"),    gettext_noop("Contents of help system..."), FALSE },
   { WXMENU_HELP_SEARCH,   gettext_noop("&Search Help..."),      gettext_noop("Search help system for keyword..."), FALSE },
   { WXMENU_HELP_COPYRIGHT,   gettext_noop("C&opyright"), gettext_noop("Show Copyright."), FALSE },

   // misc entries
   { WXMENU_MSG_QUICK_FILTER, gettext_noop("Quick &filter..."), gettext_noop("Create a new filter for the selected message")     , FALSE },
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// menu stuff
// ----------------------------------------------------------------------------

void AppendToMenu(wxMenu *menu, int& n)
{
   int id = n > 0 ? g_aMenuItems[n].idMenu : WXMENU_SEPARATOR;
   if ( id == WXMENU_SEPARATOR ) {
      menu->AppendSeparator();
   }
   else if ( id == WXMENU_SUBMENU ) {
      // append all entries until the next one with id == WXMENU_SUBMENU to a
      // submenu
      wxMenu *submenu = new wxMenu();

      int nSubMenu = n;
      for ( n++; g_aMenuItems[n].idMenu != WXMENU_SUBMENU; n++ )
      {
         AppendToMenu(submenu, n);
      }

      menu->Append(10000, // FIXME
                   wxGetTranslation(g_aMenuItems[nSubMenu].label),
                   submenu,
                   wxGetTranslation(g_aMenuItems[nSubMenu].helpstring));
   }
   else {
      menu->Append(id,
                   wxGetTranslation(g_aMenuItems[n].label),
                   wxGetTranslation(g_aMenuItems[n].helpstring),
                   g_aMenuItems[n].isCheckable);
   }
}

void AppendToMenu(wxMenu *menu, int nFirst, int nLast)
{
   // consistency check which ensures (well, helps to ensure) that the array
   // and enum are in sync
   wxASSERT( WXSIZEOF(g_aMenuItems) == WXMENU_END );

   // in debug mode we also verify if the keyboard accelerators are ok
#ifdef DEBUG
   wxString strAccels;
#endif

   for ( int n = nFirst; n <= nLast; n++ ) {
#     ifdef DEBUG
      const char *label = wxGetTranslation(g_aMenuItems[n].label);
      if ( !IsEmpty(label) ) {
         const char *p = strchr(label, '&');
         if ( p == NULL ) {
            wxLogWarning("Menu label '%s' doesn't have keyboard accelerator.",
                         label);
         }
         else {
            char c = *++p;
            if ( strAccels.Find(c) != -1 ) {
               wxLogWarning("Duplicate accelerator %c (in '%s')", c, label);
            }

            strAccels += c;
         }
      }
      // else: it must be a separator

#     endif //DEBUG

      AppendToMenu(menu, n);
   }
}

// ----------------------------------------------------------------------------
// toolbar stuff
// ----------------------------------------------------------------------------

// add the given button to the toolbar
void AddToolbarButton(wxToolBar *toolbar, int nButton)
{
   if ( nButton == WXTBAR_SEP ) {
      toolbar->AddSeparator();
   }
   else {
      wxString iconName = g_aToolBarData[nButton].bmp;
      toolbar->AddTool(g_aToolBarData[nButton].id,
#ifdef __WXMSW__
                       mApplication->GetIconManager()->GetBitmap(iconName),
#else
                       mApplication->GetIconManager()->GetIcon(iconName),
#endif
                       wxNullBitmap,
                       FALSE, -1, -1, NULL,
                       _(g_aToolBarData[nButton].tooltip));
   }
}

// add all buttons for the given frame to the toolbar
void AddToolbarButtons(wxToolBar *toolbar, wxFrameId frameId)
{
   wxASSERT( WXSIZEOF(g_aToolBarData) == WXTBAR_MAX);

#ifdef __WXMSW__
   // we use the icons of non standard size
   toolbar->SetToolBitmapSize(wxSize(24, 24));
#endif

   wxASSERT( frameId < WXFRAME_MAX );
   const int *aTbarIcons = g_aFrameToolbars[frameId];

#ifdef __WXGTK__
   // it looks better like this under GTK
   AddToolbarButton(toolbar, WXTBAR_SEP);
#endif

   for ( size_t nButton = 0; aTbarIcons[nButton] != -1 ; nButton++ ) {
      AddToolbarButton(toolbar, aTbarIcons[nButton]);
   }

   // show the identity combo in the main frame
   if ( frameId == WXFRAME_MAIN &&
        READ_APPCONFIG(MP_USERLEVEL) >= M_USERLEVEL_ADVANCED )
   {
      wxControl *combo = CreateIdentCombo(toolbar);
      if (combo) toolbar->AddControl(combo);
   }

   // next add the "Help" button
   AddToolbarButton(toolbar, WXTBAR_SEP);
   AddToolbarButton(toolbar, WXTBAR_MAIN_HELP);
   AddToolbarButton(toolbar, WXTBAR_SEP);

   // finally, add the "Close" icon - but only if we're not the main frame
   if ( frameId != WXFRAME_MAIN ) {
      AddToolbarButton(toolbar, WXTBAR_CLOSE);
      AddToolbarButton(toolbar, WXTBAR_SEP);
   }

   // and the "Exit" button for all frames
   AddToolbarButton(toolbar, WXTBAR_MAIN_EXIT);

   // must do it for the toolbar to be shown properly
   toolbar->Realize();
}


extern void
CreateMMenu(wxMenuBar *menubar, int menu_begin, int menu_end, const wxString &caption)
{
   wxMenu *pMenu = new wxMenu("", wxMENU_TEAROFF);
   AppendToMenu(pMenu, menu_begin+1, menu_end);
   menubar->Append(pMenu, caption);
}


/// Function to enable/disable a given menu:
extern void
EnableMMenu(MMenuId id, wxWindow *win, bool enable)
{
   wxFrame *frame = GetFrame(win);
   CHECK_RET(frame, "no parent frame in EnableMMenu");
   wxMenuBar *mb = frame->GetMenuBar();
   CHECK_RET(mb, "no menu bar in EnableMMenu");

   // only the main frame has MMenu_Folder, so adjust the menu index
   if ( frame->GetParent() && (id > MMenu_Folder) )
   {
      id = (MMenuId)(id - 1);
   }

   if(id == MMenu_Plugins)
   {
      if(mb->GetMenuCount() > MMenu_Help)
         mb->EnableTop(id,enable);
      else
      {
         // no Plugin menu
         if(id != MMenu_Plugins)
            mb->EnableTop(id-1,enable);
      }
   }
   else
   {
      mb->EnableTop(id, enable);
   }
}

// ----------------------------------------------------------------------------
// language menu stuff
// ----------------------------------------------------------------------------

// Check the entry corresponding to this encoding in LANG submenu
extern void CheckLanguageInMenu(wxWindow *win, wxFontEncoding encoding)
{
   wxFrame *frame = GetFrame(win);
   CHECK_RET(frame, "no parent frame in CheckLanguageInMenu");
   wxMenuBar *mb = frame->GetMenuBar();
   CHECK_RET(mb, "no menu bar in CheckLanguageInMenu");

   int id;
   switch ( encoding )
   {
      case wxFONTENCODING_ISO8859_1:
      case wxFONTENCODING_ISO8859_2:
      case wxFONTENCODING_ISO8859_3:
      case wxFONTENCODING_ISO8859_4:
      case wxFONTENCODING_ISO8859_5:
      case wxFONTENCODING_ISO8859_6:
      case wxFONTENCODING_ISO8859_7:
      case wxFONTENCODING_ISO8859_8:
      case wxFONTENCODING_ISO8859_9:
      case wxFONTENCODING_ISO8859_10:
      case wxFONTENCODING_ISO8859_11:
      case wxFONTENCODING_ISO8859_12:
      case wxFONTENCODING_ISO8859_13:
      case wxFONTENCODING_ISO8859_14:
      case wxFONTENCODING_ISO8859_15:
         id = WXMENU_LANG_ISO8859_1 + encoding - wxFONTENCODING_ISO8859_1;
         break;

      case wxFONTENCODING_CP1250:
      case wxFONTENCODING_CP1251:
      case wxFONTENCODING_CP1252:
      case wxFONTENCODING_CP1253:
      case wxFONTENCODING_CP1254:
      case wxFONTENCODING_CP1255:
      case wxFONTENCODING_CP1256:
      case wxFONTENCODING_CP1257:
         id = WXMENU_LANG_CP1250 + encoding - wxFONTENCODING_CP1250;
         break;

      case wxFONTENCODING_KOI8:
         id = WXMENU_LANG_KOI8;
         break;

#if wxCHECK_VERSION(2, 3, 0)
      case wxFONTENCODING_UTF8:
         id = WXMENU_LANG_UTF8;
         break;
#endif // 2.3.0

      default:
         wxFAIL_MSG( "Unexpected encoding in CheckLanguageInMenu" );

      case wxFONTENCODING_DEFAULT:
         id = WXMENU_LANG_DEFAULT;
   }

   // emulate the "radio menu items" as wxWin doesn't yet have this
   static const int menuIds[] =
   {
      WXMENU_LANG_DEFAULT,
      WXMENU_LANG_ISO8859_1,
      WXMENU_LANG_ISO8859_2,
      WXMENU_LANG_ISO8859_3,
      WXMENU_LANG_ISO8859_4,
      WXMENU_LANG_ISO8859_5,
      WXMENU_LANG_ISO8859_6,
      WXMENU_LANG_ISO8859_7,
      WXMENU_LANG_ISO8859_8,
      WXMENU_LANG_ISO8859_9,
      WXMENU_LANG_ISO8859_10,
      WXMENU_LANG_ISO8859_11,
      WXMENU_LANG_ISO8859_12,
      WXMENU_LANG_ISO8859_13,
      WXMENU_LANG_ISO8859_14,
      WXMENU_LANG_ISO8859_15,
      WXMENU_LANG_CP1250,
      WXMENU_LANG_CP1251,
      WXMENU_LANG_CP1252,
      WXMENU_LANG_CP1253,
      WXMENU_LANG_CP1254,
      WXMENU_LANG_CP1255,
      WXMENU_LANG_CP1256,
      WXMENU_LANG_CP1257,
      WXMENU_LANG_KOI8,
#if wxCHECK_VERSION(2, 3, 0)
      WXMENU_LANG_UTF8,
#endif // 2.3.0
   };

   for ( size_t nId = 0; nId < WXSIZEOF(menuIds); nId++ )
   {
      int idCur = menuIds[nId];
      mb->Check(idCur, idCur == id);
   }
}

// translate the menu event id to the encoding
extern wxFontEncoding GetEncodingFromMenuCommand(int id)
{
   int encoding;
   switch ( id )
   {
      default:
         wxFAIL_MSG("unexpected menu event id in GetEncodingFromMenuEvent");
         // fall through

      case WXMENU_LANG_DEFAULT:
         encoding = READ_APPCONFIG(MP_MSGVIEW_DEFAULT_ENCODING);
         break;

      case WXMENU_LANG_ISO8859_1:
      case WXMENU_LANG_ISO8859_2:
      case WXMENU_LANG_ISO8859_3:
      case WXMENU_LANG_ISO8859_4:
      case WXMENU_LANG_ISO8859_5:
      case WXMENU_LANG_ISO8859_6:
      case WXMENU_LANG_ISO8859_7:
      case WXMENU_LANG_ISO8859_8:
      case WXMENU_LANG_ISO8859_9:
      case WXMENU_LANG_ISO8859_10:
      case WXMENU_LANG_ISO8859_11:
      case WXMENU_LANG_ISO8859_12:
      case WXMENU_LANG_ISO8859_13:
      case WXMENU_LANG_ISO8859_14:
      case WXMENU_LANG_ISO8859_15:
         encoding = wxFONTENCODING_ISO8859_1 + id - WXMENU_LANG_ISO8859_1;
         break;

      case WXMENU_LANG_CP1250:
      case WXMENU_LANG_CP1251:
      case WXMENU_LANG_CP1252:
      case WXMENU_LANG_CP1253:
      case WXMENU_LANG_CP1254:
      case WXMENU_LANG_CP1255:
      case WXMENU_LANG_CP1256:
      case WXMENU_LANG_CP1257:
         encoding = wxFONTENCODING_CP1250 + id - WXMENU_LANG_CP1250;
         break;

      case WXMENU_LANG_KOI8:
         encoding = wxFONTENCODING_KOI8;
         break;

#if wxCHECK_VERSION(2, 3, 0)
      case WXMENU_LANG_UTF8:
         encoding = wxFONTENCODING_UTF8;
         break;
#endif // 2.3.0
   }

   return (wxFontEncoding)encoding;
}

/* vi: set tw=0 */
