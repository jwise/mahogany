///////////////////////////////////////////////////////////////////////////////
// Project:     M
// File name:   TemplateDialog.cpp - the template configuration dialog
// Purpose:     these dialogs are used mainly by the options dialog, but may be
//              also used from elsewhere
// Author:      Vadim Zeitlin
// Modified by: VZ at 09.05.00 to allow editing all templates
// Created:     16.07.99
// CVS-ID:      $Id$
// Copyright:   (c) 1999 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
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
#  include "strutil.h"

#  include <wx/combobox.h>
#  include <wx/layout.h>
#  include <wx/statbox.h>
#  include <wx/stattext.h>
#  include <wx/statbmp.h>
#  include <wx/textctrl.h>
#  include <wx/menu.h>
#  include <wx/listbox.h>
#  include <wx/choicdlg.h>
#endif

#include "Mdefaults.h"

#include "MessageTemplate.h"

#include "MDialogs.h"
#include "gui/wxDialogLayout.h"

#include "TemplateDialog.h"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

static const char *gs_templateNames[MessageTemplate_Max] =
{
   gettext_noop("New message"),
   gettext_noop("New article"),
   gettext_noop("Reply"),
   gettext_noop("Follow-up"),
   gettext_noop("Forward")
};


// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// the text window for editing templates: it has a popup menu which allows the
// user to insert any of existing macros
class TemplateEditor : public wxTextCtrl
{
public:
   // ctor
   TemplateEditor(const TemplatePopupMenuItem& menu, wxWindow *parent)
      : wxTextCtrl(parent, -1, "", wxDefaultPosition, wxDefaultSize,
                   wxTE_MULTILINE),
        m_menuInfo(menu)
   {
      m_menu = NULL;
   }

   virtual ~TemplateEditor() { if ( m_menu ) delete m_menu; }

   // callbacks
   void OnRClick(wxMouseEvent& event); // show the popup menu
   void OnMenu(wxCommandEvent& event); // insert the selection into text

private:
   // creates the popup menu if it doesn't exist yet
   void CreatePopupMenu();

   // CreatePopupMenu() helper
   void AppendMenuItem(wxMenu *menu, const TemplatePopupMenuItem& menuitem);

   // the popup menu description
   const TemplatePopupMenuItem& m_menuInfo;
   WX_DEFINE_ARRAY(const TemplatePopupMenuItem *, ArrayPopupMenuItems);
   ArrayPopupMenuItems m_items;

   // the popup menu itself
   wxMenu *m_menu;

   DECLARE_EVENT_TABLE()
};

// the dialog for editing the templates for the given folder (profile): it
// contains a listbox for choosing the template and the text control to edit
// the selected template
class wxFolderTemplatesDialog : public wxOptionsPageSubdialog
{
public:
   wxFolderTemplatesDialog(const TemplatePopupMenuItem& menu,
                           Profile *profile,
                           wxWindow *parent);

   // did the user really change anything?
   bool WasChanged() const { return m_wasChanged; }

   // called by wxWindows when [Ok] button was pressed
   virtual bool TransferDataFromWindow();

   // callbacks
   void OnListboxSelection(wxCommandEvent& event);

private:
   // saves changes to current template
   void SaveChanges();

   // updates the contents of the text control
   void UpdateText();

   Profile        *m_profile;
   MessageTemplateKind m_kind;         // of template being edited
   wxTextCtrl         *m_textctrl;
   bool                m_wasChanged;

   DECLARE_EVENT_TABLE()
};

// the dialog for choosing/editing all templates of the given kind
class wxAllTemplatesDialog : public wxManuallyLaidOutDialog
{
public:
   wxAllTemplatesDialog(MessageTemplateKind kind,
                        const TemplatePopupMenuItem& menu,
                        wxWindow *parent);

   // get the value of the template chosen
   wxString GetTemplateValue() const;

   // called by wxWindows when [Ok] button was pressed
   virtual bool TransferDataFromWindow();

   // callbacks
   void OnAddTemplate(wxCommandEvent& event);
   void OnDeleteTemplate(wxCommandEvent& event);
   void OnListboxSelection(wxCommandEvent& event);
   void OnComboBoxChange(wxCommandEvent& event);
   void OnUpdateUIDelete(wxUpdateUIEvent& event);

private:
   // helper function to get the correct title for the dialog
   static wxString GetTemplateTitle(MessageTemplateKind kind);

   // fill the listbox with the templates of the given m_kind
   void FillListBox();

   // ask the user if he wants to save changes to the currently selected
   // template, if it was changed
   void CheckForChanges();

   // save the changes made to the template being edited
   void SaveChanges();

   // show the currently chosen template in the text control
   void UpdateText();

   MessageTemplateKind m_kind;   // the kind of all templates we edit
   wxString m_name;              // the name of the template being edited

   // controls
   wxListBox  *m_listbox;
   wxButton   *m_btnAdd,
              *m_btnDelete;
   wxTextCtrl *m_textctrl;

   DECLARE_EVENT_TABLE()
};

// ----------------------------------------------------------------------------
// event tables
// ----------------------------------------------------------------------------

enum
{
   Button_Template_Add = 100,
   Button_Template_Delete
};

BEGIN_EVENT_TABLE(TemplateEditor, wxTextCtrl)
   EVT_MENU(-1, TemplateEditor::OnMenu)
   EVT_RIGHT_DOWN(TemplateEditor::OnRClick)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(wxFolderTemplatesDialog, wxOptionsPageSubdialog)
   EVT_LISTBOX(-1, wxFolderTemplatesDialog::OnListboxSelection)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(wxAllTemplatesDialog, wxManuallyLaidOutDialog)
   EVT_LISTBOX(-1, wxAllTemplatesDialog::OnListboxSelection)
   EVT_COMBOBOX(-1, wxAllTemplatesDialog::OnComboBoxChange)

   EVT_BUTTON(Button_Template_Add,    wxAllTemplatesDialog::OnAddTemplate)
   EVT_BUTTON(Button_Template_Delete, wxAllTemplatesDialog::OnDeleteTemplate)

   EVT_UPDATE_UI(Button_Template_Delete, wxAllTemplatesDialog::OnUpdateUIDelete)
END_EVENT_TABLE()

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// TemplateEditor
// ----------------------------------------------------------------------------

void TemplateEditor::OnRClick(wxMouseEvent& event)
{
   // create the menu if it hadn't been created yet
   CreatePopupMenu();

   // show it
   if ( m_menu )
   {
      (void)PopupMenu(m_menu, event.GetPosition());
   }
}

void TemplateEditor::OnMenu(wxCommandEvent& event)
{
   size_t id = (size_t)event.GetId();
   CHECK_RET( id < m_items.GetCount(), "unexpected menu event" );

   const TemplatePopupMenuItem *menuitem = m_items[id];
   CHECK_RET( menuitem, "no menu item" );

   String value;
   switch ( menuitem->type )
   {
         // put it here because in the last case clause we have some variables
         // which would be otherwise have to be taken inside another block...
      default:
         FAIL_MSG("unexpected menu item type");
         return;

      case TemplatePopupMenuItem::Normal:
         // nothing special to do - just insert the corresponding text
         value = menuitem->format;
         break;

      case TemplatePopupMenuItem::File:
         // choose the file (can't be more specific in messages because we
         // don't really know what is it for...)
         value = wxPFileSelector("TemplateFile",
                                 _("Please choose a file"),
                                 NULL, NULL, NULL, NULL,
                                 wxOPEN | wxFILE_MUST_EXIST,
                                 this);
         if ( !value )
         {
            // user cancelled
            return;
         }

         // fall through

      case TemplatePopupMenuItem::Text:
         if ( !value )
         {
            // get some text from user (FIXME the prompts are really stupid)
            if ( !MInputBox(&value, _("Value for template variable"),
                            "Value", this, "TemplateText") )
            {
               // user cancelled
               return;
            }
         }
         //else: it was, in fact, a filename and it was already selected

         // quote the value if needed - that is if it contains anything but
         // alphanumeric characters
         bool needsQuotes = FALSE;
         for ( const char *pc = value.c_str(); *pc; pc++ )
         {
            if ( !isalnum(*pc) )
            {
               needsQuotes = TRUE;

               break;
            }
         }

         if ( needsQuotes )
         {
            String value2('"');
            for ( const char *pc = value.c_str(); *pc; pc++ )
            {
               if ( *pc == '"' || *pc == '\\' )
               {
                  value2 += '\\';
               }

               value2 += *pc;
            }

            // closing quote
            value2 += '"';

            value = value2;
         }

         // check that the format string contains exactly what it must
         ASSERT_MSG( strutil_extract_formatspec(menuitem->format) == "s",
                     "incorrect format string" );

         value.Printf(menuitem->format, value.c_str());
         break;
   }

   WriteText(value);
}

void TemplateEditor::AppendMenuItem(wxMenu *menu,
                                    const TemplatePopupMenuItem& menuitem)
{
   switch ( menuitem.type )
   {
      case TemplatePopupMenuItem::Submenu:
         {
            // first create the entry for the submenu
            wxMenu *submenu = new wxMenu;
            menu->Append(m_items.GetCount(), menuitem.label, submenu);

            // next subitems
            for ( size_t n = 0; n < menuitem.nSubItems; n++ )
            {
               AppendMenuItem(submenu, menuitem.submenu[n]);
            }
         }
         break;

      case TemplatePopupMenuItem::None:
         // as we don't use an id, we should change m_items.GetCount(), so
         // return immediately instead of just "break"
         menu->AppendSeparator();
         return;

      case TemplatePopupMenuItem::Normal:
      case TemplatePopupMenuItem::File:
      case TemplatePopupMenuItem::Text:
         menu->Append(m_items.GetCount(), menuitem.label);
         break;

      default:
         FAIL_MSG("unknown popup menu item type");
         return;
   }

   m_items.Add(&menuitem);
}

void TemplateEditor::CreatePopupMenu()
{
   if ( m_menu )
   {
      // menu already created
      return;
   }

   // the top level pseudo item must have type "Submenu" - otherwise we
   // wouldn't have any menu at all
   CHECK_RET( m_menuInfo.type == TemplatePopupMenuItem::Submenu, "no menu?" );

   m_menu = new wxMenu;
   m_menu->SetTitle(_("Please choose"));

   for ( size_t n = 0; n < m_menuInfo.nSubItems; n++ )
   {
      AppendMenuItem(m_menu, m_menuInfo.submenu[n]);
   }
}

// ----------------------------------------------------------------------------
// wxFolderTemplatesDialog
// ----------------------------------------------------------------------------

wxFolderTemplatesDialog::wxFolderTemplatesDialog(const TemplatePopupMenuItem& menu,
                                   Profile *profile,
                                   wxWindow *parent)
                : wxOptionsPageSubdialog(profile, parent,
                                         _("Configure message templates"),
                                         "ComposeTemplates")
{
   // init members
   // ------------
   m_kind = MessageTemplate_Max;
   m_profile = profile;

   // layout the controls
   // -------------------
   wxLayoutConstraints *c;

   // first the box around everything
   wxStaticBox *box = CreateStdButtonsAndBox("");

   // then a short help message
   wxStaticText *msg = new wxStaticText
                           (
                            this, -1,
                            _("Select the template to edit in the list first. "
                              "Then right click the mouse in the text control "
                              "to get the list of all available macros.")
                           );
   c = new wxLayoutConstraints;
   c->left.SameAs(box, wxLeft, 2*LAYOUT_X_MARGIN);
   c->right.SameAs(box, wxRight, 2*LAYOUT_X_MARGIN);
   c->top.SameAs(box, wxTop, 2*LAYOUT_Y_MARGIN);
   c->height.AsIs();
   msg->SetConstraints(c);

   // on the left side is the listbox with all available templates
   wxListBox *listbox = new wxPListBox("MsgTemplate", this, -1);

   // this array should be in sync with MessageTemplateKind enum
   ASSERT_MSG( WXSIZEOF(gs_templateNames) == MessageTemplate_Max,
               "forgot to update the labels array?" );
   for ( size_t n = 0; n < WXSIZEOF(gs_templateNames); n++ )
   {
      listbox->Append(_(gs_templateNames[n]));
   }

   c = new wxLayoutConstraints;
   c->top.Below(msg, LAYOUT_Y_MARGIN);
   c->left.SameAs(box, wxLeft, 2*LAYOUT_X_MARGIN);
   c->width.AsIs();
   c->height.Absolute(5*hBtn);
   listbox->SetConstraints(c);

   // to the right of it is the text control where template file can be edited
   m_textctrl = new TemplateEditor(menu, this);
   c = new wxLayoutConstraints;
   c->top.SameAs(listbox, wxTop);
   c->height.SameAs(listbox, wxHeight);
   c->left.RightOf(listbox, LAYOUT_X_MARGIN);
   c->right.SameAs(box, wxRight, 2*LAYOUT_X_MARGIN);
   m_textctrl->SetConstraints(c);

   SetDefaultSize(6*wBtn, 10*hBtn);
}

void wxFolderTemplatesDialog::UpdateText()
{
   String templateValue = GetMessageTemplate(m_kind, m_profile);

   m_textctrl->SetValue(templateValue);
   m_textctrl->DiscardEdits();
}

void wxFolderTemplatesDialog::SaveChanges()
{
   m_wasChanged = TRUE;

   // TODO: give the user the possibility to change the auto generated name
   wxString name;
   name << m_profile->GetName() << '_' << _(gs_templateNames[m_kind]);
   name.Replace("/", "_"); // we want it flat
   SetMessageTemplate(name, m_textctrl->GetValue(), m_kind, m_profile);
}

void wxFolderTemplatesDialog::OnListboxSelection(wxCommandEvent& event)
{
   if ( m_textctrl->IsModified() )
   {
      // save it if the user doesn't veto it
      String msg;
      msg.Printf(_("You have modified the template for message of type "
                   "'%s', would you like to save it?"),
                 gs_templateNames[m_kind]);
      if ( MDialog_YesNoDialog(msg, this,
                               MDIALOG_YESNOTITLE, true, "SaveTemplate") )
      {
         SaveChanges();
      }
   }

   // load the template for the selected type into the text ctrl
   m_kind = (MessageTemplateKind)event.GetInt();
   UpdateText();
}

bool wxFolderTemplatesDialog::TransferDataFromWindow()
{
   if ( m_textctrl->IsModified() )
   {
      // don't ask - if the user pressed ok, he does want to save changes,
      // otherwise he would have chosen cancel
      SaveChanges();
   }

   return TRUE;
}

// ----------------------------------------------------------------------------
// wxAllTemplatesDialog
// ----------------------------------------------------------------------------

wxAllTemplatesDialog::wxAllTemplatesDialog(MessageTemplateKind kind,
                                           const TemplatePopupMenuItem& menu,
                                           wxWindow *parent)
                    : wxManuallyLaidOutDialog(parent,
                                              GetTemplateTitle(kind),
                                              "AllTemplates")
{
   // init member vars
   // ----------------
   m_kind = kind;

   // layout the controls
   // -------------------
   wxLayoutConstraints *c;

   // first the box around everything
   wxStaticBox *box = CreateStdButtonsAndBox("All available templates");

   // on the left side there is a combo allowing to choose the template type
   // and a listbox with all available templates for this type
   m_listbox = new wxPListBox("AllTemplates", this, -1);

   // this array should be in sync with MessageTemplateKind enum
   ASSERT_MSG( WXSIZEOF(gs_templateNames) == MessageTemplate_Max,
               "forgot to update the labels array?" );
   wxString choices[MessageTemplate_Max];
   for ( size_t n = 0; n < WXSIZEOF(gs_templateNames); n++ )
   {
      choices[n] = _(gs_templateNames[n]);
   }

   wxComboBox *combo = new wxComboBox(this, -1,
                                      gs_templateNames[m_kind],
                                      wxDefaultPosition,
                                      wxDefaultSize,
                                      WXSIZEOF(gs_templateNames), choices,
                                      wxCB_READONLY);

   c = new wxLayoutConstraints;
   c->top.SameAs(box, wxTop, 4*LAYOUT_Y_MARGIN);
   c->left.SameAs(box, wxLeft, 2*LAYOUT_X_MARGIN);
   c->width.AsIs();
   c->height.AsIs();
   combo->SetConstraints(c);

   // fill the listbox now to let it auto adjust the width before setting the
   // constraints
   FillListBox();

   c = new wxLayoutConstraints;
   c->top.Below(combo, 2*LAYOUT_Y_MARGIN);
   c->left.SameAs(box, wxLeft, 2*LAYOUT_X_MARGIN);
   c->width.SameAs(combo, wxWidth);
   c->bottom.SameAs(box, wxBottom, 2*LAYOUT_Y_MARGIN);
   m_listbox->SetConstraints(c);

   // put 2 buttons to add/delete templates along the right edge
   m_btnAdd = new wxButton(this, Button_Template_Add, _("&Add..."));
   c = new wxLayoutConstraints;
   c->width.Absolute(wBtn);
   c->height.Absolute(hBtn);
   c->right.SameAs(box, wxRight, 2*LAYOUT_X_MARGIN);
   c->top.SameAs(box, wxCentreY, -2*LAYOUT_Y_MARGIN);
   m_btnAdd->SetConstraints(c);

   m_btnDelete = new wxButton(this, Button_Template_Delete, _("&Delete"));
   c = new wxLayoutConstraints;
   c->width.Absolute(wBtn);
   c->height.Absolute(hBtn);
   c->bottom.SameAs(box, wxCentreY, 2*LAYOUT_Y_MARGIN);
   c->right.SameAs(m_btnAdd, wxRight);
   m_btnDelete->SetConstraints(c);

   // between the listbox and the buttons there is the text control where
   // template file can be edited
   m_textctrl = new TemplateEditor(menu, this);
   c = new wxLayoutConstraints;
   c->top.SameAs(combo, wxTop);
   c->bottom.SameAs(m_listbox, wxBottom);
   c->left.RightOf(m_listbox, LAYOUT_X_MARGIN);
   c->right.LeftOf(m_btnAdd, 2*LAYOUT_X_MARGIN);
   m_textctrl->SetConstraints(c);

   SetDefaultSize(6*wBtn, 10*hBtn);

   UpdateText();
}

void wxAllTemplatesDialog::FillListBox()
{
   wxArrayString names = GetMessageTemplateNames(m_kind);
   size_t count = names.GetCount();
   for ( size_t n = 0; n < count; n++ )
   {
      m_listbox->Append(names[n]);
   }
}

wxString wxAllTemplatesDialog::GetTemplateTitle(MessageTemplateKind kind)
{
   wxString title, what;
   switch ( kind )
   {
      case MessageTemplate_NewMessage:
         what = _("composing new messages");
         break;

      case MessageTemplate_NewArticle:
         what = _("composing newsgroup articles ");
         break;

      case MessageTemplate_Reply:
         what = _("replying");
         break;

      case MessageTemplate_Followup:
         what = _("writing follow ups");
         break;

      case MessageTemplate_Forward:
         what = _("forwarding");
         break;

      default:
         FAIL_MSG("unknown template kind");
   }

   title.Printf(_("Configure templates for %s"), what.c_str());

   return title;
}

wxString wxAllTemplatesDialog::GetTemplateValue() const
{
   wxString value;
   if ( !!m_name )
   {
      value = GetMessageTemplate(m_kind, m_name);
   }

   return value;
}

void wxAllTemplatesDialog::SaveChanges()
{
   wxASSERT_MSG( !!m_name, "shouldn't try to save" );

   SetMessageTemplate(m_name, m_textctrl->GetValue(), m_kind, NULL);
}

void wxAllTemplatesDialog::UpdateText()
{
   int sel = m_listbox->GetSelection();
   if ( sel == -1 )
   {
      m_textctrl->Clear();
      m_textctrl->Enable(FALSE);

      m_name.clear();
   }
   else // we have selection
   {
      m_name = m_listbox->GetString(sel);
      String value = GetMessageTemplate(m_kind, m_name);

      m_textctrl->Enable(TRUE);
      m_textctrl->SetValue(value);
   }

   // in any case, we changed it, not the user
   m_textctrl->DiscardEdits();
}

void wxAllTemplatesDialog::CheckForChanges()
{
   if ( m_textctrl->IsModified() )
   {
      // save it if the user doesn't veto it
      String msg;
      msg.Printf(_("You have modified the template '%s', "
                   "would you like to save it?"),
                 m_name.c_str());
      if ( MDialog_YesNoDialog(msg, this,
                               MDIALOG_YESNOTITLE, true, "SaveTemplate") )
      {
         SaveChanges();
      }
   }
}

void wxAllTemplatesDialog::OnComboBoxChange(wxCommandEvent& event)
{
   CheckForChanges();

   m_kind = (MessageTemplateKind)event.GetInt();
   m_listbox->Clear();
   FillListBox();
   UpdateText();
}

void wxAllTemplatesDialog::OnListboxSelection(wxCommandEvent& event)
{
   CheckForChanges();

   UpdateText();
}

void wxAllTemplatesDialog::OnDeleteTemplate(wxCommandEvent& event)
{
   wxASSERT_MSG( !!m_name, "shouldn't try to delete" );

   String msg;
   msg.Printf(_("Do you really want to delete the template '%s'?"),
              m_name.c_str());
   if ( MDialog_YesNoDialog(msg, this,
                            MDIALOG_YESNOTITLE, false, "DeleteTemplate") )
   {
      m_listbox->Delete(m_listbox->GetSelection());

      DeleteMessageTemplate(m_kind, m_name);
   }
}

void wxAllTemplatesDialog::OnAddTemplate(wxCommandEvent& event)
{
   // get the name for the new template
   wxString name;
   if ( !MInputBox(
                   &name,
                   _("Create new template"),
                   _("Name for the new template:"),
                   this,
                   "AddTemplate"
                  ) )
   {
      // cancelled
      return;
   }

   // append the new string to the listbox and select it
   m_name = name;
   m_listbox->Append(name);
   m_listbox->SetSelection(m_listbox->GetCount() - 1);
   m_textctrl->Clear();
   m_textctrl->DiscardEdits();
}

void wxAllTemplatesDialog::OnUpdateUIDelete(wxUpdateUIEvent& event)
{
   // only enable delete button if some template was chosen
   event.Enable(!!m_name);
}

bool wxAllTemplatesDialog::TransferDataFromWindow()
{
   if ( m_textctrl->IsModified() )
   {
      // we were editing something, save the changes
      SaveChanges();
   }

   return TRUE;
}

// ----------------------------------------------------------------------------
// our public interface
// ----------------------------------------------------------------------------

// edit the templates for the given folder/profile
bool ConfigureTemplates(Profile *profile,
                        wxWindow *parent,
                        const TemplatePopupMenuItem& menu)
{
   wxFolderTemplatesDialog dlg(menu, profile, parent);
   if ( dlg.ShowModal() == wxID_OK && dlg.WasChanged() )
   {
      return TRUE;
   }
   else
   {
      return FALSE;
   }
}

// select a template from all existing ones
String ChooseTemplateFor(MessageTemplateKind kind,
                         wxWindow *parent,
                         const TemplatePopupMenuItem& menu)
{
   wxString value;
   wxAllTemplatesDialog dlg(kind, menu, parent);
   if ( dlg.ShowModal() == wxID_OK )
   {
      value = dlg.GetTemplateValue();
   }

   return value;
}

// edit any templates
void EditTemplates(wxWindow *parent,
                   const TemplatePopupMenuItem& menu)
{
   // first get the kind of templates to edit
   wxString templateKinds[MessageTemplate_Max];
   templateKinds[MessageTemplate_NewMessage] = _("Composing new messages");
   templateKinds[MessageTemplate_NewArticle] = _("Writing new articles");
   templateKinds[MessageTemplate_Reply] = _("Replying");
   templateKinds[MessageTemplate_Forward] = _("Forwarding");
   templateKinds[MessageTemplate_Followup] = _("Following up");

   int index = wxGetSingleChoiceIndex
               (
                _("What kind of templates would you like to edit?\n"
                  "Mahogany uses different templates for:"),
                _("Mahogany : edit templates"),
                WXSIZEOF(templateKinds), templateKinds,
                parent
               );

   if ( index != -1 )
   {
      wxAllTemplatesDialog dlg((MessageTemplateKind)index, menu, parent);

      dlg.ShowModal();
   }
}

