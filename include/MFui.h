///////////////////////////////////////////////////////////////////////////////
// Project:     M - cross platform e-mail GUI client
// File name:   MFui.h
// Purpose:     various MailFolder and UI (simultaneously) related stuff
// Author:      Vadim Zeitlin
// Modified by:
// Created:     04.07.02 (extracted from MailFolder.h)
// CVS-ID:      $Id$
// Copyright:   (c) 2002 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     M license
///////////////////////////////////////////////////////////////////////////////

#ifndef _MFUI_H_
#define _MFUI_H_

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

/// how to show the message size in the viewer?
enum MessageSizeShow
{
   /// choose lines/bytes/kbytes/mbytes automatically
   MessageSize_Automatic,

   /// always show bytes/kbytes/mbytes
   MessageSize_AutoBytes,

   /// always show size in bytes
   MessageSize_Bytes,

   /// always show size in Kb
   MessageSize_KBytes,

   /// always show size in Mb
   MessageSize_MBytes,

   /// end of enum marker
   MessageSize_Max
};

/// SizeToString() flags
enum
{
   /// terse size string by default
   SizeToString_Default,

   /// verbose size string
   SizeToString_Verbose
};

// ----------------------------------------------------------------------------
// functions
// ----------------------------------------------------------------------------

/**
 @name Conversion to textual representation helpers
*/
//@{

/** Utility function to get a textual representation of a message
   status.

   @param message flags
   @return string representation
*/
extern String ConvertMessageStatusToString(int status);

/**
 Returns the string containing the size as it should be shown to the user.

 @param sizeBytes the size of the message in bytes
 @param sizeLines the size of message in lines (only if text)
 @param show how should we show the size?
 @param verbose returns verbose string if equal to SizeToString_Verbose
 @return string containing the text for display
*/
extern String SizeToString(unsigned long sizeBytes,
                           unsigned long sizeLines = 0,
                           MessageSizeShow show = MessageSize_Automatic,
                           int verbose = SizeToString_Default);

//@}

#endif // _MFUI_H_

