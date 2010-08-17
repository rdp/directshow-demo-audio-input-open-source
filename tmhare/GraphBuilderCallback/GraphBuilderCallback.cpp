#include "stdafx.h" // the normal dshow headers and utility classes
#include "registry.h" // Registry manipulation class not included here because it has other dependencies
#include "GraphBuilderCallback.h"  // See the header file comments for sample usage

// The constructor takes an HWND which could be the parent window
// if you are going to have a GUI.  Customize to meet your needs.
CGraphBuilderCallback::CGraphBuilderCallback(HWND window) :
	CUnknown(NAME("GraphBuilderCallback"), NULL)
{
	AddRef();
}

CGraphBuilderCallback::~CGraphBuilderCallback()
{
}

STDMETHODIMP CGraphBuilderCallback::NonDelegatingQueryInterface(REFIID iid, void** ppv)
{
	if (iid == __uuidof(IAMGraphBuilderCallback))
		return GetInterface((IAMGraphBuilderCallback*)this, ppv);
	
	return CUnknown::NonDelegatingQueryInterface(iid, ppv);
}

// Should the filter with <moniker> be added to the graph ? S_OK : E_FAIL
//
// S_OK means that the filter will be tested for use in the graph by intelligent
// connect but it may not end up in the graph
STDMETHODIMP CGraphBuilderCallback::SelectedFilter(IMoniker *moniker)
{
	HRESULT hr(S_OK);

	// The moniker display name is what is shown in the filter list in GraphEdit
	PWSTR name(NULL);
	hr = moniker->GetDisplayName(NULL, NULL, &name);
	CString moniker_display_name;
	if (FAILED(hr) || !name)
		return S_FALSE;
	moniker_display_name = name;
	::CoTaskMemFree(name);

	// Get the location of the filter from the default registry entry under HKR/CLSID/{filter clsid}/InprocServer32
	int i = moniker_display_name.ReverseFind('\\');
	CString location;
	if (i != -1) {
		CString clsid(_T("CLSID\\"));
		clsid += moniker_display_name.Mid(i + 1); // this is the CLSID of the filter which can be tested against a filter blacklist
		
		// ** test and return E_FAIL here if you do not want the filter added to the graph ** //

		clsid += _T("\\InprocServer32");
//     Implement your own registry code here
		CRegistry reg;
		reg.GetRootEntry(clsid, _T(""), location);
	}

	// Get the friendly name of the filter
	CComPtr<IPropertyBag> property_bag;
	hr = moniker->BindToStorage(0, 0, IID_IPropertyBag, (void **)&property_bag);
	if (FAILED(hr))
		return S_FALSE;
	VARIANT varName;
	VariantInit(&varName);
	hr = property_bag->Read(L"FriendlyName", &varName, 0);
	CString friendly_name;
	if (SUCCEEDED(hr) && varName.bstrVal)
		friendly_name = varName.bstrVal;
	else
		friendly_name = _T("Unknown");
	VariantClear(&varName);

#ifdef DEBUG
	// Show filter info
	CString info(_T("Selected Filter:\n\n Moniker: "));
	info += moniker_display_name;
	info += _T("\n\n Location: ");
	info += location;
	info += _T("\n\n FriendlyName: ");
	info += friendly_name;
	::AfxMessageBox(info, MB_OK);
#endif

	return S_OK;
}

// Add the created <filter> to the graph ? S_OK : E_FAIL
//
// Called after the filter is created (never called if the filter is rejected in SelectFilter above).
//
// Not generally useful unless you want to call interfaces on the filter to determine if it is useful
// for your graph.
STDMETHODIMP CGraphBuilderCallback::CreatedFilter(IBaseFilter *filter)
{
	return S_OK;
}

