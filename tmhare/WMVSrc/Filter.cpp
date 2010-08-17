#include "stdafx.h"
#include <initguid.h>
using namespace std;

#define TRY(hr, X) {hr = X; if(!SUCCEEDED(hr)) throw hr;}

// Setup data for filter registration
const AMOVIESETUP_MEDIATYPE sudOpPinTypes =
{ 
    &MEDIATYPE_Stream,
    &MEDIASUBTYPE_NULL 
};

const AMOVIESETUP_PIN sudOpPin =
{ 
    L"Output",
    FALSE,
    TRUE,
    FALSE,
    FALSE,
    &CLSID_NULL,
    0,
    1,
    &sudOpPinTypes 
};

const AMOVIESETUP_FILTER sudAsync =
{ 
    &CLSID_ASFStreamSrc,
    L"ASF IStream source",
    MERIT_UNLIKELY,
    1,
    &sudOpPin
};

CFactoryTemplate g_Templates[1] = 
{
    {
        L"ASF IStream source",
        &CLSID_ASFStreamSrc,
        CEncSrc::CreateInstance,
        NULL,
        &sudAsync
    }
};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

////////////////////////////////////////////////////////////////////////
// Exported entry points 
////////////////////////////////////////////////////////////////////////

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

STDAPI DllRegisterServer()
{
    return AMovieDllRegisterServer2(TRUE);
}
//////////////////////////////////////////////////////////////////////////

STDAPI DllUnregisterServer()
{
    return AMovieDllRegisterServer2(FALSE);
}
//////////////////////////////////////////////////////////////////////////

BOOL APIENTRY DllMain(HANDLE hModule, DWORD  dwReason, LPVOID lpReserved)
{
    return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CEncSrc
//////////////////////////////////////////////////////////////////////////
// Constructor
CEncSrc::CEncSrc(LPUNKNOWN pUnk, HRESULT *phr) :
    CSource(NAME("Encrypted File Source"), pUnk, CLSID_ASFStreamSrc), 
    CSourceSeeking(NAME("Seek object"), pUnk, phr, &m_cStateLock), 
    m_bActive(FALSE), m_bPaused(FALSE), m_bSeek(FALSE), m_bEOF(FALSE), m_Helper(this)
{
    CAutoLock cAutoLock(&m_cStateLock);
    m_hAsyncEvent = CreateEvent(NULL, FALSE, FALSE, 0);     // Create an event for handling asynchronous calls
    *phr = WMCreateReader(NULL, 0, &m_pReader);             // Create WM reader
}
//////////////////////////////////////////////////////////////////////////

// Dtor
CEncSrc::~CEncSrc()
{
    Cleanup();
    CloseHandle(m_hAsyncEvent);                     // Free the event 
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// IUnknown
//////////////////////////////////////////////////////////////////////////
CUnknown* WINAPI CEncSrc::CreateInstance(LPUNKNOWN pUnk, HRESULT *phr)
{
    ASSERT(phr);
    return (CUnknown*)(CSource*)(new CEncSrc(pUnk, phr));
}
//////////////////////////////////////////////////////////////////////////

STDMETHODIMP CEncSrc::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
    CHECK_GET_INTERFACE(IFileSourceFilter);
    CHECK_GET_INTERFACE(IMediaSeeking);
    return CSource::NonDelegatingQueryInterface(riid, ppv);
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// IFileSourceFilter
//////////////////////////////////////////////////////////////////////////
STDMETHODIMP CEncSrc::Load(LPCOLESTR lpwszFileName, const AM_MEDIA_TYPE *pmt)
{
    CAutoLock lck(&m_cStateLock);
    USES_CONVERSION;
    CheckPointer(lpwszFileName, E_POINTER);
    HRESULT hRet = S_OK;
    
    try
    {
        IWMReaderCallback* pWMCallback = (IWMReaderCallback*)&m_Helper;

        // Following code can be used to make the reader use an IStream rather than opening a file
//         CComQIPtr<IWMReaderAdvanced2> pAdv2(m_pReader);         // Get the advanced reader interface
//         CComQIPtr<IStream> pStream(m_pPlugin);                  // Get the IStream of the decryptor plugin object
//         TRY(hRet, pAdv2->OpenStream(pStream, pWMCallback, 0));  // Set the reader to use the IStream

        m_pReader->Open(lpwszFileName, pWMCallback, 0);

        
        // Wait for 5 seconds at most to open 
        if(WaitForSingleObject(m_hAsyncEvent, INFINITE) == WAIT_TIMEOUT)
            throw RPC_E_TIMEOUT;
        TRY(hRet, m_hrAsync);           // Test for error
        GetWMStreamsAndMediaTypes();    // Get the media types for each stream
        CreateWMPins();                 // Create a pin for each stream
        GetWMVDuration(lpwszFileName);  // Get duration and store it

        m_File = lpwszFileName;
    }
    catch(HRESULT h)
    {
        hRet = h;
    }

    return hRet;
}
//////////////////////////////////////////////////////////////////////////

STDMETHODIMP CEncSrc::GetCurFile(LPOLESTR * ppszFileName, AM_MEDIA_TYPE *pmt)
{
    CheckPointer(ppszFileName, E_POINTER);
    *ppszFileName = NULL;
    wstring wsName = m_File;
    if(!wsName.empty()) 
    {
        DWORD n = sizeof(WCHAR)*(1 + wsName.size());
        *ppszFileName = (LPOLESTR) CoTaskMemAlloc( n );
        CopyMemory(*ppszFileName, wsName.c_str(), n);
    }

//     if(pmt)
//         CopyMediaType(&m_mt);

    return NOERROR;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
//IMediaFilter
//////////////////////////////////////////////////////////////////////
STDMETHODIMP CEncSrc::Run(REFERENCE_TIME tStart)
{
    CAutoLock lock(&m_cStateLock);
    if(m_bPaused)
        m_pReader->Resume();            // If we are running after a pause, resume the reader
    return CSource::Run(tStart);
}
//////////////////////////////////////////////////////////////////////

STDMETHODIMP CEncSrc::Pause()
{
    CAutoLock lock(&m_cStateLock);
    if(m_bActive)
    {
        m_pReader->Pause();     // If we are paused while running, pause the reader
        m_bPaused = TRUE;
    }

    return CSource::Pause();
}
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// IWMReaderCallback 
//////////////////////////////////////////////////////////////////////////
STDMETHODIMP CEncSrc::OnSample(DWORD dwOutputNum, QWORD qwSampleTime, QWORD qwSampleDuration, DWORD dwFlags, INSSBuffer *pSample, void *pvContext)        
{
    // Get the pin which is associated with this stream and queue the sample
    CEncWMVPin *pPin = (CEncWMVPin *)m_paStreams[dwOutputNum];
    if(pPin->IsConnected())
    {
        CAutoLock lock(&pPin->m_cLock);
        pSample->AddRef();                                      // Add a reference to the sample so its alive till we use it
        pPin->m_Samples.push(WMSample(dwFlags, pSample));  // Queue the sample
    }
    return S_OK;
}
//////////////////////////////////////////////////////////////////////////

STDMETHODIMP CEncSrc::OnStatus(WMT_STATUS Status, HRESULT hr, WMT_ATTR_DATATYPE dwType, BYTE *pValue, void *pvContext) 
{
    // Flag to indicate whether we fire the event
    bool bFireEvent = false;
    switch(Status)
    {

    case WMT_CLOSED:            // On open and close and stop , just fire the event
    case WMT_OPENED:
    case WMT_STOPPED:
        bFireEvent = true;
        break;

    case WMT_STARTED:           // On start reset EOF flag
        m_bEOF = FALSE;
        break;

    case WMT_EOF:               // On error or EOF fire the event
        if(!m_bEOF)
            m_bEOF = TRUE;
        bFireEvent = true;
        break;
    }

    if(bFireEvent)
    {
        m_hrAsync = hr;
        SetEvent(m_hAsyncEvent);
    }

    return S_OK;
}
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// CSourceSeeking
//////////////////////////////////////////////////////////////////////////
HRESULT CEncSrc::ChangeStart()
{
    if(m_State == State_Paused)         // If we are streaming
    {
        m_pReader->Pause();             // Pause the reader object
        m_bSeek = TRUE;                 // Signal that we are in a seek

        // Flush the pins ( as per CSourceseeking help in Dshow docs )
        for(size_t i = 0; i < m_mtArray.size(); ++i)
        {
            CEncWMVPin* pPin = (CEncWMVPin*)m_paStreams[i];
            if(pPin->ThreadExists()) 
            {
                pPin->DeliverBeginFlush();      // Send flush request downstream
                pPin->Stop();                   // Stop streaming
                pPin->DeliverEndFlush();        // Send flush stop request
                pPin->Run();                    // Resume streaming
            }
        }

        m_pReader->Start(m_rtStart, 0, 1.0, 0); // Start the reader at the new position   
        m_bSeek = FALSE;                        // Signal that we are done seeking
    }
    return S_OK;
}
//////////////////////////////////////////////////////////////////////////


// Delete the pins and free media types
void CEncSrc::Cleanup()
{
    for(size_t i = 0; i < m_mtArray.size(); ++i) 
        CoTaskMemFree(m_mtArray[i]);                // Free all the pin media types
}

//////////////////////////////////////////////////////////////////////////
// Wmv specific methods 
//////////////////////////////////////////////////////////////////////////

// Get file duration from meta data
void CEncSrc::GetWMVDuration(LPCOLESTR pwszFileName)
{
    HRESULT hr;
    CComQIPtr<IWMHeaderInfo3> pHdrInfo(m_pReader);   // Get header info 3 interface

    WORD wStream = 0;                          // Get the duration
    WMT_ATTR_DATATYPE attrType = WMT_TYPE_QWORD;    
    WORD wSize = sizeof(QWORD);
    TRY(hr, pHdrInfo->GetAttributeByName(&wStream, g_wszWMDuration, &attrType, (BYTE*)&m_rtDuration, &wSize));
}
//////////////////////////////////////////////////////////////////////////

// Get the WM media types for each stream
void CEncSrc::GetWMStreamsAndMediaTypes()
{
    CAutoLock cAutoLock(&m_cStateLock);
    CComQIPtr<IWMProfile> pProf(m_pReader);
    HRESULT hr;
    DWORD dwNumStreams;

    pProf->GetStreamCount(&dwNumStreams);           // Get streams count
    for(size_t i = 0; i < dwNumStreams; ++i)        // Iterate over each stream
    {
        CComPtr<IWMStreamConfig> pStreamConfig;     // Get IWMStreamConfig interface
        pProf->GetStream(i, &pStreamConfig);

        CComPtr<IWMOutputMediaProps> pOutProps;     // Save the media type
        m_pReader->GetOutputProps(i, &pOutProps);

        if(pOutProps)
        {
            WORD cbName;                                // Get the name of the stream and save it
            wstring wsName;
            TRY(hr, pStreamConfig->GetStreamName(NULL, &cbName));
            wsName.resize(cbName);
            pStreamConfig->GetStreamName((WCHAR*)wsName.data(), &cbName);
            m_PinNames.push_back(wsName);

            DWORD cbSize;
            AM_MEDIA_TYPE *pmt;
            TRY(hr, pOutProps->GetMediaType(NULL, &cbSize));
            pmt = (AM_MEDIA_TYPE *)CoTaskMemAlloc(cbSize);
            TRY(hr, pOutProps->GetMediaType((WM_MEDIA_TYPE*)pmt, &cbSize));
            m_mtArray.push_back(pmt);
        }
    }
}

//////////////////////////////////////////////////////////////////////////

// Create a pin for each stream
void CEncSrc::CreateWMPins()
{
    CAutoLock cAutoLock(&m_cStateLock);
    m_paStreams = (CSourceStream **) new CEncWMVPin*[m_mtArray.size()];      // Allocate pin array

    CComQIPtr<IWMReaderAdvanced> pAdv(m_pReader);
    for(size_t i = 0; i < m_mtArray.size(); ++i)
    {
        HRESULT hrDummy;

        // Create a pin, (its added automatically to the array)
        CEncWMVPin *pPin = new CEncWMVPin(&hrDummy, this, (LPCWSTR)m_PinNames[i].c_str(), m_mtArray[i], i == 0);;
        if(i == 0)
            m_pFirstPin = pPin;

        // Get the maximum possible sample size 
        pAdv->GetMaxOutputSampleSize(i, &pPin->m_dwSampleSize);
    }
}
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// CEncWMVPin
//////////////////////////////////////////////////////////////////////////

CEncWMVPin::CEncWMVPin(HRESULT *phr, CEncSrc *pParent, LPCWSTR pPinName, AM_MEDIA_TYPE *pmt, BOOL bMaster) :
CSourceStream(NAME("WMV Source pin"),phr, pParent, pPinName), m_bMaster(bMaster)
{
    CAutoLock lock(&m_cLock);
    m_pParent = pParent;
    m_mt = *pmt;
}
//////////////////////////////////////////////////////////////////////////

// Dtor
CEncWMVPin::~CEncWMVPin()
{
    CAutoLock lock(&m_cLock);
    while(!m_Samples.empty())               // Discard any samples left in the queue
    {
        WMSample s = m_Samples.front();
        m_Samples.pop();
        s.pINSSBuffer->Release();
    }
}
//////////////////////////////////////////////////////////////////////////

// Pump the data 
HRESULT CEncWMVPin::FillBuffer(IMediaSample *pms)
{
    // If there are no queued samples and the graph is running , then wait for them 
    // unless its EOF or a seek
    while(m_Samples.empty() && m_pParent->m_bActive && !m_pParent->m_bSeek &&  !m_pParent->m_bEOF) 
        Sleep(50);

    // If its a seek then exit
    if(m_pParent->m_bSeek) 
    {
        pms->SetActualDataLength(0);
        pms->SetDiscontinuity(TRUE);
        return NOERROR;
    }

    // If graph is inactive stop cueing samples
    if(!m_pParent->m_bActive || m_pParent->m_bEOF) 
        return S_FALSE;

    // Get the critical section
    WMSample s;
    {
        CAutoLock lock(&m_cLock);
        // Dequeue the sample
        s = m_Samples.front();
        m_Samples.pop();
    }

    DWORD dwLen;
    BYTE *pDataDest, *pDataSrc;

    // Get the buffer and size 
    if(SUCCEEDED(s.pINSSBuffer->GetBufferAndLength(&pDataSrc, &dwLen)))
    {
        // Copy the sample
        pms->GetPointer(&pDataDest);
        memcpy(pDataDest, pDataSrc, dwLen);

        // Set length, flags and timestamp ( no timestamp works best!! )
        pms->SetActualDataLength(dwLen);
        pms->SetDiscontinuity((s.dwFlags & WM_SF_DISCONTINUITY));
        pms->SetSyncPoint(s.dwFlags & WM_SF_CLEANPOINT);
        pms->SetTime(NULL, NULL);
    }

    // Release the WM sample (we had AddRef'd it in OnSample())
    s.pINSSBuffer->Release();

    return NOERROR;
}
//////////////////////////////////////////////////////////////////////////

// See Directshow help topic for IAMStreamConfig for details on this method
HRESULT CEncWMVPin::GetMediaType(int iPosition, CMediaType *pmt)
{
    if(!iPosition) 
    {
        *pmt = m_mt;
        return S_OK;
    }
    return VFW_S_NO_MORE_ITEMS;
}
//////////////////////////////////////////////////////////////////////////

// This method is called to see if a given output format is supported
HRESULT CEncWMVPin::CheckMediaType(const CMediaType *pmt)
{
    if(*pmt == m_mt) // we accept the one and only media type
        return S_OK;
    else
        return S_FALSE;
}
//////////////////////////////////////////////////////////////////////////

// This method is called after the pins are connected to allocate buffers to stream data
HRESULT CEncWMVPin::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties)
{
    CAutoLock cAutoLock(m_pFilter->pStateLock());
    HRESULT hr = NOERROR;

    pProperties->cBuffers = 5;
    pProperties->cbBuffer = m_dwSampleSize;

    ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pProperties,&Actual);

    if(FAILED(hr)) return hr;
    if(Actual.cbBuffer < pProperties->cbBuffer) return E_FAIL;

    return NOERROR;
}
//////////////////////////////////////////////////////////////////////////

HRESULT CEncWMVPin::Active()
{
    CAutoLock lock(&m_cLock);        // Grab the critical section
    m_pParent->m_bActive = TRUE;            // Signal that we are active and streaming
    if(m_bMaster)                           // If we are the master pin then start the reader object
        m_pParent->m_pReader->Start(m_pParent->m_rtStart, 0, 1.0, 0);
    return CSourceStream::Active();         // Call the baseclass 
}

//////////////////////////////////////////////////////////////////////////

HRESULT CEncWMVPin::Inactive()
{
    if(m_bMaster)                       // If we are the master pin then stop the reader
        m_pParent->m_pReader->Stop();
    m_pParent->m_bActive = FALSE;       // Signal that we are inactive
    return CSourceStream::Inactive();   // Call the baseclass 
}

//////////////////////////////////////////////////////////////////////////

// Deliver a new segment after seek ( as explained in Dshow docs )
HRESULT CEncWMVPin::OnThreadStartPlay()
{
    return DeliverNewSegment(m_pParent->m_rtStart, m_pParent->m_rtDuration, 1.0);
}
