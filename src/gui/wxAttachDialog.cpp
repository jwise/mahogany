///////////////////////////////////////////////////////////////////////////////
// Project:     M - cross platform e-mail GUI client
// File name:   gui/wxAttachDialog.cpp
// Purpose:     implements the attachment properties dialog
// Author:      Vadim Zeitlin
// Modified by:
// Created:     02.09.02
// CVS-ID:      $Id$
// Copyright:   (c) 2002 Vadim Zeitlin
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
   #include "Mcommon.h"

   #include <wx/layout.h>

   #include <wx/textctrl.h>
   #include <wx/radiobox.h>
   #include <wx/checkbox.h>
   #include <wx/statbox.h>
#endif

#include "gui/wxDialogLayout.h"

#include "AttachDialog.h"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// and the disposition strings
static const char *DISPOSITIONS[AttachmentProperties::Disposition_Max] =
{
   "INLINE",
   "ATTACHMENT",
};

// ----------------------------------------------------------------------------
// wxAttachmentDialog: the dialog class
// ----------------------------------------------------------------------------

class wxAttachmentDialog : public wxManuallyLaidOutDialog
{
public:
   wxAttachmentDialog(wxWindow *parent,
                      AttachmentProperties *properties,
                      bool *allowDisable);

   virtual bool TransferDataToWindow();
   virtual bool TransferDataFromWindow();

   bool HasChanges() const { return m_isDirty; }

private:
   // the data we edit
   AttachmentProperties& m_props;

   // the "changed" flag
   bool m_isDirty;

   // pointer to "don't show again" flag, may be NULL
   bool *m_allowDisable;

   // the GUI controls
   wxTextCtrl *m_txtFilename,
              *m_txtName,
              *m_txtMime;

   wxRadioBox *m_radioDisposition;

   wxCheckBox *m_chkDontShowAgain;
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// AttachmentProperties
// ----------------------------------------------------------------------------

AttachmentProperties::AttachmentProperties()
{
   disposition = Disposition_Max;
}

String AttachmentProperties::GetDisposition() const
{
   return DISPOSITIONS[disposition];
}

void AttachmentProperties::SetDisposition(const String& disp)
{
   for ( size_t n = 0; n < WXSIZEOF(DISPOSITIONS); n++ )
   {
      if ( disp.CmpNoCase(DISPOSITIONS[n]) == 0 )
      {
         disposition = (Disposition)n;
         return;
      }
   }

   FAIL_MSG( "unknown disposition string" );
}

// ----------------------------------------------------------------------------
// wxAttachmentDialog construction
// ----------------------------------------------------------------------------

wxAttachmentDialog::wxAttachmentDialog(wxWindow *parent,
                                       AttachmentProperties *properties,
                                       bool *allowDisable)
                  : wxManuallyLaidOutDialog(parent,
                                            _("Attachment properties"),
                                            "AttachmentDialog"),
                    m_props(*properties)
{
   // init the data
   // -------------
   
   m_isDirty = false;
   m_allowDisable = allowDisable;

   // create the controls
   // -------------------

   // first the buttons and the optional checkbox
   wxStaticBox *box = CreateStdButtonsAndBox("");

   if ( m_allowDisable )
   {
      m_chkDontShowAgain = new wxCheckBox(this, -1, _("&Don't show again"));

      wxLayoutConstraints *c = new wxLayoutConstraints;
      c->left.SameAs(box, wxLeft, LAYOUT_X_MARGIN);
      c->width.AsIs();
      c->height.AsIs();
      c->centreY.SameAs(FindWindow(wxID_OK), wxCentreY);

      m_chkDontShowAgain->SetConstraints(c);
   }

   // next all the "real" controls

   // enum should be in sync with labels array
   enum
   {
      Label_Filename,
      Label_Name,
      Label_Disposition,
      Label_MIME,
      Label_Max
   };

   wxArrayString labels;
   labels.Add(_("Local &file name:"));
   labels.Add(_("&Name in the message:"));
   labels.Add(_("&Disposition"));
   labels.Add(_("&MIME type:"));

   ASSERT_MSG( labels.GetCount() == Label_Max, "labels not in sync" );

   long widthMax = GetMaxLabelWidth(labels, this);

   static const wxCoord MARGIN = 2*LAYOUT_X_MARGIN;

   m_txtFilename = CreateFileEntry(this, labels[Label_Filename],
                                   widthMax, NULL, MARGIN);
   m_txtName = CreateTextWithLabel(this, labels[Label_Name],
                                   widthMax, m_txtFilename, MARGIN);
   m_radioDisposition = CreateRadioBox(this,
                                       labels[Label_Disposition] +
                                       ":&inline:&attachment",
                                       widthMax, m_txtName, MARGIN);
   m_txtMime = CreateTextWithLabel(this, labels[Label_MIME],
                                   widthMax, m_radioDisposition, MARGIN);


   SetDefaultSize(6*wBtn, 8*hBtn);
}

// ----------------------------------------------------------------------------
// wxAttachmentDialog data transfer
// ----------------------------------------------------------------------------

bool wxAttachmentDialog::TransferDataToWindow()
{
   CHECK( m_props.disposition != AttachmentProperties::Disposition_Max,
          false, "disposition field hasn't been initialized" );

   m_txtFilename->SetValue(m_props.filename);
   m_txtName->SetValue(m_props.name);
   m_radioDisposition->SetSelection(m_props.disposition);
   m_txtMime->SetValue(m_props.mimetype.GetFull());

   if ( m_allowDisable )
      m_chkDontShowAgain->SetValue(*m_allowDisable);

   return true;
}

bool wxAttachmentDialog::TransferDataFromWindow()
{
   AttachmentProperties propsNew;

   const String strMime = m_txtMime->GetValue();
   propsNew.mimetype = strMime;
   if ( !propsNew.mimetype.IsOk() )
   {
      wxLogError(_("MIME type \"%s\" is illegal."), strMime.c_str());
      return false;
   }

   propsNew.filename = m_txtFilename->GetValue();
   propsNew.name = m_txtName->GetValue();
   propsNew.disposition = (AttachmentProperties::Disposition)
                              m_radioDisposition->GetSelection();

   if ( propsNew != m_props )
   {
      m_isDirty = true;
      m_props = propsNew;
   }

   if ( m_allowDisable )
      *m_allowDisable = m_chkDontShowAgain->GetValue();

   return true;
}

// ----------------------------------------------------------------------------
// public API
// ----------------------------------------------------------------------------

bool
ShowAttachmentDialog(wxWindow *parent,
                     AttachmentProperties *properties,
                     bool *allowDisable)
{
   CHECK( properties, false, "NULL properties in EditAttachmentProperties" );

   wxAttachmentDialog dlg(parent, properties, allowDisable);

   return dlg.ShowModal() == wxID_OK && dlg.HasChanges();
}


