///////////////////////////////////////////////////////////////////////////////
// Project:     Mahogany - cross platform e-mail GUI client
// File name:   gui/wxFolderView.h - wxFolderView and related classes
// Purpose:     wxFolderView is used to show to the user folder contents
// Author:      Karsten Ball�der (Ballueder@gmx.net)
// Modified by:
// Created:     1997
// CVS-ID:      $Id$
// Copyright:   (c) 1997-2001 Mahogany team
// Licence:     M license
///////////////////////////////////////////////////////////////////////////////

#ifndef WXFOLDERVIEW_H
#define WXFOLDERVIEW_H

#ifdef __GNUG__
   #pragma interface "wxFolderView.h"
#endif

#include "Mdefaults.h"
#include "wxMFrame.h"
#include "FolderView.h"
#include "MEvent.h"
#include "Mpers.h"

#include <wx/persctrl.h>
#include <wx/splitter.h>

class wxFolderViewPanel;
class wxFolderView;
class wxFolderListCtrl;
class wxFolderMsgWindow;
class wxMFrame;
class MailFolder;
class MessageView;
class MsgCmdProc;
class ASMailFolder;
class ASTicketList;
class HeaderInfoList_obj;
class FolderViewAsyncStatus;

enum wxFolderListCtrlFields
{
   WXFLC_STATUS = 0,
   WXFLC_DATE,
   WXFLC_SIZE,
   WXFLC_FROM,
   WXFLC_SUBJECT,
   WXFLC_NUMENTRIES,
   WXFLC_NONE = WXFLC_NUMENTRIES // must be equal for GetColumnByIndex()
};

// ----------------------------------------------------------------------------
// wxFolderView: allows the user to view messages in the folder
// ----------------------------------------------------------------------------

class wxFolderView : public FolderView
{
public:
   /** Constructor
       @param parent   the parent window
   */
   static wxFolderView *Create(MWindow *parent = NULL);

   /// first time constructor
   wxFolderView(wxWindow *parent);

   /// Destructor
   virtual ~wxFolderView();

   virtual bool MoveToNextUnread();

   /** Set the associated folder.
       @param folder the folder to display or NULL
   */
   virtual void SetFolder(MailFolder *mf);

   /** Open the specified folder
       @param folder the folder to open
       @return true if opened ok, false otherwise
    */
   bool OpenFolder(MFolder *folder);

   /// called on Menu selection
   void OnCommandEvent(wxCommandEvent &event);


   /** Open some messages.
       @param messages array holding the message numbers
   */
   void OpenMessages(UIdArray const &messages);

   /**
       Expunge messages
    */
   void ExpungeMessages();

   /**
       Mark messages read/unread
       @param read whether message should be marked read (true) or unread
    */
   Ticket MarkRead(const UIdArray& messages, bool read);

   /** For use by the listctrl: get last previewed uid: */
   UIdType GetPreviewUId(void) const { return m_uidPreviewed; }

   /** Are we previewing anything? */
   bool HasPreview() const { return GetPreviewUId() != UID_ILLEGAL; }

   /** Returns false if no items are selected
    */
   bool HasSelection() const;

   /** Gets an array containing the uids of the selected messages. If there is
       no selection, the array will contain the focused UID (if any)

       @return the array of selections, may be empty
    */
   UIdArray GetSelections() const;

   /** Get either the the currently focused message
   */
   UIdType GetFocus() const;

   /// Show a message in the preview window.
   void PreviewMessage(long uid);

   /// [de]select all items
   void SelectAll(bool on = true);

   /// select all messages by some status/flag
   void SelectAllByStatus(MailFolder::MessageStatus status, bool isSet = true);

   /// return the MWindow pointer:
   MWindow *GetWindow(void) const { return m_SplitterWindow; }

   /// event processing
   virtual bool OnMEvent(MEventData& event)
   {
      if ( event.GetId() == MEventId_OptionsChange )
      {
         OnOptionsChange((MEventOptionsChangeData &)event);

         return TRUE;
      }

      return FolderView::OnMEvent(event);
   }

   /// Search messages for certain criteria.
   virtual void SearchMessages(void);

   /// process folder delete event
   virtual void OnFolderDeleteEvent(const String& folderName);
   /// update the folderview
   virtual void OnFolderUpdateEvent(MEventFolderUpdateData &event);
   /// update the folderview
   virtual void OnFolderExpungeEvent(MEventFolderExpungeData &event);
   /// close the folder
   virtual void OnFolderClosedEvent(MEventFolderClosedData &event);
   /// update the folderview
   virtual void OnMsgStatusEvent(MEventMsgStatusData &event);
   /// the derived class should react to the result to an asynch operation
   virtual void OnASFolderResultEvent(MEventASFolderResultData &event);

   /// called when our message viewer changes
   virtual void OnMsgViewerChange(wxWindow *viewerNew);

   /// return profile name for persistent controls
   const wxString& GetFullName(void) const { return m_fullname; }

   /// for use by the listctrl:
   ASTicketList *GetTicketList(void) const { return m_TicketList; }

   /// for use by the listctrl only:
   bool GetFocusFollowMode(void) const { return m_FocusFollowMode; }

   /// called when the focused (== current) item in the listctrl changes
   void OnFocusChange(void);

   /// get the parent frame of the folder view
   MFrame *GetParentFrame() const { return m_Frame; }

   /// process a keyboard event, return true if processed
   bool HandleCharEvent(wxKeyEvent& event);

protected:
   /// update the view after new messages appeared in the folder
   void Update();

   /// forget the currently shown folder
   void Clear();

   /// set the folder to show, can't be NULL (unlike in SetFolder)
   void ShowFolder(MailFolder *mf);

   /// set the currently previewed UID
   void SetPreviewUID(UIdType uid);

   /// invalidate the last previewed UID
   void InvalidatePreviewUID() { SetPreviewUID(UID_ILLEGAL); }

   /// select the first interesting message in the folder
   void SelectInitialMessage(const HeaderInfoList_obj& hil);

   /// select the next unread message, return false if no more
   bool SelectNextUnread();

   /// get the number of the messages we show
   inline size_t GetHeadersCount() const;

private:
   /// the full name of the folder opened in this folder view
   wxString m_fullname;

   /// number of deleted messages in the folder
   unsigned long m_nDeleted;

   /// its parent
   MWindow *m_Parent;
   /// and the parent frame
   MFrame *m_Frame;

   /// either a listctrl or a treectrl
   wxFolderListCtrl *m_FolderCtrl;

   /// a splitter window: it contains m_FolderCtrl and m_MessageWindow
   wxSplitterWindow *m_SplitterWindow;

   /// container window for the message viewer (it changes, we don't)
   wxFolderMsgWindow *m_MessageWindow;

   /// the preview window
   MessageView *m_MessagePreview;

   /// the command processor object
   MsgCmdProc *m_msgCmdProc;

   /// UId of last previewed message (may be UID_ILLEGAL)
   UIdType m_uidPreviewed;

   /// index of the message being previewed in the list control
   long m_itemPreviewed;

   /// UId of the focused message, may be different from m_uidPreviewed!
   UIdType m_uidFocused;

   /// a list of pending tickets from async operations
   ASTicketList *m_TicketList;

   /// do we have focus-follow enabled?
   bool m_FocusFollowMode;

   /// the data we store in the profile
   struct AllProfileSettings
   {
      // default copy ctor is ok for now, add one if needed later!

      bool operator==(const AllProfileSettings& other) const;
      bool operator!=(const AllProfileSettings& other) const
         { return !(*this == other); }

      /// the strftime(3) format for date
      String dateFormat;
      /// TRUE => display time/date in GMT
      bool dateGMT;
      /// the folder view control colours
      wxColour BgCol,         // background (same for all messages)
               FgCol,         // normal text colour
               NewCol,        // text colour for new messages
               FlaggedCol,    //                 flagged
               RecentCol,     //                 recent
               DeletedCol,    //                 deleted
               UnreadCol;     //                 unseen
      /// font attributes
      int font, size;
      /// do we want to preview messages when activated?
      bool previewOnSingleClick;
      /// strip e-mail address from sender and display only name?
      bool senderOnlyNames;
      /// replace "From" with "To" for messages sent from oneself?
      bool replaceFromWithTo;
      /// all the addresses corresponding to "oneself"
      wxArrayString returnAddresses;
      /// the order of columns
      int columns[WXFLC_NUMENTRIES];
      /// how to show the size
      MessageSizeShow showSize;
   } m_settings;

   /// read the values from the profile into AllProfileSettings structure
   void ReadProfileSettings(AllProfileSettings *settings);

   /// get the full key to use in persistent message boxes
   String GetFullPersistentKey(MPersMsgBox key);

   /// tell the list ctrl to use the new options
   void ApplyOptions();

   /// handler of options change event, refreshes the view if needed
   void OnOptionsChange(MEventOptionsChangeData& event);

   /// MEventManager reg info
   void *m_regOptionsChange;

   // allow it to access m_MessagePreview;
   friend class wxFolderListCtrl;

   // allow it to call Update()
   friend class FolderViewMessagesDropWhere;
};

// ----------------------------------------------------------------------------
// wxFolderViewFrame: a frame containing just a folder view
// ----------------------------------------------------------------------------

class wxFolderViewFrame : public wxMFrame
{
public:
   /* Opens a FolderView for a mail folder defined by a profile entry.
      @param folder folder to open in the folder view
      @parent parent window (use top level frame if NULL)
      @return pointer to FolderViewFrame or NULL
   */
   static wxFolderViewFrame *Create(MFolder *folder,
                                    wxMFrame *parent = NULL);

   /// dtor
   ~wxFolderViewFrame();

   // callbacks
   void OnCommandEvent(wxCommandEvent& event);
   void OnUpdateUI(wxUpdateUIEvent& event);

   /** This virtual method returns either NULL or a (not incref'd)
       pointer to the profile of the mailfolder being displayed, for
       those wxMFrames which have a folder displayed. Used to make the
       compose view inherit the current folder's settings.
   */
   virtual Profile *GetFolderProfile(void)
      {
         return m_FolderView ? m_FolderView->GetProfile() : NULL;
      }

   /// don't even think of using this!
   wxFolderViewFrame(void) { wxFAIL_MSG("unreachable"); }

private:
   void InternalCreate(wxFolderView *fv, wxMFrame *parent = NULL);

   /// ctor
   wxFolderViewFrame(String const &name, wxMFrame *parent);

   /// the associated folder view
   wxFolderView *m_FolderView;

   DECLARE_DYNAMIC_CLASS(wxFolderViewFrame)
   DECLARE_EVENT_TABLE()
};


#endif // WXFOLDERVIEW_H
