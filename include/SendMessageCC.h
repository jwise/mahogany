//////////////////////////////////////////////////////////////////////////////
// Project:     M - cross platform e-mail GUI client
// File name:   SendMessageCC.h: declaration of SendMessageCC
// Purpose:     sending/posting of mail messages with c-client lib
// Author:      Karsten Ball�der
// Modified by:
// Created:     1998
// CVS-ID:      $Id$
// Copyright:   (C) 1999-2001 by M-Team
// Licence:     M license
///////////////////////////////////////////////////////////////////////////////

#ifndef SENDMESSAGECC_H
#define SENDMESSAGECC_H

#ifdef   __GNUG__
#   pragma interface "SendMessageCC.h"
#endif

#include "FolderType.h"

class Profile;

// ----------------------------------------------------------------------------
// MessageHeadersList: the list of custom (or extra) headers
// ----------------------------------------------------------------------------

struct MessageHeader
{
   MessageHeader(const String& name, const String& value)
      : m_name(name), m_value(value)
   {
   }

   String m_name,
          m_value;
};

M_LIST_OWN(MessageHeadersList, MessageHeader);

// ----------------------------------------------------------------------------
// SendMessageCC: allows to send/post messages using c-client
// ----------------------------------------------------------------------------

class SendMessageCC : public SendMessage
{
public:
   /** Creates an empty object, setting some initial values.
       @param profile optional pointer for a parent profile
       @param protocol which protocol to use for sending
   */
   SendMessageCC(Profile *profile,
                 Protocol protocol = Prot_Default);

   // implement the base class pure virtuals

   // standard headers
   // ----------------

   virtual void SetSubject(const String &subject);

   virtual void SetAddresses(const String &To,
                             const String &CC = "",
                             const String &BCC = "");

   virtual void SetFrom(const String &from,
                        const String &replyaddress = "",
                        const String &sender = "");

   virtual void SetNewsgroups(const String &groups);

   virtual void SetHeaderEncoding(wxFontEncoding enc);

   // custom headers
   // --------------

   virtual void AddHeaderEntry(const String &entry, const String &value);

   virtual void RemoveHeaderEntry(const String& name);

   virtual bool HasHeaderEntry(const String& name) const;

   virtual String GetHeaderEntry(const String &key) const;

   // message body
   // ------------

   virtual void AddPart(MimeType::Primary type,
                        const void *buf, size_t len,
                        const String &subtype = M_EMPTYSTRING,
                        const String &disposition = "INLINE",
                        MessageParameterList const *dlist = NULL,
                        MessageParameterList const *plist = NULL,
                        wxFontEncoding enc = wxFONTENCODING_SYSTEM);

   virtual void WriteToString(String  &output);

   virtual void WriteToFile(const String &filename, bool append = true);

   virtual void WriteToFolder(const String &foldername);

   virtual bool SendOrQueue(bool sendAlways = false);

   /// destructor
   virtual ~SendMessageCC();

   enum Mode { Mode_SMTP, Mode_NNTP };

protected:
   /** Sends the message.
       @return true on success
   */
   bool Send(void);

   /// set sender address fields
   void SetupFromAddresses(void);

   /** Builds the message, i.e. prepare to send it.
    @param forStorage if this is TRUE, store some extra information
    that is not supposed to be send, like BCC header. */
   void Build(bool forStorage = FALSE);

   /// translate the (wxWin) encoding to (MIME) charset
   String EncodingToCharset(wxFontEncoding enc);

   /// encode the string using m_encHeaders encoding
   String EncodeHeaderString(const String& header, bool isAddressField = false);

   /// encode the address field using m_encHeaders
   void EncodeAddress(struct mail_address *adr);

   /// encode all entries in the list of addresses
   void EncodeAddressList(struct mail_address *adr);

   /// write the message using the specified writer function
   bool WriteMessage(soutr_t writer, void *where);

   /// Parses string for folder aliases, removes them and stores them in m_FccList.
   void ExtractFccFolders(String &addresses);

   /// sets one address field in the envelope
   void SetAddressField(ADDRESS **pAdr, const String& address);

   /// filters out erroneous addresses
   void CheckAddressFieldForErrors(ADDRESS *adr);

   /// get the iterator pointing to the given header or m_extraHeaders.end()
   MessageHeadersList::iterator FindHeaderEntry(const String& name) const;

private:
   /** @name Description of the message being sent */
   //@{

   /// the envelope
   ENVELOPE *m_Envelope;

   /// the body
   BODY     *m_Body;

   /// the next and last body parts
   PART     *m_NextPart,
            *m_LastPart;

   //@}

   /// the profile containing our settings
   Profile *m_profile;

   /// server name to use
   String m_ServerHost;

   /// for servers requiring authentication
   String m_UserName,
          m_Password;

#ifdef USE_SSL
   /// use SSL ?
   bool m_UseSSLforSMTP, m_UseSSLforNNTP;

   /// check validity of ssl-cert? <-> self-signed certs
   bool m_UseSSLUnsignedforSMTP,
        m_UseSSLUnsignedforNNTP;
#endif // USE_SSL

   /** @name Address fields
    */
   //@{

   /// the full From: address
   String m_From;

   /// the full value of Reply-To: header (may be empty)
   String m_ReplyTo;

   /// the full value of Sender: header (may be empty)
   String m_Sender;

   /// the saved value of Bcc: set by call to SetAddresses()
   String m_Bcc;
   //@}

   /// if not empty, name of xface file
   String m_XFaceFile;

   /// Outgoing folder name or empty
   String m_OutboxName;

   /// "Sent" folder name or empty
   String m_SentMailName;

   /// Default charset
   String m_CharSet;

   /// default hostname
   String m_DefaultHost;

#ifdef OS_UNIX
   /// command for Sendmail if needed
   String m_SendmailCmd;
#endif // OS_UNIX

   /// the header encoding (wxFONTENCODING_SYSTEM if none)
   wxFontEncoding m_encHeaders;

   /// 2nd stage constructor, see constructor
   void Create(Protocol protocol, Profile *iprof);

   /// Protocol used for sending
   Protocol m_Protocol;

   /** @name variables managed by Build() */
   //@{

   /**
     m_headerNames and m_headerValues are the "final" arrays of headers, i.e.
     they are passed to c-client when we send the message while the extra
     headers are used to construct them.
    */

   /// names of header lines
   const char **m_headerNames;
   /// values of header lines
   const char **m_headerValues;

   /// extra headers to be added before sending
   MessageHeadersList m_extraHeaders;

   /// a list of folders to save copies in
   kbStringList m_FccList;

   //@}

   // give it access to m_headerNames nad m_headerValues
   friend class Rfc822OutputRedirector;
};


#endif
