/*-*- c++ -*-********************************************************
 * Mdummy - a dummy module for Mahogany                             *
 *                                                                  *
 * (C) 1999-2000 by Karsten Ball�der (Ballueder@gmx.net)            *
 *                                                                  *
 * $Id$
 *******************************************************************/

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "Mpch.h"

#ifndef USE_PCH
#   include "Mconfig.h"
#   include "Mcommon.h"
#   include "MDialogs.h"
#endif

#include "MModule.h"

#include "Mversion.h"
#include "MInterface.h"

///------------------------------
/// MModule interface:
///------------------------------

class DummyModule : public MModule
{
   MMODULE_DEFINE();

private:
   /** Dummy constructor.
       As the class has no usable interface, this doesn�t do much, but
       it displays a small dialog to say hello.
   */
   DummyModule();

   MInterface *m_Minterface;

   DEFAULT_ENTRY_FUNC
};


MMODULE_BEGIN_IMPLEMENT(DummyModule,
                        "Mdummy",
                        "none",
                        "Dummy module for Mahogany",
                        "0.00")
   MMODULE_PROP(description, "This module does not do anything, "
                             "it simply gets loaded, opens a dialog and "
                             "that's all. It's purpose is to serve as an "
                             "example and template for writing real modules.")
   MMODULE_PROP(author, "Karsten Ball�der <karsten@phy.hw.ac.uk>")
MMODULE_END_IMPLEMENT(DummyModule)


///------------------------------
/// Own functionality:
///------------------------------

/* static */
MModule *
DummyModule::Init(int version_major, int version_minor, int version_release,
                  MInterface *minterface, int *errorCode)
{
   return new DummyModule();
}


DummyModule::DummyModule()
           : MModule()
{
   GetMInterface()->MessageDialog(
      "This message is created by the DummyModule plugin\n"
      "for Mahogany. This module has been loaded at runtime\n"
      "and is not part of the normal Mahogany executable.",
      NULL,
      "Welcome from DummyModule!");
}
