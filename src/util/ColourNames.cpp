///////////////////////////////////////////////////////////////////////////////
// Project:     M
// File name:   util/ColourNames.cpp
// Purpose:     implementation of functions from ColourNames.h
// Author:      Vadim Zeitlin
// Modified by:
// Created:     14.03.04 (extracted from defunct miscutil.cpp)
// CVS-ID:      $Id$
// Copyright:   (c) 1999-2004 M-Team
// Licence:     M licence
///////////////////////////////////////////////////////////////////////////////

#include "Mpch.h"

#ifndef   USE_PCH
#  include "Mcommon.h"
#  include "Mdefaults.h"

#  include "wx/colour.h"
#  include "wx/gdicmn.h"
#endif // USE_PCH

#include "ColourNames.h"

// ---------------------------------------------------------------------------
// colour to string and vice versa conversions
// ---------------------------------------------------------------------------

static const wxChar *rgbSpecificationString = gettext_noop("RGB(%d, %d, %d)");

bool ParseColourString(const String& name, wxColour* colour)
{
   if ( name.empty() )
      return FALSE;

   wxString customColourString(wxGetTranslation(rgbSpecificationString));

   // first check if it's a RGB specification
   int red, green, blue;
   if ( wxSscanf(name, customColourString, &red, &green, &blue) == 3 )
   {
      // it's a custom colour
      if ( colour )
         colour->Set(red, green, blue);
   }
   else // a colour name
   {
#if wxCHECK_VERSION(2,5,0)
      wxColour col = wxTheColourDatabase->Find(name);
      if ( !col.Ok() )
         return FALSE;

      if ( colour )
         *colour = col;
#else
      wxColour *col = wxTheColourDatabase->FindColour(name);
      if ( !col )
         return FALSE;

      if ( colour )
         *colour = *col;
#endif
   }

   return TRUE;
}

String GetColourName(const wxColour& colour)
{
   wxString colName(wxTheColourDatabase->FindName(colour));
   if ( !colName )
   {
      // no name for this colour
      colName.Printf(wxGetTranslation(rgbSpecificationString),
                     colour.Red(), colour.Green(), colour.Blue());
   }
   else
   {
      // at least under X the string returned is always capitalized,
      // convert it to lower case (it doesn't really matter, but capitals
      // look ugly)
      colName.MakeLower();
   }

   return colName;
}

// get the colour by name and warn the user if it failed
void GetColourByName(wxColour *colour,
                     const String& name,
                     const String& def)
{
   if ( !ParseColourString(name, colour) )
   {
      wxLogError(_("Cannot find a colour named '%s', using default instead.\n"
                   "(please check the settings)"), name.c_str());
      *colour = def;
   }
}

void ReadColour(wxColour *col, Profile *profile, const MOption& opt)
{
   CHECK_RET( profile, _T("NULL profile in ReadColour()") );

   const String value = GetOptionValue(profile, opt).GetTextValue();
   GetColourByName(col, value, GetStringDefault(opt));
}