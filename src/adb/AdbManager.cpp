///////////////////////////////////////////////////////////////////////////////
// Project:     M - cross platform e-mail GUI client
// File name:   adb/AdbManager.cpp - implementation of AdbManager class
// Purpose:     AdbManager manages all AdbBooks used by the application
// Author:      Vadim Zeitlin
// Modified by:
// Created:     09.08.98
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

#include "Mpch.h"

#ifndef USE_PCH
#  include "Mcommon.h"
#  include "Profile.h"
#  include "MApplication.h"

#  include <wx/log.h>
#  include <wx/config.h>
#  include <wx/dynarray.h>
#endif // USE_PCH

#include "adb/AdbEntry.h"
#include "adb/AdbFrame.h"        // for GetAdbEditorConfigPath()
#include "adb/AdbBook.h"
#include "adb/AdbManager.h"
#include "adb/AdbDataProvider.h"
#include "adb/AdbDialogs.h"

// ----------------------------------------------------------------------------
// types
// ----------------------------------------------------------------------------

// cache: we store pointers to all already created ADBs here (it means that
// once created they're not deleted until the next call to ClearCache)
static ArrayAdbBooks gs_booksCache;

// and the provider (names) for each book
static wxArrayString gs_provCache;

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

// helper function: does a recursive search for entries/groups matching the
// given pattern (see AdbManager.h for the possible values of where and how
// paramaters)
static void GroupLookup(
                        ArrayAdbEntries& aEntries,
                        ArrayAdbEntries *aMoreEntries,
                        AdbEntryGroup *pGroup,
                        const String& what,
                        int where,
                        int how,
                        ArrayAdbGroups *aGroups = NULL
                       );

// search in the books specified or in all loaded books otherwise
//
// return true if anything was found, false otherwise
static bool AdbLookupForEntriesOrGroups(
                                        ArrayAdbEntries& aEntries,
                                        ArrayAdbEntries *aMoreEntries,
                                        const String& what,
                                        int where,
                                        int how,
                                        const ArrayAdbBooks *paBooks,
                                        ArrayAdbGroups *aGroups = NULL
                                       );

#define CLEAR_ADB_ARRAY(entries)            \
  {                                         \
    size_t nCount = entries.GetCount();     \
    for ( size_t n = 0; n < nCount; n++ ) { \
      entries[n]->DecRef();                 \
    }                                       \
  }

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// global functions
// ----------------------------------------------------------------------------

// recursive (depth first) search
static void GroupLookup(ArrayAdbEntries& aEntries,
                        ArrayAdbEntries *aMoreEntries,
                        AdbEntryGroup *pGroup,
                        const String& what,
                        int where,
                        int how,
                        ArrayAdbGroups *aGroups)
{
  // check if this book doesn't match itself: the other groups are checked in
  // the parent one but the books don't have a parent
  String nameMatch;
  if ( aGroups ) {
     // we'll use it below
     nameMatch = what.Lower() + _T('*');

     // is it a book?
     if ( !((AdbElement *)pGroup)->GetGroup() ) {
       AdbBook *book = (AdbBook *)pGroup;

       if ( book->GetName().Lower().Matches(nameMatch) ) {
         pGroup->IncRef();

         aGroups->Add(pGroup);
       }
     }
  }

  wxArrayString aNames;
  size_t nGroupCount = pGroup->GetGroupNames(aNames);
  for ( size_t nGroup = 0; nGroup < nGroupCount; nGroup++ ) {
    AdbEntryGroup *pSubGroup = pGroup->GetGroup(aNames[nGroup]);

    GroupLookup(aEntries, aMoreEntries, pSubGroup, what, where, how);

    // groups are matched by name only (case-insensitive)
    if ( aGroups && aNames[nGroup].Lower().Matches(nameMatch) ) {
      aGroups->Add(pSubGroup);
    }
    else {
      pSubGroup->DecRef();
    }
  }

  aNames.Empty();
  size_t nEntryCount = pGroup->GetEntryNames(aNames);
  for ( size_t nEntry = 0; nEntry < nEntryCount; nEntry++ ) {
    AdbEntry *pEntry = pGroup->GetEntry(aNames[nEntry]);

    if ( pEntry->GetField(AdbField_ExpandPriority) == _T("-1") )
    {
      // never use this one for expansion
      pEntry->DecRef();
      continue;
    }

    // we put the entries which match with their nick name in one array and
    // the entries which match anywhere else in the other one: see AdbExpand()
    // to see why
    switch ( pEntry->Matches(what, where, how) ) {
      default:                  // matches elsewhere
        if ( aMoreEntries ) {
          aMoreEntries->Add(pEntry);
          break;
        }
        // else: fall through

      case AdbLookup_NickName:  // match in the entry name
        aEntries.Add(pEntry);
        break;

      case 0:                   // not found at all
        pEntry->DecRef();
        break;
    }
  }
}

static bool
AdbLookupForEntriesOrGroups(ArrayAdbEntries& aEntries,
                            ArrayAdbEntries *aMoreEntries,
                            const String& what,
                            int where,
                            int how,
                            const ArrayAdbBooks *paBooks,
                            ArrayAdbGroups *aGroups)
{
  wxASSERT( aEntries.IsEmpty() );

  if ( paBooks == NULL || paBooks->IsEmpty() )
    paBooks = &gs_booksCache;

  size_t nBookCount = paBooks->Count();
  for ( size_t nBook = 0; nBook < nBookCount; nBook++ ) {
    GroupLookup(aEntries, aMoreEntries,
                (*paBooks)[nBook], what, where, how, aGroups);
  }

  // return true if something found
  return !aEntries.IsEmpty() ||
         (aMoreEntries && !aMoreEntries->IsEmpty()) ||
         (aGroups && !aGroups->IsEmpty());
}

bool AdbLookup(ArrayAdbEntries& aEntries,
               const String& what,
               int where,
               int how,
               AdbEntryGroup *group)
{
   if ( !group )
      return AdbLookupForEntriesOrGroups(aEntries, NULL, what, where, how, NULL);

   // look just in this group
   GroupLookup(aEntries, NULL, group, what, where, how);

   return !aEntries.IsEmpty();
}

bool
AdbExpand(wxArrayString& results, const String& what, int how, wxFrame *frame)
{
  AdbManager_obj manager;
  CHECK( manager, FALSE, _T("can't expand address: no AdbManager") );

  results.Empty();

  if ( what.empty() )
     return FALSE;

  manager->LoadAll();

  static const int lookupMode = AdbLookup_NickName |
                                AdbLookup_FullName |
                                AdbLookup_EMail;

  // check for a group match too
  ArrayAdbGroups aGroups;
  ArrayAdbEntries aEntries,
                  aMoreEntries;

  // expansion may take a long time ...
  if ( frame )
  {
    wxLogStatus(frame,
                _("Looking for matches for '%s' in the address books..."),
                what.c_str());
  }

  wxBusyCursor bc;

  if ( AdbLookupForEntriesOrGroups(aEntries, &aMoreEntries, what, lookupMode,
                                   how, NULL, &aGroups ) ) {
    // first of all, if we had any nick name matches, ignore all the other ones
    // but otherwise use them as well
    if ( aEntries.IsEmpty() ) {
      aEntries = aMoreEntries;

      // prevent the dialog below from showing "more matches" button
      aMoreEntries.Clear();
    }

    // merge both arrays into one big one: notice that the order is important,
    // the groups should come first (see below)
    ArrayAdbElements aEverything;
    size_t n;

    size_t nGroupCount = aGroups.GetCount();
    for ( n = 0; n < nGroupCount; n++ ) {
      aEverything.Add(aGroups[n]);
    }

    wxArrayString emails;
    wxString email;
    size_t nEntryCount = aEntries.GetCount();
    for ( n = 0; n < nEntryCount; n++ ) {
      AdbEntry *entry = aEntries[n];

      entry->GetField(AdbField_EMail, &email);
      if ( emails.Index(email) == wxNOT_FOUND ) {
        emails.Add(email);
        aEverything.Add(entry);
      }
      else { // don't propose duplicate entries
        // need to free it here as it won't be freed with all other entries
        // from aEverything below
        entry->DecRef();
      }
    }

    // let the user choose the one he wants
    int rc = AdbShowExpandDialog(aEverything, aMoreEntries, nGroupCount, frame);

    if ( rc != -1 ) {
      size_t index = (size_t)rc;

      if ( index < nGroupCount ) {
        // it's a group, take all entries from it
        AdbEntryGroup *group = aGroups[index];
        wxArrayString aEntryNames;
        size_t count = group->GetEntryNames(aEntryNames);
        for ( n = 0; n < count; n++ ) {
          AdbEntry *entry = group->GetEntry(aEntryNames[n]);
          if ( entry ) {
            results.Add(entry->GetDescription());

            entry->DecRef();
          }
        }

        if ( frame ) {
          wxLogStatus(frame, _("Expanded '%s' using entries from group '%s'"),
                      what.c_str(), group->GetDescription().c_str());
        }
      }
      else {
        // one entry, but in which array?
        size_t count = aEverything.GetCount();
        AdbEntry *entry = index < count ? (AdbEntry *)aEverything[index]
                                        : aMoreEntries[index - count];
        results.Add(entry->GetDescription());

        if ( frame ) {
          String name;
          entry->GetField(AdbField_FullName, &name);
          wxLogStatus(frame, _("Expanded '%s' using entry '%s'"),
                      what.c_str(), name.c_str());
        }
      }
    }
    //else: cancelled by user

    // free all entries and groups
    CLEAR_ADB_ARRAY(aMoreEntries);
    CLEAR_ADB_ARRAY(aEverything);
  }
  else {
    if ( frame )
      wxLogStatus(frame, _("No matches for '%s'."), what.c_str());
  }

  return !results.IsEmpty();
}

// ----------------------------------------------------------------------------
// AdbManager static functions and variables
// ----------------------------------------------------------------------------

AdbManager *AdbManager::ms_pManager = NULL;

// create the manager object if it doesn't exist yet and return it
AdbManager *AdbManager::Get()
{
  if ( ms_pManager ==  NULL ) {
    // create it
    ms_pManager = new AdbManager;

    // artificially bump up the ref count so it will be never deleted (if the
    // calling code behaves properly) until the very end of the application
    ms_pManager->IncRef();

    wxLogTrace(_T("adb"), _T("AdbManager created."));
  }
  else {
    // just inc ref count on the existing one
    ms_pManager->IncRef();
  }

  wxASSERT( ms_pManager != NULL );

  return ms_pManager;
}

// decrement the ref count of the manager object
void AdbManager::Unget()
{
  wxCHECK_RET( ms_pManager,
               _T("AdbManager::Unget() called without matching Get()!") );

  if ( !ms_pManager->DecRef() ) {
    // the object deleted itself
    ms_pManager = NULL;

    wxLogTrace(_T("adb"), _T("AdbManager deleted."));
  }
}

// force the deletion of ms_pManager
void AdbManager::Delete()
{
  // we should only be called when the program terminates, otherwise some
  // objects could still have references to us
  ASSERT_MSG( !mApplication->IsRunning(),
              _T("AdbManager::Delete() called, but the app is still running!") );

#ifdef DEBUG
  size_t count = 0;
#endif

  while ( ms_pManager ) {
    #ifdef DEBUG
      count++;
    #endif

    Unget();
  }

  // there should be _exactly_ one extra IncRef(), not several
  ASSERT_MSG( count < 2, _T("Forgot AdbManager::Unget() somewhere") );
}

// ----------------------------------------------------------------------------
// AdbManager public methods
// ----------------------------------------------------------------------------

// create a new address book using specified provider or trying all of them
// if it's NULL
AdbBook *AdbManager::CreateBook(const String& name,
                                AdbDataProvider *provider,
                                String *providerName)
{
   AdbBook *book = NULL;

   // first see if we don't already have it
   book = FindInCache(name, provider);
   if ( book )
   {
      book->IncRef();
      return book;
   }

   // no, must create a new one
   AdbDataProvider *prov = provider;
   if ( prov == NULL ) {
    // try to find it
    AdbDataProvider::AdbProviderInfo *info = AdbDataProvider::ms_listProviders;
    while ( info ) {
      prov = info->CreateProvider();
      if ( prov->TestBookAccess(name, AdbDataProvider::Test_OpenReadOnly) ||
            prov->TestBookAccess(name, AdbDataProvider::Test_Create) ) {
        if ( providerName ) {
          // return the provider name if asked for it
          *providerName = info->szName;
        }
        break;
      }

      prov->DecRef();
      prov = NULL;

      info = info->pNext;
    }
  }

  if ( prov ) {
    book = prov->CreateBook(name);
  }

  if ( book == NULL ) {
    wxLogError(_("Can't open the address book '%s'."), name.c_str());
  }
  else {
    book->IncRef();
    gs_booksCache.Add(book);
    gs_provCache.Add(prov->GetProviderName());
  }

  if ( prov && !provider ) {
    // only if it's the one we created, not the one which was passed in!
    prov->DecRef();
  }

  return book;
}

size_t AdbManager::GetBookCount() const
{
  ASSERT_MSG( gs_provCache.Count() == gs_booksCache.Count(),
              _T("mismatch between gs_booksCache and gs_provCache!") );

  return gs_booksCache.Count();
}

AdbBook *AdbManager::GetBook(size_t n) const
{
  AdbBook *book = gs_booksCache[n];
  book->IncRef();

  return book;
}

// FIXME it shouldn't even know about AdbEditor existence! but this would
// involve changing AdbFrame.cpp to use this function somehow and I don't have
// the time to do it right now...
void AdbManager::LoadAll()
{
  wxConfigBase *conf = mApplication->GetProfile()->GetConfig();
  conf->SetPath(GetAdbEditorConfigPath());

  wxArrayString astrAdb, astrProv;
  RestoreArray(conf, astrAdb, _T("AddressBooks"));
  RestoreArray(conf, astrProv, _T("Providers"));

  wxString strProv;
  AdbDataProvider *pProvider;
  size_t nCount = astrAdb.Count();
  for ( size_t n = 0; n < nCount; n++ ) {
    if ( n < astrProv.Count() )
      strProv = astrProv[n];
    else
      strProv.Empty();

    if ( strProv.empty() )
      pProvider = NULL;
    else
      pProvider = AdbDataProvider::GetProviderByName(strProv);

    // it's getting worse and worse... we're using our knowledge of internal
    // structure of AdbManager here: we know the book will not be deleted
    // after this DecRef because the cache also has a lock on it
    SafeDecRef(CreateBook(astrAdb[n], pProvider));

    SafeDecRef(pProvider);
  }
}

void AdbManager::ClearCache()
{
  size_t nCount = gs_booksCache.Count();
  for ( size_t n = 0; n < nCount; n++ ) {
    gs_booksCache[n]->DecRef();
  }

  gs_booksCache.Clear();

  gs_provCache.Clear();
}

AdbManager::~AdbManager()
{
  ClearCache();
}

// ----------------------------------------------------------------------------
// AdbManager private methods
// ----------------------------------------------------------------------------

AdbBook *AdbManager::FindInCache(const String& name,
                                 const AdbDataProvider *provider) const
{
  AdbBook *book;
  size_t nCount = gs_booksCache.Count();
  for ( size_t n = 0; n < nCount; n++ ) {
    if ( provider && provider->GetProviderName() != gs_provCache[n] ) {
      // don't compare books belonging to different providers
      continue;
    }

    book = gs_booksCache[n];
    if ( book->IsSameAs(name) )
      return book;
  }

  return NULL;
}

// ----------------------------------------------------------------------------
// AdbManager debugging support
// ----------------------------------------------------------------------------

#ifdef DEBUG

String AdbManager::DebugDump() const
{
  String str = MObjectRC::DebugDump();
  str << (int)gs_booksCache.Count() << _T("books in cache");

  return str;
}

#endif // DEBUG

/* vi: set ts=2 sw=2: */
