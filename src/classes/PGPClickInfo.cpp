///////////////////////////////////////////////////////////////////////////////
// Project:     M - cross platform e-mail GUI client
// File name:   PGPClickInfo.cpp: implementation of ClickablePGPInfo
// Purpose:     ClickablePGPInfo is for "(inter)active" PGP objects which can
//              appear in MessageView
// Author:      Vadim Zeitlin
// Modified by:
// Created:     13.12.02 (extracted from viewfilt/PGP.cpp)
// CVS-ID:      $Id$
// Copyright:   (c) 1997-2002 Mahogany team
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

   #include <wx/menu.h>
#endif //USE_PCH

#include "MessageView.h"

#include "modules/MCrypt.h"

#include "PGPClickInfo.h"

// ----------------------------------------------------------------------------
// PGPMenu: used by ClickablePGPInfo
// ----------------------------------------------------------------------------

class PGPMenu : public wxMenu
{
public:
   PGPMenu(const ClickablePGPInfo *pgpInfo, const wxChar *title);

   void OnCommandEvent(wxCommandEvent &event);

private:
   // menu command ids
   enum
   {
      RAW_TEXT,
      DETAILS
   };

   const ClickablePGPInfo * const m_pgpInfo;

   DECLARE_EVENT_TABLE()
};

// ============================================================================
// PGPMenu implementation
// ============================================================================

BEGIN_EVENT_TABLE(PGPMenu, wxMenu)
   EVT_MENU(-1, PGPMenu::OnCommandEvent)
END_EVENT_TABLE()

PGPMenu::PGPMenu(const ClickablePGPInfo *pgpInfo, const wxChar *title)
       : wxMenu(wxString::Format(_("PGP: %s"), title)),
         m_pgpInfo(pgpInfo)
{
   // create the menu items
   Append(RAW_TEXT, _("Show ra&w text..."));
   AppendSeparator();
   Append(DETAILS, _("&Details..."));
}

void
PGPMenu::OnCommandEvent(wxCommandEvent &event)
{
   switch ( event.GetId() )
   {
      case DETAILS:
         m_pgpInfo->ShowDetails();
         break;

      case RAW_TEXT:
         m_pgpInfo->ShowRawText();
         break;

      default:
         FAIL_MSG( _T("unexpected command in PGPMenu") );
   }
}

// ============================================================================
// ClickablePGPInfo implementation
// ============================================================================

// ----------------------------------------------------------------------------
// ClickablePGPInfo ctor/dtor
// ----------------------------------------------------------------------------

ClickablePGPInfo::ClickablePGPInfo(MessageView *msgView,
                                   const String& label,
                                   const String& bmpName,
                                   const wxColour& colour)
                : ClickableInfo(msgView),
                  m_label(label),
                  m_bmpName(bmpName),
                  m_colour(colour)
{
   m_log = NULL;
}

ClickablePGPInfo::~ClickablePGPInfo()
{
   delete m_log;
}

// ----------------------------------------------------------------------------
// ClickablePGPInfo accessors
// ----------------------------------------------------------------------------

wxBitmap
ClickablePGPInfo::GetBitmap() const
{
   return mApplication->GetIconManager()->GetBitmap(m_bmpName);
}

wxColour
ClickablePGPInfo::GetColour() const
{
   return m_colour;
}

String
ClickablePGPInfo::GetLabel() const
{
   return m_label;
}

// ----------------------------------------------------------------------------
// ClickablePGPInfo operations
// ----------------------------------------------------------------------------

void
ClickablePGPInfo::OnLeftClick(const wxPoint&) const
{
   ShowDetails();
}

void
ClickablePGPInfo::OnRightClick(const wxPoint& pt) const
{
   PGPMenu menu(this, m_label);

   m_msgView->GetWindow()->PopupMenu(&menu, pt);
}

void
ClickablePGPInfo::OnDoubleClick(const wxPoint&) const
{
   ShowDetails();
}

void
ClickablePGPInfo::ShowDetails() const
{
   // TODO: something better
   if ( m_log )
   {
      String allText;
      allText.reserve(4096);

      const size_t count = m_log->GetMessageCount();
      for ( size_t n = 0; n < count; n++ )
      {
         allText << m_log->GetMessage(n) << _T('\n');
      }

      MDialog_ShowText(m_msgView->GetWindow(),
                       _("PGP Information"),
                       allText,
                       _T("PGPDetails"));
   }
   else // no log??
   {
      wxLogMessage(_("Sorry, no PGP details available."));
   }
}

void
ClickablePGPInfo::ShowRawText() const
{
   MDialog_ShowText(m_msgView->GetWindow(),
                    _("PGP Message Raw Text"),
                    m_textRaw,
                    _T("PGPRawText"));
}


