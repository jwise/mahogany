///////////////////////////////////////////////////////////////////////////////
// Project:     M - cross platform e-mail GUI client
// File name:   gui/wxFiltersDialog.h - filter-related constants/functions
// Purpose:     this file doesn't deal with filters themselves, but with our
//              representation of them (how we store them in profile...)
// Author:      Vadim Zeitlin
// Modified by:
// Created:     25.05.00 (extracted from gui/wxFiltersDialog.cpp)
// CVS-ID:      $Id$
// Copyright:   (c) 2000 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     M license
///////////////////////////////////////////////////////////////////////////////

#ifndef _WXFILTERSDIALOG_H_
#define _WXFILTERSDIALOG_H_

#ifdef __GNUG__
   #pragma interface "wxFiltersDialog.h"
#endif

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

#include "MFilter.h"

#define ORC_Types_Enum   MFDialogTest
#define ORC_Where_Enum   MFDialogTarget
#define OAC_Types_Enum   MFDialogAction
#define ORC_Logical_Enum MFDialogLogical

// ----------------------------------------------------------------------------
// functions
// ----------------------------------------------------------------------------

/// configure all existing filters
extern bool ConfigureAllFilters(wxWindow *parent = NULL);

/// configure the filters to use for the folder
extern bool ConfigureFiltersForFolder(MFolder *folder, wxWindow *parent = NULL);

/// a function to edit/create a filter: will modify provided filterDesc
extern bool ConfigureFilter(MFilterDesc *filterDesc,
                            wxWindow *parent = NULL);

/// allows to create a filter from the subject/from values
extern bool CreateQuickFilter(MFolder *folder,
                              const String& from,
                              const String& subject,
                              wxWindow *parent = NULL);

/// write the filter to the given profile
extern
bool SaveSimpleFilter(Profile *profile,
                      const wxString& name,
                      ORC_Types_Enum cond,
                      ORC_Where_Enum condWhere,
                      const wxString& condWhat,
                      OAC_Types_Enum action,
                      const wxString& actionArg,
                      wxString *program = NULL);

#endif // _WXFILTERSDIALOG_H_
