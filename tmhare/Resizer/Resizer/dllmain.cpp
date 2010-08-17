#ifdef _DEBUG
#pragma comment( lib, "strmbasd" )
#else
#pragma comment( lib, "strmbase" )
#endif

#pragma comment( lib, "strmiids" )
#pragma comment( lib, "winmm" )

#include <streams.h>
#include <initguid.h>
#include "Resizer.h"

//////////////////////////////////////////////////////////////////////////
// This file contains the standard COM glue code required for registering the 
// resizer filter 
//////////////////////////////////////////////////////////////////////////

#define g_wszResizer L"RGB Resizer"
// Filter setup data
const AMOVIESETUP_MEDIATYPE sudPinTypes = { &MEDIATYPE_Video, &MEDIASUBTYPE_NULL};

const AMOVIESETUP_PIN sudResizerPins[] =
{
    { 
        L"Input",             // Pins string name
        FALSE,                // Is it rendered
        FALSE,                // Is it an output
        FALSE,                // Are we allowed none
        FALSE,                // And allowed many
        &CLSID_NULL,          // Connects to filter
        NULL,                 // Connects to pin
        1,                    // Number of types
        &sudPinTypes          // Pin information
    },
    { 
        L"Output",            // Pins string name
        FALSE,                // Is it rendered
        TRUE,                 // Is it an output
        FALSE,                // Are we allowed none
        FALSE,                // And allowed many
        &CLSID_NULL,          // Connects to filter
        NULL,                 // Connects to pin
        1,                    // Number of types
        &sudPinTypes          // Pin information
    }
};

const AMOVIESETUP_FILTER sudResizer =
{
    &CLSID_Resizer,	// Filter CLSID
    g_wszResizer,				// String name
    MERIT_DO_NOT_USE,			// Filter merit
    2,							// Number of pins
    sudResizerPins					// Pin information
};


CFactoryTemplate g_Templates[] = 
{
    { g_wszResizer, &CLSID_Resizer, CResizer::CreateInstance, NULL, &sudResizer }
};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);    

////////////////////////////////////////////////////////////////////////
// Exported entry points for registration and unregistration 
// (in this case they only call through to default implementations).
////////////////////////////////////////////////////////////////////////

STDAPI DllRegisterServer()
{
    return AMovieDllRegisterServer2( TRUE );
}

STDAPI DllUnregisterServer()
{
    return AMovieDllRegisterServer2( FALSE );
}

//
// DllEntryPoint
//
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, 
                      DWORD  dwReason, 
                      LPVOID lpReserved)
{
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}

