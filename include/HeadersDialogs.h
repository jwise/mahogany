///////////////////////////////////////////////////////////////////////////////
// Project:     M
// File name:   HeadersDialogs.h - dialogs to configure headers
// Purpose:     utility dialogs used from various places
// Author:      Vadim Zeitlin
// Modified by:
// Created:     14.04.99
// CVS-ID:      $Id$
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     M license
///////////////////////////////////////////////////////////////////////////////

#ifndef _HEADERDIALOGS_H
#define _HEADERDIALOGS_H

class wxWindow;

/** custom header type: a custom header may be added only to mail messages,
    only to news messages or to both types. Also, the header with same name can
    be used with different values for mail and news messages.
 */
enum CustomHeaderType
{
   CustomHeader_News,
   CustomHeader_Mail,
   CustomHeader_Both,
   CustomHeader_Max,
   CustomHeader_Invalid = CustomHeader_Max
};

/** Show the dialog to configure outgoing headers for given profile

    @return true if Ok button was pressed, false otherwise
 */
extern bool ConfigureComposeHeaders(ProfileBase *profile, wxWindow *parent);

/** Show the dialog to configure message view headers for given profile

    @return true if Ok button was pressed, false otherwise
 */
extern bool ConfigureMsgViewHeaders(ProfileBase *profile, wxWindow *parent);

/** Show the dialog to allow the user change a value for a custom header,
    returns the header name and value in output variables.

    Also remembers if the user wants this header to always have this value - in
    this case, the header name/value are remembered in the "CustomHeaders"
    subgroup of the profile object.

    The dialog behaves in slightly different way if the last parameter (type)
    has the value CustomHeader_Invalid. In this case, it will always save the
    header name and value in the profile and will also let the user choose
    himself the CustomHeaderType (Mail/News/Both).

    @param profile to use to store the custom header (if user decides so)
    @param parent the parent window
    @param headerName [out] the name of the header
    @param headerValue [out] the value of the header
    @param storedInProfile [out] TRUE if the entry was remembered
    @parent type [in] for which messages should we remember this header?
    @return true if Ok button was pressed, false otherwise
 */
extern bool ConfigureCustomHeader(ProfileBase *profile, wxWindow *parent,
                                  String *headerName, String *headerValue,
                                  bool *storedInProfile = (bool *)NULL,
                                  CustomHeaderType type = CustomHeader_Both);

/** Show the dialog allowing the user to change all custom headers which will
    be used for outgoing message. Unlike ConfigureCustomHeader, this function
    always saves changes in the profile.

    @return true if Ok button was pressed, false otherwise
 */
extern bool ConfigureCustomHeaders(ProfileBase *profile, wxWindow *parent);

#endif // _HEADERDIALOGS_H
