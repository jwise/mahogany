// //// //// //// //// //// //// //// //// //// //// //// //// //// //// //////
// Project:     M - cross platform e-mail GUI client
// File name:   adb/AdbEntry.h - ADB data record interface
// Purpose:
// Author:      Vadim Zeitlin
// Modified by:
// Created:     09.07.98
// CVS-ID:      $Id$
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     M license
// //// //// //// //// //// //// //// //// //// //// //// //// //// //// //////

#ifndef   _ADBENTRY_H
#define   _ADBENTRY_H

#include "MObject.h"    // the base class declaration

#include "miscutil.h"   // GetFullEmailAddress

// forward declaration for classes we use
class AdbElement;
class AdbEntry;
class AdbEntryGroup;

class wxArrayString;

// ============================================================================
// ADB constants
// ============================================================================

// all fields
// NB: these constants must be kept in sync with AdbEntry::ms_aFields
enum AdbField
{
  AdbField_NamePageFirst,                     // NB: must be 0!
  AdbField_NickName = AdbField_NamePageFirst, // NB: and this one also
  AdbField_FullName,                          //
  AdbField_FirstName,                         //
  AdbField_FamilyName,                        //
  AdbField_Prefix,                            // M., Mme, ...
  AdbField_Title,                             //
  AdbField_Organization,                      //
  AdbField_Birthday,                          // date
  AdbField_Comments,                          // arbitrary (multi line) text
  AdbField_NamePageLast,                      //

  AdbField_EMailPageFirst = AdbField_NamePageLast,
  AdbField_EMail = AdbField_EMailPageFirst,   // main e-mail address
  AdbField_HomePage,                          // WWW
  AdbField_ICQ,                               // ICQ number
  AdbField_PrefersHTML,                       // if not, send text only
  AdbField_OtherEMails,                       // additional e-mail addresses
  AdbField_EMailPageLast,

  // 2 copies of address fields: for home (_H_) and for the office (_O_)
  AdbField_H_AddrPageFirst = AdbField_EMailPageLast,
  AdbField_H_StreetNo = AdbField_H_AddrPageFirst,
  AdbField_H_Street,
  AdbField_H_Locality,
  AdbField_H_City,
  AdbField_H_Postcode,
  AdbField_H_Country,
  AdbField_H_POBox,
  AdbField_H_Phone,
  AdbField_H_Fax,
  AdbField_H_AddrPageLast,

  AdbField_O_AddrPageFirst = AdbField_H_AddrPageLast,
  AdbField_O_StreetNo = AdbField_O_AddrPageFirst,
  AdbField_O_Street,
  AdbField_O_Locality,
  AdbField_O_City,
  AdbField_O_Postcode,
  AdbField_O_Country,
  AdbField_O_POBox,
  AdbField_O_Phone,
  AdbField_O_Fax,
  AdbField_O_AddrPageLast,

  AdbField_Max = AdbField_O_AddrPageLast
};

// where AdbEntry::Matches() should look for a match
enum
{
  AdbLookup_NickName      = 0x0001,
  AdbLookup_FullName      = 0x0002,
  AdbLookup_Organization  = 0x0004,
  AdbLookup_EMail         = 0x0008,
  AdbLookup_HomePage      = 0x0010,
  AdbLookup_Everywhere    = 0xffff
};

// how should it do it
enum
{
  AdbLookup_Match         = 0x0000, // default: case insensitive match
  AdbLookup_CaseSensitive = 0x0001,
  AdbLookup_Substring     = 0x0002, // match "foo" as "*foo*"
  AdbLookup_StartsWith    = 0x0004  // match "foo" as "foo*"
};

// ============================================================================
// ADB classes
// ============================================================================

/**
  The common base class for AdbEntry and AdbEntryGroup.

  As this class derives from MObjectRC, both AdbEntry and
  AdbEntryGroup.do too, so they use reference counting: see the
  comments in MObject.h for more details about it.
*/
class AdbElement : public MObjectRC
{
public:
  /// the group this entry/group belongs to (never NULL for these classes)
  virtual AdbEntryGroup *GetGroup() const = 0;

  /// get the text describing the user to present the user with
  virtual String GetDescription() const = 0;
};

/**
  Data stored for each entry in the ADB. It can be read an modified, in the
  latter case the entry is responsible for saving it (i.e. there is no separate
  Save() function).
*/
class AdbEntry : public AdbElement
{
public:
  // accessors
    /// retrieve the value of a field (see enum AdbField for index values)
  virtual void GetFieldInternal(size_t n, String *pstr) const = 0;
    /// retrieve the value of a field (see enum AdbField for index values)
  virtual void GetField(size_t n, String *pstr) const = 0;
    /// get the count of additional e-mail addresses (i.e. except the 1st one)
  virtual size_t GetEMailCount() const = 0;
    /// get an additional e-mail adderss
  virtual void GetEMail(size_t n, String *pstr) const = 0;

  // changing the data
    /// set any text field with this function
  virtual void SetField(size_t n, const String& strValue) = 0;
    /// add an additional e-mail (primary one is changed with SetField)
  virtual void AddEMail(const String& strEMail) = 0;
    /// delete all additional e-mails
  virtual void ClearExtraEMails() = 0;

  // if dirty flag is set, the entry will automatically save itself when
  // deleted (the flag is set automatically when the entry is modified)
    /// has the entry been changed?
  virtual bool IsDirty() const = 0;
    /// prevent the entry from saving itself by resetting the dirty flag
  virtual void ClearDirty() = 0;

  // other operations
    /// check whether we match the given string (see AdbLookup_xxx constants)
  virtual bool Matches(const char *str, int where, int how) = 0;
    /// description of an item is the name and the address
  virtual String GetDescription() const
  {
     String name, address;
     GetField(AdbField_FullName, &name);
     GetField(AdbField_EMail, &address);

     // the full form is "FullName <email>", but if the "fullname" is empty,
     // we take "nickname" instead (it can not be empty normally)
     if ( !name )
        GetField(AdbField_NickName, &name);

     return GetFullEmailAddress(name, address);
  }
};

class AdbEntryCommon : public AdbEntry
{
 public:
   /** Retrieve the value of a field (see enum AdbField for index
       values). Tries to generate meaningful return values for empty
       FirstName/FamilyName fields. */
  virtual void GetField(size_t n, wxString *pstr) const;
};
/**
  A group of ADB entries which contains the entries and other groups.

  This class derives from MObjectRC and uses reference counting, see
  the comments in MObject.h for more details about it.  */
class AdbEntryGroup : public AdbElement
{
public:
  // accessors
    /// get the name of the group
  virtual String GetName() const = 0;
    /// get the names of all entries, returns the number of them
  virtual size_t GetEntryNames(wxArrayString& aNames) const = 0;

    /// get entry by name
  virtual AdbEntry *GetEntry(const String& name) const = 0;

    /// check whether an entry or group by this name exists
  virtual bool Exists(const String& path) const = 0;

    /// get the names of all groups, returns the number of them
  virtual size_t GetGroupNames(wxArrayString& aNames) const = 0;

    /// get the name of the group/group by name
  virtual AdbEntryGroup *GetGroup(const String& name) const = 0;

  // operations
    /// create entry/subgroup
  virtual AdbEntry *CreateEntry(const String& strName) = 0;
  virtual AdbEntryGroup *CreateGroup(const String& strName) = 0;

    /// delete entry/subgroup
  virtual void DeleteEntry(const String& strName) = 0;
  virtual void DeleteGroup(const String& strName) = 0;

    /// find entry by name (returns NULL if not found)
  virtual AdbEntry *FindEntry(const char *szName) = 0;

  // misc
    /// description of a group is just its name
  virtual String GetDescription() const { return GetName(); }

  /** Return the icon name if set. The numeric return value must be -1 
      for the default, or an index into the image list in AdbFrame.cpp.
  */
  virtual int GetIconId() const { return -1; }
};

// ============================================================================
// base class for a common implementation model of AdbEntry - one in which all
// data is stored in memory
// ============================================================================

class AdbEntryStoredInMemory : public AdbEntryCommon
{
public:
  AdbEntryStoredInMemory() { m_bDirty = FALSE; }

  // we can implement some of the base class functions in the manner independent
  // of the exact nature of the derived class
  virtual void GetFieldInternal(size_t n, String *pstr) const;
  virtual void SetField(size_t n, const String& strValue);
  virtual void AddEMail(const String& strEMail)
    { m_astrEmails.Add(strEMail); m_bDirty = TRUE; }
  virtual void ClearExtraEMails();
  virtual size_t GetEMailCount() const { return m_astrEmails.Count(); }
  virtual void GetEMail(size_t n, String *p) const { *p = m_astrEmails[n]; }
  virtual void ClearDirty() { m_bDirty = FALSE; }
  virtual bool IsDirty() const { return m_bDirty; }
  virtual bool Matches(const char *str, int where, int how);

protected:
  wxArrayString m_astrFields; // all text entries (some may be not present)
  wxArrayString m_astrEmails; // all email addresses except for the first one

  bool m_bDirty;              // dirty flag
};

#endif  //_ADBENTRY_H
