///////////////////////////////////////////////////////////////////////////////
// Project:     M - cross platform e-mail GUI client
// File name:   Trailer.cpp: implementation of trailer viewer filter
// Purpose:     Trailer handles the tails appended to the end of the message
// Author:      Vadim Zeitlin
// Modified by:
// Created:     30.11.02
// CVS-ID:      $Id$
// Copyright:   (c) 2002 Vadim Zeitlin <vadim@wxwindows.org>
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
#endif //USE_PCH

#include "MTextStyle.h"

#include "ViewFilter.h"

// ----------------------------------------------------------------------------
// TrailerFilter declaration
// ----------------------------------------------------------------------------

class TrailerFilter : public ViewFilter
{
public:
   TrailerFilter(MessageView *msgView, ViewFilter *next, bool enable)
      : ViewFilter(msgView, next, enable) { }

protected:
   virtual void DoProcess(String& text,
                          MessageViewer *viewer,
                          MTextStyle& style);
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// TrailerFilter
// ----------------------------------------------------------------------------

// this filter has a high priority as it should be normally applied before
// all the filters working on the message body
IMPLEMENT_VIEWER_FILTER(TrailerFilter,
                        ViewFilter::Priority_High + 20,
                        true,      // initially enabled
                        _("Trailer"),
                        "(c) 2002 Vadim Zeitlin <vadim@wxwindows.org>");

void
TrailerFilter::DoProcess(String& text,
                         MessageViewer *viewer,
                         MTextStyle& style)
{
   // we try to detect a line formed by only dashes or underscores not too far
   // away from the message end

   // we assume the string is non empty below
   if ( text.empty() )
      return;

   const char *start = text.c_str();
   const char *pc = start + text.length() - 1;

   // while we're not too far from end
   for ( size_t numLinesFromEnd = 0; numLinesFromEnd < 10; numLinesFromEnd++ )
   {
      // does this seem to be a separator line?
      char chDel = *pc;
      if ( chDel != '-' && chDel != '_' )
      {
         // no
         chDel = '\0';
      }

      // look for the start of this line:
      //
      // (a) checking that it consists solely of delimiter characters
      //     if there is a chance that this is a delimiter line
      while ( chDel != '\0' && *pc != '\n' && pc >= start )
      {
         if ( *pc-- != chDel )
         {
            // it's not a delimiter line, finally
            chDel = '\0';
         }
      }

      // (b) simply (and faster) if it's not a delimiter line anyhow
      while ( *pc != '\n' && pc >= start )
      {
         pc--;
      }

      // did we find a delimiter line?
      if ( chDel )
      {
         // yes, but it may start either at pc or at pc + 1
         if ( *pc == '\n' )
            pc++;

         // remember the tail and cut it off
         String tail = pc;
         text.resize(pc - start);

         // trailers may be embedded, so call ourselves recursively to check
         // for them again
         Process(text, viewer, style);

         // and now show the trailer in special style
         wxColour colOld = style.GetTextColour();
         style.SetTextColour(*wxGREEN);

         m_next->Process(tail, viewer, style);

         style.SetTextColour(colOld);

         // done!
         return;
      }
      //else: no

      if ( pc == start )
      {
         // we came to the very beginning of the message and found nothing
         break;
      }

      // continue going backwards after skipping the new line ("\r\n")
      ASSERT_MSG( *pc == '\n', _T("why did we stop then?") );
      ASSERT_MSG( pc[-1] == '\r', _T("line doesn't end in\"\\r\\n\"?") );

      pc -= 2;
   }

   // nothing found, process the rest normally
   m_next->Process(text, viewer, style);
}

