///////////////////////////////////////////////////////////////////////////////
// Name:        common/vcard.cpp
// Purpose:     wxVCard class implementation
// Author:      Vadim Zeitlin
// Modified by:
// Created:     12.05.00
// RCS-ID:      $Id$
// Copyright:   (c) 2000 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "vcard.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/log.h"
    #include "wx/intl.h"
    #include "wx/app.h"
    #include "wx/dynarray.h"
    #include "wx/filefn.h"
#endif //WX_PRECOMP

#include "wx/module.h"
#include "wx/datetime.h"

#include "../vcard/vcc.h"

#define VOBJECT_DEFINED

#include "wx/vcard.h"


// - required by vcard parser:
extern "C" {
void Parse_Debug(const char *s)
{
#ifdef DEBUG
	wxLogDebug(s);
#endif
}
};

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// a module to clean up memory used by vCard lib
class wxVCardModule : public wxModule
{
public:
    virtual bool OnInit() { return TRUE; }
    virtual void OnExit() { cleanStrTbl(); }

private:
    DECLARE_DYNAMIC_CLASS(wxVCardModule)
};

IMPLEMENT_DYNAMIC_CLASS(wxVCardModule, wxModule);

// ============================================================================
// implementation: generic vObject API wrappers
// ============================================================================

// ----------------------------------------------------------------------------
// wxVCardObject construction
// ----------------------------------------------------------------------------

// wrap an existing vObject
wxVCardObject::wxVCardObject(VObject *vObj)
{
    m_vObj = vObj;
}

// create a new, empty vObject
wxVCard::wxVCard()
{
    m_vObj = newVObject(VCCardProp);
}

// construct a new sub object
wxVCardObject::wxVCardObject(wxVCardObject *parent, const wxString& name)
{
    if ( parent )
    {
        m_vObj = addProp(parent->m_vObj, name);
    }
    else
    {
        wxFAIL_MSG(_T("NULL parent in wxVCardObject ctor"));

        m_vObj = NULL;
    }
}

wxVCardObject::~wxVCardObject()
{
}

wxVCard::~wxVCard()
{
    if ( m_vObj )
    {
        cleanVObject(m_vObj);
    }
}

// load object(s) from file
/* static */ wxArrayCards wxVCard::CreateFromFile(const wxString& filename)
{
    wxArrayCards vcards;

    VObject *vObj = Parse_MIME_FromFileName((char *)filename.mb_str());
    if ( !vObj )
    {
        wxLogError(_("The file '%s' doesn't contain any vCard objects."),
                   filename.c_str());
    }
    else
    {
        while ( vObj )
        {
            if ( wxStricmp(vObjectName(vObj), VCCardProp) == 0 )
            {
                vcards.Add(new wxVCard(vObj));
            }
            //else: it is not a vCard

            vObj = nextVObjectInList(vObj);
        }
    }

    return vcards;
}

// load the first vCard object from file
wxVCard::wxVCard(const wxString& filename)
{
    // reuse CreateFromFile(): it shouldn't be that inefficent as we assume
    // that the file will in general contain only one vObject if the user code
    // uses this ctor
    wxArrayCards vcards = CreateFromFile(filename);
    size_t nCards = vcards.GetCount();
    if ( nCards == 0 )
    {
        m_vObj = NULL;
    }
    else
    {
        m_vObj = vcards[0]->m_vObj;
        vcards[0]->m_vObj = NULL;

        WX_CLEAR_ARRAY(vcards);
    }
}

// ----------------------------------------------------------------------------
// name and properties access
// ----------------------------------------------------------------------------

wxString wxVCardObject::GetName() const
{
    return vObjectName(m_vObj);
}

wxVCardObject::Type wxVCardObject::GetType() const
{
    // the values are the same as in Type enum, just cast
    return m_vObj ? (Type)vObjectValueType(m_vObj) : Invalid;
}

void wxVCardObject::SetValue(const wxString& val)
{
    setVObjectStringZValue(m_vObj, val);
}

void wxVCardObject::SetValue(unsigned int val)
{
    setVObjectIntegerValue(m_vObj, val);
}

void wxVCardObject::SetValue(unsigned long val)
{
    setVObjectLongValue(m_vObj, val);
}

bool wxVCardObject::GetValue(wxString *val) const
{
    // the so called Unicode support in the vCard library is broken beyond any
    // repair, it seems that they just don't know what Unicode is, let alone
    // what to do with it :-(
#if 0
    if ( GetType() == String )
        *val = vObjectStringZValue(m_vObj);
#if wxUSE_WCHAR_T
    else if ( GetType() == UString )
        *val = vObjectUStringZValue(m_vObj);
#endif // wxUSE_WCHAR_T
    else
        return FALSE;
#else // 1
    if ( GetType() != UString )
        return FALSE;

    char *s = fakeCString(vObjectUStringZValue(m_vObj));
    *val = s;
    deleteStr(s);
#endif // 0/1

    return TRUE;
}

bool wxVCardObject::GetValue(unsigned int *val) const
{
    if ( GetType() != Int )
        return FALSE;

    *val = vObjectIntegerValue(m_vObj);

    return TRUE;
}

bool wxVCardObject::GetValue(unsigned long *val) const
{
    if ( GetType() != Long )
        return FALSE;

    *val = vObjectLongValue(m_vObj);

    return TRUE;
}

bool wxVCardObject::GetNamedPropValue(const char *name, wxString *val) const
{
    wxString value;
    wxVCardObject *vcObj = GetProperty(name);
    if ( vcObj )
    {
        vcObj->GetValue(val);
        delete vcObj;

        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

wxString wxVCardObject::GetValue() const
{
    wxString value;
    GetValue(&value);
    return value;
}

// ----------------------------------------------------------------------------
// enumeration properties
// ----------------------------------------------------------------------------

wxVCardObject *wxVCardObject::GetFirstProp(void **cookie) const
{
    VObjectIterator *iter = new VObjectIterator;
    initPropIterator(iter, m_vObj);

    *cookie = iter;

    return GetNextProp(cookie);
}

wxVCardObject *wxVCardObject::GetNextProp(void **cookie) const
{
    VObjectIterator *iter = *(VObjectIterator **)cookie;
    if ( iter && moreIteration(iter) )
    {
        return new wxVCardObject(nextVObject(iter));
    }
    else // no more properties
    {
        delete iter;

        *cookie = NULL;

        return NULL;
    }
}

wxVCardObject *wxVCardObject::GetProperty(const wxString& name) const
{
    VObject *vObj = isAPropertyOf(m_vObj, name);

    return vObj ? new wxVCardObject(vObj) : NULL;
}

// ----------------------------------------------------------------------------
// enumerating properties of the given name
// ----------------------------------------------------------------------------

VObject *wxVCard::GetFirstPropOfName(const char *name, void **cookie) const
{
    VObjectIterator *iter = new VObjectIterator;
    initPropIterator(iter, m_vObj);

    *cookie = iter;

    return GetNextPropOfName(name, cookie);
}

VObject *wxVCard::GetNextPropOfName(const char *name, void **cookie) const
{
    VObjectIterator *iter = *(VObjectIterator **)cookie;
    if ( iter )
    {
        while ( moreIteration(iter) )
        {
            VObject *vObj = nextVObject(iter);
            if ( wxStricmp(vObjectName(vObj), name) == 0 )
            {
                // found one with correct name
                return vObj;
            }
        }
    }

    // no more properties with this name
    delete iter;

    *cookie = NULL;

    return NULL;
}

// this macro implements GetFirst/Next function for the properties of given
// name
#define IMPLEMENT_ENUM_PROPERTIES(classname, propname)                      \
    wxVCard##classname *wxVCard::GetFirst##classname(void **cookie) const   \
    {                                                                       \
        VObject *vObj = GetFirstPropOfName(propname, cookie);               \
        return vObj ? new wxVCard##classname(vObj) : NULL;                  \
    }                                                                       \
                                                                            \
    wxVCard##classname *wxVCard::GetNext##classname(void **cookie) const    \
    {                                                                       \
        VObject *vObj = GetNextPropOfName(propname, cookie);                \
        return vObj ? new wxVCard##classname(vObj) : NULL;                  \
    }

IMPLEMENT_ENUM_PROPERTIES(Address, VCAdrProp)
IMPLEMENT_ENUM_PROPERTIES(AddressLabel, VCDeliveryLabelProp)
IMPLEMENT_ENUM_PROPERTIES(PhoneNumber, VCTelephoneProp)
IMPLEMENT_ENUM_PROPERTIES(EMail, VCEmailAddressProp)

#undef IMPLEMENT_ENUM_PROPERTIES

// ----------------------------------------------------------------------------
// simple standard string properties
// ----------------------------------------------------------------------------

bool wxVCard::GetFullName(wxString *fullname) const
{
    return GetNamedPropValue(VCFullNameProp, fullname);
}

bool wxVCard::GetTitle(wxString *title) const
{
    return GetNamedPropValue(VCTitleProp, title);
}

bool wxVCard::GetBusinessRole(wxString *role) const
{
    return GetNamedPropValue(VCBusinessRoleProp, role);
}

bool wxVCard::GetComment(wxString *comment) const
{
    return GetNamedPropValue(VCCommentProp, comment);
}

bool wxVCard::GetURL(wxString *url) const
{
    return GetNamedPropValue(VCURLProp, url);
}

bool wxVCard::GetUID(wxString *uid) const
{
    return GetNamedPropValue(VCUniqueStringProp, uid);
}

bool wxVCard::GetVersion(wxString *version) const
{
    return GetNamedPropValue(VCVersionProp, version);
}

// ----------------------------------------------------------------------------
// composite properties
// ----------------------------------------------------------------------------

bool wxVCard::GetName(wxString *familyName,
                      wxString *givenName,
                      wxString *additionalNames,
                      wxString *namePrefix,
                      wxString *nameSuffix) const
{
    wxVCardObject *vcobjName = GetProperty(VCNameProp);
    if ( !vcobjName )
        return FALSE;

    if ( familyName )
        vcobjName->GetNamedPropValue(VCFamilyNameProp, familyName);
    if ( givenName )
        vcobjName->GetNamedPropValue(VCGivenNameProp, givenName);
    if ( additionalNames )
        vcobjName->GetNamedPropValue(VCAdditionalNamesProp, additionalNames);
    if ( namePrefix )
        vcobjName->GetNamedPropValue(VCNamePrefixesProp, namePrefix);
    if ( nameSuffix )
        vcobjName->GetNamedPropValue(VCNameSuffixesProp, nameSuffix);

    return TRUE;
}

bool wxVCard::GetOrganization(wxString *name, wxString *unit) const
{
    wxVCardObject *vcobjOrg = GetProperty(VCOrgProp);
    if ( !vcobjOrg )
        return FALSE;

    if ( name )
        vcobjOrg->GetNamedPropValue(VCOrgNameProp, name);
    if ( unit )
        vcobjOrg->GetNamedPropValue(VCOrgUnitProp, unit);

    return TRUE;
}

// ----------------------------------------------------------------------------
// other (non string) std properties
// ----------------------------------------------------------------------------

bool wxVCard::GetBirthDay(wxDateTime *datetime)
{
    wxString value;
    if ( !GetNamedPropValue(VCBirthDateProp, &value) )
        return FALSE;

    if ( !datetime->ParseDate(value) )
    {
        return FALSE;
    }

    return TRUE;
}

// ----------------------------------------------------------------------------
// adding properties
// ----------------------------------------------------------------------------

void wxVCardObject::AddProperty(const wxString& name)
{
    (void)addProp(m_vObj, name);
}

void wxVCardObject::AddProperty(const wxString& name, const wxString& value)
{
    VObject *vObj = addPropValue(m_vObj, name, value);

    // mark multiline properties as being encoded in QP
    if ( value.Find(_T('\n')) != wxNOT_FOUND )
    {
        addProp(vObj, VCQuotedPrintableProp);
    }
}

void wxVCardObject::SetProperty(const wxString& name, const wxString& value)
{
    VObject *vObj = isAPropertyOf(m_vObj, name);
    if ( vObj )
    {
        wxVCardObject(vObj).SetValue(value);
    }
    else
    {
        AddProperty(name, value);
    }
}

bool wxVCardObject::DeleteProperty(const wxString& name)
{
    VObject *vObj = isAPropertyOf(m_vObj, name);
    if ( !vObj )
        return FALSE;

    if ( !delVObjectProp(m_vObj, vObj) )
    {
        wxFAIL_MSG(_T("failed to delete VObject property?"));
    }

    return TRUE;
}

void wxVCard::ClearAddresses()
{
    ClearAllProps(VCAdrProp);
}

void wxVCard::ClearAddressLabels()
{
    ClearAllProps(VCDeliveryLabelProp);
}

void wxVCard::ClearPhoneNumbers()
{
    ClearAllProps(VCTelephoneProp);
}

void wxVCard::ClearEMails()
{
    ClearAllProps(VCEmailAddressProp);
}

void wxVCard::ClearAllProps(const wxString& name)
{
    while ( DeleteProperty(name) )
        ;
}

// ----------------------------------------------------------------------------
// setting standard properties
// ----------------------------------------------------------------------------

void wxVCard::SetFullName(const wxString& fullName)
{
    SetProperty(VCFullNameProp, fullName);
}

void wxVCard::SetName(const wxString& familyName,
                      const wxString& givenName,
                      const wxString& additionalNames,
                      const wxString& namePrefix,
                      const wxString& nameSuffix)
{
    VObject *prop = isAPropertyOf(m_vObj, VCNameProp);
    if ( !prop )
        prop = addProp(m_vObj, VCNameProp);

    if ( !!familyName )
        addPropValue(prop, VCFamilyNameProp, familyName);
    if ( !!givenName )
        addPropValue(prop, VCGivenNameProp, givenName);
    if ( !!additionalNames )
        addPropValue(prop, VCAdditionalNamesProp, additionalNames);
    if ( !!namePrefix )
        addPropValue(prop, VCNamePrefixesProp, namePrefix);
    if ( !!nameSuffix )
        addPropValue(prop, VCNameSuffixesProp, nameSuffix);
}

void wxVCard::SetTitle(const wxString& title)
{
    SetProperty(VCTitleProp, title);
}

void wxVCard::SetBusinessRole(const wxString& role)
{
    SetProperty(VCBusinessRoleProp, role);
}

void wxVCard::SetOrganization(const wxString& name,
                              const wxString& unit)
{
    VObject *prop = isAPropertyOf(m_vObj, VCOrgProp);
    if ( !prop )
        prop = addProp(m_vObj, VCOrgProp);

    if ( !!name )
        addPropValue(prop, VCOrgNameProp, name);
    if ( !!unit )
        addPropValue(prop, VCOrgUnitProp, unit);
}

void wxVCard::SetComment(const wxString& comment)
{
    SetProperty(VCCommentProp, comment);
}

void wxVCard::SetURL(const wxString& url)
{
    SetProperty(VCURLProp, url);
}

void wxVCard::SetUID(const wxString& uid)
{
    SetProperty(VCUniqueStringProp, uid);
}

void wxVCard::SetVersion(const wxString& version)
{
    SetProperty(VCVersionProp, version);
}

// ----------------------------------------------------------------------------
// setting other (non string) std properties
// ----------------------------------------------------------------------------

void wxVCard::SetBirthDay(const wxDateTime& datetime)
{
    SetProperty(VCBirthDateProp, datetime.FormatISODate());
}

void wxVCard::AddAddress(const wxString& postoffice,
                         const wxString& extaddr,
                         const wxString& street,
                         const wxString& city,
                         const wxString& region,
                         const wxString& postalcode,
                         const wxString& country,
                         int flags)
{
    VObject *vObj = addProp(m_vObj, VCAdrProp);
    if ( !!postoffice )
        addPropValue(vObj, VCPostalBoxProp, postoffice);
    if ( !!extaddr )
        addPropValue(vObj, VCExtAddressProp, extaddr);
    if ( !!street )
        addPropValue(vObj, VCStreetAddressProp, street);
    if ( !!city )
        addPropValue(vObj, VCCityProp, city);
    if ( !!region )
        addPropValue(vObj, VCRegionProp, region);
    if ( !!postalcode )
        addPropValue(vObj, VCPostalCodeProp, postalcode);
    if ( !!country )
        addPropValue(vObj, VCCountryNameProp, country);

    wxVCardAddrOrLabel::SetFlags(vObj, flags);
}

void wxVCard::AddAddressLabel(const wxString& label, int flags)
{
    VObject *vObj = addPropValue(m_vObj, VCDeliveryLabelProp, label);
    addProp(vObj, VCQuotedPrintableProp);

    wxVCardAddrOrLabel::SetFlags(vObj, flags);
}

void wxVCard::AddPhoneNumber(const wxString& phone, int flags)
{
    VObject *vObj = addPropValue(m_vObj, VCTelephoneProp, phone);

    wxVCardPhoneNumber::SetFlags(vObj, flags);
}

void wxVCard::AddEMail(const wxString& email, wxVCardEMail::Type type)
{
    addPropValue(m_vObj, VCEmailAddressProp, email);

    wxASSERT_MSG( type == wxVCardEMail::Internet,
                  _T("support for other email types not implemented") );
}

// ----------------------------------------------------------------------------
// outputing vObjects
// ----------------------------------------------------------------------------

// write out the object
wxString wxVCardObject::Write() const
{
    char* p = writeMemVObject(NULL, 0, m_vObj);
    wxString s = p;
    free(p);

    return s;
}

// Write() to a file
bool wxVCardObject::Write(const wxString& filename) const
{
    writeVObjectToFile((char *)filename.mb_str(), m_vObj);

    return TRUE; // writeVObjectToFile() is void @#$@#$@!!
}

// write out the internal representation
void wxVCardObject::Dump(const wxString& filename)
{
    // it is ok for m_vObj to be NULL
    printVObjectToFile((char *)filename.mb_str(), m_vObj);
}

// ============================================================================
// implementation of wxVCardObject subclasses
// ============================================================================

// a macro which allows to abbreviate GetFlags() methods: to use it, you must
// have local variables like in wxVCardAddrOrLabel::GetFlags() below
#define CHECK_FLAG(propname, flag)  \
    prop = GetProperty(propname);   \
    if ( prop )                     \
    {                               \
        flags |= flag;              \
        delete prop;                \
    }

    // anothero ne to set flags in vObject
#define SET_FLAG(propname, flag) if ( flags & flag ) addProp(vObj, propname)

// ----------------------------------------------------------------------------
// wxVCardAddrOrLabel
// ----------------------------------------------------------------------------

int wxVCardAddrOrLabel::GetFlags() const
{
    int flags = 0;
    wxVCardObject *prop;

    CHECK_FLAG(VCDomesticProp, Domestic);
    CHECK_FLAG(VCInternationalProp, Intl);
    CHECK_FLAG(VCPostalProp, Postal);
    CHECK_FLAG(VCParcelProp, Parcel);
    CHECK_FLAG(VCHomeProp, Home);
    CHECK_FLAG(VCWorkProp, Work);

    if ( !flags )
    {
        // this is the default flags value - but if any flag(s) are given, they
        // override it (and not combine with it)
        flags = Intl | Postal | Parcel | Work;
    }

    return flags;
}

/* static */ void wxVCardAddrOrLabel::SetFlags(VObject *vObj, int flags)
{
    if ( flags == Default )
        return;

    SET_FLAG(VCDomesticProp, Domestic);
    SET_FLAG(VCInternationalProp, Intl);
    SET_FLAG(VCPostalProp, Postal);
    SET_FLAG(VCParcelProp, Parcel);
    SET_FLAG(VCHomeProp, Home);
    SET_FLAG(VCWorkProp, Work);
}

// ----------------------------------------------------------------------------
// wxVCardAddress
// ----------------------------------------------------------------------------

wxVCardAddress::wxVCardAddress(VObject *vObj)
              : wxVCardAddrOrLabel(vObj)
{
    wxASSERT_MSG( GetName() == VCAdrProp, _T("this is not a vCard address") );
}

wxString wxVCardAddress::GetPropValue(const wxString& name) const
{
    wxString val;
    wxVCardObject *prop = GetProperty(name);
    if ( prop )
    {
        prop->GetValue(&val);
        delete prop;
    }

    return val;
}

wxString wxVCardAddress::GetPostOffice() const
{
    return GetPropValue(VCPostalBoxProp);
}

wxString wxVCardAddress::GetExtAddress() const
{
    return GetPropValue(VCExtAddressProp);
}

wxString wxVCardAddress::GetStreet() const
{
    return GetPropValue(VCStreetAddressProp);
}

wxString wxVCardAddress::GetLocality() const
{
    return GetPropValue(VCCityProp);
}

wxString wxVCardAddress::GetRegion() const
{
    return GetPropValue(VCRegionProp);
}

wxString wxVCardAddress::GetPostalCode() const
{
    return GetPropValue(VCPostalCodeProp);
}

wxString wxVCardAddress::GetCountry() const
{
    return GetPropValue(VCCountryNameProp);
}

// ----------------------------------------------------------------------------
// wxVCardAddressLabel
// ----------------------------------------------------------------------------

wxVCardAddressLabel::wxVCardAddressLabel(VObject *vObj)
                   : wxVCardAddrOrLabel(vObj)
{
    wxASSERT_MSG( GetName() == VCDeliveryLabelProp,
                  _T("this is not a vCard address label") );
}

// ----------------------------------------------------------------------------
// wxVCardPhoneNumber
// ----------------------------------------------------------------------------

wxVCardPhoneNumber::wxVCardPhoneNumber(VObject *vObj)
                  : wxVCardObject(vObj)
{
    wxASSERT_MSG( GetName() == VCTelephoneProp,
                  _T("this is not a vCard telephone number") );
}

int wxVCardPhoneNumber::GetFlags() const
{
    int flags = 0;
    wxVCardObject *prop;

    CHECK_FLAG(VCPreferredProp, Preferred);
    CHECK_FLAG(VCWorkProp, Work);
    CHECK_FLAG(VCHomeProp, Home);
    CHECK_FLAG(VCVoiceProp, Voice);
    CHECK_FLAG(VCFaxProp, Fax);
    CHECK_FLAG(VCMessageProp, Messaging);
    CHECK_FLAG(VCCellularProp, Cellular);
    CHECK_FLAG(VCPagerProp, Pager);
    CHECK_FLAG(VCBBSProp, BBS);
    CHECK_FLAG(VCModemProp, Modem);
    CHECK_FLAG(VCCarProp, Car);
    CHECK_FLAG(VCISDNProp, ISDN);
    CHECK_FLAG(VCVideoProp, Video);

    if ( !flags )
    {
        // this is the default flags value
        flags = Voice;
    }

    return flags;
}

/* static */ void wxVCardPhoneNumber::SetFlags(VObject *vObj, int flags)
{
    SET_FLAG(VCPreferredProp, Preferred);
    SET_FLAG(VCWorkProp, Work);
    SET_FLAG(VCHomeProp, Home);
    SET_FLAG(VCVoiceProp, Voice);
    SET_FLAG(VCFaxProp, Fax);
    SET_FLAG(VCMessageProp, Messaging);
    SET_FLAG(VCCellularProp, Cellular);
    SET_FLAG(VCPagerProp, Pager);
    SET_FLAG(VCBBSProp, BBS);
    SET_FLAG(VCModemProp, Modem);
    SET_FLAG(VCCarProp, Car);
    SET_FLAG(VCISDNProp, ISDN);
    SET_FLAG(VCVideoProp, Video);
}

// ----------------------------------------------------------------------------
// wxVCardEMail
// ----------------------------------------------------------------------------

wxVCardEMail::wxVCardEMail(VObject *vObj)
            : wxVCardObject(vObj)
{
    wxASSERT_MSG( GetName() == VCEmailAddressProp,
                  _T("this is not a vCard email address") );
}

wxVCardEMail::Type wxVCardEMail::GetType() const
{
    static const char *emailTypes[] =
    {
        VCInternetProp,
        VCX400Prop,
    };

    // the property names and types should be in sync
    wxASSERT_MSG( WXSIZEOF(emailTypes) == Max, _T("forgot to update") );

    size_t n;
    for ( n = 0; n < Max; n++ )
    {
        if ( isAPropertyOf(m_vObj, emailTypes[n]) )
        {
            break;
        }
    }

    return (Type)n;
}
