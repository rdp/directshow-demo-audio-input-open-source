#pragma once

// {5C32760E-7184-4d15-AD56-67FA8BC1F8EB}
DEFINE_GUID(CLSID_ASFStreamSrc, 0x5c32760e, 0x7184, 0x4d15, 0xad, 0x56, 0x67, 0xfa, 0x8b, 0xc1, 0xf8, 0xeb);

class CEncWMVPin;

// Struct for queued WMV samples
struct WMSample
{
    DWORD dwFlags;
    INSSBuffer *pINSSBuffer;
    WMSample(DWORD adwFlags, INSSBuffer *apINSSBuffer) :dwFlags(adwFlags), pINSSBuffer(apINSSBuffer) {}
    WMSample() {}
};
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Helpers to prevent circular reference between wm reader and filter
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////

// Class which implements IWMReaderCallback forwarding the calls to the filter 
// Filter holds a reference to the WM reader and the WM reader holds a reference to IWMReaderCallback
// THis causes a circular reference and objects leak on exit
// Using this class as a proxy avoids that problem
class CWMVSrcHelper : public CUnknown, public IWMReaderCallback
{
    IWMReaderCallback *m_pFilter;

public :
    CWMVSrcHelper(IWMReaderCallback *pFilter):CUnknown(NAME("WMV Src Helper object"), 0), m_pFilter(pFilter) 
    {
        AddRef();       // AddRef once
    }

    //////////////////////////////////////////////////////////////////////////
    // IUnknown
    //////////////////////////////////////////////////////////////////////////
    DECLARE_IUNKNOWN;
    // Expose IWMReaderCallback
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv) 
    {
        CHECK_GET_INTERFACE(IWMReaderCallback);
        return CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }

    //////////////////////////////////////////////////////////////////////////
    // IWMReaderCallback - Forward calls to the filter
    //////////////////////////////////////////////////////////////////////////
    HRESULT STDMETHODCALLTYPE FORWARD6(OnSample, m_pFilter, DWORD, dwOutputNum, QWORD, cnsSampleTime, QWORD, cnsSampleDuration, DWORD, dwFlags, INSSBuffer*, pSample, void*, pvContext);
    HRESULT STDMETHODCALLTYPE FORWARD5(OnStatus, m_pFilter, WMT_STATUS, Status, HRESULT, hr, WMT_ATTR_DATATYPE, dwType, BYTE*, pValue, void*, pvContext);
};
//////////////////////////////////////////////////////////////////////////


// Source filter implementing push source for WMV 
class CEncSrc : virtual public CSource, virtual public CSourceSeeking, public IFileSourceFilter, public IWMReaderCallback
{
    friend class CEncWMVPin;

    //CComPtr<IUnknown> m_pStream;            // Pointer to the IStream
    wstring m_File;
    void Cleanup();

    //////////////////////////////////////////////////////////////////////////
    // WMV source specific stuff
    //////////////////////////////////////////////////////////////////////////
    HANDLE m_hAsyncEvent;                       // Event signaled when the WM reader completes opening
    HRESULT m_hrAsync;                          // Return value from the WM reader 
    BOOL m_bEOF, m_bActive, m_bPaused, m_bSeek; // Flags to indicate EOF, whether the filter is streaming or paused and whether we are in a seek
    vector<AM_MEDIA_TYPE*> m_mtArray;           // Array of Media types 
    vector<wstring> m_PinNames;                 // Name for each stream
    CEncWMVPin *m_pFirstPin;                    // Pointer to the first pin which we call the master pin
    CWMVSrcHelper m_Helper;                     // Callback helper object to prevent circular reference
    CComPtr<IWMReader> m_pReader;               // WMV reader object
    void GetWMVDuration(LPCOLESTR pwszFile);    // Gets the duration of the file 
    void GetWMStreamsAndMediaTypes();           // Gets the media types in the WMV
    void CreateWMPins();                        // Creates a pin for each stream

public:
     CEncSrc(LPUNKNOWN pUnk, HRESULT *phr);
    ~CEncSrc();

    //////////////////////////////////////////////////////////////////////////
    //  IUnknown
    //////////////////////////////////////////////////////////////////////////
    // We cant use DECLARE_IUNKNOWN since we have 2 CUnknown parent classes from CSource and CSourceSeeking
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) { return CSource::GetOwner()->QueryInterface(riid,ppv); }
    STDMETHODIMP_(ULONG) AddRef() { return CSource::GetOwner()->AddRef(); }
    STDMETHODIMP_(ULONG) Release() { return CSource::GetOwner()->Release(); }

    static CUnknown* WINAPI CreateInstance(LPUNKNOWN, HRESULT *);
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv);

    //////////////////////////////////////////////////////////////////////////
    //  IFileSourceFilter 
    //////////////////////////////////////////////////////////////////////////
    STDMETHODIMP Load(LPCOLESTR lpwszFileName, const AM_MEDIA_TYPE *pmt);
    STDMETHODIMP GetCurFile(LPOLESTR *ppszFileName, AM_MEDIA_TYPE *pmt);

    //////////////////////////////////////////////////////////////////////////
    // IMediaFilter 
    //////////////////////////////////////////////////////////////////////////
    STDMETHODIMP Run(REFERENCE_TIME rtStart);
    STDMETHODIMP Pause();

    //////////////////////////////////////////////////////////////////////////
    // IWMReaderCallback called by the helper object for push mode
    //////////////////////////////////////////////////////////////////////////
    STDMETHODIMP OnSample(DWORD dwOutputNum, QWORD cnsSampleTime, QWORD cnsSampleDuration, DWORD dwFlags, INSSBuffer *pSample, void *pvContext);
    STDMETHODIMP OnStatus(WMT_STATUS Status, HRESULT hr, WMT_ATTR_DATATYPE dwType, BYTE *pValue, void *pvContext);

    //////////////////////////////////////////////////////////////////////////
    // CSourceSeeking 
    //////////////////////////////////////////////////////////////////////////
    HRESULT ChangeStart();
    HRESULT ChangeStop() DUMMYIMPLEMENT;
    HRESULT ChangeRate() DUMMYIMPLEMENT;
    
};
//////////////////////////////////////////////////////////////////////////

// WMV Push pin implementation
class CEncWMVPin : public CSourceStream
{
public:
    CCritSec m_cLock;           // Critical section
    CEncSrc *m_pParent;         // parent filter
    queue<WMSample> m_Samples;  // queue of samples
    BOOL m_bMaster;             // Flag indicating if its the master pin 
    DWORD m_dwSampleSize;       // Maximum sample size, used to decide buffer size

    CEncWMVPin(HRESULT *phr, CEncSrc *pParent, LPCWSTR pPinName, AM_MEDIA_TYPE *pmt, BOOL bMaster);
    ~CEncWMVPin();

    //////////////////////////////////////////////////////////////////////////
    //  CSourceStream overrides
    //////////////////////////////////////////////////////////////////////////
    HRESULT FillBuffer(IMediaSample *pms);
    HRESULT DecideBufferSize(IMemAllocator *pIMemAlloc, ALLOCATOR_PROPERTIES *pProperties);
    HRESULT CheckMediaType(const CMediaType *pMediaType);
    HRESULT GetMediaType(int iPosition, CMediaType *pmt);
    HRESULT Active(void); 
    HRESULT Inactive();
    HRESULT OnThreadStartPlay(void);
};
//////////////////////////////////////////////////////////////////////////

