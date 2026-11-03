#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dxgi.h>
#include <fstream>
#include <filesystem>
#include <string>
static HMODULE module=nullptr,systemDxgi=nullptr,foreignDxgi=nullptr,mapDll=nullptr;
static INIT_ONCE once=INIT_ONCE_STATIC_INIT;
static thread_local bool loading=false;
static thread_local unsigned depth=0;
struct Call{Call(){++depth;}~Call(){--depth;}};
static BOOL CALLBACK initialize(PINIT_ONCE,PVOID,PVOID*){
 loading=true;wchar_t sys[32768],self[32768];GetSystemDirectoryW(sys,32768);systemDxgi=LoadLibraryW((std::wstring(sys)+L"\\dxgi.dll").c_str());
 GetModuleFileNameW(module,self,32768);auto folder=std::filesystem::path(self).parent_path();
 std::ifstream input(folder/L"SinkingMap"/L"chain-dxgi.txt");std::string name;
 if(std::getline(input,name)&&!name.empty()&&name.find_first_of("\\/:")==name.npos)foreignDxgi=LoadLibraryW((folder/name).c_str());
 // Keep the actual map module separate, so old packages can use the same compatibility bridge.
 mapDll=LoadLibraryW((folder/L"SinkingMap-map.dll").c_str());
 if(mapDll){using Factory=HRESULT(WINAPI*)(REFIID,void**);auto fn=(Factory)GetProcAddress(mapDll,"CreateDXGIFactory");IUnknown* factory=nullptr;if(fn&&SUCCEEDED(fn(IID_IUnknown,(void**)&factory))&&factory)factory->Release();}
 loading=false;return TRUE;
}
static FARPROC resolve(const char*name){if(!loading)InitOnceExecuteOnce(&once,initialize,nullptr,nullptr);if(!loading&&depth==1&&foreignDxgi){auto fn=GetProcAddress(foreignDxgi,name);if(fn)return fn;}return systemDxgi?GetProcAddress(systemDxgi,name):nullptr;}
extern "C" __declspec(dllexport) HRESULT WINAPI CreateDXGIFactory(REFIID i,void**p){Call c;using Fn=HRESULT(WINAPI*)(REFIID,void**);auto fn=(Fn)resolve("CreateDXGIFactory");return fn?fn(i,p):E_FAIL;}
extern "C" __declspec(dllexport) HRESULT WINAPI CreateDXGIFactory1(REFIID i,void**p){Call c;using Fn=HRESULT(WINAPI*)(REFIID,void**);auto fn=(Fn)resolve("CreateDXGIFactory1");return fn?fn(i,p):E_FAIL;}
extern "C" __declspec(dllexport) HRESULT WINAPI CreateDXGIFactory2(UINT f,REFIID i,void**p){Call c;using Fn=HRESULT(WINAPI*)(UINT,REFIID,void**);auto fn=(Fn)resolve("CreateDXGIFactory2");return fn?fn(f,i,p):E_FAIL;}
extern "C" __declspec(dllexport) HRESULT WINAPI DXGIGetDebugInterface1(UINT f,REFIID i,void**p){Call c;using Fn=HRESULT(WINAPI*)(UINT,REFIID,void**);auto fn=(Fn)resolve("DXGIGetDebugInterface1");return fn?fn(f,i,p):E_FAIL;}
extern "C" __declspec(dllexport) HRESULT WINAPI DXGIDeclareAdapterRemovalSupport(){Call c;using Fn=HRESULT(WINAPI*)();auto fn=(Fn)resolve("DXGIDeclareAdapterRemovalSupport");return fn?fn():E_FAIL;}
BOOL WINAPI DllMain(HINSTANCE h,DWORD why,LPVOID){if(why==DLL_PROCESS_ATTACH){module=h;DisableThreadLibraryCalls(h);}return TRUE;}
