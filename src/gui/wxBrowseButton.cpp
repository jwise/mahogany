///////////////////////////////////////////////////////////////////////////////
// Project:     M
// File name:   gui/wxBrowseButton.cpp - implementation of browse button
//              classes declared in gui/wxBrowseButton.h.
// Purpose:
// Author:      Vadim Zeitlin
// Modified by:
// Created:     24.12.98
// CVS-ID:      $Id$
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
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

#  include <wx/cmndata.h>
#  include <wx/statbmp.h>
#  include <wx/dcmemory.h>
#  include <wx/layout.h>
#  include <wx/statbox.h>
#  include <wx/dirdlg.h>
#endif

#include <wx/imaglist.h>
#include <wx/colordlg.h>

#include "MFolder.h"
#include "MFolderDialogs.h"

#include "gui/wxIconManager.h"
#include "gui/wxDialogLayout.h"

#include "gui/wxBrowseButton.h"

#include "miscutil.h" // for ParseColorString and GetColorName

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

WX_DEFINE_ARRAY(wxBitmap *, wxIconArray);

class wxIconView : public wxListCtrl
{
public:
   // the size of icons as we show them
   static const int ms_iconSize;

   wxIconView(wxDialog *parent,
              const wxIconArray& icons,
              int selection);
};

class wxIconSelectionDialog : public wxManuallyLaidOutDialog
{
public:
   wxIconSelectionDialog(wxWindow *parent,
                         const wxString& title,
                         const wxIconArray& icons,
                         int selection);

   // accessors
   size_t GetSelection() const { return m_selection; }

   // event handlers
   void OnIconSelected(wxListEvent& event);
   void OnUpdateUI(wxUpdateUIEvent& event);

private:
   int m_selection;

   DECLARE_EVENT_TABLE()
};

// notify the wxColorBrowseButton about changes to its associated text
// control
class wxColorTextEvtHandler : public wxEvtHandler
{
public:
   wxColorTextEvtHandler(wxColorBrowseButton *btn)
   {
      m_btn = btn;
   }

protected:
   void OnText(wxCommandEvent& event)
   {
      m_btn->UpdateColorFromText();

      event.Skip();
   }

   void OnDestroy(wxWindowDestroyEvent& event)
   {
      event.Skip();

      // delete ourselves as this is the only place where we can do it
      m_btn->OnTextDelete();
   }

private:
   wxColorBrowseButton *m_btn;

   DECLARE_EVENT_TABLE()
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// event tables
// ----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxBrowseButton, wxButton)
   EVT_BUTTON(-1, wxBrowseButton::OnButtonClick)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(wxIconSelectionDialog, wxManuallyLaidOutDialog)
   EVT_UPDATE_UI(wxID_OK, wxIconSelectionDialog::OnUpdateUI)
   EVT_LIST_ITEM_SELECTED(-1, wxIconSelectionDialog::OnIconSelected)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(wxColorTextEvtHandler, wxEvtHandler)
   EVT_TEXT(-1, wxColorTextEvtHandler::OnText)
   EVT_WINDOW_DESTROY(wxColorTextEvtHandler::OnDestroy)
END_EVENT_TABLE()

// ----------------------------------------------------------------------------
// wxFileBrowseButton
// ----------------------------------------------------------------------------

void wxFileBrowseButton::DoBrowse()
{
   // get the last position
   wxString strLastDir, strLastFile, strLastExt, strPath = GetText();
   wxSplitPath(strPath, &strLastDir, &strLastFile, &strLastExt);

   long style = m_open ? wxOPEN | wxHIDE_READONLY
                       : wxSAVE | wxOVERWRITE_PROMPT;
   if ( m_existingOnly )
      style |= wxFILE_MUST_EXIST;

   wxFileDialog dialog(this, "",
                       strLastDir, strLastFile,
                       _(wxALL_FILES),
                       style);

   if ( dialog.ShowModal() == wxID_OK )
   {
      SetText(dialog.GetPath());
   }
}

// ----------------------------------------------------------------------------
// wxDirBrowseButton
// ----------------------------------------------------------------------------

void wxDirBrowseButton::DoBrowse()
{
   DoBrowseHelper(this);
}

void wxDirBrowseButton::DoBrowseHelper(wxTextBrowseButton *browseBtn)
{
   wxDirDialog dialog(browseBtn,
                      _("Please choose a directory"),
                      browseBtn->GetText());

   if ( dialog.ShowModal() == wxID_OK )
   {
      browseBtn->SetText(dialog.GetPath());
   }
}

// ----------------------------------------------------------------------------
// wxFileOrDirBrowseButton
// ----------------------------------------------------------------------------

void wxFileOrDirBrowseButton::DoBrowse()
{
   if ( m_browseForFile )
   {
      wxFileBrowseButton::DoBrowse();
   }
   else
   {
      // unfortuanately we don't derive from wxDirBrowseButton and so we can't
      // write wxDirBrowseButton::DoBrowse()...
      wxDirBrowseButton::DoBrowseHelper(this);
   }
}

void wxFileOrDirBrowseButton::UpdateTooltip()
{
   wxString msg;
   msg.Printf(_("Browse for a %s"), IsBrowsingForFiles() ? _("file")
                                                         : _("directory"));
   SetToolTip(msg);
}

// ----------------------------------------------------------------------------
// wxFolderBrowseButton
// ----------------------------------------------------------------------------

wxFolderBrowseButton::wxFolderBrowseButton(wxTextCtrl *text,
                                           wxWindow *parent,
                                           MFolder *folder)
                    : wxTextBrowseButton(text, parent, _("Browse for folder"))
{
   m_folder = folder;

   SafeIncRef(m_folder);
}

void wxFolderBrowseButton::DoBrowse()
{
   MFolder *folder = MDialog_FolderChoose(this, m_folder);

   if ( folder && folder != m_folder )
   {
      SafeDecRef(m_folder);

      m_folder = folder;
      SetText(m_folder->GetFullName());
   }
   //else: nothing changed or user cancelled the dialog
}

MFolder *wxFolderBrowseButton::GetFolder() const
{
   SafeIncRef(m_folder);

   return m_folder;
}

wxFolderBrowseButton::~wxFolderBrowseButton()
{
   SafeDecRef(m_folder);
}

// ----------------------------------------------------------------------------
// wxColorBrowseButton
// ----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxColorBrowseButton, wxButton)

wxColorBrowseButton::wxColorBrowseButton(wxTextCtrl *text, wxWindow *parent)
                   : wxTextBrowseButton(text, parent, _("Choose colour"))
{
   m_hasText = TRUE;

   m_evtHandlerText = new wxColorTextEvtHandler(this);
   GetTextCtrl()->PushEventHandler(m_evtHandlerText);
}

wxColorBrowseButton::~wxColorBrowseButton()
{
   // the order of control deletion is undetermined, so handle both cases
   if ( m_hasText )
   {
      // we're deleted before the text control
      GetTextCtrl()->PopEventHandler(TRUE /* delete it */);
   }
   else
   {
      // the text control had been already deleted
      delete m_evtHandlerText;
   }
}

void wxColorBrowseButton::OnTextDelete()
{
   m_hasText = FALSE;
}

void wxColorBrowseButton::DoBrowse()
{
   wxColourData colData;

   wxString colName = GetText();
   if ( !colName.empty() )
   {
      (void)ParseColourString(colName, &m_color);
      colData.SetColour(m_color);
   }

   wxColourDialog dialog(this, &colData);

   if ( dialog.ShowModal() == wxID_OK )
   {
      colData = dialog.GetColourData();
      m_color = colData.GetColour();

      SetText(GetColourName(m_color));

      UpdateColor();
   }
}

void wxColorBrowseButton::SetValue(const wxString& text)
{
   if ( !text.empty() )
   {
      (void)ParseColourString(text, &m_color);
   }
   else // no valid colour, use default one
   {
      m_color = GetParent()->GetBackgroundColour();
   }

   UpdateColor();

   SetText(text);
}

void wxColorBrowseButton::UpdateColorFromText()
{
   if ( ParseColourString(GetText(), &m_color) )
   {
      UpdateColor();
   }
}

void wxColorBrowseButton::UpdateColor()
{
   if ( !m_color.Ok() )
      return;

   // some combinations of the fg/bg colours may be unreadable, so change the
   // fg colour to be visible
   wxColour colFg;
   if ( m_color.Red() < 127 && m_color.Blue() < 127 && m_color.Green() < 127 )
   {
      colFg = *wxWHITE;
   }
   else
   {
      colFg = *wxBLACK;
   }

   SetForegroundColour(colFg);
   SetBackgroundColour(m_color);
}

// ----------------------------------------------------------------------------
// wxIconBrowseButton
// ----------------------------------------------------------------------------

wxBitmap wxIconBrowseButton::GetBitmap(size_t nIcon) const
{
   wxIconManager *iconManager = mApplication->GetIconManager();

   return iconManager->GetBitmap(m_iconNames[nIcon]);
}

void wxIconBrowseButton::SetIcon(size_t nIcon)
{
   CHECK_RET( nIcon < m_iconNames.GetCount(), "invalid icon index" );

   if ( m_nIcon == (int)nIcon )
      return;

   m_nIcon = nIcon;
   if ( m_staticBitmap )
   {
      wxBitmap bmp = GetBitmap(m_nIcon);

      // scale the icon if necessary
      int w1, h1; // size of the icon on the screen
      m_staticBitmap->GetSize(&w1, &h1);

      // size of the icon
      int w2 = bmp.GetWidth(),
          h2 = bmp.GetHeight();

      if ( (w1 != w2) || (h1 != h2) )
      {
         bmp = wxImage(bmp).Rescale(w1, h1).ConvertToBitmap();
      }
      //else: the size is already correct

      m_staticBitmap->SetBitmap(bmp);
   }
}

void wxIconBrowseButton::DoBrowse()
{
   size_t n, nIcons = m_iconNames.GetCount();

   wxIconArray icons;
   icons.Alloc(nIcons);

   for ( n = 0; n < nIcons; n++ )
   {
      wxBitmap bmp = GetBitmap(n);

      // save some typing
      static const int size = wxIconView::ms_iconSize;
      if ( bmp.GetWidth() != size || bmp.GetHeight() != size )
      {
         // must resize the icon
         wxImage image(bmp);
         image.Rescale(size, size);
         bmp = image.ConvertToBitmap();
      }

      icons.Add(new wxBitmap(bmp));
   }

   wxIconSelectionDialog dlg(this, _("Choose icon"), icons, m_nIcon);

   if ( dlg.ShowModal() == wxID_OK )
   {
      size_t icon = dlg.GetSelection();
      if ( (int)icon != m_nIcon )
      {
         SetIcon(icon);

         OnIconChange();
      }
   }

   for ( n = 0; n < nIcons; n++ )
   {
      delete icons[n];
   }
}

// ----------------------------------------------------------------------------
// wxFolderIconBrowseButton
// ----------------------------------------------------------------------------

wxFolderIconBrowseButton::wxFolderIconBrowseButton(wxWindow *parent,
                                                   const wxString& tooltip)
                        : wxIconBrowseButton(parent, tooltip)
{
   size_t nIcons = GetNumberOfFolderIcons();
   wxArrayString icons;
   for ( size_t n = 0; n < nIcons; n++ )
   {
      icons.Add(GetFolderIconName(n));
   }

   SetIcons(icons);
}

// ----------------------------------------------------------------------------
// wxIconView - the canvas which shows all icons
// ----------------------------------------------------------------------------

#ifdef __WXGTK__
const int wxIconView::ms_iconSize = 16;
#else
const int wxIconView::ms_iconSize = 32;
#endif

wxIconView::wxIconView(wxDialog *parent,
                       const wxIconArray& icons,
                       int selection)
          : wxListCtrl(parent, -1, wxDefaultPosition, wxDefaultSize,
                       wxLC_ICON |
                       wxLC_SINGLE_SEL |
                       wxLC_AUTOARRANGE |
                       wxLC_ALIGN_LEFT)
{
   size_t n, count = icons.GetCount();
   wxImageList *imageList = new wxImageList(ms_iconSize, ms_iconSize,
                                            TRUE, count);
   for ( n = 0; n < count; n++ )
   {
      imageList->Add(*icons[n]);
   }

   SetImageList(imageList, wxIMAGE_LIST_NORMAL);

   for ( n = 0; n < count; n++ )
   {
      InsertItem(n, n);
   }

   if ( selection != -1 )
   {
      SetItemState(selection, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
   }
}

// ----------------------------------------------------------------------------
// wxIconSelectionDialog - dialog which allows the user to select an icon
// ----------------------------------------------------------------------------

wxIconSelectionDialog::wxIconSelectionDialog(wxWindow *parent,
                                             const wxString& title,
                                             const wxIconArray& icons,
                                             int selection)
                     : wxManuallyLaidOutDialog(parent, title, "IconSelect")
{
   // layout the controls
   // -------------------
   wxLayoutConstraints *c;

   // Ok and Cancel buttons and a static box around everything else
   wxStaticBox *box = CreateStdButtonsAndBox(_("&Current icon"));

   wxIconView *iconView = new wxIconView(this, icons, selection);

   c = new wxLayoutConstraints;
   c->centreY.SameAs(box, wxCentreY);
   c->left.SameAs(box, wxLeft, 2*LAYOUT_X_MARGIN);
   c->right.SameAs(box, wxRight, 2*LAYOUT_X_MARGIN);
   c->height.Absolute(wxIconView::ms_iconSize + 9*LAYOUT_X_MARGIN);
   iconView->SetConstraints(c);

   SetDefaultSize(8*wBtn, wxIconView::ms_iconSize + 6*hBtn);
}

void wxIconSelectionDialog::OnIconSelected(wxListEvent& event)
{
   m_selection = event.GetIndex();
}

void wxIconSelectionDialog::OnUpdateUI(wxUpdateUIEvent& event)
{
   event.Enable(m_selection != -1);
}

