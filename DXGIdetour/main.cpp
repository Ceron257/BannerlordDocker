#include <windows.h>
#include <detours.h>
#include <dxgi.h>

#include <atomic>
#include <iostream>
#include <mutex>

using namespace std;

HRESULT (WINAPI *CreateDXGIFactory1_original) (REFIID riid, _COM_Outptr_ void **ppFactory) = CreateDXGIFactory1;

mutex sEnumAdaptersDetouredLock;
atomic<bool> sEnumAdaptersDetoured = false;
mutex sGetDescDetouredLock;
atomic<bool> sGetDescDetoured = false;
atomic<bool> sOverwriteVendorID = true;

auto static constexpr EnumAdaptersVFTableIndex = 7;
auto static constexpr GetDesc1VFTableIndex = 8;

auto static constexpr MicrosoftVendorID = 0x1414;
auto static constexpr NvidiaVendorID = 0x10de;

class DXGIFactoryDetour
{
  public:
    HRESULT STDMETHODCALLTYPE Mine_EnumAdapters( 
            UINT Adapter,
            _COM_Outptr_  IDXGIAdapter **ppAdapter);
    
    using DetouredEnumAdapters = HRESULT (STDMETHODCALLTYPE DXGIFactoryDetour::*)(
            UINT Adapter,
            _COM_Outptr_  IDXGIAdapter **ppAdapter);

    static DetouredEnumAdapters Real_EnumAdapters;
};

class IDXGIAdapter1Detour
{
  public:
    HRESULT STDMETHODCALLTYPE Mine_GetDesc1( 
            _Out_  DXGI_ADAPTER_DESC1 *pDesc);
    
    using IDXGIAdapter1GetDesc = HRESULT (STDMETHODCALLTYPE IDXGIAdapter1Detour::*)(
            _Out_  DXGI_ADAPTER_DESC1 *pDesc);

    static IDXGIAdapter1GetDesc Real_GetDesc1;
};

DXGIFactoryDetour::DetouredEnumAdapters DXGIFactoryDetour::Real_EnumAdapters = nullptr;
IDXGIAdapter1Detour::IDXGIAdapter1GetDesc IDXGIAdapter1Detour::Real_GetDesc1 = nullptr;

void detourGetDesc(void* vtablePtr)
{
  lock_guard<mutex> guard (sGetDescDetouredLock);
  if (sGetDescDetoured)
    return;
  sGetDescDetoured = true;
  
  void* vtable = *reinterpret_cast<void**>(vtablePtr);
  void* functionPtr = reinterpret_cast<void**>(vtable)[GetDesc1VFTableIndex];
  IDXGIAdapter1Detour::Real_GetDesc1 = 
      reinterpret_cast<IDXGIAdapter1Detour::IDXGIAdapter1GetDesc&> (functionPtr);

  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());
  auto myGetDesc = &IDXGIAdapter1Detour::Mine_GetDesc1;
  LONG error = DetourAttach(&(PVOID&)IDXGIAdapter1Detour::Real_GetDesc1,
  *(PBYTE*)&myGetDesc);
  if (error != NO_ERROR)
    cout << "Error during GetDesc1() attach: " << error << endl;
  error = DetourTransactionCommit();

#if DETOURS_64BIT
  cout << "----------------: " << endl;
#else
  cout << "--------: " << endl;
#endif

  if (error == NO_ERROR)
    cout << "Detoured GetDesc1() successfully" << endl;
  else
    cout << "Error detouring GetDesc1(): " << error << endl;
}


HRESULT STDMETHODCALLTYPE DXGIFactoryDetour::Mine_EnumAdapters( 
            UINT Adapter,
            _COM_Outptr_  IDXGIAdapter **ppAdapter)
{
    cout << "  DXGIFactoryDetour::EnumAdapters was called! (this: " << this << ")" << endl;
    IDXGIAdapter* microsoftAdapter = nullptr;
    UINT i = 0;
    sOverwriteVendorID = false; // we don't want to overwrite our calls
    while ((this->*Real_EnumAdapters)(i, &microsoftAdapter) != DXGI_ERROR_NOT_FOUND)
    {
      DXGI_ADAPTER_DESC desc;
      if (microsoftAdapter->GetDesc(&desc) == NO_ERROR &&
          desc.VendorId == MicrosoftVendorID)
      {
        break;
      }
      ++i;
    }
    if (microsoftAdapter != nullptr)
      *ppAdapter = microsoftAdapter;
    sOverwriteVendorID = true;
    detourGetDesc(microsoftAdapter);
    return Adapter < 1 ? NO_ERROR : DXGI_ERROR_NOT_FOUND;
}

HRESULT STDMETHODCALLTYPE IDXGIAdapter1Detour::Mine_GetDesc1(
            _Out_  DXGI_ADAPTER_DESC1 *pDesc)
{
  auto result = (this->*Real_GetDesc1) (pDesc);
  if (sGetDescDetoured && pDesc != nullptr)
    pDesc->VendorId = NvidiaVendorID;
  
  return result;
}

void detourEnumAdapters(void* vtablePtr)
{
  lock_guard<mutex> guard (sEnumAdaptersDetouredLock);
  if (sEnumAdaptersDetoured)
    return;
  sEnumAdaptersDetoured = true;
  
  void* vtable = *reinterpret_cast<void**>(vtablePtr);
  void* functionPtr = reinterpret_cast<void**>(vtable)[EnumAdaptersVFTableIndex];
  DXGIFactoryDetour::Real_EnumAdapters = 
      reinterpret_cast<DXGIFactoryDetour::DetouredEnumAdapters&> (functionPtr);

  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());
  auto myEnumAdapters = &DXGIFactoryDetour::Mine_EnumAdapters;
  LONG error = DetourAttach(&(PVOID&)DXGIFactoryDetour::Real_EnumAdapters,
  *(PBYTE*)&myEnumAdapters);
  if (error != NO_ERROR)
    cout << "Error during attach EnumAdapters(): " << error << endl;
  error = DetourTransactionCommit();

#if DETOURS_64BIT
  cout << "----------------: " << endl;
#else
  cout << "--------: " << endl;
#endif

  if (error == NO_ERROR)
    cout << "Detoured EnumAdapters() successfully" << endl;
  else
    cout << "Error detouring EnumAdapters(): " << error << endl;
}

HRESULT CreateDXGIFactory1_mine (REFIID riid, _COM_Outptr_ void **ppFactory)
{
  cout << "  Detoured CreateDXGIFactory() was called!" << endl;
  auto const result =  CreateDXGIFactory1_original (riid, ppFactory);
  cout << "  detouring EnumAdapters():" << endl;
  detourEnumAdapters(*ppFactory);
  cout << "  detoured EnumAdapters():" << endl;
  return result;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
  (void)hinst;
  (void)reserved;

  if (DetourIsHelperProcess())
    return TRUE;

  if (dwReason == DLL_PROCESS_ATTACH)
  {
    DetourRestoreAfterWith();

#if DETOURS_64BIT
    cout << "----------------: " << endl;
#else
    cout << "--------: " << endl;
#endif

    SYSTEM_INFO si;
    GetSystemInfo(&si);

    if (si.wProcessorArchitecture == 9)
      cout << "x64 Processor" << endl;
    else if (si.wProcessorArchitecture == 0)
      cout << "x86 Processor" << endl;
    else if (si.wProcessorArchitecture == 6)
      cout << "ia64 Processor" << endl;
    else
      cout << si.wProcessorArchitecture << " Processor" << endl;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)CreateDXGIFactory1_original, (PVOID) CreateDXGIFactory1_mine);
    LONG error = DetourTransactionCommit();

#if DETOURS_64BIT
    cout << "----------------: " << endl;
#else
    cout << "--------: " << endl;
#endif

    if (error == NO_ERROR)
      cout << "Detoured CreateDXGIFactory1() successfully" << endl;
    else
      cout << "Error detouring CreateDXGIFactory1(): " << error << endl;
  }
  else if (dwReason == DLL_PROCESS_DETACH)
  {
    LONG error = DetourTransactionBegin();
    if (error != NO_ERROR)
      cout << "Error: Detach detours Transaction begin failed: " << error << endl;
    error = DetourUpdateThread(GetCurrentThread());
    if (error != NO_ERROR)
      cout << "Error: detach detours UpdateThread failed: " << error << endl;
    error = DetourDetach(&(PVOID&)CreateDXGIFactory1_original, (PVOID) CreateDXGIFactory1_mine);
    if (error != NO_ERROR)
      cout << "Error: detours detach CreateDXIFactory1() failed: " << error << endl;
    if (sEnumAdaptersDetoured)
    {
      auto myEnumAdapters = &DXGIFactoryDetour::Mine_EnumAdapters;
      error = DetourDetach(&(PVOID&)DXGIFactoryDetour::Real_EnumAdapters,
      *(PBYTE*)&myEnumAdapters);
      if (error != NO_ERROR)
        cout << "Error: detours detach EnumAdapters() failed: " << error << endl;
    }
    if (sGetDescDetoured)
    {
      auto myGetDesc = &IDXGIAdapter1Detour::Mine_GetDesc1;
      error = DetourDetach(&(PVOID&)IDXGIAdapter1Detour::Real_GetDesc1,
      *(PBYTE*)&myGetDesc);
      if (error != NO_ERROR)
        cout << "Error: detach detours GetDesc1() failed: " << error << endl;
    }
    
    error = DetourTransactionCommit();
    if (error != NO_ERROR)
      printf("5 Error detach detours failed: %ld\n", error);
  }

  return TRUE;
}
