// //// //// //// //// //// //// //// //// //// //// //// //// //// //// //////
// Project:     M
// File name:   MFolderDialogs.h - functions to invoke dialogs dealing with
//              folder management, such as folder creation, changing folder
//              properties &c
// Purpose:
// Author:      Vadim Zeitlin
// Modified by:
// Created:     23.12.98
// CVS-ID:      $Id$
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     M license
// // //// //// //// //// //// //// //// //// //// //// //// //// //// //// ///

#ifndef _MFOLDERDIALOGS_H
#define _MFOLDERDIALOGS_H

// -----------------------------------------------------------------------------
// forward declarations
// -----------------------------------------------------------------------------

class wxWindow;
class MFolder;

// -----------------------------------------------------------------------------
// constants
// -----------------------------------------------------------------------------

// which page of the folder creation dialog to show first?
enum FolderCreatePage
{
   FolderCreatePage_Default = -1, // default must have value -1
   FolderCreatePage_Folder,
   FolderCreatePage_NewMail,
   FolderCreatePage_Compose,
   FolderCreatePage_MsgView,
   FolderCreatePage_FolderView,
   FolderCreatePage_FolderTree,
   FolderCreatePage_Max
};

// -----------------------------------------------------------------------------
// macros
// -----------------------------------------------------------------------------

// get the notebook page by index and cast it to the right type
#define GET_FOLDER_PAGE(nbook)   ((wxFolderPropertiesPage *)nbook->GetPage(FolderCreatePage_Folder))
#define GET_OPTIONS_PAGE(nbook, n)  ((wxOptionsPage *)nbook->GetPage(n))

// -----------------------------------------------------------------------------
// functions
// -----------------------------------------------------------------------------

/**
  shows folder creation dialog, returns a pointer to created folder or NULL.

  @param parentFolder is the default parent folder or NULL
  @return the returned folder object must be DecRef()d by the caller (if !NULL)
*/
extern MFolder *ShowFolderCreateDialog
                (
                  wxWindow *parent = NULL,
                  FolderCreatePage page = FolderCreatePage_Default,
                  MFolder *parentFolder = NULL
                );

/**
  shows folder properties for the given folder and allows changing some of
  them, returns TRUE if anything was changed, FALSE otherwise
*/

extern bool ShowFolderPropertiesDialog(MFolder *folder,
                                       wxWindow *parent = NULL);

/**
  shows all existing subfolders (not in the program, but on the server) of the
  given folder and allows the user to browse them and create them.

  @return TRUE if any folders were created, FALSE otherwise
*/
extern bool ShowFolderSubfoldersDialog(MFolder *folder,
                                       wxWindow *parent = NULL);

/**
  asks the user for the new name of the folder: returns new name to show in
  the tree and the new name for the underlying mailbox; if either of them is
  empty then it shouldn't be changed

  @param folder folder to rename
  @param folderName holds the new folder name in the tree on return
  @param mboxName holds the new mailbox name on return
  @param parent the parent window
  @return true if we need to do something, false if the dialog was cancelled
*/
extern bool ShowFolderRenameDialog(const MFolder *folder,
                                   String *folderName,
                                   String *mboxName,
                                   wxWindow *parent = NULL);

#endif // _MFOLDERDIALOGS_H
