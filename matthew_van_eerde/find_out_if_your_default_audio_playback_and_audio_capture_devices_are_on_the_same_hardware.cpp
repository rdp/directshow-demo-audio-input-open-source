// main.cpp

#define INITGUID

#include <windows.h>
#include <tchar.h>

const GUID GUID_NULL = { 0 };

#include <atlstr.h>
#include <mmdeviceapi.h>
#include <devicetopology.h>
#include <functiondiscoverykeys.h>

#define LOG(formatstring, ...) _tprintf(formatstring _T("\n"), __VA_ARGS__)

// helper class to CoInitialize/CoUninitialize
class CCoInitialize {
private:
    HRESULT m_hr;
public:
    CCoInitialize(PVOID pReserved, HRESULT &hr)
        : m_hr(E_UNEXPECTED) { hr = m_hr = CoInitialize(pReserved); }
    ~CCoInitialize() { if (SUCCEEDED(m_hr)) { CoUninitialize(); } }
};

// helper class to CoTaskMemFree
class CCoTaskMemFreeOnExit {
private:
    PVOID m_p;
public:
    CCoTaskMemFreeOnExit(PVOID p) : m_p(p) {}
    ~CCoTaskMemFreeOnExit() { CoTaskMemFree(m_p); }
};

// helper class to PropVariantClear
class CPropVariantClearOnExit {
private:
    PROPVARIANT *m_p;
public:
    CPropVariantClearOnExit(PROPVARIANT *p) : m_p(p) {}
    ~CPropVariantClearOnExit() { PropVariantClear(m_p); }
};

// find the default capture and render audio devices
// determine whether they are on the same audio hardware
int _tmain() {
    HRESULT hr = S_OK;

    // initialize COM
    CCoInitialize ci(NULL, hr);
    if (FAILED(hr)) {
        LOG(_T("CoInitialize failed: hr = 0x%08x"), hr);
        return __LINE__;
    }

    // get enumerator
    CComPtr<IMMDeviceEnumerator> pMMDeviceEnumerator;
    hr = pMMDeviceEnumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator));
    if (FAILED(hr)) {
        LOG(_T("CoCreateInstance(IMMDeviceEnumerator) failed: hr = 0x%08x"), hr);
        return __LINE__;
    }

    // get default render/capture endpoints
    CComPtr<IMMDevice> pRenderEndpoint;
    hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pRenderEndpoint);
    if (FAILED(hr)) {
        LOG(_T("GetDefaultAudioEndpoint(eRender, eConsole) failed: hr = 0x%08x"), hr);
        return __LINE__;
    }
    
    CComPtr<IMMDevice> pCaptureEndpoint;
    hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pCaptureEndpoint);
    if (FAILED(hr)) {
        LOG(_T("GetDefaultAudioEndpoint(eCapture, eConsole) failed: hr = 0x%08x"), hr);
        return __LINE__;
    }

    // get endpoint device topologies
    CComPtr<IDeviceTopology> pRenderEndpointTopology;
    hr = pRenderEndpoint->Activate(__uuidof(IDeviceTopology), CLSCTX_ALL, NULL, (void**)&pRenderEndpointTopology);
    if (FAILED(hr)) {
        LOG(_T("Render endpoint Activate(IDeviceTopology) failed: hr = 0x%08x"), hr);
        return __LINE__;
    }

    CComPtr<IDeviceTopology> pCaptureEndpointTopology;
    hr = pCaptureEndpoint->Activate(__uuidof(IDeviceTopology), CLSCTX_ALL, NULL, (void**)&pCaptureEndpointTopology);
    if (FAILED(hr)) {
        LOG(_T("Capture endpoint Activate(IDeviceTopology) failed: hr = 0x%08x"), hr);
        return __LINE__;
    }

    // get KS filter device IDs
    LPWSTR szRenderFilterId;
    CComPtr<IConnector> pConnector;
    hr = pRenderEndpointTopology->GetConnector(0, &pConnector);
    if (FAILED(hr)) {
        LOG(_T("Render endpoint topology GetConnector(0) failed: hr = 0x%08x"), hr);
        return __LINE__;
    }

    hr = pConnector->GetDeviceIdConnectedTo(&szRenderFilterId);
    if (FAILED(hr)) {
        LOG(_T("Render connector GetDeviceIdConnectedTo() failed: hr = 0x%08x"), hr);
        return __LINE__;
    }
    CCoTaskMemFreeOnExit freeRender(szRenderFilterId);
    LOG(_T("KS filter ID for render endpoint:\n\t%ls"), szRenderFilterId);

    LPWSTR szCaptureFilterId;
    pConnector = NULL;
    hr = pCaptureEndpointTopology->GetConnector(0, &pConnector);
    if (FAILED(hr)) {
        LOG(_T("Capture endpoint topology GetConnector(0) failed: hr = 0x%08x"), hr);
        return __LINE__;
    }

    hr = pConnector->GetDeviceIdConnectedTo(&szCaptureFilterId);
    if (FAILED(hr)) {
        LOG(_T("Capture connector GetDeviceIdConnectedTo() failed: hr = 0x%08x"), hr);
        return __LINE__;
    }
    CCoTaskMemFreeOnExit freeCapture(szCaptureFilterId);
    LOG(_T("KS filter ID for capture endpoint:\n\t%ls"), szCaptureFilterId);

    // get IMMDevices for each associated devnode
    CComPtr<IMMDevice> pRenderDevnode;
    hr = pMMDeviceEnumerator->GetDevice(szRenderFilterId, &pRenderDevnode);
    if (FAILED(hr)) {
        LOG(_T("Getting render devnode via IMMDeviceEnumerator::GetDevice(\"%ls\") failed: hr = 0x%08x"), szRenderFilterId, hr);
        return __LINE__;
    }
    
    CComPtr<IMMDevice> pCaptureDevnode;
    hr = pMMDeviceEnumerator->GetDevice(szCaptureFilterId, &pCaptureDevnode);
    if (FAILED(hr)) {
        LOG(_T("Getting capture devnode via IMMDeviceEnumerator::GetDevice(\"%ls\") failed: hr = 0x%08x"), szCaptureFilterId, hr);
        return __LINE__;
    }
    LOG(_T(""));

    // open property set on each devnode
    CComPtr<IPropertyStore> pRenderDevnodePropertyStore;
    hr = pRenderDevnode->OpenPropertyStore(STGM_READ, &pRenderDevnodePropertyStore);
    if (FAILED(hr)) {
        LOG(_T("Getting render devnode property store failed: hr = 0x%08x"), hr);
        return __LINE__;
    }

    CComPtr<IPropertyStore> pCaptureDevnodePropertyStore;
    hr = pCaptureDevnode->OpenPropertyStore(STGM_READ, &pCaptureDevnodePropertyStore);
    if (FAILED(hr)) {
        LOG(_T("Getting capture devnode property store failed: hr = 0x%08x"), hr);
        return __LINE__;
    }

    // get PKEY_Device_InstanceId property
    PROPVARIANT varRenderInstanceId; PropVariantInit(&varRenderInstanceId);
    CPropVariantClearOnExit clearRender(&varRenderInstanceId);
    hr = pRenderDevnodePropertyStore->GetValue(PKEY_Device_InstanceId, &varRenderInstanceId);
    if (FAILED(hr)) {
        LOG(_T("Render devnode property store GetValue(PKEY_Device_InstanceId) failed: hr = 0x%08x"), hr);
        return __LINE__;
    }
    if (VT_LPWSTR != varRenderInstanceId.vt) {
        LOG(_T("Render instance id variant type is %u - expected VT_LPWSTR"), varRenderInstanceId.vt);
        return __LINE__;
    }
    LOG(_T("Instance Id of default render device: %ls"), varRenderInstanceId.pwszVal);

    PROPVARIANT varCaptureInstanceId; PropVariantInit(&varCaptureInstanceId);
    CPropVariantClearOnExit clearCapture(&varCaptureInstanceId);
    hr = pCaptureDevnodePropertyStore->GetValue(PKEY_Device_InstanceId, &varCaptureInstanceId);
    if (FAILED(hr)) {
        LOG(_T("Capture devnode property store GetValue(PKEY_Device_InstanceId) failed: hr = 0x%08x"), hr);
        return __LINE__;
    }
    if (VT_LPWSTR != varCaptureInstanceId.vt) {
        LOG(_T("Capture instance id variant type is %u - expected VT_LPWSTR"), varCaptureInstanceId.vt);
        return __LINE__;
    }
    LOG(_T("Instance Id of default capture device: %ls"), varCaptureInstanceId.pwszVal);

    LOG(_T(""));

    // paydirt
    if (0 == _wcsicmp(varRenderInstanceId.pwszVal, varCaptureInstanceId.pwszVal)) {
        LOG(_T("Default render and capture audio endpoints ARE on the same hardware."));
    } else {
        LOG(_T("Default render and capture audio endpoints ARE NOT on the same hardware."));
    }

    return 0;
}

