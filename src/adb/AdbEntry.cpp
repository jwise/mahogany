///////////////////////////////////////////////////////////////////////////////
// Project:     M - cross platform e-mail GUI client
// File name:   adb/AdbEntry.cpp - AdbEntryStoredInMemory implementation
// Purpose:     implementation of AdbEntry functions common to all derived
//              classes which fully load their data in memory
// Author:      Vadim Zeitlin
// Modified by:
// Created:     05.02.99
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

// M
#include "Mpch.h"

#ifndef  USE_PCH
#  include "Mcommon.h"
#endif //USE_PCH

#include "adb/AdbEntry.h"

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// AdbEntryCommon class
// ----------------------------------------------------------------------------

void
AdbEntryCommon::GetField(size_t n, wxString *pstr) const
{
   if(n != AdbField_FirstName
      && n != AdbField_FamilyName)
      GetFieldInternal(n, pstr);
   else
   {
      GetFieldInternal(n, pstr);
      if(pstr->Length()) // not empty
         return; // nothing to do
      GetFieldInternal(AdbField_FullName, pstr);
      if(! pstr->Length()) // empty, try nick name
         GetFieldInternal(AdbField_NickName, pstr);
      if(! pstr->Length()) // empty, nothing we can do about
         return;
      // not empty:
      if(n == AdbField_FirstName)
         *pstr = pstr->BeforeLast(' ');
      else if(n == AdbField_FamilyName)
         *pstr = pstr->AfterLast(' ');
   }   
}

// ----------------------------------------------------------------------------
// AdbEntryStoredInMemory class
// ----------------------------------------------------------------------------

// we store only the fields which were non-empty, so check the index
void AdbEntryStoredInMemory::GetFieldInternal(size_t n, String *pstr) const
{
  if ( n < m_astrFields.Count() )
    *pstr = m_astrFields[n];
  else
    pstr->Empty();
}

// the problem here is that we may have only several first strings in the
// m_astrFields array, so we need to add some before setting n-th field
void AdbEntryStoredInMemory::SetField(size_t n, const wxString& strValue)
{
  size_t nCur = m_astrFields.Count();
  // add some empty fields if needed
  for ( int nAdd = 0; nAdd < (int)(n - nCur + 1); nAdd++ )
    m_astrFields.Add(wxGetEmptyString());

  if ( m_astrFields[n] != strValue ) {
    m_astrFields[n] = strValue;
    m_bDirty = TRUE;
  }
}

void AdbEntryStoredInMemory::ClearExtraEMails()
{
  if ( !m_astrEmails.IsEmpty() ) {
    m_astrEmails.Empty();
    m_bDirty = TRUE;
  }
  //else: don't set dirty flag if it didn't change anything
}

bool AdbEntryStoredInMemory::Matches(const char *szWhat, int where, int how)
{
  wxString strWhat;

  // substring lookup looks for a part of the string, "starts with" means
  // what is says, otherwise the entire string should be matched by the pattern
  if ( how & AdbLookup_Substring )
    strWhat << '*' << szWhat << '*';
  else if ( how & AdbLookup_StartsWith )
    strWhat << szWhat << '*';
  else
    strWhat = szWhat;

  // if the search is not case sensitive, transform everything to lower case
  if ( (how & AdbLookup_CaseSensitive) == 0 )
    strWhat.MakeLower();

  wxString strField;

  #define CHECK_MATCH(field)                                        \
    if ( where & AdbLookup_##field ) {                              \
      strField = m_astrFields[AdbField_##field];                    \
      if ( (how & AdbLookup_CaseSensitive) == 0 )                   \
        strField.MakeLower();                                       \
      if ( strField.Matches(strWhat) )                              \
        return TRUE;                                                \
    }

  CHECK_MATCH(NickName);
  CHECK_MATCH(FullName);
  CHECK_MATCH(Organization);
  CHECK_MATCH(HomePage);

  // special case: we have to look in _all_ e-mail addresses
  // NB: this search is always case insensitive, because e-mail addresses are
  if ( where & AdbLookup_EMail ) {
    if ( m_astrFields.Count() > AdbField_EMail ) {
      strField = m_astrFields[AdbField_EMail];
      strField.MakeLower();
      if ( strField.Matches(strWhat) )
        return TRUE;
    }

    // look in additional email addresses too
    size_t nCount = m_astrEmails.Count();
    for ( size_t n = 0; n < nCount; n++ ) {
      strField = m_astrEmails[n];
      strField.MakeLower();
      if ( strField.Matches(strWhat) )
        return TRUE;
    }
  }

  return FALSE;
}
