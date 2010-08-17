#pragma warning(disable:4710)  // 'function' not inlined (optimization)
#pragma warning(disable:4355)

#define NOTIMPLEMENTED { return E_NOTIMPL; }
#define DUMMYIMPLEMENT { return S_OK; }
#define CHECK_GET_INTERFACE(X) if(riid == IID_ ## X) return GetInterface((X*)this, ppv);
#define RETURN_ON_FAIL(x) if(FAILED(x)) return x;

// Macros to declare methods that forward calls to another object
#define FORWARD0(f, o) f() { return o->f(); }
#define FORWARD1(f, o, t1, a1) f(t1 a1) { return o->f(a1); }
#define FORWARD2(f, o, t1, a1, t2, a2) f(t1 a1, t2 a2) { return o->f(a1, a2); }
#define FORWARD3(f, o, t1, a1, t2, a2, t3, a3) f(t1 a1, t2 a2, t3 a3) { return o->f(a1, a2, a3); }
#define FORWARD4(f, o, t1, a1, t2, a2, t3, a3, t4, a4) f(t1 a1, t2 a2, t3 a3, t4 a4) { return o->f(a1, a2, a3, a4); }
#define FORWARD5(f, o, t1, a1, t2, a2, t3, a3, t4, a4, t5, a5) f(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5) { return o->f(a1, a2, a3, a4, a5); }
#define FORWARD6(f, o, t1, a1, t2, a2, t3, a3, t4, a4, t5, a5, t6, a6) f(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) { return o->f(a1, a2, a3, a4, a5, a6); }

#include <streams.h>
#include <atlbase.h>
#include <atlconv.h>
#include <atlcomcli.h>
#include <olectl.h>
#include <dvdmedia.h>
#include <bdatypes.h>
#include <wmsdk.h>
#include <Dshowasf.h>
#include <initguid.h>

#include <cassert>
#include <cstdio>
#include <map>
#include <vector>
#include <queue>
#include <algorithm>
#include <cctype>       // std::toupper
using namespace std;

#include <initguid.h>
//#include "../MappedFileReader.h"
#include "asyncio.h"
#include "filter.h"



