///////////////////////////////////////////////////////////////////////////////
// Project:     M
// File name:   gui/wxOptionsDlg.cpp - M options dialog
// Purpose:     allows to easily change from one dialog all program options
// Author:      Vadim Zeitlin
// Modified by:
// Created:     20.08.98
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

#ifndef USE_PCH
#  include "Mcommon.h"
#  include "MApplication.h"
#  include "Profile.h"
#  include "guidef.h"
#  include "MHelp.h"
#  include "strutil.h"
#  include "Mpers.h"

#  include <wx/dynarray.h>
#  include <wx/checkbox.h>
#  include <wx/listbox.h>
#  include <wx/radiobox.h>
#  include <wx/statbox.h>
#  include <wx/stattext.h>
#  include <wx/statbmp.h>
#  include <wx/textctrl.h>
#  include <wx/textdlg.h>  // for wxGetTextFromUser()

#  include <wx/utils.h>    // for wxStripMenuCodes()
#endif

#include <wx/log.h>
#include <wx/imaglist.h>
#include <wx/notebook.h>
#include <wx/resource.h>
#include <wx/confbase.h>

#include <wx/menuitem.h>
#include <wx/checklst.h>

#include <wx/layout.h>

#include "wx/persctrl.h"

#include "Mpers.h"
#include "MDialogs.h"
#include "Mdefaults.h"
#include "Mcallbacks.h"

#include "gui/wxIconManager.h"
#include "gui/wxDialogLayout.h"
#include "gui/wxOptionsDlg.h"
#include "gui/wxOptionsPage.h"

#include "HeadersDialogs.h"
#include "FolderView.h"
#include "TemplateDialog.h"

// ----------------------------------------------------------------------------
// conditional compilation
// ----------------------------------------------------------------------------

#ifdef OS_UNIX
   // define this to allow using MTA (typically only for Unix)
   #define USE_SENDMAIL

   // we use externabl browser for HTML help under Unix
   #define USE_EXT_HTML_HELP

   // BBDB support only makes sense for Unix
   #define USE_BBDB
#endif

// define this to have additional TCP parameters in the options dialog
#undef USE_TCP_TIMEOUTS

// ----------------------------------------------------------------------------
// data
// ----------------------------------------------------------------------------

// first and last are shifted by -1, i.e. the range of fields for the page Foo
// is from ConfigField_FooFirst + 1 to ConfigField_FooLast inclusive - this is
// more convenient as the number of entries in a page is then just Last - First
// without any +1.
//
// only the wxOptionsPage ctor and ms_aIdentityPages depend on this, so if this
// is (for some reason) changed, it would be the only places to change.
//
// if you modify this enum, you must modify the data below too (search for
// DONT_FORGET_TO_MODIFY to find it)
enum ConfigFields
{
   // identity
   ConfigField_IdentFirst = -1,
   ConfigField_PersonalName,
   ConfigField_ReturnAddress,
   ConfigField_ReplyAddress,
   ConfigField_HostnameHelp,
   ConfigField_AddDefaultHostname,
   ConfigField_Hostname,
   ConfigField_SetReplyFromTo,
   ConfigField_VCardHelp,
   ConfigField_UseVCard,
   ConfigField_VCardFile,
   ConfigField_UserLevelHelp,
   ConfigField_UserLevel,
   ConfigField_SetPassword,
   ConfigField_IdentLast = ConfigField_SetPassword,

   // network
   ConfigField_NetworkFirst = ConfigField_IdentLast,
   ConfigField_ServersHelp,
   ConfigField_PopServer,
   ConfigField_ImapServer,
#ifdef USE_SENDMAIL
   ConfigField_UseSendmail,
   ConfigField_SendmailCmd,
#endif
   ConfigField_MailServer,
   ConfigField_NewsServer,
   ConfigField_MailServerLoginHelp,
   ConfigField_MailServerLogin,
   ConfigField_MailServerPassword,
   ConfigField_NewsServerLogin,
   ConfigField_NewsServerPassword,
   ConfigField_GuessSender,
   ConfigField_Sender,
#ifdef USE_SSL
   ConfigField_SSLtext,
   ConfigField_SmtpServerSSL,
   ConfigField_NntpServerSSL,
#endif
   ConfigField_DialUpHelp,
   ConfigField_DialUpSupport,
   ConfigField_BeaconHost,
#ifdef OS_WIN
   ConfigField_NetConnection,
#elif defined(OS_UNIX)
   ConfigField_NetOnCommand,
   ConfigField_NetOffCommand,
#endif // platform
   ConfigField_TimeoutInfo,
   ConfigField_OpenTimeout,
#ifdef USE_TCP_TIMEOUTS
   ConfigField_ReadTimeout,
   ConfigField_WriteTimeout,
   ConfigField_CloseTimeout,
#endif // USE_TCP_TIMEOUTS
   Configfield_IMAPLookAhead,
   ConfigField_RshHelp,
   ConfigField_RshTimeout,
   ConfigField_NetworkLast = ConfigField_RshTimeout,

   // compose
   ConfigField_ComposeFirst = ConfigField_NetworkLast,
   ConfigField_UseOutgoingFolder,
   ConfigField_OutgoingFolder,
   ConfigField_WrapMargin,
   ConfigField_WrapAuto,
   ConfigField_ReplyString,
   ConfigField_ReplyCollapse,
   ConfigField_ReplyCharacters,
   ConfigField_ReplyUseSenderInitials,
   ConfigField_ReplyQuoteEmpty,

   ConfigField_DetectSig,
#if wxUSE_REGEX
   ConfigField_ReplySigSeparatorHelp,
   ConfigField_ReplySigSeparator,
#endif // wxUSE_REGEX

   ConfigField_Signature,
   ConfigField_SignatureFile,
   ConfigField_SignatureSeparator,
   ConfigField_XFaceFile,
   ConfigField_AdbSubstring,
   ConfigField_ComposeViewFontFamily,
   ConfigField_ComposeViewFontSize,
   ConfigField_ComposeViewFGColour,
   ConfigField_ComposeViewBGColour,

   ConfigField_ComposeHeaders,
   ConfigField_ComposeTemplates,

   ConfigField_ComposeLast = ConfigField_ComposeTemplates,

   // folders
   ConfigField_FoldersFirst = ConfigField_ComposeLast,
   ConfigField_ReopenLastFolder_HelpText,
   ConfigField_DontOpenAtStartup,
   ConfigField_ReopenLastFolder,
   ConfigField_OpenFolders,
   ConfigField_MainFolder,
   ConfigField_FolderProgressHelpText,
   ConfigField_FolderProgressThreshold,
   ConfigField_FolderMaxHelpText,
   ConfigField_FolderMaxHeadersNumHard,
   ConfigField_FolderMaxHeadersNum,
   ConfigField_FolderMaxMsgSize,
   ConfigField_NewMailFolder,
   ConfigField_PollIncomingDelay,
   ConfigField_CollectAtStartup,
   ConfigField_UpdateInterval,
   ConfigField_CloseDelay_HelpText,
   ConfigField_CloseDelay,
   ConfigField_AutocollectFolder,
   ConfigField_UseOutbox,
   ConfigField_OutboxName,
   ConfigField_UseTrash,
   ConfigField_TrashName,
   ConfigField_FoldersFileFormat,
   ConfigField_FolderTreeBgColour,
   ConfigField_StatusFormatHelp,
   ConfigField_StatusFormat_StatusBar,
   ConfigField_StatusFormat_TitleBar,
   ConfigField_FoldersLast = ConfigField_StatusFormat_TitleBar,

#ifdef USE_PYTHON
   // python
   ConfigField_PythonFirst = ConfigField_FoldersLast,
   ConfigField_Python_HelpText,
   ConfigField_EnablePython,
   ConfigField_PythonPath,
   ConfigField_StartupScript,
   ConfigField_CallbackFolderOpen,
   ConfigField_CallbackFolderUpdate,
   ConfigField_CallbackFolderExpunge,
   ConfigField_CallbackSetFlag,
   ConfigField_CallbackClearFlag,
   ConfigField_PythonLast = ConfigField_CallbackClearFlag,
#else  // !USE_PYTHON
   ConfigField_PythonLast = ConfigField_FoldersLast,
#endif // USE_PYTHON

   // message view
   ConfigField_MessageViewFirst = ConfigField_PythonLast,
   ConfigField_MessageViewFontFamily,
   ConfigField_MessageViewFontSize,
   ConfigField_MessageViewFGColour,
   ConfigField_MessageViewBGColour,
   ConfigField_MessageViewUrlColour,
   ConfigField_MessageViewHeaderNamesColour,
   ConfigField_MessageViewHeaderValuesColour,
   ConfigField_MessageViewQuotedColourize,
   ConfigField_MessageViewQuotedCycleColours,
   ConfigField_MessageViewQuotedColour1,
   ConfigField_MessageViewQuotedColour2,
   ConfigField_MessageViewQuotedColour3,
   ConfigField_MessageViewProgressHelp,
   ConfigField_MessageViewProgressThresholdSize,
   ConfigField_MessageViewProgressThresholdTime,
   ConfigField_MessageViewInlineGraphics,
   ConfigField_MessageViewInlineGraphicsSize,
   ConfigField_MessageViewAutoDetectEncoding,
   ConfigField_MessageViewPlainIsText,
   ConfigField_MessageViewRfc822IsText,
   ConfigField_ViewWrapMargin,
   ConfigField_ViewWrapAuto,
#ifdef OS_UNIX
   ConfigField_MessageViewFaxSupport,
   ConfigField_MessageViewFaxDomains,
   ConfigField_MessageViewFaxConverter,
#endif // Unix
   ConfigField_MessageViewHeaders,
   ConfigField_MessageViewDateFormat,
   ConfigField_MessageViewTitleBarFormat,
   ConfigField_MessageViewLast = ConfigField_MessageViewTitleBarFormat,

   // folder view options
   ConfigField_FolderViewFirst = ConfigField_MessageViewLast,
   ConfigField_FolderViewNewMailHelp,
   ConfigField_FolderViewNewMailUseCommand,
   ConfigField_FolderViewNewMailCommand,
   ConfigField_FolderViewNewMailShowMsg,
   ConfigField_FolderViewShowHelpText,
   ConfigField_FolderViewShowFirst,
   ConfigField_FolderViewShowFirstUnread,
   ConfigField_FolderViewPreviewOnSelect,
   ConfigField_FolderViewHelpText2,
   ConfigField_FolderViewOnlyNames,
   ConfigField_FolderViewReplaceFrom,
   ConfigField_FolderViewReplaceFromAddresses,
   ConfigField_FolderViewFontFamily,
   ConfigField_FolderViewFontSize,
   ConfigField_FolderViewFGColour,
   ConfigField_FolderViewBGColour,
   ConfigField_FolderViewNewColour,
   ConfigField_FolderViewRecentColour,
   ConfigField_FolderViewUnreadColour,
   ConfigField_FolderViewDeletedColour,
   ConfigField_FolderViewThreadMessages,
#if defined(EXPERIMENTAL_JWZ_THREADING)
#if wxUSE_REGEX
   ConfigField_FolderViewSimplifyingRegex,
   ConfigField_FolderViewReplacementString,
#endif // wxUSE_REGEX
   ConfigField_FolderViewGatherSubjects,
#if !wxUSE_REGEX
   ConfigField_FolderViewRemoveListPrefixGathering,
#endif
   ConfigField_FolderViewBreakThreads,
#if !wxUSE_REGEX
   ConfigField_FolderViewRemoveListPrefixBreaking,
#endif
   ConfigField_FolderViewIndentIfDummy,
#endif // EXPERIMENTAL_JWZ_THREADING
   ConfigField_FolderViewSortMessagesBy,
   ConfigField_FolderViewHeaders,
   ConfigField_FolderViewSizeUnits,
   ConfigField_FolderViewStatusHelp,
   ConfigField_FolderViewUpdateStatus,
   ConfigField_FolderViewStatusBarFormat,
   ConfigField_FolderViewLast = ConfigField_FolderViewStatusBarFormat,

   // folder tree options
   ConfigField_FolderTreeFirst = ConfigField_FolderViewLast,
   ConfigField_FolderTreeFormatHelp,
   ConfigField_FolderTreeFormat,
   ConfigField_FolderTreePropagateHelp,
   ConfigField_FolderTreePropagate,
   ConfigField_FolderTreeLast = ConfigField_FolderTreePropagate,

   // autocollecting and address books options
   ConfigField_AdbFirst = ConfigField_FolderTreeLast,
   ConfigField_AutoCollect_HelpText,
   ConfigField_AutoCollect,
   ConfigField_AutoCollectAdb,
   ConfigField_AutoCollectNameless,
#ifdef USE_BBDB
   ConfigField_Bbdb_HelpText,
   ConfigField_Bbdb_IgnoreAnonymous,
   ConfigField_Bbdb_GenerateUnique,
   ConfigField_Bbdb_AnonymousName,
   ConfigField_Bbdb_SaveOnExit,
   ConfigField_AdbLast = ConfigField_Bbdb_SaveOnExit,
#else // !USE_BBDB
   ConfigField_AdbLast = ConfigField_AutoCollectNameless,
#endif // USE_BBDB/!USE_BBDB

   // helper programs
   ConfigField_HelpersFirst = ConfigField_AdbLast,
   ConfigField_HelpersHelp1,
   ConfigField_Browser,
#ifndef OS_WIN    // we don't care about browser kind under Windows
   ConfigField_BrowserIsNetscape,
#endif // !Win
   ConfigField_BrowserInNewWindow,

#ifdef USE_EXT_HTML_HELP
   ConfigField_HelpersHelp2,
   ConfigField_HelpBrowser,
   ConfigField_HelpBrowserIsNetscape,
#endif // USE_EXT_HTML_HELP

   ConfigField_HelpExternalEditor,
   ConfigField_ExternalEditor,
   ConfigField_AutoLaunchExtEditor,

#ifdef OS_UNIX
   ConfigField_ImageConverter,
   ConfigField_ConvertGraphicsFormat,
#endif // OS_UNIX

   ConfigField_HelpNewMailCommand,
   ConfigField_NewMailCommand,
   ConfigField_HelpersLast = ConfigField_NewMailCommand,

   // other options
   ConfigField_OthersFirst = ConfigField_HelpersLast,
   ConfigField_LogHelp,
   ConfigField_ShowLog,
   ConfigField_LogToFile,
   ConfigField_MailLog,
   ConfigField_ShowTips,
   ConfigField_Splash,
   ConfigField_SplashDelay,
   ConfigField_AutosaveHelp,
   ConfigField_AutosaveDelay,
   ConfigField_ConfirmExit,
   ConfigField_OpenOnClick,
   ConfigField_ShowHiddenFolders,
   ConfigField_HelpDir,
#ifdef USE_SSL
   ConfigField_SslHelp,
   ConfigField_SslDllName,
   ConfigField_CryptoDllName,
#endif // USE_SSL
#ifdef OS_UNIX
   ConfigField_AFMPath,
   ConfigField_FocusFollowsMouse,
   ConfigField_DockableMenubars,
   ConfigField_DockableToolbars,
   ConfigField_ToolbarsFlatButtons,
#endif // OS_UNIX
   ConfigField_ReenableDialog,
   ConfigField_AwayHelp,
   ConfigField_AwayAutoEnter,
   ConfigField_AwayAutoExit,
   ConfigField_AwayRemember,
   ConfigField_OthersLast = ConfigField_AwayRemember,

   ConfigField_SyncFirst = ConfigField_OthersLast,
   ConfigField_RemoteSynchroniseMessage,
   ConfigField_RSynchronise,
   ConfigField_RSConfigFolder,
   ConfigField_RSFilters,
   ConfigField_RSIds,
   ConfigField_RSFolders,
   ConfigField_RSFolderGroup,
   ConfigField_SyncStore,
   ConfigField_SyncRetrieve,
   ConfigField_SyncLast = ConfigField_SyncRetrieve,

   // the end
   ConfigField_Max
};

// -----------------------------------------------------------------------------
// our notebook class
// -----------------------------------------------------------------------------

// notebook for the options
class wxOptionsNotebook : public wxNotebookWithImages
{
public:
   // icon names
   static const char *ms_aszImages[];

   wxOptionsNotebook(wxWindow *parent);

   // the profile we use - just the global one here
   Profile *GetProfile() const { return Profile::CreateProfile(""); }
};

// notebook for the given options page
class wxCustomOptionsNotebook : public wxNotebookWithImages
{
public:
   wxCustomOptionsNotebook(wxWindow *parent,
                           size_t nPages,
                           const wxOptionsPageDesc *pageDesc,
                           const wxString& configForNotebook,
                           Profile *profile);

   virtual ~wxCustomOptionsNotebook() { delete [] m_aImages; }

private:
   // this method creates and fills m_aImages and returns it
   const char **GetImagesArray(size_t nPages, const wxOptionsPageDesc *pageDesc);

   // the images names and NULL
   const char **m_aImages;
};

// -----------------------------------------------------------------------------
// dialog classes
// -----------------------------------------------------------------------------

class wxGlobalOptionsDialog : public wxOptionsEditDialog
{
public:
   wxGlobalOptionsDialog(wxFrame *parent, const wxString& configKey = "OptionsDlg");

   virtual ~wxGlobalOptionsDialog();

   // override base class functions
   virtual void CreateNotebook(wxPanel *panel);
   virtual bool TransferDataToWindow();

   // unimplemented default ctor for DECLARE_DYNAMIC_CLASS
   wxGlobalOptionsDialog() { wxFAIL_MSG("should be never used"); }

   // return TRUE if this dialog edits global options for the program, FALSE
   // if this is another kind of dialog
   virtual bool IsGlobalOptionsDialog() const { return TRUE; }

protected:
   // implement base class pure virtual
   virtual Profile *GetProfile() const
   {
      return ((wxOptionsNotebook *)m_notebook)->GetProfile();
   }

private:
   DECLARE_DYNAMIC_CLASS(wxGlobalOptionsDialog)
};

// just like wxGlobalOptionsDialog but uses the given wxOptionsPage and not the
// standard ones
class wxCustomOptionsDialog : public wxGlobalOptionsDialog
{
public:
   // minimal ctor, use SetPagesDesc() and SetProfile() later
   wxCustomOptionsDialog(wxFrame *parent,
                         const wxString& configForDialog = "CustomOptions",
                         const wxString& configForNotebook = "CustomNotebook")
      : wxGlobalOptionsDialog(parent, configForDialog),
        m_configForNotebook(configForNotebook)
   {
      SetProfile(NULL);
      SetPagesDesc(0, NULL);
   }

   // full ctor specifying everything we need
   wxCustomOptionsDialog(size_t nPages,
                         const wxOptionsPageDesc *pageDesc,
                         Profile *profile,
                         wxFrame *parent,
                         const wxString& configForDialog = "CustomOptions",
                         const wxString& configForNotebook = "CustomNotebook")
      : wxGlobalOptionsDialog(parent, configForDialog),
        m_configForNotebook(configForNotebook)
   {
      m_profile = profile;
      SafeIncRef(m_profile);

      SetPagesDesc(nPages, pageDesc);
   }

   // delayed initializetion: use these methods for an object constructed with
   // the first ctor
   void SetPagesDesc(size_t nPages, const wxOptionsPageDesc *pageDesc)
   {
      m_nPages = nPages;
      m_pageDesc = pageDesc;
   }

   void SetProfile(Profile *profile)
   {
      m_profile = profile;
      SafeIncRef(m_profile);
   }

   // dtor
   virtual ~wxCustomOptionsDialog()
   {
      SafeDecRef(m_profile);
   }

   // overloaded base class virtual
   virtual void CreateNotebook(wxPanel *panel)
   {
      m_notebook = new wxCustomOptionsNotebook(panel,
                                               m_nPages,
                                               m_pageDesc,
                                               m_configForNotebook,
                                               m_profile);
   }

private:
   // the number and descriptions of the pages we show
   size_t m_nPages;
   const wxOptionsPageDesc *m_pageDesc;

   // the profile
   Profile *m_profile;

   // the config key where notebook will remember its last page
   wxString m_configForNotebook;
};

// an identity edit dialog: works with settings in an identity profile, same as
// wxGlobalOptionsDialog otherwise
class wxIdentityOptionsDialog : public wxCustomOptionsDialog
{
public:
   wxIdentityOptionsDialog(const wxString& identity, wxFrame *parent)
      : wxCustomOptionsDialog(parent, "IdentDlg", "IdentNotebook"),
        m_identity(identity)
   {
      // use the identity profile
      Profile *profile = Profile::CreateIdentity(identity);
      SetProfile(profile);
      profile->DecRef();   // SetProfile() will hold on it

      // set the pages descriptions: we use the standard pages of the options
      // dialog, but not all of them
      CreatePagesDesc();
      SetPagesDesc(m_nPages, m_aPages);

      SetTitle(wxString::Format(_("Settings for identity '%s'"), m_identity.c_str()));
   }

   virtual ~wxIdentityOptionsDialog()
   {
      delete [] m_aPages;
   }

   // editing identities shouldn't give warning about "important programs
   // settings were changed" as the only really important ones are the global
   // ones
   virtual void SetDoTest() { SetDirty(); } // TODO: might do something here
   virtual void SetGiveRestartWarning() { }

   // we're not the global options dialog
   virtual bool IsGlobalOptionsDialog() const { return FALSE; }

private:
   // create our pages desc: do it dynamically because they may depend on the
   // user level (which can change) in the future
   void CreatePagesDesc();

   // the identity which we edit
   wxString m_identity;

   // the array of descriptions of our pages
   size_t m_nPages;
   wxOptionsPageDesc *m_aPages;
};

// another dialog (not for options this one) which allows to restore the
// previously changed settings
class wxRestoreDefaultsDialog : public wxProfileSettingsEditDialog
{
public:
   wxRestoreDefaultsDialog(Profile *profile, wxFrame *parent);

   // reset the selected options to their default values
   virtual bool TransferDataFromWindow();

private:
   wxCheckListBox *m_checklistBox;
};

// ----------------------------------------------------------------------------
// event tables and such
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxGlobalOptionsDialog, wxOptionsEditDialog)

BEGIN_EVENT_TABLE(wxOptionsPage, wxNotebookPageBase)
   // any change should make us dirty
   EVT_CHECKBOX(-1, wxOptionsPage::OnControlChange)
   EVT_RADIOBOX(-1, wxOptionsPage::OnControlChange)
   EVT_CHOICE(-1, wxOptionsPage::OnControlChange)
   EVT_TEXT(-1, wxOptionsPage::OnChange)

   // listbox events handling
   EVT_BUTTON(-1, wxOptionsPage::OnListBoxButton)

   EVT_UPDATE_UI(wxOptionsPage_BtnModify, wxOptionsPage::OnUpdateUIListboxBtns)
   EVT_UPDATE_UI(wxOptionsPage_BtnDelete, wxOptionsPage::OnUpdateUIListboxBtns)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(wxOptionsPageCompose, wxOptionsPage)
   // buttons invoke subdialogs
   EVT_BUTTON(-1, wxOptionsPageCompose::OnButton)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(wxOptionsPageMessageView, wxOptionsPage)
   // buttons invoke subdialogs
   EVT_BUTTON(-1, wxOptionsPageMessageView::OnButton)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(wxOptionsPageFolderView, wxOptionsPage)
   // buttons invoke subdialogs
   EVT_BUTTON(-1, wxOptionsPageFolderView::OnButton)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(wxOptionsPageFolders, wxOptionsPage)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(wxOptionsPageIdent, wxOptionsPage)
   // buttons invoke subdialogs
   EVT_BUTTON(-1, wxOptionsPageIdent::OnButton)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(wxOptionsPageSync, wxOptionsPage)
   EVT_BUTTON(-1, wxOptionsPageSync::OnButton)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(wxOptionsPageOthers, wxOptionsPage)
   EVT_BUTTON(-1, wxOptionsPageOthers::OnButton)
END_EVENT_TABLE()

// ============================================================================
// data: both of these arrays *must* be in sync with ConfigFields enum!
// ============================================================================

// The labels of all fields, their types and also the field they "depend on"
// (being dependent on another field only means that if that field is disabled
//  or unchecked, we're disabled too)
//
// Special case: if the index of the field we depend on is negative, the
// update logic is inverted: this field will be enabled only if the other one
// is disabled/unchecked. Note that it is impossible to have the field to
// inversely depend on the field with index 1 - but this is probably ok.
//
// If you modify this array, search for DONT_FORGET_TO_MODIFY and modify data
// there too
const wxOptionsPage::FieldInfo wxOptionsPageStandard::ms_aFields[] =
{
   // general config and identity
   { gettext_noop("&Personal name"),               Field_Text,    -1,                        },
   { gettext_noop("&E-mail address"),              Field_Text | Field_Vital,   -1, },
   { gettext_noop("&Reply address"),               Field_Text | Field_Advanced,   -1, },
   { gettext_noop("The following host name can be used as a default host "
                  "name for local mail addresses."),
                                                   Field_Message, -1 },
   { gettext_noop("&Add this hostname if none specified"), Field_Bool, -1 },
   { gettext_noop("&Hostname"),                    Field_Text | Field_Vital,   ConfigField_AddDefaultHostname, },
   { gettext_noop("Reply return address from &To: field"), Field_Bool | Field_Advanced, -1, },
   { gettext_noop(
      "You may want to attach your personal information card (vCard)\n"
      "to all outoing messages. In this case you will need to specify\n"
      "a file containing it.\n"
      "Notice that such file can be created by exporting an address\n"
      "book entry in the vCard format."), Field_Message, -1 },
   { gettext_noop("Attach a v&Card to outgoing messages"), Field_Bool,    -1,                        },
   { gettext_noop("&vCard file"),                  Field_File, ConfigField_UseVCard,                        },
   { gettext_noop("Some more rarely used and less obvious program\n"
                  "features are only accessible in the so-called\n"
                  "`advanced' user mode which may be set here."),
                                                   Field_Message | Field_Global,   -1,                        },
   { gettext_noop("User &level:novice:advanced"),  Field_Combo | Field_Global,   -1,                        },
   { gettext_noop("Set &global password"),  Field_SubDlg | Field_Global, -1,},

   // network
   { gettext_noop("The following fields are used as default values for the\n"
                  "corresponding server names. You may set them independently\n"
                  "for each folder as well, overriding the values specified "
                  "here"),                         Field_Message, -1,                        },
   { gettext_noop("&POP server"),                  Field_Text,    -1,                        },
   { gettext_noop("&IMAP server"),                 Field_Text,    -1,                        },
#ifdef USE_SENDMAIL
   { gettext_noop("Use local mail transfer a&gent"), Field_Bool, -1,           },
   { gettext_noop("Local MTA &command"), Field_Text, ConfigField_UseSendmail },
#endif // USE_SENDMAIL
   { gettext_noop("SMTP (&mail) server"),          Field_Text | Field_Vital,
#ifdef USE_SENDMAIL
                                                   -ConfigField_UseSendmail,
#else
                                                   -1
#endif
   },
   { gettext_noop("NNTP (&news) server"),          Field_Text,    -1,
   },
   { gettext_noop(
      "Some SMTP or NNTP servers require a user Id or login.\n"
      "You might need to enter the e-mail address provided by\n"
      "your ISP here. Only enter a password if told to do so\n"
      "by your ISP as it is not usually required."),
     Field_Message, -1,                        },
   { gettext_noop("SMTP server &user ID"),         Field_Text,
#ifdef USE_SENDMAIL
                                                   -ConfigField_UseSendmail,
#else
                                                   -1
#endif
   },
   { gettext_noop("SMTP server pa&ssword"),        Field_Passwd, ConfigField_MailServerLogin,           },
   { gettext_noop("NNTP server user &ID"),         Field_Text,   -1,           },
   { gettext_noop("NNTP server pass&word"),        Field_Passwd, ConfigField_NewsServerLogin,           },

   { gettext_noop("Try to guess SMTP sender header"), Field_Bool | Field_Advanced, ConfigField_MailServerLogin,           },
   { gettext_noop("SMTP sender header"), Field_Text | Field_Advanced, -ConfigField_GuessSender,           },
#ifdef USE_SSL
   { gettext_noop("Mahogany can attempt to use SSL (secure sockets layer) to send\n"
                  "mail or news. Tick the following boxes to activate this.")
     , Field_Message, -1 },
   { gettext_noop("SMTP server uses SS&L"), Field_Bool,    -1,                        },
   { gettext_noop("NNTP s&erver uses SSL"), Field_Bool,    -1,                        },
#endif // USE_SSL
   { gettext_noop("Mahogany contains support for dial-up networks and can detect if the\n"
                  "network is up or not. It can also be used to connect and disconnect the\n"
                  "network. To aid in detecting the network status, you can specify a beacon\n"
                  "host which should only be reachable if the network is up, e.g. the WWW\n"
                  "server of your ISP. Leave it empty to use the SMTP server for this.")
     , Field_Message | Field_Global, -1 },
   { gettext_noop("&Dial-up network support"),    Field_Bool | Field_Global,    -1,                        },
   { gettext_noop("&Beacon host (e.g. www.yahoo.com)"), Field_Text | Field_Global,   ConfigField_DialUpSupport},
#ifdef OS_WIN
   { gettext_noop("&RAS connection to use"),   Field_Combo | Field_Global, ConfigField_DialUpSupport},
#elif defined(OS_UNIX)
   { gettext_noop("Command to &activate network"),   Field_Text | Field_Global, ConfigField_DialUpSupport},
   { gettext_noop("Command to &deactivate network"), Field_Text | Field_Global, ConfigField_DialUpSupport},
#endif // platform
   { gettext_noop("The following timeout value is used for TCP connections to\n"
                  "remote mail or news servers."), Field_Message | Field_Global | Field_Advanced, -1 },
   { gettext_noop("&Open timeout (in seconds)"),  Field_Number | Field_Global | Field_Advanced,    -1,                        },
#ifdef USE_TCP_TIMEOUTS
   { gettext_noop("&Read timeout"),                Field_Number | Field_Global | Field_Advanced,    -1,                        },
   { gettext_noop("&Write timeout"),               Field_Number | Field_Global | Field_Advanced,    -1,                        },
   { gettext_noop("&Close timeout"),               Field_Number |
     Field_Global | Field_Advanced,    -1,                        },
#endif // USE_TCP_TIMEOUTS
   { gettext_noop("&Pre-fetch this many msgs"), Field_Number | Field_Global | Field_Advanced,    -1,                        },
   { gettext_noop("If the RSH timeout below is greater than 0, Mahogany will\n"
                  "first try to connect to IMAP servers using rsh instead of\n"
                  "sending passwords in clear text. However, if the server\n"
                  "does not support rsh connections, enabling this option can\n"
                  "lead to unneeded delays."),     Field_Message | Field_Global | Field_Advanced,    -1,                        },
   { gettext_noop("&Rsh timeout"),                 Field_Number | Field_Global | Field_Advanced,    -1,                        },

   // compose
   { gettext_noop("Sa&ve sent messages"),          Field_Bool,    -1,                        },
   { gettext_noop("&Folder for sent messages"),
                                                   Field_Folder,    ConfigField_UseOutgoingFolder },
   { gettext_noop("&Wrap margin"),                 Field_Number | Field_Global,  -1,                        },
   { gettext_noop("Wra&p lines automatically"),    Field_Bool | Field_Global,  -1,                        },
   { gettext_noop("&Reply string in subject"),     Field_Text,    -1,                        },
   { gettext_noop("Co&llapse reply markers"
                  ":no:collapse:collapse & count"),Field_Combo,   -1,                        },
   { gettext_noop("Reply prefi&x"),                Field_Text,    -1,                        },
   { gettext_noop("Prepend &sender initials"),     Field_Bool,    ConfigField_ReplyCharacters,                        },
   { gettext_noop("&Quote empty lines too"),       Field_Bool |
                                                   Field_Advanced,    ConfigField_ReplyCharacters,                        },
   { gettext_noop("Detect and remove signature when replying"),       Field_Bool |
                                                   Field_Advanced,    -1 ,                        },
#if wxUSE_REGEX
   { gettext_noop("This regular expression is used to detect the beginning\n"
                  "of the signature of the message replied to."),
                                                   Field_Message | Field_Advanced, ConfigField_DetectSig },
   { gettext_noop("Signature separator"),          Field_Text | Field_Advanced,    ConfigField_DetectSig,     },
#endif // wxUSE_REGEX

   { gettext_noop("&Use signature"),               Field_Bool,    -1,                        },
   { gettext_noop("&Signature file"),              Field_File,    ConfigField_Signature      },
   { gettext_noop("Use signature se&parator"),     Field_Bool,    ConfigField_Signature      },

   { gettext_noop("Configure &XFace..."),                  Field_XFace,  -1          },
   { gettext_noop("Mail alias substring ex&pansion"),
                                                   Field_Bool,    -1,                        },
   { gettext_noop("Font famil&y"
                  ":default:decorative:roman:script:swiss:modern:teletype"),
                                                   Field_Combo | Field_Global,   -1},
   { gettext_noop("Font si&ze"),                   Field_Number | Field_Global,  -1},
   { gettext_noop("Foreground c&olour"),           Field_Color | Field_Global,   -1},
   { gettext_noop("Back&ground colour"),           Field_Color | Field_Global,   -1},

   { gettext_noop("Configure &headers..."),        Field_SubDlg,  -1},
   { gettext_noop("Configure &templates..."),      Field_SubDlg,  -1},

   // folders
   { gettext_noop("You may choose to not open any folders at all on startup,\n"
                  "reopen all folders which were open when the program was closed\n"
                  "for the last time or explicitly specify the folders to reopen:"),
                  Field_Message | Field_AppWide, -1 },
   { gettext_noop("Don't open any folders at startup"), Field_Bool | Field_AppWide, -1, },
   { gettext_noop("Reopen last open folders"), Field_Bool | Field_AppWide,
                                               -ConfigField_DontOpenAtStartup, },
   { gettext_noop("Folders to open on &startup"),  Field_List |
                                                   Field_Restart |
                                                   Field_AppWide,
                                                   -ConfigField_ReopenLastFolder,           },
   { gettext_noop("Folder opened in &main frame"), Field_Folder |
                                                   Field_Restart |
                                                   Field_AppWide,
                                                   -ConfigField_ReopenLastFolder,                        },
   { gettext_noop("A progress dialog will be shown while retrieving more\n"
                  "the specified number of messages. Set it to 0 to never\n"
                  "show the progress dialog at all."), Field_Message, -1},
   { gettext_noop("&Threshold for displaying progress dialog"), Field_Number, -1},
   { gettext_noop("The following settings allow to limit the amount of data\n"
                  "retrieved from remote server: if the message size or\n"
                  "number is greater than the value specified here, you\n"
                  "will be asked for confirmation before transfering data.\n"
                  "Additionally, if you set the hard limit, only that many\n"
                  "messages will be downloaded without asking."),
                                                   Field_Message,  -1 },
   { gettext_noop("&Hard message limit"),  Field_Number,   -1 },
   { gettext_noop("Ask if &number of messages >"),  Field_Number,   -1 },
   { gettext_noop("Ask if size of &message (in Kb) >"), Field_Number,   -1 },
   { gettext_noop("Folder where to collect &new mail"), Field_Folder | Field_AppWide, -1},
   { gettext_noop("Poll for &new mail interval in seconds"), Field_Number, -1},
   { gettext_noop("Poll for new mail at s&tartup"), Field_Bool | Field_AppWide, -1},
   { gettext_noop("&Ping/check folder interval in seconds"), Field_Number, -1},
   { gettext_noop("Mahogany may keep the folder open after closing it\n"
                  "for some time to make reopening the folder faster.\n"
                  "This is useful for folders you often reopen."), Field_Message, -1 },
   { gettext_noop("&Keep open for (seconds)"), Field_Number, -1},
   { gettext_noop("Folder to save &collected messages to"), Field_Folder | Field_AppWide, -1 },
   { gettext_noop("Send outgoing messages later"), Field_Bool, -1 },
   { gettext_noop("Folder for &outgoing messages"), Field_Folder, ConfigField_UseOutbox },
   { gettext_noop("Use &Trash folder"), Field_Bool, -1},
   { gettext_noop("&Trash folder name"), Field_Folder, ConfigField_UseTrash},
   { gettext_noop("Default format for mailbox files"
      ":Unix mbx mailbox:Unix mailbox:MMDF (SCO Unix):Tenex (Unix MM format)"),
     Field_Combo | Field_AppWide, -1},
   { gettext_noop("Folder tree &background"), Field_Color | Field_AppWide, -1 },
   { gettext_noop("You can specify the format for the strings shown in the\n"
                  "status and title bars. Use %f for the folder name and\n"
                  "%t, %r and %n for the number of all, recent and new\n"
                  "messages respectively."), Field_Message, -1 },
   { gettext_noop("Status &bar format"), Field_Text, -1 },
   { gettext_noop("T&itle bar format"), Field_Text, -1 },

#ifdef USE_PYTHON
   // python
   { gettext_noop("Python is the built-in scripting language which can be\n")
     gettext_noop("used to extend Mahogany's functionality. It is not essential\n")
     gettext_noop("for the program's normal operation."), Field_Message, -1},
   { gettext_noop("&Enable Python"),               Field_Bool |
                                                   Field_AppWide, -1,                        },
   { gettext_noop("Python &path"),                 Field_Text |
                                                   Field_AppWide, ConfigField_EnablePython   },
   { gettext_noop("&Startup script"),              Field_File |
                                                   Field_AppWide, ConfigField_EnablePython   },
   { gettext_noop("&Folder open callback"),        Field_Text,    ConfigField_EnablePython   },
   { gettext_noop("Folder &update callback"),      Field_Text,    ConfigField_EnablePython   },
   { gettext_noop("Folder e&xpunge callback"),     Field_Text,    ConfigField_EnablePython   },
   { gettext_noop("Flag &set callback"),           Field_Text,    ConfigField_EnablePython   },
   { gettext_noop("Flag &clear callback"),         Field_Text,    ConfigField_EnablePython   },
#endif // USE_PYTHON

   // message view
   { gettext_noop("&Font family"
                  ":default:decorative:roman:script:swiss:modern:teletype"),
                                                   Field_Combo,   -1 },
   { gettext_noop("Font si&ze"),                   Field_Number,  -1 },
   { gettext_noop("Foreground c&olour"),           Field_Color,   -1 },
   { gettext_noop("Back&ground colour"),           Field_Color,   -1 },
   { gettext_noop("Colour for &URLs"),             Field_Color,   -1 },
   { gettext_noop("Colour for header &names"),     Field_Color,   -1 },
   { gettext_noop("Colour for header &values"),    Field_Color,   -1 },
   { gettext_noop("Colourize &quoted text"),       Field_Bool,    -1 },
   { gettext_noop("Reuse colours when too many quotation marks"),Field_Bool,    ConfigField_MessageViewQuotedColourize },
   { gettext_noop("Colour for &1st level of quoted text"),Field_Color,   ConfigField_MessageViewQuotedColourize },
   { gettext_noop("Colour for &2nd level of quoted text"),Field_Color,   ConfigField_MessageViewQuotedColourize },
   { gettext_noop("Colour for &3nd level of quoted text"),Field_Color,   ConfigField_MessageViewQuotedColourize },
   { gettext_noop("A progress dialog can be shown during the message download\n"
                  "if it is bigger than the given size or takes longer than the\n"
                  "specified time (use -1 to disable progress dialog entirely)"), Field_Message, -1 },
   { gettext_noop("Dialog minimal size t&hreshold (kb)"),             Field_Number,    -1 },
   { gettext_noop("Progress dialog &delay (seconds)"),             Field_Number,    -1 },
   { gettext_noop("Show images &inline"),             Field_Bool,    -1 },
   { gettext_noop("But only if size is less than (kb)"), Field_Number, ConfigField_MessageViewInlineGraphics },
   { gettext_noop("&Autodetect font encoding"),    Field_Bool,    -1 },
   { gettext_noop("Display &text attachments inline"),Field_Bool,    -1 },
   { gettext_noop("Display &mail messages as text"),Field_Bool,    -1 },
   { gettext_noop("&Wrap margin"),                 Field_Number,  -1,                        },
   { gettext_noop("Wra&p lines automatically"),    Field_Bool,  -1,                        },
#ifdef OS_UNIX
   { gettext_noop("Support special &fax mailers"), Field_Bool,    -1 },
   { gettext_noop("&Domains sending faxes"),       Field_Text,    ConfigField_MessageViewFaxSupport},
   { gettext_noop("Conversion program for fa&xes"), Field_File,    ConfigField_MessageViewFaxSupport},
#endif // unix
   { gettext_noop("Configure &headers to show..."),Field_SubDlg,   -1 },
   { gettext_noop("Configure &format for displaying dates"),         Field_SubDlg,    -1                     },
   { gettext_noop("&Title of message view frame"),         Field_Text,    -1                     },

   // folder view
   { gettext_noop("When new mail message appears in this folder Mahogany\n"
                  "may execute an external command and/or show a message "
                  "about it."), Field_Message,    -1 },
   { gettext_noop("E&xecute new mail command"), Field_Bool,    -1 },
   { gettext_noop("New mail &command"), Field_File, ConfigField_FolderViewNewMailUseCommand},
   { gettext_noop("Show new mail &notification"), Field_Bool,    -1 },
   { gettext_noop("\nWhat happens when the folder is opened? If selecting\n"
                  "unread message is on, Mahogany will select the first or\n"
                  "last unread message depending on the next option. If there\n"
                  "are no unread messages, it will just select first or last\n"
                  "message."), Field_Message,  -1 },
   { gettext_noop("&Select first message (or the last one)"), Field_Bool, -1 },
   { gettext_noop("Select first &unread"), Field_Bool, -1 },
   { gettext_noop("Preview message when &selected"), Field_Bool,    -1 },
   { gettext_noop("\nThe following settings control appearance of the messages list:"), Field_Message,  -1 },
   { gettext_noop("Show only sender's name, not &e-mail"), Field_Bool,    -1 },
   { gettext_noop("Show \"&To\" for messages from oneself"), Field_Bool,    -1 },
   { gettext_noop("&Addresses to replace with \"To\""),  Field_List, ConfigField_FolderViewReplaceFrom,           },
   { gettext_noop("Font famil&y"
                  ":default:decorative:roman:script:swiss:modern:teletype"),
                                                   Field_Combo,   -1},
   { gettext_noop("Font si&ze"),                   Field_Number,  -1},
   { gettext_noop("Foreground c&olour"),           Field_Color,   -1},
   { gettext_noop("&Backgroud colour"),            Field_Color,   -1},
   { gettext_noop("Colour for &new message"),      Field_Color,   -1},
   { gettext_noop("Colour for &recent messages"),  Field_Color,   -1},
   { gettext_noop("Colour for u&nread messages"),  Field_Color,   -1},
   { gettext_noop("Colour for &deleted messages" ),Field_Color,   -1},
   { gettext_noop("&Thread messages"),             Field_Bool,    -1},

#if defined(EXPERIMENTAL_JWZ_THREADING)
#if wxUSE_REGEX
   { gettext_noop("Regex used to simplify subjects"),       Field_Text,    ConfigField_FolderViewThreadMessages},
   { gettext_noop("Replacement string for the matched part"),       Field_Text,    ConfigField_FolderViewThreadMessages},
#endif // wxUSE_REGEX
   { gettext_noop("Gather messages with same subject"),              Field_Bool,    ConfigField_FolderViewThreadMessages},
#if !wxUSE_REGEX
   { gettext_noop("Remove list prefix to compare subjects to gather messages"),         Field_Bool,    ConfigField_FolderViewGatherSubjects},
#endif // !wxUSE_REGEX
   { gettext_noop("B&reak thread when subject changes"),             Field_Bool,    ConfigField_FolderViewThreadMessages},
#if !wxUSE_REGEX
   { gettext_noop("Remove list prefix to compare subjects to break threads"),         Field_Bool,    ConfigField_FolderViewBreakThreads},
#endif // !wxUSE_REGEX
   { gettext_noop("Indent messages with missing ancestor"),          Field_Bool,    ConfigField_FolderViewThreadMessages},
#endif // EXPERIMENTAL_JWZ_THREADING

   { gettext_noop("&Sort messages by..."),         Field_SubDlg,  -1},
   { gettext_noop("Configure &columns to show..."),Field_SubDlg,   -1 },
   // combo choices must be in sync with MessageSizeShow enum values
   { gettext_noop("Show size in &units of:automatic:autobytes:bytes:kbytes:mbytes"),
                                                   Field_Combo,   -1 },
   { gettext_noop("You can choose to show the information about\n"
                  "the currently selected message in the status bar.\n"
                  "You can use the same macros as in the template\n"
                  "dialog (i.e. $subject, $from, ...) in this string."),
                                                   Field_Message, -1 },
   { gettext_noop("&Use status bar"),              Field_Bool,    -1 },
   { gettext_noop("&Status bar line format"),      Field_Text,    ConfigField_FolderViewUpdateStatus                     },

   // folder tree
   { gettext_noop("Mahogany can show the number of messages in the folder\n"
                  "directly in the folder tree. You may wish to disable\n"
                  "this feature to speed it up slightly by leaving the text\n"
                  "below empty or enter a string containing %t and/or %u\n"
                  "to be replaced with the total number of messages and the\n"
                  "number of unseen messages respectively."),
                  Field_Message, -1 },
   { gettext_noop("Folder tree format:"), Field_Text, -1 },
   { gettext_noop("By default, if the folder has new/recent/unread messages\n"
                  "its parent is shown in the same state as well. Disable\n"
                  "it below if you don't like it (this makes sense mostly\n"
                  "for folders such as \"Trash\" or \"Sent Mail\")."), Field_Message, -1 },
   { gettext_noop("Parent shows status"), Field_Bool, -1 },

   // adb: autocollect and bbdb options
   { gettext_noop("Mahogany may automatically remember all e-mail addresses in the messages you\n"
                  "receive in a special addresss book. This is called 'address\n"
                  "autocollection' and may be turned on or off from this page."),
                                                   Field_Message, -1                     },
   { gettext_noop("&Autocollect addresses"),       Field_Action,  -1,                    },
   { gettext_noop("Address &book to use"),         Field_Text, ConfigField_AutoCollect   },
   { gettext_noop("Ignore addresses without &names"), Field_Bool, ConfigField_AutoCollect},
#ifdef USE_BBDB
   { gettext_noop("The following settings configure the support of the Big Brother\n"
                  "addressbook (BBDB) format. This is supported only for compatibility\n"
                  "with other software (emacs). The normal addressbook is unaffected by\n"
                  "these settings."), Field_Message, -1},
   { gettext_noop("&Ignore entries without names"), Field_Bool, -1 },
   { gettext_noop("&Generate unique aliases"),      Field_Bool, -1 },
   { gettext_noop("&Name for nameless entries"),    Field_Text, ConfigField_Bbdb_GenerateUnique },
   { gettext_noop("&Save on exit"),                 Field_Action, -1 },
#endif // USE_BBDB

   // helper programs
   { gettext_noop("The following program will be used to open URLs embedded in messages:"),       Field_Message, -1                      },
   { gettext_noop("Open &URLs with"),             Field_File,    -1                      },
      // we don't care if it is Netscape or not under Windows
#ifndef OS_WIN
   { gettext_noop("URL &browser is Netscape"),    Field_Bool,    -1                      },
#endif // OS_UNIX
   { gettext_noop("Open browser in new &window"), Field_Bool,
      // under Unix we can only implement this with Netscape, under Windows it
      // also works with IE (and presumably others too)
#ifdef OS_WIN
                  -1
#else // Unix
                  ConfigField_BrowserIsNetscape
#endif // Win/Unix
   },
#ifdef USE_EXT_HTML_HELP
   { gettext_noop("The following program will be used to view the online help system:"),     Field_Message, -1                      },
   { gettext_noop("&Help viewer"),                Field_File,    -1                      },
   { gettext_noop("Help &viewer is Netscape"),    Field_Bool,    -1                      },
#endif // USE_EXT_HTML_HELP
   { gettext_noop("You may configure the external editor to be used when composing the messages\n"
                  "and optionally choose to launch it automatically."),
                                                  Field_Message, -1                      },
   { gettext_noop("&External editor"),            Field_Text,    -1                      },
   { gettext_noop("Always &use it"),              Field_Bool, ConfigField_ExternalEditor },
#ifdef OS_UNIX
   { gettext_noop("&Image format converter"),     Field_File,    -1                      },
   { gettext_noop("Conversion &graphics format"
                  ":XPM:PNG:BMP:JPG:GIF:PCX:PNM"),    Field_Combo,   -1 },
#endif
   { gettext_noop("The following line will be executed each time new mail is received:"),       Field_Message, -1                      },
   { gettext_noop("&New Mail Command"),           Field_File,    -1                      },

   // other options
   { gettext_noop("Mahogany may log everything into the log window, a file\n"
                  "or both simultaneously. Additionally, you may enable mail\n"
                  "debugging option to get much more detailed messages about\n"
                  "mail folder accesses: although this slows down the program\n"
                  "a lot, it is very useful to diagnose the problems.\n\n"
                  "Please turn it on and join the log output to any bug\n"
                  "reports you send us (of course, everybody knows that there\n"
                  "are no bugs in Mahogany, but just in case :-)"),             Field_Message,    -1,                    },
   { gettext_noop("Show &log window"),             Field_Bool,    -1,                    },
   { gettext_noop("Log to &file"),                 Field_File,    -1,                    },
   { gettext_noop("Debug server and mailbox access"), Field_Bool, -1                     },
   { gettext_noop("Show &tips at startup"),        Field_Bool,    -1,                    },
   { gettext_noop("&Splash screen at startup"),    Field_Bool | Field_Restart, -1,                    },
   { gettext_noop("Splash screen &delay"),         Field_Number,  ConfigField_Splash     },
   { gettext_noop("If autosave delay is not 0, the program will periodically\n"
                  "save all unsaved information which reduces the risk of loss\n"
                  "of information"),               Field_Message, -1                     },
   { gettext_noop("&Autosave delay"),              Field_Number, -1                      },
   { gettext_noop("Confirm &exit"),                Field_Bool | Field_Restart, -1                     },
   { gettext_noop("Open folder on single &click"), Field_Bool,    -1                     },
   { gettext_noop("Show &hidden folders in the folder tree"), Field_Bool,    -1                     },

   { gettext_noop("Directory with the help files"), Field_Dir, -1 },

#ifdef USE_SSL
   /* The two settings are not really Field_Restart, but if one has
      tried to use SSL before setting them correctly, then
      MailFolderCC will not attempt to load the libs again. So we just
      pretent that you always have to restart it to prevent users from
      complaining to us if it doesn't work. I'm lazy. KB*/
   { gettext_noop("Mahogany can use SSL (Secure Sockets Layer) for secure,\n"
                  "encrypted communications, if you have the libssl and libcrypto\n"
                  "shared libraries (DLLs) on your system."),
     Field_Message, -1                     },
   { gettext_noop("Path where to find s&hared libssl"), Field_File|Field_Restart,    -1                     },
   { gettext_noop("Path where to find sha&red libcrypto"), Field_File|Field_Restart,    -1                     },
#endif // USE_SSL
#ifdef OS_UNIX
   { gettext_noop("&Path where to find AFM files"), Field_Dir,    -1                     },
   { gettext_noop("&Focus follows mouse"), Field_Bool,    -1                     },
   { gettext_noop("Use floating &menu-bars"), Field_Bool,    -1                     },
   { gettext_noop("Use floating &tool-bars"), Field_Bool,    -1                     },
   { gettext_noop("Tool-bars with f&lat buttons"), Field_Bool,    -1                     },
#endif // OS_UNIX
   { gettext_noop("&Reenable disabled message boxes..."), Field_SubDlg, -1 },
   { gettext_noop("\"Away\", or unattended, state is a special mode in\n"
                  "which Mahogany tries to avoid any interaction with the user,\n"
                  "e.g. new mail notification is disabled, no progress dialogs\n"
                  "are shown &&c.\n"
                  "\n"
                  "This is useful if you want to not be distracted by new\n"
                  "mail arrival temporarily without having to shut down."), Field_Message, -1 },
   { gettext_noop("Enter awa&y mode when idle during (min):"), Field_Number, -1 },
   { gettext_noop("E&xit away mode automatically"), Field_Bool, -1 },
   { gettext_noop("Rememeber a&way status"), Field_Bool, -1 },

   // sync page
   { gettext_noop("Mahogany can synchronise part of its configuration\n"
                  "with settings stored in a special folder. This can\n"
                  "be used to share settings between machines by storing\n"
                  "them in a special IMAP folder on the server.\n"
                  "Please read the documentation first! It is accessible\n"
                  "via the Help button below."),
                  Field_Message, -1 },
   { gettext_noop("Sync options with remote server"), Field_Bool|Field_Global, -1 },
   { gettext_noop("Remote (IMAP) folder for synchronisation"), Field_Folder|Field_Global, ConfigField_RSynchronise },
   { gettext_noop("Sync filter rules"), Field_Bool|Field_Global, ConfigField_RSynchronise },
   { gettext_noop("Sync identities"), Field_Bool|Field_Global, ConfigField_RSynchronise },
   { gettext_noop("Sync part of the folder tree"), Field_Bool|Field_Global, ConfigField_RSynchronise },
   { gettext_noop("Folder group to synchronise"), Field_Folder|Field_Global, ConfigField_RSFolders },
   { gettext_noop("&Store settings..."), Field_SubDlg | Field_Global, ConfigField_RSynchronise },
   { gettext_noop("&Retrieve settings..."), Field_SubDlg | Field_Global, ConfigField_RSynchronise },
};

// FIXME ugly, ugly, ugly... config settings should be living in an array from
//       the beginning which would avoid us all these contorsions
#define CONFIG_ENTRY(name)  ConfigValueDefault(name, name##_D)
// even worse: dummy entries for message fields
#define CONFIG_NONE()  ConfigValueNone()

// if you modify this array, search for DONT_FORGET_TO_MODIFY and modify data
// there too
const ConfigValueDefault wxOptionsPageStandard::ms_aConfigDefaults[] =
{
   // identity
   CONFIG_ENTRY(MP_PERSONALNAME),
   CONFIG_ENTRY(MP_FROM_ADDRESS),
   CONFIG_ENTRY(MP_REPLY_ADDRESS),
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_ADD_DEFAULT_HOSTNAME),
   CONFIG_ENTRY(MP_HOSTNAME),
   CONFIG_ENTRY(MP_SET_REPLY_FROM_TO),
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_USEVCARD),
   CONFIG_ENTRY(MP_VCARD),
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_USERLEVEL),
   CONFIG_NONE(),

   // network
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_POPHOST),
   CONFIG_ENTRY(MP_IMAPHOST),
#ifdef USE_SENDMAIL
   CONFIG_ENTRY(MP_USE_SENDMAIL),
   CONFIG_ENTRY(MP_SENDMAILCMD),
#endif // USE_SENDMAIL
   CONFIG_ENTRY(MP_SMTPHOST),
   CONFIG_ENTRY(MP_NNTPHOST),
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_SMTPHOST_LOGIN),
   CONFIG_ENTRY(MP_SMTPHOST_PASSWORD),
   CONFIG_ENTRY(MP_NNTPHOST_LOGIN),
   CONFIG_ENTRY(MP_NNTPHOST_PASSWORD),
   CONFIG_ENTRY(MP_GUESS_SENDER),
   CONFIG_ENTRY(MP_SENDER),
#ifdef USE_SSL
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_SMTPHOST_USE_SSL),
   CONFIG_ENTRY(MP_NNTPHOST_USE_SSL),
#endif // USE_SSL
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_DIALUP_SUPPORT),
   CONFIG_ENTRY(MP_BEACONHOST),
#ifdef OS_WIN
   CONFIG_ENTRY(MP_NET_CONNECTION),
#elif defined(OS_UNIX)
   CONFIG_ENTRY(MP_NET_ON_COMMAND),
   CONFIG_ENTRY(MP_NET_OFF_COMMAND),
#endif // platform
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_TCP_OPENTIMEOUT),
#ifdef USE_TCP_TIMEOUTS
   CONFIG_ENTRY(MP_TCP_READTIMEOUT),
   CONFIG_ENTRY(MP_TCP_WRITETIMEOUT),
   CONFIG_ENTRY(MP_TCP_CLOSETIMEOUT),
#endif // USE_TCP_TIMEOUTS
   CONFIG_ENTRY(MP_IMAP_LOOKAHEAD),
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_TCP_RSHTIMEOUT),

   // compose
   CONFIG_ENTRY(MP_USEOUTGOINGFOLDER), // where to keep copies of messages sent
   CONFIG_ENTRY(MP_OUTGOINGFOLDER),
   CONFIG_ENTRY(MP_WRAPMARGIN),
   CONFIG_ENTRY(MP_AUTOMATIC_WORDWRAP),
   CONFIG_ENTRY(MP_REPLY_PREFIX),
   CONFIG_ENTRY(MP_REPLY_COLLAPSE_PREFIX),
   CONFIG_ENTRY(MP_REPLY_MSGPREFIX),
   CONFIG_ENTRY(MP_REPLY_MSGPREFIX_FROM_SENDER),
   CONFIG_ENTRY(MP_REPLY_QUOTE_EMPTY),

   CONFIG_ENTRY(MP_REPLY_DETECT_SIG),
#if wxUSE_REGEX
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_REPLY_SIG_SEPARATOR),
#endif // wxUSE_REGEX

   CONFIG_ENTRY(MP_COMPOSE_USE_SIGNATURE),
   CONFIG_ENTRY(MP_COMPOSE_SIGNATURE),
   CONFIG_ENTRY(MP_COMPOSE_USE_SIGNATURE_SEPARATOR),
   CONFIG_ENTRY(MP_COMPOSE_XFACE_FILE),
   CONFIG_ENTRY(MP_ADB_SUBSTRINGEXPANSION),
   CONFIG_ENTRY(MP_CVIEW_FONT),
   CONFIG_ENTRY(MP_CVIEW_FONT_SIZE),
   CONFIG_ENTRY(MP_CVIEW_FGCOLOUR),
   CONFIG_ENTRY(MP_CVIEW_BGCOLOUR),
   CONFIG_NONE(),
   CONFIG_NONE(),

   // folders
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_DONTOPENSTARTUP),
   CONFIG_ENTRY(MP_REOPENLASTFOLDER),
   CONFIG_ENTRY(MP_OPENFOLDERS),
   CONFIG_ENTRY(MP_MAINFOLDER),
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_FOLDERPROGRESS_THRESHOLD),
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_MAX_HEADERS_NUM_HARD),
   CONFIG_ENTRY(MP_MAX_HEADERS_NUM),
   CONFIG_ENTRY(MP_MAX_MESSAGE_SIZE),
   CONFIG_ENTRY(MP_NEWMAIL_FOLDER),
   CONFIG_ENTRY(MP_POLLINCOMINGDELAY),
   CONFIG_ENTRY(MP_COLLECTATSTARTUP),
   CONFIG_ENTRY(MP_UPDATEINTERVAL),
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_FOLDER_CLOSE_DELAY),
   CONFIG_ENTRY(MP_NEWMAIL_FOLDER),
   CONFIG_ENTRY(MP_USE_OUTBOX), // where to store message before sending them
   CONFIG_ENTRY(MP_OUTBOX_NAME),
   CONFIG_ENTRY(MP_USE_TRASH_FOLDER),
   CONFIG_ENTRY(MP_TRASH_FOLDER),
   CONFIG_ENTRY(MP_FOLDER_FILE_DRIVER),
   CONFIG_ENTRY(MP_FOLDER_BGCOLOUR),
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_FOLDERSTATUS_STATBAR),
   CONFIG_ENTRY(MP_FOLDERSTATUS_TITLEBAR),

   // python
#ifdef USE_PYTHON
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_USEPYTHON),
   CONFIG_ENTRY(MP_PYTHONPATH),
   CONFIG_ENTRY(MP_STARTUPSCRIPT),
   CONFIG_ENTRY(MCB_FOLDEROPEN),
   CONFIG_ENTRY(MCB_FOLDERUPDATE),
   CONFIG_ENTRY(MCB_FOLDEREXPUNGE),
   CONFIG_ENTRY(MCB_FOLDERSETMSGFLAG),
   CONFIG_ENTRY(MCB_FOLDERCLEARMSGFLAG),
#endif // USE_PYTHON

   // message views
   CONFIG_ENTRY(MP_MVIEW_FONT),
   CONFIG_ENTRY(MP_MVIEW_FONT_SIZE),
   CONFIG_ENTRY(MP_MVIEW_FGCOLOUR),
   CONFIG_ENTRY(MP_MVIEW_BGCOLOUR),
   CONFIG_ENTRY(MP_MVIEW_URLCOLOUR),
   CONFIG_ENTRY(MP_MVIEW_HEADER_NAMES_COLOUR),
   CONFIG_ENTRY(MP_MVIEW_HEADER_VALUES_COLOUR),
   CONFIG_ENTRY(MP_MVIEW_QUOTED_COLOURIZE),
   CONFIG_ENTRY(MP_MVIEW_QUOTED_CYCLE_COLOURS),
   CONFIG_ENTRY(MP_MVIEW_QUOTED_COLOUR1),
   CONFIG_ENTRY(MP_MVIEW_QUOTED_COLOUR2),
   CONFIG_ENTRY(MP_MVIEW_QUOTED_COLOUR3),
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_MESSAGEPROGRESS_THRESHOLD_SIZE),
   CONFIG_ENTRY(MP_MESSAGEPROGRESS_THRESHOLD_TIME),
   CONFIG_ENTRY(MP_INLINE_GFX),
   CONFIG_ENTRY(MP_INLINE_GFX_SIZE),
   CONFIG_ENTRY(MP_MSGVIEW_AUTO_ENCODING),
   CONFIG_ENTRY(MP_PLAIN_IS_TEXT),
   CONFIG_ENTRY(MP_RFC822_IS_TEXT),
   CONFIG_ENTRY(MP_VIEW_WRAPMARGIN),
   CONFIG_ENTRY(MP_VIEW_AUTOMATIC_WORDWRAP),
#ifdef OS_UNIX
   CONFIG_ENTRY(MP_INCFAX_SUPPORT),
   CONFIG_ENTRY(MP_INCFAX_DOMAINS),
   CONFIG_ENTRY(MP_TIFF2PS),
#endif
   CONFIG_ENTRY(MP_MSGVIEW_HEADERS),
   CONFIG_ENTRY(MP_DATE_FMT),
   CONFIG_ENTRY(MP_MVIEW_TITLE_FMT),

   // folder view
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_USE_NEWMAILCOMMAND),
   CONFIG_ENTRY(MP_NEWMAILCOMMAND),
   CONFIG_ENTRY(MP_SHOW_NEWMAILMSG),
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_AUTOSHOW_FIRSTMESSAGE),
   CONFIG_ENTRY(MP_AUTOSHOW_FIRSTUNREADMESSAGE),
   CONFIG_ENTRY(MP_PREVIEW_ON_SELECT),
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_FVIEW_NAMES_ONLY),
   CONFIG_ENTRY(MP_FVIEW_FROM_REPLACE),
   CONFIG_ENTRY(MP_FROM_REPLACE_ADDRESSES),
   CONFIG_ENTRY(MP_FVIEW_FONT),
   CONFIG_ENTRY(MP_FVIEW_FONT_SIZE),
   CONFIG_ENTRY(MP_FVIEW_FGCOLOUR),
   CONFIG_ENTRY(MP_FVIEW_BGCOLOUR),
   CONFIG_ENTRY(MP_FVIEW_NEWCOLOUR),
   CONFIG_ENTRY(MP_FVIEW_RECENTCOLOUR),
   CONFIG_ENTRY(MP_FVIEW_UNREADCOLOUR),
   CONFIG_ENTRY(MP_FVIEW_DELETEDCOLOUR),
   CONFIG_ENTRY(MP_MSGS_USE_THREADING),

#if defined(EXPERIMENTAL_JWZ_THREADING)
#if wxUSE_REGEX
   CONFIG_ENTRY(MP_MSGS_SIMPLIFYING_REGEX),
   CONFIG_ENTRY(MP_MSGS_REPLACEMENT_STRING),
#endif // wxUSE_REGEX
   CONFIG_ENTRY(MP_MSGS_GATHER_SUBJECTS),
#if !wxUSE_REGEX
   CONFIG_ENTRY(MP_MSGS_REMOVE_LIST_PREFIX_GATHERING),
#endif // !wxUSE_REGEX
   CONFIG_ENTRY(MP_MSGS_BREAK_THREAD),
#if !wxUSE_REGEX
   CONFIG_ENTRY(MP_MSGS_REMOVE_LIST_PREFIX_BREAKING),
#endif // !wxUSE_REGEX
   CONFIG_ENTRY(MP_MSGS_INDENT_IF_DUMMY),
#endif // EXPERIMENTAL_JWZ_THREADING

   CONFIG_NONE(), // sorting subdialog
   CONFIG_NONE(), // columns subdialog
   CONFIG_ENTRY(MP_FVIEW_SIZE_FORMAT),
   CONFIG_NONE(), // status/title format help
   CONFIG_ENTRY(MP_FVIEW_STATUS_UPDATE),
   CONFIG_ENTRY(MP_FVIEW_STATUS_FMT),

   // folder tree
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_FTREE_FORMAT),
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_FTREE_PROPAGATE),

   // autocollect
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_AUTOCOLLECT),
   CONFIG_ENTRY(MP_AUTOCOLLECT_ADB),
   CONFIG_ENTRY(MP_AUTOCOLLECT_NAMED),
#ifdef USE_BBDB
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_BBDB_IGNOREANONYMOUS),
   CONFIG_ENTRY(MP_BBDB_GENERATEUNIQUENAMES),
   CONFIG_ENTRY(MP_BBDB_ANONYMOUS),
   CONFIG_ENTRY(MP_BBDB_SAVEONEXIT),
#endif // USE_BBDB

   // helper programs
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_BROWSER),
#ifndef OS_WIN
   CONFIG_ENTRY(MP_BROWSER_ISNS),
#endif // OS_WIN
   CONFIG_ENTRY(MP_BROWSER_INNW),
#ifdef USE_EXT_HTML_HELP
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_HELPBROWSER),
   CONFIG_ENTRY(MP_HELPBROWSER_ISNS),
#endif // USE_EXT_HTML_HELP
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_EXTERNALEDITOR),
   CONFIG_ENTRY(MP_ALWAYS_USE_EXTERNALEDITOR),
#ifdef OS_UNIX
   CONFIG_ENTRY(MP_CONVERTPROGRAM),
   CONFIG_ENTRY(MP_TMPGFXFORMAT),
#endif
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_NEWMAILCOMMAND),

   // other
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_SHOWLOG),
   CONFIG_ENTRY(MP_LOGFILE),
   CONFIG_ENTRY(MP_DEBUG_CCLIENT),
   CONFIG_ENTRY(MP_SHOWTIPS),
   CONFIG_ENTRY(MP_SHOWSPLASH),
   CONFIG_ENTRY(MP_SPLASHDELAY),
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_AUTOSAVEDELAY),
   CONFIG_ENTRY(MP_CONFIRMEXIT),
   CONFIG_ENTRY(MP_OPEN_ON_CLICK),
   CONFIG_ENTRY(MP_SHOW_HIDDEN_FOLDERS),
   CONFIG_ENTRY(MP_HELPDIR),
#ifdef USE_SSL
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_SSL_DLL_SSL),
   CONFIG_ENTRY(MP_SSL_DLL_CRYPTO),
#endif // USE_SSL
#ifdef OS_UNIX
   CONFIG_ENTRY(MP_AFMPATH),
   CONFIG_ENTRY(MP_FOCUS_FOLLOWSMOUSE),
   CONFIG_ENTRY(MP_DOCKABLE_MENUBARS),
   CONFIG_ENTRY(MP_DOCKABLE_TOOLBARS),
   CONFIG_ENTRY(MP_FLAT_TOOLBARS),
#endif // OS_UNIX
   CONFIG_NONE(), // reenable disabled msg boxes
   CONFIG_NONE(), // away help
   CONFIG_ENTRY(MP_AWAY_AUTO_ENTER),
   CONFIG_ENTRY(MP_AWAY_AUTO_EXIT),
   CONFIG_ENTRY(MP_AWAY_REMEMBER),

   // sync
   CONFIG_NONE(),
   CONFIG_ENTRY(MP_SYNC_REMOTE),
   CONFIG_ENTRY(MP_SYNC_FOLDER),
   CONFIG_ENTRY(MP_SYNC_FILTERS),
   CONFIG_ENTRY(MP_SYNC_IDS),
   CONFIG_ENTRY(MP_SYNC_FOLDERS),
   CONFIG_ENTRY(MP_SYNC_FOLDERGROUP),
   CONFIG_NONE(),
   CONFIG_NONE(),
};

#undef CONFIG_ENTRY
#undef CONFIG_NONE

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxOptionsPage
// ----------------------------------------------------------------------------

wxOptionsPage::wxOptionsPage(FieldInfoArray aFields,
                             ConfigValuesArray aDefaults,
                             size_t nFirst,
                             size_t nLast,
                             wxNotebook *notebook,
                             const char *title,
                             Profile *profile,
                             int helpId,
                             int image)
             : wxNotebookPageBase(notebook)
{
   m_idListbox = -1; // no listbox by default

   m_aFields = aFields;
   m_aDefaults = aDefaults;

   notebook->AddPage(this, title, FALSE /* don't select */, image);

   m_Profile = profile;
   m_Profile->IncRef();

   m_HelpId = helpId;

   m_nFirst = nFirst;
   m_nLast = nLast;

   CreateControls();
}

void wxOptionsPage::CreateControls()
{
   size_t n;

   // some fields are only shown in 'advanced' mode, so check if we're in it
   bool isAdvanced = READ_APPCONFIG(MP_USERLEVEL) >= M_USERLEVEL_ADVANCED;

   // some others are not shown when we're inside an identity or folder dialog
   // but only in the global one
   wxGlobalOptionsDialog *dialog = GET_PARENT_OF_CLASS(this, wxGlobalOptionsDialog);
   bool isIdentDialog = dialog && !dialog->IsGlobalOptionsDialog();
   bool isFolderDialog = !dialog;

   // first determine the longest label
   wxArrayString aLabels;
   for ( n = m_nFirst; n < m_nLast; n++ ) {
      int flags = GetFieldFlags(n);

      // don't show the global settings when editing the identity dialog and
      // don't show the advanced ones in the novice mode
      if ( (!isAdvanced && (flags & Field_Advanced)) ||
           (isIdentDialog && (flags & Field_Global)) ||
           (isFolderDialog && (flags & Field_AppWide)) )
      {
         // skip this one
         continue;
      }

      // do it only for text control labels
      switch ( GetFieldType(n) ) {
      case Field_Passwd:
      case Field_Number:
      case Field_Dir:
      case Field_File:
      case Field_Color:
      case Field_Folder:
      case Field_Bool:
         // fall through: for this purpose (finding the longest label)
         // they're the same as text
      case Field_Action:
      case Field_Text:
         break;

      default:
         // don't take into account the other types
         continue;
      }

      aLabels.Add(_(m_aFields[n].label));
   }

   long widthMax = GetMaxLabelWidth(aLabels, this);

   // now create the controls
   int styleText = wxALIGN_RIGHT;
   wxControl *last = NULL; // last control created
   for ( n = m_nFirst; n < m_nLast; n++ ) {
      int flags = GetFieldFlags(n);
      if ( (!isAdvanced && (flags & Field_Advanced)) ||
           (isIdentDialog && (flags & Field_Global)) ||
           (isFolderDialog && (flags & Field_AppWide)) )
      {
         // skip this one
         m_aControls.Add(NULL);
         m_aDirtyFlags.Add(false);

         continue;
      }

      switch ( GetFieldType(n) ) {
         case Field_Dir:
            last = CreateDirEntry(_(m_aFields[n].label), widthMax, last);
            break;

         case Field_File:
            last = CreateFileEntry(_(m_aFields[n].label), widthMax, last);
            break;

         case Field_Folder:
            last = CreateFolderEntry(_(m_aFields[n].label), widthMax, last);
            break;

         case Field_Color:
            last = CreateColorEntry(_(m_aFields[n].label), widthMax, last);
            break;

         case Field_Action:
            last = CreateActionChoice(_(m_aFields[n].label), widthMax, last);
            break;

         case Field_Combo:
            // a hack to dynamicially fill the RAS connections combo box under
            // Windows - I didn't find anything better to do right now, may be
            // later (feel free to tell me if you have any ideas)
#ifdef OS_WIN
            if ( n == ConfigField_NetConnection )
            {
               wxString title = _(m_aFields[n].label);

               // may be NULL if we don't use dial up manager at all
               wxDialUpManager *dial =
                  ((wxMApp *)mApplication)->GetDialUpManager();;
               if ( dial )
               {
                  wxArrayString aConnections;
                  dial->GetISPNames(aConnections);

                  if ( !aConnections.IsEmpty() )
                  {
                     title << ':' << strutil_flatten_array(aConnections);
                  }
               }

               last = CreateChoice(title, widthMax, last);
            }
            else
#endif // OS_WIN
                last = CreateChoice(_(m_aFields[n].label), widthMax, last);
            break;

         case Field_Passwd:
            styleText |= wxTE_PASSWORD;
            // fall through

         case Field_Number:
            // fall through -- for now they're the same as text
         case Field_Text:
            last = CreateTextWithLabel(_(m_aFields[n].label), widthMax, last,
                                       0, styleText);

            // reset
            styleText = wxALIGN_RIGHT;
            break;

         case Field_List:
            last = CreateListbox(_(m_aFields[n].label), last);
            break;

         case Field_Bool:
            last = CreateCheckBox(_(m_aFields[n].label), widthMax, last);
            break;

         case Field_Message:
            last = CreateMessage(_(m_aFields[n].label), last);
            break;

         case Field_SubDlg:
            last = CreateButton(_(m_aFields[n].label), last);
            break;

         case Field_XFace:
            last = CreateXFaceButton(_(m_aFields[n].label), widthMax, last);
            break;

         default:
            wxFAIL_MSG("unknown field type in CreateControls");
         }

      wxCHECK_RET( last, "control creation failed" );

      if ( flags & Field_Vital )
         m_aVitalControls.Add(last);
      if ( flags & Field_Restart )
         m_aRestartControls.Add(last);

      m_aControls.Add(last);
      m_aDirtyFlags.Add(false);
   }
}

bool wxOptionsPage::OnChangeCommon(wxControl *control)
{
   int index = m_aControls.Index(control);

   if ( index == wxNOT_FOUND )
   {
      // we can get events from the text controls from "file open" dialog here
      // too - just skip them silently
      return FALSE;
   }

   // mark this control as being dirty
   m_aDirtyFlags[(size_t)index] = true;

   // update this page controls state
   UpdateUI();

   // mark the dialog as being dirty
   wxOptionsEditDialog *dialog = GET_PARENT_OF_CLASS(this, wxOptionsEditDialog);
   CHECK( dialog, FALSE, "option page without option dialog?" );

   if ( m_aVitalControls.Index(control) != -1 )
      dialog->SetDoTest();
   else
      dialog->SetDirty();

   if ( m_aRestartControls.Index(control) != -1 )
      dialog->SetGiveRestartWarning();

   return TRUE;
}

void wxOptionsPage::OnChange(wxEvent& event)
{
   if ( !OnChangeCommon((wxControl *)event.GetEventObject()) )
   {
      // not our event
      event.Skip();
   }
}

void wxOptionsPage::OnControlChange(wxEvent& event)
{
   OnChange(event);
}

void wxOptionsPage::UpdateUI()
{
   for ( size_t n = m_nFirst; n < m_nLast; n++ ) {
      int nCheckField = m_aFields[n].enable;
      if ( nCheckField != -1 ) {
         wxControl *control = GetControl(n);

         // the control might be NULL if it is an advanced control and the
         // user level is novice
         if ( !control )
            continue;

         // HACK: if the index of the field we depend on is negative, inverse
         //       the usual logic, i.e. only enable this field if the checkbox
         //       is cleared and disabled it otherwise
         bool inverseMeaning = nCheckField < 0;
         if ( inverseMeaning ) {
            nCheckField = -nCheckField;
         }

         wxASSERT( nCheckField >= 0 && nCheckField < ConfigField_Max );

         // avoid signed/unsigned mismatch in expressions
         size_t nCheck = (size_t)nCheckField;
         wxCHECK_RET( nCheck >= m_nFirst && nCheck < m_nLast,
                      "control index out of range" );

         bool bEnable = true;
         wxControl *controlDep = GetControl(nCheck);
         if ( !controlDep )
         {
            // control we depend on wasn't created: this is possible if it is
            // advanced/global control and we're in novice/identity mode, so
            // just ignore it
            continue;
         }

         // first of all, check if the control we depend on is not disabled
         // itself because some other control overrides it too - if it is
         // disabled, we are disabled as well
         if ( !controlDep->IsEnabled() )
         {
            bEnable = false;
         }
         else // control we depend on is enabled
         {
            if ( GetFieldType(nCheck) == Field_Bool )
            {
               // enable only if the checkbox is checked
               wxCheckBox *checkbox = wxStaticCast(controlDep, wxCheckBox);

               bEnable = checkbox->GetValue();
            }
            else if ( GetFieldType(nCheck) == Field_Action )
            {
               // only enable if the radiobox selection is 0 (meaning "yes")
               wxRadioBox *radiobox = wxStaticCast(controlDep, wxRadioBox);

               if ( radiobox->GetSelection() == 0 ) // FIXME hardcoded!
                  bEnable = false;
            }
            else
            {
               // assume that this is one of the text controls
               wxTextCtrl *text = wxStaticCast(controlDep, wxTextCtrl);
               wxCHECK_RET( text, "can't depend on this control type" );

               // only enable if the text control has something
               bEnable = !text->GetValue().IsEmpty();
            }

            if ( inverseMeaning )
               bEnable = !bEnable;
         }

         control->Enable(bEnable);

         switch ( GetFieldType(n) )
         {
               // for file entries, also disable the browse button
            case Field_File:
            case Field_Dir:
            case Field_Color:
            case Field_Folder:
               wxASSERT( control->IsKindOf(CLASSINFO(wxTextCtrl)) );

               EnableTextWithButton((wxTextCtrl *)control, bEnable);
               break;

            case Field_Passwd:
            case Field_Number:
            case Field_Text:
               wxASSERT( control->IsKindOf(CLASSINFO(wxTextCtrl)) );

               EnableTextWithLabel((wxTextCtrl *)control, bEnable);
               break;

            case Field_List:
               EnableListBox(wxDynamicCast(control, wxListBox), bEnable);
               break;

            case Field_Combo:
               EnableControlWithLabel(control, bEnable);

            default:
                ;
         }
      }
      // this field is always enabled
   }
}

// read the data from config
bool wxOptionsPage::TransferDataToWindow()
{
   // disable environment variable expansion here because we want the user to
   // edit the real value stored in the config
   ProfileEnvVarSave suspend(m_Profile, false);

   String strValue;
   long lValue = 0;
   for ( size_t n = m_nFirst; n < m_nLast; n++ )
   {
      if ( strcmp(m_aDefaults[n].name, "none") == 0 )
      {
         // it doesn't have any associated value
         continue;
      }

      if ( m_aDefaults[n].IsNumeric() )
      {
         lValue = m_Profile->readEntry(m_aDefaults[n].name,
                                       (int)m_aDefaults[n].lValue);
         strValue.Printf("%ld", lValue);
      }
      else {
         // it's a string
         strValue = m_Profile->readEntry(m_aDefaults[n].name,
                                         m_aDefaults[n].szValue);
      }

      wxControl *control = GetControl(n);
      if ( !control )
         continue;

      switch ( GetFieldType(n) ) {
      case Field_Text:
      case Field_Number:
         if ( GetFieldType(n) == Field_Number ) {
            wxASSERT( m_aDefaults[n].IsNumeric() );

            strValue.Printf("%ld", lValue);
         }
         else {
            wxASSERT( !m_aDefaults[n].IsNumeric() );
         }

         // can only have text value
      case Field_Passwd:
         if( GetFieldType(n) == Field_Passwd )
            strValue = strutil_decrypt(strValue);
      case Field_Dir:
      case Field_File:
      case Field_Color:
      case Field_Folder:
         wxStaticCast(control, wxTextCtrl)->SetValue(strValue);
         break;

      case Field_Bool:
         wxASSERT( m_aDefaults[n].IsNumeric() );
         wxStaticCast(control, wxCheckBox)->SetValue(lValue != 0);
         break;

      case Field_Action:
         wxStaticCast(control, wxRadioBox)->SetSelection(lValue);
         break;

      case Field_Combo:
         wxStaticCast(control, wxChoice)->SetSelection(lValue);
         break;

      case Field_List:
         wxASSERT( !m_aDefaults[n].IsNumeric() );

         {
            wxListBox *lbox = wxStaticCast(control, wxListBox);

            // split it on the separator char: this is ':' for everything
            // except ConfigField_OpenFolders where it is ';' for config
            // backwards compatibility
            char ch = n == ConfigField_OpenFolders ? ';' : ':';
            wxArrayString entries = strutil_restore_array(ch, strValue);

            size_t count = entries.GetCount();
            for ( size_t m = 0; m < count; m++ )
            {
               lbox->Append(entries[m]);
            }
         }
         break;

      case Field_XFace:
      {
         wxXFaceButton *btnXFace = (wxXFaceButton *)control;
         if ( READ_CONFIG(m_Profile, MP_COMPOSE_USE_XFACE) )
            btnXFace->SetFile(READ_CONFIG(m_Profile, MP_COMPOSE_XFACE_FILE));
         else
            btnXFace->SetFile("");
      }
      break;

      case Field_SubDlg:      // these settings will be read later
         break;

      case Field_Message:
      default:
         wxFAIL_MSG("unexpected field type");
      }

      // the dirty flag was set from the OnChange() callback, reset it!
      ClearDirty(n);
   }

   return TRUE;
}

// write the data to config
bool wxOptionsPage::TransferDataFromWindow()
{
   String strValue;
   long lValue = 0;
   for ( size_t n = m_nFirst; n < m_nLast; n++ )
   {
      // only write the controls which were really changed
      if ( !IsDirty(n) )
         continue;

      wxControl *control = GetControl(n);
      if ( !control )
         continue;

      switch ( GetFieldType(n) )
      {
         case Field_Passwd:
         case Field_Text:
         case Field_Dir:
         case Field_File:
         case Field_Color:
         case Field_Folder:
         case Field_Number:
            strValue = wxStaticCast(control, wxTextCtrl)->GetValue();

            if( GetFieldType(n) == Field_Passwd )
               strValue = strutil_encrypt(strValue);

            if ( GetFieldType(n) == Field_Number ) {
               wxASSERT( m_aDefaults[n].IsNumeric() );

               lValue = atol(strValue);
            }
            else {
               wxASSERT( !m_aDefaults[n].IsNumeric() );
            }
            break;

         case Field_Bool:
            wxASSERT( m_aDefaults[n].IsNumeric() );

            lValue = wxStaticCast(control, wxCheckBox)->GetValue();
            break;

         case Field_Action:
            wxASSERT( m_aDefaults[n].IsNumeric() );

            lValue = wxStaticCast(control, wxRadioBox)->GetSelection();
            break;

         case Field_Combo:
            wxASSERT( m_aDefaults[n].IsNumeric() );

            lValue = wxStaticCast(control, wxChoice)->GetSelection();
            break;

         case Field_List:
            wxASSERT( !m_aDefaults[n].IsNumeric() );

            // join it using a separator char: this is ':' for everything except
            // ConfigField_OpenFolders where it is ';' for config backwards
            // compatibility
            {
               char ch = n == ConfigField_OpenFolders ? ';' : ':';
               wxListBox *listbox = wxStaticCast(control, wxListBox);
               size_t count = listbox->GetCount();
               for ( size_t m = 0; m < count; m++ ) {
                  if ( !strValue.IsEmpty() ) {
                     strValue << ch;
                  }

                  strValue << listbox->GetString(m);
               }
            }
            break;

         case Field_Message:
         case Field_SubDlg:      // already done
         case Field_XFace:       // already done
            break;

         default:
            wxFAIL_MSG("unexpected field type");
      }

      if ( m_aDefaults[n].IsNumeric() )
      {
         m_Profile->writeEntry(m_aDefaults[n].name, (int)lValue);
      }
      else
      {
         // it's a string
         m_Profile->writeEntry(m_aDefaults[n].name, strValue);
      }
   }

   // TODO life is easy as we don't check for errors...
   return TRUE;
}

// wxOptionsPage listbox handling

void wxOptionsPage::OnListBoxButton(wxCommandEvent& event)
{
   if ( m_idListbox == -1 )
   {
      // see comment in OnUpdateUIListboxBtns()
      event.Skip();

      return;
   }

   switch ( event.GetId() )
   {
      case wxOptionsPage_BtnNew:
         OnListBoxAdd();
         break;

      case wxOptionsPage_BtnModify:
         OnListBoxModify();
         break;

      case wxOptionsPage_BtnDelete:
         OnListBoxDelete();
         break;

      default:
         ;
   }
}

bool wxOptionsPage::OnListBoxAdd()
{
   // get the string from user
   wxString str;
   if ( !MInputBox(&str, m_lboxDlgTitle, m_lboxDlgPrompt,
                   GET_PARENT_OF_CLASS(this, wxDialog), m_lboxDlgPers) ) {
      return FALSE;
   }

   wxListBox *listbox = wxStaticCast(GetControl(m_idListbox), wxListBox);
   wxCHECK_MSG( listbox, FALSE, "expected a listbox" );

   // check that it's not already there
   if ( listbox->FindString(str) != -1 ) {
      // it is, don't add it twice
      wxLogError(_("String '%s' is already present in the list, not added."),
                 str.c_str());

      return FALSE;
   }
   else {
      // ok, do add it
      listbox->Append(str);

      wxOptionsPage::OnChangeCommon(listbox);

      return TRUE;
   }
}

bool wxOptionsPage::OnListBoxModify()
{
   wxListBox *l = wxStaticCast(GetControl(m_idListbox), wxListBox);
   wxCHECK_MSG( l, FALSE, "expected a listbox" );
   int nSel = l->GetSelection();

   wxCHECK_MSG( nSel != -1, FALSE, "should be disabled" );

   wxString val = wxGetTextFromUser(m_lboxDlgPrompt,
                                    m_lboxDlgTitle,
                                    l->GetString(nSel),
                                    GET_PARENT_OF_CLASS(this, wxDialog));
   if ( !val || val == l->GetString(nSel) )
   {
      // cancelled or unchanged
      return FALSE;
   }

   l->SetString(nSel, val);

   wxOptionsPage::OnChangeCommon(l);

   return TRUE;
}

bool wxOptionsPage::OnListBoxDelete()
{
   wxListBox *l = wxStaticCast(GetControl(m_idListbox), wxListBox);
   wxCHECK_MSG( l, FALSE, "expected a listbox" );
   int nSel = l->GetSelection();

   wxCHECK_MSG( nSel != -1, FALSE, "should be disabled" );

   l->Delete(nSel);
   wxOptionsPage::OnChangeCommon(l);

   return TRUE;
}

void wxOptionsPage::OnUpdateUIListboxBtns(wxUpdateUIEvent& event)
{
   if ( m_idListbox == -1 )
   {
      // unfortunately this does happen sometimes: I discovered it when the
      // program crashed trying to process UpdateUI event from a popup menu in
      // the folder tree window shown from an options page because some item had
      // the same id as wxOptionsPage_BtnModify - I've changed the id now to
      // make it unique, but it's not impossible that we'll have another one in
      // the future, so be careful here
      event.Skip();
   }
   else
   {
      wxListBox *lbox = wxStaticCast(GetControl(m_idListbox), wxListBox);
      wxCHECK_RET( lbox, "expected a listbox here" );

      event.Enable(lbox->GetSelection() != -1);
   }
}

// ----------------------------------------------------------------------------
// wxOptionsPageDynamic
// ----------------------------------------------------------------------------

wxOptionsPageDynamic::wxOptionsPageDynamic(wxNotebook *parent,
                                           const char *title,
                                           Profile *profile,
                                           FieldInfoArray aFields,
                                           ConfigValuesArray aDefaults,
                                           size_t nFields,
                                           size_t nOffset,
                                           int helpId,
                                           int image)
                     : wxOptionsPage(aFields - nOffset,
                                     aDefaults - nOffset,
                                     nOffset, nOffset + nFields,
                                     parent, title, profile, helpId, image)
{
}

// ----------------------------------------------------------------------------
// wxOptionsPageStandard
// ----------------------------------------------------------------------------

wxOptionsPageStandard::wxOptionsPageStandard(wxNotebook *notebook,
                                             const char *title,
                                             Profile *profile,
                                             size_t nFirst,
                                             size_t nLast,
                                             int helpId)
                     : wxOptionsPage(ms_aFields, ms_aConfigDefaults,
                                     // see enum ConfigFields for the
                                     // explanation of "+1"
                                     nFirst + 1, nLast + 1,
                                     notebook, title, profile, helpId,
                                     notebook->GetPageCount())
{
   // check that we didn't forget to update one of the arrays...
   wxASSERT( WXSIZEOF(ms_aConfigDefaults) == ConfigField_Max );
   wxASSERT( WXSIZEOF(ms_aFields) == ConfigField_Max );
}

// ----------------------------------------------------------------------------
// wxOptionsPageCompose
// ----------------------------------------------------------------------------

wxOptionsPageCompose::wxOptionsPageCompose(wxNotebook *parent,
                                           Profile *profile)
                    : wxOptionsPageStandard(parent,
                                    _("Compose"),
                                    profile,
                                    ConfigField_ComposeFirst,
                                    ConfigField_ComposeLast,
                                    MH_OPAGE_COMPOSE)
{
}

void wxOptionsPageCompose::OnButton(wxCommandEvent& event)
{
   bool dirty;

   wxObject *obj = event.GetEventObject();
   if ( obj == GetControl(ConfigField_ComposeHeaders) )
   {
      // create and show the "outgoing headers" config dialog
      dirty = ConfigureComposeHeaders(m_Profile, this);
   }
   else if ( obj == GetControl(ConfigField_ComposeTemplates) )
   {
      dirty = ConfigureTemplates(m_Profile, this);
   }
   else if ( obj == GetControl(ConfigField_XFaceFile) )
   {
      dirty = PickXFaceDialog(m_Profile, this);
      if(dirty)
      {
         wxXFaceButton *btn = (wxXFaceButton*)obj;
         // Why doesn�t UpdateUI() have the same effect here?
         if(READ_CONFIG(m_Profile, MP_COMPOSE_USE_XFACE))
            btn->SetFile(READ_CONFIG(m_Profile,MP_COMPOSE_XFACE_FILE));
         else
            btn->SetFile("");
      }
   }
   else
   {
      FAIL_MSG("click from alien button in compose view page");

      dirty = FALSE;

      event.Skip();
   }

   if ( dirty )
   {
      // something changed - make us dirty
      wxOptionsEditDialog *dialog = GET_PARENT_OF_CLASS(this, wxOptionsEditDialog);

      wxCHECK_RET( dialog, "options page without a parent dialog?" );

      dialog->SetDirty();
   }
}

bool wxOptionsPageCompose::TransferDataFromWindow()
{
   bool rc = wxOptionsPage::TransferDataFromWindow();
   if ( rc && READ_CONFIG(m_Profile, MP_USE_OUTBOX) )
   {
      /* Make sure the Outbox setting is consistent across all
         folders! */
      wxString outbox = READ_CONFIG(m_Profile, MP_OUTBOX_NAME);
      wxString globalOutbox = READ_APPCONFIG(MP_OUTBOX_NAME);
      if(outbox != globalOutbox)
      {
         /* Erasing the local value should be good enough, but let�s
            play it safe. */
         m_Profile->writeEntry(MP_OUTBOX_NAME, globalOutbox);
         wxString msg;
         msg.Printf(_("You set the name of the outbox for temporarily storing messages\n"
                      "before sending them to be �%s�. This is different from the\n"
                      "setting in the global options, which is �%s�. A you can have\n"
                      "only a single outbox, the value has been restored to be �%s�."),
                    outbox.c_str(), globalOutbox.c_str(), globalOutbox.c_str());
         wxLogError(msg);
      }
   }
   return rc;
}

// ----------------------------------------------------------------------------
// wxOptionsPageMessageView
// ----------------------------------------------------------------------------

wxOptionsPageMessageView::wxOptionsPageMessageView(wxNotebook *parent,
                                                   Profile *profile)
   : wxOptionsPageStandard(parent,
                   _("Message View"),
                   profile,
                   ConfigField_MessageViewFirst,
                   ConfigField_MessageViewLast,
                   MH_OPAGE_MESSAGEVIEW)
{
}

void wxOptionsPageMessageView::OnButton(wxCommandEvent& event)
{
   bool dirty;

   wxObject *obj = event.GetEventObject();
   if ( obj == GetControl(ConfigField_MessageViewDateFormat) )
      dirty = ConfigureDateFormat(m_Profile, this);
   else if ( obj == GetControl(ConfigField_MessageViewHeaders) )
      dirty = ConfigureMsgViewHeaders(m_Profile, this);
   else
   {
      wxFAIL_MSG( "alien button" );

      dirty = false;
   }

   if ( dirty )
   {
      // something changed - make us dirty
      wxOptionsEditDialog *dialog = GET_PARENT_OF_CLASS(this, wxOptionsEditDialog);
      wxCHECK_RET( dialog, "options page without a parent dialog?" );
      dialog->SetDirty();
   }
}

// ----------------------------------------------------------------------------
// wxOptionsPageFolderView
// ----------------------------------------------------------------------------

wxOptionsPageFolderView::wxOptionsPageFolderView(wxNotebook *parent,
                                                 Profile *profile)
   : wxOptionsPageStandard(parent,
                           _("Folder View"),
                           profile,
                           ConfigField_FolderViewFirst,
                           ConfigField_FolderViewLast,
                           MH_OPAGE_MESSAGEVIEW)
{
   m_idListbox = ConfigField_FolderViewReplaceFromAddresses;
   m_lboxDlgTitle = _("My own addresses");
   m_lboxDlgPrompt = _("Address");
   m_lboxDlgPers = "LastMyAddress";
}

bool wxOptionsPageFolderView::TransferDataToWindow()
{
   bool bRc = wxOptionsPage::TransferDataToWindow();

   if ( bRc )
   {
      // if the listbox is empty, add the reply-to address to it
      wxListBox *listbox = wxStaticCast(GetControl(m_idListbox), wxListBox);
      if ( !listbox->GetCount() )
      {
         listbox->Append(READ_CONFIG(m_Profile, MP_FROM_ADDRESS));
      }
   }

   return bRc;
}

bool wxOptionsPageFolderView::TransferDataFromWindow()
{
   // if the listbox contains just the return address, empty it: it is the
   // default anyhow and this avoids remembering it in config
   wxListBox *listbox = wxStaticCast(GetControl(m_idListbox), wxListBox);
   if ( listbox->GetCount() == 1 &&
        listbox->GetString(0) == READ_CONFIG(m_Profile, MP_FROM_ADDRESS) )
   {
      listbox->Clear();
   }

   return wxOptionsPage::TransferDataFromWindow();
}

void wxOptionsPageFolderView::OnButton(wxCommandEvent& event)
{
   bool dirty;

   wxObject *obj = event.GetEventObject();
   if ( obj == GetControl(ConfigField_FolderViewSortMessagesBy) )
      dirty = ConfigureSorting(m_Profile, this);
   else if ( obj == GetControl(ConfigField_FolderViewHeaders) )
      dirty = ConfigureFolderViewHeaders(m_Profile, this);
   else
   {
      event.Skip();

      dirty = false;
   }

   if ( dirty )
   {
      // something changed - make us dirty
      wxOptionsEditDialog *dialog = GET_PARENT_OF_CLASS(this, wxOptionsEditDialog);
      wxCHECK_RET( dialog, "options page without a parent dialog?" );
      dialog->SetDirty();
   }
}

// ----------------------------------------------------------------------------
// wxOptionsPageFolderTree
// ----------------------------------------------------------------------------

wxOptionsPageFolderTree::wxOptionsPageFolderTree(wxNotebook *parent,
                                                 Profile *profile)
   : wxOptionsPageStandard(parent,
                           _("Folder Tree"),
                           profile,
                           ConfigField_FolderTreeFirst,
                           ConfigField_FolderTreeLast)
{
}

// ----------------------------------------------------------------------------
// wxOptionsPageIdent
// ----------------------------------------------------------------------------

wxOptionsPageIdent::wxOptionsPageIdent(wxNotebook *parent,
                                       Profile *profile)
                  : wxOptionsPageStandard(parent,
                                  _("Identity"),
                                  profile,
                                  ConfigField_IdentFirst,
                                  ConfigField_IdentLast,
                                  MH_OPAGE_IDENT)
{
}

void wxOptionsPageIdent::OnButton(wxCommandEvent& event)
{
   wxObject *obj = event.GetEventObject();
   if ( obj == GetControl(ConfigField_SetPassword) )
   {
      (void) PickGlobalPasswdDialog(m_Profile, this);
   }
   else
   {
      FAIL_MSG("click from alien button in compose view page");
      event.Skip();
   }
}

// ----------------------------------------------------------------------------
// wxOptionsPageNetwork
// ----------------------------------------------------------------------------

wxOptionsPageNetwork::wxOptionsPageNetwork(wxNotebook *parent,
                                           Profile *profile)
                    : wxOptionsPageStandard(parent,
                                    _("Network"),
                                    profile,
                                    ConfigField_NetworkFirst,
                                    ConfigField_NetworkLast,
                                    MH_OPAGE_NETWORK)
{
}

// ----------------------------------------------------------------------------
// wxOptionsPagePython
// ----------------------------------------------------------------------------

#ifdef USE_PYTHON

wxOptionsPagePython::wxOptionsPagePython(wxNotebook *parent,
                                         Profile *profile)
                   : wxOptionsPageStandard(parent,
                                   _("Python"),
                                   profile,
                                   ConfigField_PythonFirst,
                                   ConfigField_PythonLast,
                                   MH_OPAGE_PYTHON)
{
}

#endif // USE_PYTHON

// ----------------------------------------------------------------------------
// wxOptionsPageAdb
// ----------------------------------------------------------------------------


wxOptionsPageAdb::wxOptionsPageAdb(wxNotebook *parent,
                                    Profile *profile)
                : wxOptionsPageStandard(parent,
                                _("Addressbook"),
                                profile,
                                ConfigField_AdbFirst,
                                ConfigField_AdbLast,
                                MH_OPAGE_ADB)
{
}

// ----------------------------------------------------------------------------
// wxOptionsPageSync
// ----------------------------------------------------------------------------

wxOptionsPageSync::wxOptionsPageSync(wxNotebook *parent,
                                     Profile *profile)
                 : wxOptionsPageStandard(parent,
                                         _("Synchronize"),
                                         profile,
                                         ConfigField_SyncFirst,
                                         ConfigField_SyncLast,
                                         MH_OPAGE_SYNC)
{
   m_SyncRemote = -1;
}

bool wxOptionsPageSync::TransferDataToWindow()
{
   bool rc = wxOptionsPage::TransferDataToWindow();
   if ( rc )
   {
      m_SyncRemote = READ_CONFIG(m_Profile, MP_SYNC_REMOTE);
   }

   return rc;
}

bool wxOptionsPageSync::TransferDataFromWindow()
{
   bool rc = wxOptionsPage::TransferDataFromWindow();
   if ( rc )
   {
      bool syncRemote = READ_CONFIG(m_Profile, MP_SYNC_REMOTE) != 0;
      if ( syncRemote && !m_SyncRemote )
      {
         if ( MDialog_YesNoDialog
              (
               _("You have activated remote configuration synchronisation.\n"
                 "\n"
                 "Do you want to store the current setup now?"),
               this,
               _("Store settings now?"),
               true,
               GetPersMsgBoxName(M_MSGBOX_OPT_STOREREMOTENOW)
              )
            )
         {
            SaveRemoteConfigSettings();
         }
      }
   }

   return rc;
}

void wxOptionsPageSync::OnButton(wxCommandEvent& event)
{
   wxObject *obj = event.GetEventObject();
   bool save = obj == GetControl(ConfigField_SyncStore);
   if ( !save && (obj != GetControl(ConfigField_SyncRetrieve)) )
   {
      // nor save nor restore, not interesting
      event.Skip();

      return;
   }

   // maybe the user changed the folder setting but didn't press [Apply] ye
   // - still use the current setting, not the one from config
   String foldernameOld = READ_APPCONFIG(MP_SYNC_FOLDER);

   wxTextCtrl *text = wxStaticCast(GetControl(ConfigField_RSConfigFolder),
                                   wxTextCtrl);
   String foldername = text->GetValue();

   if ( foldername != foldernameOld )
   {
      mApplication->GetProfile()->writeEntry(MP_SYNC_FOLDER, foldername);
   }

   // false means don't ask confirmation
   bool ok = save ? SaveRemoteConfigSettings(false)
                  : RetrieveRemoteConfigSettings(false);
   if ( !ok )
   {
      wxLogError(_("Failed to %s remote configuration %s '%s'."),
                 save ? _("save") : _("retrieve"),
                 save ? _("to") : _("from"),
                 foldername.c_str());
   }
   else
   {
      wxLogMessage(_("Successfully %s remote configuration %s '%s'."),
                   save ? _("saved") : _("retrieved"),
                   save ? _("to") : _("from"),
                   foldername.c_str());
   }

   // restore old value regardless
   if ( foldername != foldernameOld )
   {
      mApplication->GetProfile()->writeEntry(MP_SYNC_FOLDER, foldernameOld);
   }
}

// ----------------------------------------------------------------------------
// wxOptionsPageOthers
// ----------------------------------------------------------------------------

wxOptionsPageOthers::wxOptionsPageOthers(wxNotebook *parent,
                                         Profile *profile)
                   : wxOptionsPageStandard(parent,
                                   _("Miscellaneous"),
                                   profile,
                                   ConfigField_OthersFirst,
                                   ConfigField_OthersLast,
                                   MH_OPAGE_OTHERS)
{
   m_nAutosaveDelay =
   m_nAutoAwayDelay = -1;
}

void wxOptionsPageOthers::OnButton(wxCommandEvent& event)
{
   wxObject *obj = event.GetEventObject();
   if ( obj == GetControl(ConfigField_ReenableDialog) )
   {
      if ( ReenablePersistentMessageBoxes(this) )
      {
         wxOptionsEditDialog *dialog = GET_PARENT_OF_CLASS(this, wxOptionsEditDialog);
         wxCHECK_RET( dialog, "options page without a parent dialog?" );
         dialog->SetDirty();
      }
   }
   else
   {
      event.Skip();
   }
}

bool wxOptionsPageOthers::TransferDataToWindow()
{
   // if the user checked "don't ask me again" checkbox in the message box
   // these setting might be out of date - synchronize

   // TODO this should be table based too probably...
   m_Profile->writeEntry(MP_CONFIRMEXIT, wxPMessageBoxEnabled(MP_CONFIRMEXIT));

   bool rc = wxOptionsPage::TransferDataToWindow();
   if ( rc )
   {
      m_nAutosaveDelay = READ_CONFIG(m_Profile, MP_AUTOSAVEDELAY);
      m_nAutoAwayDelay = READ_CONFIG(m_Profile, MP_AWAY_AUTO_ENTER);
   }

   return rc;
}

bool wxOptionsPageOthers::TransferDataFromWindow()
{
   bool rc = wxOptionsPage::TransferDataFromWindow();
   if ( rc )
   {
      // now if the user checked "confirm exit" checkbox we must reenable
      // the message box by erasing the stored answer to it
      wxPMessageBoxEnable(MP_CONFIRMEXIT,
                          READ_CONFIG(m_Profile, MP_CONFIRMEXIT) != 0);

      // restart the timer if the timeout changed
      long delayNew = READ_CONFIG(m_Profile, MP_AUTOSAVEDELAY);
      if ( delayNew != m_nAutosaveDelay )
      {
         if ( !mApplication->RestartTimer(MAppBase::Timer_Autosave) )
         {
            wxLogError(_("Invalid delay value for autosave timer."));

            rc = false;
         }
      }

      // and the other timer too
      delayNew = READ_CONFIG(m_Profile, MP_AWAY_AUTO_ENTER);
      if ( delayNew != m_nAutoAwayDelay )
      {
         if ( !mApplication->RestartTimer(MAppBase::Timer_Away) )
         {
            wxLogError(_("Invalid delay value for auto away timer."));

            rc = false;
         }
      }

      // show/hide the log window depending on the new setting value
      bool showLog = READ_CONFIG(m_Profile, MP_SHOWLOG) != 0;
      if ( showLog != mApplication->IsLogShown() )
      {
         mApplication->ShowLog(showLog);
      }

      mApplication->SetLogFile(READ_CONFIG(m_Profile, MP_LOGFILE));
   }

   return rc;
}

// ----------------------------------------------------------------------------
// wxOptionsPageHelpers
// ----------------------------------------------------------------------------

wxOptionsPageHelpers::wxOptionsPageHelpers(wxNotebook *parent,
                                         Profile *profile)
   : wxOptionsPageStandard(parent,
                   _("Helpers"),
                   profile,
                   ConfigField_HelpersFirst,
                   ConfigField_HelpersLast,
                   MH_OPAGE_HELPERS)
{
}

// ----------------------------------------------------------------------------
// wxOptionsPageFolders
// ----------------------------------------------------------------------------

wxOptionsPageFolders::wxOptionsPageFolders(wxNotebook *parent,
                                           Profile *profile)
   : wxOptionsPageStandard(parent,
                   _("Folders"),
                   profile,
                   ConfigField_FoldersFirst,
                   ConfigField_FoldersLast,
                   MH_OPAGE_FOLDERS)
{
   m_nIncomingDelay =
   m_nPingDelay = -1;

   m_idListbox = ConfigField_OpenFolders;
   m_lboxDlgTitle = _("Folders to open on startup");
   m_lboxDlgPrompt = _("Folder name");
   m_lboxDlgPers = "LastStartupFolder";
}

bool wxOptionsPageFolders::TransferDataToWindow()
{
   bool bRc = wxOptionsPage::TransferDataToWindow();

   if ( bRc )
   {
      // we add the folder opened in the main frame to the list of folders
      // opened on startup if it's not yet among them
      wxControl *control = GetControl(m_idListbox);
      if ( control )
      {
         wxListBox *listbox = wxStaticCast(control, wxListBox);
         wxString strMain = GetControlText(ConfigField_MainFolder);
         int n = listbox->FindString(strMain);
         if ( n == -1 )
         {
            listbox->Append(strMain);
         }
      }
      //else: the listbox is not created in this dialog at all

      m_nIncomingDelay = READ_CONFIG(m_Profile, MP_POLLINCOMINGDELAY);
      m_nPingDelay = READ_CONFIG(m_Profile, MP_UPDATEINTERVAL);
   }

   return bRc;
}

bool wxOptionsPageFolders::TransferDataFromWindow()
{
   // undo what we did in TransferDataToWindow: remove the main folder from
   // the list of folders to be opened on startup
   wxControl *control = GetControl(ConfigField_OpenFolders);
   if ( control )
   {
      wxListBox *listbox = wxStaticCast(control, wxListBox);
      wxString strMain = GetControlText(ConfigField_MainFolder);
      int n = listbox->FindString(strMain);
      if ( n != -1 )
      {
         listbox->Delete(n);
      }
   }

   bool rc = wxOptionsPage::TransferDataFromWindow();
   if ( rc )
   {
      long nIncomingDelay = READ_CONFIG(m_Profile, MP_POLLINCOMINGDELAY),
           nPingDelay = READ_CONFIG(m_Profile, MP_UPDATEINTERVAL);

      if ( nIncomingDelay != m_nIncomingDelay )
      {
         wxLogTrace("timer", "Restarting timer for polling incoming folders");

         rc = mApplication->RestartTimer(MAppBase::Timer_PollIncoming);
      }

      if ( rc && (nPingDelay != m_nPingDelay) )
      {
         wxLogTrace("timer", "Restarting timer for pinging folders");

         rc = mApplication->RestartTimer(MAppBase::Timer_PingFolder);
      }

      if ( !rc )
      {
         wxLogError(_("Failed to restart the timers, please change the "
                      "delay to a valid value."));
      }
   }

   if ( rc )
   {
      // update the frame title/status bar if needed
      if ( IsDirty(ConfigField_StatusFormat_StatusBar) ||
           IsDirty(ConfigField_StatusFormat_TitleBar) )
      {
         // TODO: send the folder status change event
      }
   }

   return rc;
}

// ----------------------------------------------------------------------------
// wxGlobalOptionsDialog
// ----------------------------------------------------------------------------

wxGlobalOptionsDialog::wxGlobalOptionsDialog(wxFrame *parent, const wxString& configKey)
               : wxOptionsEditDialog(parent, _("Program options"), configKey)
{
}

bool
wxGlobalOptionsDialog::TransferDataToWindow()
{
   if ( !wxOptionsEditDialog::TransferDataToWindow() )
      return FALSE;

   int nPageCount = m_notebook->GetPageCount();
   for ( int nPage = 0; nPage < nPageCount; nPage++ ) {
      ((wxOptionsPage *)m_notebook->GetPage(nPage))->UpdateUI();
   }

   return TRUE;
}

void wxGlobalOptionsDialog::CreateNotebook(wxPanel *panel)
{
   m_notebook = new wxOptionsNotebook(panel);
}

wxGlobalOptionsDialog::~wxGlobalOptionsDialog()
{
   // save settings
   Profile::FlushAll();
}

// ----------------------------------------------------------------------------
// wxCustomOptionsNotebook is a notebook which contains the given page
// ----------------------------------------------------------------------------

wxCustomOptionsNotebook::wxCustomOptionsNotebook
                         (
                          wxWindow *parent,
                          size_t nPages,
                          const wxOptionsPageDesc *pageDesc,
                          const wxString& configForNotebook,
                          Profile *profile
                         )
                       : wxNotebookWithImages(
                                              configForNotebook,
                                              parent,
                                              GetImagesArray(nPages, pageDesc)
                                             )
{
   // use the global profile by default
   if ( profile )
      profile->IncRef();
   else
      profile = Profile::CreateProfile("");


   for ( size_t n = 0; n < nPages; n++ )
   {
      // the page ctor will add it to the notebook
      const wxOptionsPageDesc& desc = pageDesc[n];
      wxOptionsPageDynamic *page = new wxOptionsPageDynamic(
                                                            this,
                                                            desc.title,
                                                            profile,
                                                            desc.aFields,
                                                            desc.aDefaults,
                                                            desc.nFields,
                                                            desc.nOffset,
                                                            desc.helpId,
                                                            n  // image index
                                                           );
      page->Layout();
   }

   profile->DecRef();
}

// return the array which should be passed to wxNotebookWithImages ctor
const char **
wxCustomOptionsNotebook::GetImagesArray(size_t nPages,
                                        const wxOptionsPageDesc *pageDesc)
{
   m_aImages = new const char *[nPages + 1];

   for ( size_t n = 0; n < nPages; n++ )
   {
      m_aImages[n] = pageDesc[n].image;
   }

   m_aImages[nPages] = NULL;

   return m_aImages;
}

// ----------------------------------------------------------------------------
// wxOptionsNotebook manages its own image list
// ----------------------------------------------------------------------------

// should be in sync with the enum OptionsPage in wxOptionsDlg.h!
const char *wxOptionsNotebook::ms_aszImages[] =
{
   "ident",
   "network",
   "compose",
   "folders",
#ifdef USE_PYTHON
   "python",
#endif
   "msgview",
   "folderview",
   "foldertree",
   "adrbook",
   "helpers",
   "sync",
   "miscopt",
   NULL
};

// create the control and add pages too
wxOptionsNotebook::wxOptionsNotebook(wxWindow *parent)
                 : wxNotebookWithImages("OptionsNotebook", parent, ms_aszImages)
{
   // don't forget to update both the array above and the enum!
   wxASSERT( WXSIZEOF(ms_aszImages) == OptionsPage_Max + 1);

   Profile *profile = GetProfile();

   // create and add the pages
   new wxOptionsPageIdent(this, profile);
   new wxOptionsPageNetwork(this, profile);
   new wxOptionsPageCompose(this, profile);
   new wxOptionsPageFolders(this, profile);
#ifdef USE_PYTHON
   new wxOptionsPagePython(this, profile);
#endif
   new wxOptionsPageMessageView(this, profile);
   new wxOptionsPageFolderView(this, profile);
   new wxOptionsPageFolderTree(this, profile);
   new wxOptionsPageAdb(this, profile);
   new wxOptionsPageHelpers(this, profile);
   new wxOptionsPageSync(this, profile);
   new wxOptionsPageOthers(this, profile);

   profile->DecRef();
}

// ----------------------------------------------------------------------------
// wxIdentityOptionsDialog
// ----------------------------------------------------------------------------

void wxIdentityOptionsDialog::CreatePagesDesc()
{
   // TODO: it would be better to have custom pages here as not all settings
   //       make sense for the identities, but it's easier to use the standard
   //       ones

   size_t nOffset;
   m_nPages = 3;
   m_aPages = new wxOptionsPageDesc[3];

   // identity page
   nOffset = ConfigField_IdentFirst + 1;
   m_aPages[0] = wxOptionsPageDesc
   (
      _("Identity"),
      wxOptionsNotebook::ms_aszImages[OptionsPage_Ident],
      MH_OPAGE_IDENT,
      wxOptionsPageStandard::ms_aFields + nOffset,
      wxOptionsPageStandard::ms_aConfigDefaults + nOffset,
      ConfigField_IdentLast - ConfigField_IdentFirst,
      nOffset
   );

   // network page
   nOffset = ConfigField_NetworkFirst + 1;
   m_aPages[1] = wxOptionsPageDesc
   (
      _("Network"),
      wxOptionsNotebook::ms_aszImages[OptionsPage_Network],
      MH_OPAGE_NETWORK,
      wxOptionsPageStandard::ms_aFields + nOffset,
      wxOptionsPageStandard::ms_aConfigDefaults + nOffset,
      ConfigField_NetworkLast - ConfigField_NetworkFirst,
      nOffset
   );

   // compose page
   nOffset = ConfigField_ComposeFirst + 1;
   m_aPages[2] = wxOptionsPageDesc
   (
      _("Compose"),
      wxOptionsNotebook::ms_aszImages[OptionsPage_Compose],
      MH_OPAGE_COMPOSE,
      wxOptionsPageStandard::ms_aFields + nOffset,
      wxOptionsPageStandard::ms_aConfigDefaults + nOffset,
      ConfigField_ComposeLast - ConfigField_ComposeFirst,
      nOffset
   );
};

// ----------------------------------------------------------------------------
// wxRestoreDefaultsDialog implementation
// ----------------------------------------------------------------------------

wxRestoreDefaultsDialog::wxRestoreDefaultsDialog(Profile *profile,
                                                 wxFrame *parent)
                       : wxProfileSettingsEditDialog
                         (
                           profile,
                           "RestoreOptionsDlg",
                           parent,
                           _("Restore default options")
                         )
{
   wxLayoutConstraints *c;

   // create the Ok and Cancel buttons in the bottom right corner
   wxStaticBox *box = CreateStdButtonsAndBox(_("&All settings"));

   // create a short help message above

   wxStaticText *msg = new wxStaticText
                           (
                              this, -1,
                              _("Please check the settings whose values\n"
                                "should be reset to the defaults.")
                           );
   c = new wxLayoutConstraints;
   c->left.SameAs(box, wxLeft, 2*LAYOUT_X_MARGIN);
   c->right.SameAs(box, wxRight, 2*LAYOUT_X_MARGIN);
   c->top.SameAs(box, wxTop, 4*LAYOUT_Y_MARGIN);
   c->height.AsIs();
   msg->SetConstraints(c);

   // create the checklistbox in the area which is left
   c = new wxLayoutConstraints;
   c->left.SameAs(box, wxLeft, 2*LAYOUT_X_MARGIN);
   c->right.SameAs(box, wxRight, 2*LAYOUT_X_MARGIN);
   c->top.Below(msg, 2*LAYOUT_Y_MARGIN);
   c->bottom.SameAs(box, wxBottom, 2*LAYOUT_Y_MARGIN);

   m_checklistBox = new wxCheckListBox(this, -1);
   m_checklistBox->SetConstraints(c);

   // add the items to the checklistbox
   for ( size_t n = 0; n < ConfigField_Max; n++ )
   {
      switch ( wxOptionsPageStandard::GetStandardFieldType(n) )
      {
         case wxOptionsPage::Field_Message:
            // this is not a setting, just some help text
            continue;

         case wxOptionsPage::Field_SubDlg:
            // TODO inject in the checklistbox the settings of subdlg
            continue;

         case wxOptionsPage::Field_XFace:
            // TODO inject the settings of subdlg
            continue;

      default:
            break;
      }

      m_checklistBox->Append(
            wxStripMenuCodes(_(wxOptionsPageStandard::ms_aFields[n].label)));
   }

   // set the initial and minimal size
   SetDefaultSize(4*wBtn, 10*hBtn, FALSE /* not minimal size */);
}

bool wxRestoreDefaultsDialog::TransferDataFromWindow()
{
   // delete the values of all selected settings in this profile - this will
   // effectively restore their default values
#if wxCHECK_VERSION(2, 3, 2)
   size_t count = (size_t)m_checklistBox->GetCount();
#else
   size_t count = (size_t)m_checklistBox->Number();
#endif
   for ( size_t n = 0; n < count; n++ )
   {
      if ( m_checklistBox->IsChecked(n) )
      {
         MarkDirty();

         GetProfile()->GetConfig()->DeleteEntry(
               wxOptionsPageStandard::ms_aConfigDefaults[n].name);
      }
   }

   return TRUE;
}

// ----------------------------------------------------------------------------
// our public interface
// ----------------------------------------------------------------------------

void ShowOptionsDialog(wxFrame *parent, OptionsPage page)
{
   wxGlobalOptionsDialog dlg(parent);
   dlg.CreateAllControls();
   dlg.SetNotebookPage(page);
   dlg.Layout();
   (void)dlg.ShowModal();
}

bool ShowRestoreDefaultsDialog(Profile *profile, wxFrame *parent)
{
   wxRestoreDefaultsDialog dlg(profile, parent);
   (void)dlg.ShowModal();

   return dlg.HasChanges();
}

void ShowCustomOptionsDialog(size_t nPages,
                             const wxOptionsPageDesc *pageDesc,
                             Profile *profile,
                             wxFrame *parent)
{
   wxCustomOptionsDialog dlg(nPages, pageDesc, profile, parent);
   dlg.CreateAllControls();
   dlg.Layout();

   (void)dlg.ShowModal();
}

void ShowIdentityDialog(const wxString& identity, wxFrame *parent)
{
   wxIdentityOptionsDialog dlg(identity, parent);
   dlg.CreateAllControls();
   dlg.Layout();

   (void)dlg.ShowModal();
}
