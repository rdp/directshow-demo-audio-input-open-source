//------------------------------------------------------------------------------
// File: Dump.h
//
// Desc: DirectShow sample code - definitions for dump renderer.
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// Adapted to use CBaseRenderer in November 2008 by The March Hare
//------------------------------------------------------------------------------


class CDump;

#define BYTES_PER_LINE 20
#define FIRST_HALF_LINE TEXT  ("   %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x")
#define SECOND_HALF_LINE TEXT (" %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x")


//  CDump object

class CDump : public IFileSinkFilter,  public CBaseRenderer
{
    CCritSec m_Lock;                // Main renderer critical section

    HANDLE   m_hFile;               // Handle to file for dumping
    LPOLESTR m_pFileName;           // The filename where we dump
    BOOL     m_fWriteError;
	REFERENCE_TIME m_tLast;         // Last sample receive time

public:

    DECLARE_IUNKNOWN

    CDump(LPUNKNOWN pUnk, HRESULT *phr);
    ~CDump();

    static CUnknown * WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

    // Write string, followed by CR/LF, to a file
    void WriteString(TCHAR *pString);

    // Write raw data stream to a file
    HRESULT Write(PBYTE pbData, LONG lDataLength);

    // Implements the IFileSinkFilter interface
    STDMETHODIMP SetFileName(LPCOLESTR pszFileName,const AM_MEDIA_TYPE *pmt);
    STDMETHODIMP GetCurFile(LPOLESTR * ppszFileName,AM_MEDIA_TYPE *pmt);

    // Open and close the file as necessary
    STDMETHODIMP Run(REFERENCE_TIME tStart);
    STDMETHODIMP Pause();
    STDMETHODIMP Stop();
	HRESULT DoRenderSample(IMediaSample *pSample);
	HRESULT CheckMediaType(const CMediaType *);
    // Write detailed information about this sample to a file
    HRESULT WriteStringInfo(IMediaSample *pSample);

private:

    // Overriden to say what interfaces we support where
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);

    // Open and write to the file
    HRESULT OpenFile();
    HRESULT CloseFile();
    HRESULT HandleWriteFailure();
};

