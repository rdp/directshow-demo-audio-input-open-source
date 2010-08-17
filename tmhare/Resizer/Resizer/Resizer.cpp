#include <windows.h>
#include <streams.h>
#include <dvdmedia.h>
#include <initguid.h>
#include "Resizer.h"

//////////////////////////////////////////////////////////////////////////
// Most of this code is 'embraced and extended from the DSWIzard filter wizard 
// generated code. My main contribution is the indentation and the resizing algorithm
// rep.movsd@gmail.com
//////////////////////////////////////////////////////////////////////////
#define TRANSFORM_NAME L"RGB Resizer Filter"

// returns width of row rounded up to modulo 4
int RowWidth(int w) 
{
	if (w % 4) w += 4 - w % 4;
	return w;
}

CResizer::CResizer(TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr) :
    CTransformFilter(tszName, punk, CLSID_Resizer), 
    m_outX(352), m_outY(288), m_bMakeTables(true)
{
} 

CResizer::~CResizer() 
{}

// CreateInstance
// Provide the way for COM to create a Resizer object
CUnknown *CResizer::CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
    CResizer *pNewObject = new CResizer(NAME("Resizer"), punk, phr);
    // Waste to check for NULL, If new fails, the app is screwed anyway
    return pNewObject;
}

// NonDelegatingQueryInterface
// Reveals IResizer
STDMETHODIMP CResizer::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
    if (riid == __uuidof(IResizer)) 
        return GetInterface((IResizer *) this, ppv);

    return CTransformFilter::NonDelegatingQueryInterface(riid, ppv);
}

// Transforms the input and saves results in the the output
HRESULT CResizer::Transform(IMediaSample *pIn, IMediaSample *pOut)
{
    if(m_bMakeTables)
    {
        MakeScaleTables();
        m_bMakeTables = false;
    }

    unsigned char *pDst = 0;
    unsigned char *pSrc = 0;
    pIn->GetPointer((unsigned char **)&pSrc);
    pOut->GetPointer((unsigned char **)&pDst);
    if(m_outY == m_inY && m_outX == m_inX)
        memcpy(pDst, pSrc, pIn->GetSize());
    else
        ScaleBuf(pSrc, pDst);

    LONGLONG MediaStart, MediaEnd;
    REFERENCE_TIME TimeStart, TimeEnd;

    pIn->GetTime(&TimeStart, &TimeEnd);
    pIn->GetMediaTime(&MediaStart,&MediaEnd);
    
    pOut->SetTime(&TimeStart, &TimeEnd);
    pOut->SetMediaTime(&MediaStart,&MediaEnd);
    pOut->SetSyncPoint(pIn->IsSyncPoint() == S_OK);
    pOut->SetPreroll(pIn->IsPreroll() == S_OK);
    pOut->SetDiscontinuity(pIn->IsDiscontinuity() == S_OK);
  
    return NOERROR;
}

// CheckInputType
HRESULT CResizer::CheckInputType(const CMediaType *mtIn)
{
    // check this is a VIDEOINFOHEADER type and RGB
    GUID subtypeIn = *mtIn->Subtype();
    if( *mtIn->FormatType() == FORMAT_VideoInfo &&
        (subtypeIn == MEDIASUBTYPE_RGB555 || subtypeIn == MEDIASUBTYPE_RGB565 ||
        subtypeIn == MEDIASUBTYPE_RGB24 || subtypeIn == MEDIASUBTYPE_RGB32))
    {
        BITMAPINFOHEADER *pbih = &((VIDEOINFOHEADER*)mtIn->Format())->bmiHeader;
        m_inX = pbih->biWidth;
        m_inY = pbih->biHeight;
        m_bMakeTables = true;
        return S_OK;
    }
    return VFW_E_INVALID_MEDIA_TYPE;
}

// Checktransform
// Check a transform can be done between these formats
HRESULT CResizer::CheckTransform(const CMediaType *mtIn, const CMediaType *mtOut)
{
    GUID subtypeIn = *mtIn->Subtype();
    GUID subtypeOut = *mtOut->Subtype();

    if(subtypeIn == subtypeOut && (subtypeIn == MEDIASUBTYPE_RGB555 || subtypeIn == MEDIASUBTYPE_RGB565 || 
       subtypeIn == MEDIASUBTYPE_RGB24 || subtypeIn == MEDIASUBTYPE_RGB32) )
    return S_OK;

    return VFW_E_INVALID_MEDIA_TYPE;
}

// DecideBufferSize
// Tell the output pin's allocator what size buffers we
// require. Can only do this when the input is connected
HRESULT CResizer::DecideBufferSize(IMemAllocator *pAlloc,ALLOCATOR_PROPERTIES *pProperties)
{
    // Is the input pin connected
    if (!m_pInput->IsConnected()) 
        return E_UNEXPECTED;

    HRESULT hr = NOERROR;

    CMediaType *inMediaType = &m_pInput->CurrentMediaType();
    VIDEOINFOHEADER *vihIn = (VIDEOINFOHEADER *)inMediaType->Format();
    m_bytesPerPixel = vihIn->bmiHeader.biBitCount / 8;

    pProperties->cBuffers = 1;
    pProperties->cbBuffer = RowWidth(m_outX) * m_outY * m_bytesPerPixel;

    ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pProperties,&Actual);
    if (FAILED(hr)) return hr;
    if (pProperties->cBuffers > Actual.cBuffers || pProperties->cbBuffer > Actual.cbBuffer) 
        return E_FAIL;

    return NOERROR;
}

// GetMediaType
// Returns the supported media types in order of preferred  types (starting with iPosition=0)
HRESULT CResizer::GetMediaType(int iPosition, CMediaType *pMediaType)
{
    // Is the input pin connected
    if (!m_pInput->IsConnected()) 
        return E_UNEXPECTED;

    if (iPosition < 0)
        return E_INVALIDARG;

    // Do we have more items to offer
    if (iPosition > 0)
        return VFW_S_NO_MORE_ITEMS;

	// get input dimensions
	CMediaType *inMediaType = &m_pInput->CurrentMediaType();
    VIDEOINFOHEADER *vihIn = (VIDEOINFOHEADER*)inMediaType->Format();
    m_bytesPerPixel = vihIn->bmiHeader.biBitCount / 8;
	pMediaType->SetFormatType(&FORMAT_VideoInfo);
	pMediaType->SetType(&MEDIATYPE_Video);
	pMediaType->SetSubtype(inMediaType->Subtype());
	pMediaType->SetSampleSize(RowWidth(m_outX) * m_outY * m_bytesPerPixel);
	pMediaType->SetTemporalCompression(FALSE);
	VIDEOINFOHEADER *vihOut = (VIDEOINFOHEADER *)pMediaType->ReallocFormatBuffer(sizeof(VIDEOINFOHEADER));

	// set VIDEOINFOHEADER
	memset(vihOut, 0, sizeof(VIDEOINFOHEADER));
	double frameRate = vihIn->AvgTimePerFrame / 10000000.0;
	vihOut->dwBitRate = (int)(frameRate * m_outY * m_outX * m_bytesPerPixel);
	vihOut->AvgTimePerFrame = vihIn->AvgTimePerFrame;

	// set BITMAPINFOHEADER
	vihOut->bmiHeader.biBitCount = m_bytesPerPixel * 8;
	vihOut->bmiHeader.biCompression = BI_RGB;
	vihOut->bmiHeader.biHeight = m_outY;
    vihOut->bmiHeader.biWidth = m_outX;
	vihOut->bmiHeader.biPlanes = 1;
	vihOut->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	vihOut->bmiHeader.biSizeImage = RowWidth(m_outX) * m_outY * m_bytesPerPixel;

    return NOERROR;
}

//////////////////////////////////////////////////////////////////////////
// Build the lookup tables used for scaling the input into the output
// For each destination pixel x and y the corresponding offset in the
// source buffer is stored
//////////////////////////////////////////////////////////////////////////
void CResizer::MakeScaleTables()
{
    double xStep = (double)m_inX / m_outX;
    double yStep = (double)m_inY / m_outY;

    int inRowBytes = RowWidth(m_inX * m_bytesPerPixel);
    int inY, inX;
    for(int y = 0; y < m_outY; ++y)    
    {
        inY = y * yStep;
        m_iSrcScanLine[y] = (inY * inRowBytes);
    }

    for(int x = 0; x < m_outX; ++x)
    {
        inX = x * xStep;
        m_iSrcPixelOffset[x] = (inX * m_bytesPerPixel);
    }
}

//////////////////////////////////////////////////////////////////////////
// This method scales the bitmap data from the input to output buffer
// When either of the formats change, the tables are rebuilt
//////////////////////////////////////////////////////////////////////////
void CResizer::ScaleBuf(BYTE *in, BYTE *out)
{
    UCHAR *dstPixel = out;
    UCHAR *srcScanLine, *srcPixel;
    // separate loops for each bpp
    for(int y = 0; y < m_outY; ++y)
    {
        srcScanLine = in + m_iSrcScanLine[y];
        switch(m_bytesPerPixel)
        {
            case 4: 
            for(int x = 0; x < m_outX; ++x)
            {
                srcPixel = srcScanLine + m_iSrcPixelOffset[x];
                *dstPixel++ = *srcPixel++;
                *dstPixel++ = *srcPixel++;
                *dstPixel++ = *srcPixel++;
                *dstPixel++ = *srcPixel++;
            }
            break;

            case 3: 
            for(int x = 0; x < m_outX; ++x)
            {
                srcPixel = srcScanLine + m_iSrcPixelOffset[x];
                *dstPixel++ = *srcPixel++;
                *dstPixel++ = *srcPixel++;
                *dstPixel++ = *srcPixel++;
            }
            break;
                
            case 2: 
            for(int x = 0; x < m_outX; ++x)
            {
                srcPixel = srcScanLine + m_iSrcPixelOffset[x];
                *dstPixel++ = *srcPixel++;
                *dstPixel++ = *srcPixel++;
            }
            break;
                
            case 1: 
            for(int x = 0; x < m_outX; ++x)
            {
                srcPixel = srcScanLine + m_iSrcPixelOffset[x];
                *dstPixel++ = *srcPixel++;
            }
            break;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
// IResizer
//////////////////////////////////////////////////////////////////////////
STDMETHODIMP CResizer::SetOutput(int x, int y)
{
    CAutoLock cAutolock(&m_ResizerLock);
    m_outX = x;
    m_outY = y;
    return NOERROR;
}
