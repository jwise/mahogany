/*-*- c++ -*-********************************************************
 * wxComposeView.h: a window displaying a mail message              *
 *                                                                  *
 * (C) 1998 by Karsten Ball�der (Ballueder@usa.net)                 *
 *                                                                  *
 * $Id$                                                             *
 ********************************************************************
 * $Log$
 * Revision 1.7  1998/05/14 17:34:30  VZ
 * added wxWin2 event map to wxComposeView class (fixed layout problem, menu
 * commands now also work)
 *
 * Revision 1.6  1998/05/12 12:19:00  VZ
 *
 * fixes to Windows fixes. Compiles under wxGTK if you #define USE_APPCONF.
 *
 * Revision 1.5  1998/05/02 15:21:33  KB
 * Fixed the #if/#ifndef etc mess - previous sources were not compilable.
 *
 * Revision 1.4  1998/05/01 14:02:41  KB
 * Ran sources through conversion script to enforce #if/#ifdef and space/TAB
 * conventions.
 *
 * Revision 1.3  1998/03/26 23:05:38  VZ
 * Necessary changes to make it compile under Windows (VC++ only)
 * Header reorganization to be able to use precompiled headers
 *
 * Revision 1.2  1998/03/16 18:22:42  karsten
 * started integration of python, fixed bug in wxFText/word wrapping
 *
 * Revision 1.1  1998/03/14 12:21:14  karsten
 * first try at a complete archive
 *
 *******************************************************************/
#ifndef	WXCOMPOSEVIEW_H
#define WXCOMPOSEVIEW_H

#ifdef __GNUG__
#pragma interface "wxComposeView.h"
#endif

#ifndef USE_PCH
#  undef    T
#  include	<map>

#  include	<Message.h>
#  include	<wxMenuDefs.h>
#  include	<wxMFrame.h>
#  include	<Profile.h>

   using namespace std;
#endif

class wxFTOList;
class wxComposeView;
class wxFTCanvas;


/// just for now, FIXME!
#define	WXCOMPOSEVIEW_FTCANVAS_YPOS	80

/** A wxWindows ComposeView class
  */

class wxComposeView : public wxMFrame //FIXME: public ComposeViewBase
{
   DECLARE_DYNAMIC_CLASS(wxComposeView)
private:
   /// is initialised?
   bool initialised;

   /// a profile
   Profile	* profile;

   /// the panel
   wxPanel	* panel;

   /// compose menu
   wxMenu	*composeMenu;
   
   /**@name Input fields. */
   //@{
   /// The To: field
   wxText	*txtTo;
   wxMessage	*txtToLabel;
   // last length of To field (for expansion)
   int		txtToLastLength;
   /// The CC: field
   wxText	*txtCC;
   wxMessage	*txtCCLabel;
   /// The BCC: field
   wxText	*txtBCC;
   wxMessage	*txtBCCLabel;
   /// The Subject: field
   wxText	*txtSubject;
   wxMessage	*txtSubjectLabel;
   /// the canvas for displaying the mail
   wxFTCanvas	*ftCanvas;
   /// the alias expand button
   wxButton	*aliasButton;
   //@}

   /// the popup menu
   wxMenu	*popupMenu;
   /**@name The interface to its canvas. */
   //@{
   /// the ComposeView canvas class
   friend class wxCVCanvas;
   /// Process a Mouse Event.
   void	ProcessMouse(wxMouseEvent &event);
   //@}

   typedef std::map<unsigned long, String> MapType;
   MapType	fileMap;
   unsigned long	nextFileID;

   /// makes the canvas
   void CreateFTCanvas(void);
public:
   /** quasi-Constructor
       @param iname  name of windowclass
       @param parent parent window
       @param parentProfile parent profile
       @param to default value for To field
       @param cc default value for Cc field
       @param bcc default value for Bcc field
       @param hide if true, do not show frame
   */
   void Create(const String &iname = String("wxComposeView"),
	       wxFrame *parent = NULL,
	       ProfileBase *parentProfile = NULL,
	       String const &to = "",
	       String const &cc = "",
	       String const &bcc = "",
	       bool hide = false);

   /** Constructor
       @param iname  name of windowclass
       @param parentProfile parent profile
       @param parent parent window
       @param hide if true, do not show frame
   */
   wxComposeView(const String &iname = String("wxComposeView"),
		 wxFrame *parent = NULL,
		 ProfileBase *parentProfile = NULL,
		 bool hide = false);
   
   /// Destructor
   ~wxComposeView();

   /// return true if initialised
   inline bool	IsInitialised(void) const { return initialised; }

   /// insert a file into buffer
   void InsertFile(void);
   
   /// sets To field
   void SetTo(const String &to);
   
   /// sets CC field
   void SetCC(const String &cc);

   /// sets Subject field
   void SetSubject(const String &subj);

   /// inserts a text
   void InsertText(const String &txt);
   
   /// make a printout of input window
   void Print(void);

   /// send the message
   void Send(void);
   
   /// called on Menu selection
   void OnMenuCommand(int id);
   
   /// for button
   void OnExpand(wxCommandEvent &event);
   
#ifdef  USE_WXWINDOWS2
   //@{ Menu callbacks
      ///
   void OnInsertFile(wxCommandEvent&) 
      { OnMenuCommand(WXMENU_COMPOSE_INSERTFILE); }
      ///
   void OnSend(wxCommandEvent&) { OnMenuCommand(WXMENU_COMPOSE_SEND); }
      ///
   void OnPrint(wxCommandEvent&) { OnMenuCommand(WXMENU_COMPOSE_PRINT); }
      ///
   void OnClear(wxCommandEvent&) { OnMenuCommand(WXMENU_COMPOSE_CLEAR); }

   /// resize callback
   void OnSize(wxSizeEvent& eventSize);

   DECLARE_EVENT_TABLE()
#else //wxWin1
   /// resize callback
   void OnSize(int w, int h);

   /// for button
   void OnCommand(wxWindow &win, wxCommandEvent &event);
#endif //wxWin1/2
};

#endif
