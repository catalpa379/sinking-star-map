#define STANDALONE
#include "../native/map.cpp"
#include <cassert>
#include <iostream>
int forwarded=0;
LRESULT CALLBACK original(HWND,UINT,WPARAM,LPARAM){++forwarded;return 17;}
int main(int argc,char**argv){
 using namespace sm;assert(argc==2);auto path=wide(argv[1]);
 loadBindings(path);assert(mapBinding==((2<<8)|'M'));assert(refreshBinding==((2<<8)|'R'));
 assert(useMapBinding(mapBinding,'M',2,false));assert(!useMapBinding(mapBinding,'M',0,false));
 assert(!useMapBinding('M','M',0,true));assert(!useMapBinding(VK_LEFT,VK_LEFT,0,true));assert(useMapBinding(VK_F9,VK_F9,0,true));
 assert(validBinding((7<<8)|'A'));assert(!validBinding(VK_ESCAPE));assert(!validBinding(VK_TAB));assert(!validBinding((1<<8)|VK_F4));assert(!validBinding(-1));
 assert(!saveBindings(path,VK_F9,VK_F9));loadBindings(path);assert(mapBinding==((2<<8)|'M'));
 mapBinding=VK_F9;gameProc=original;wnd=nullptr;editingBindings=false;
 assert(intercept(nullptr,WM_KEYDOWN,VK_F8,0)==17&&forwarded==1);
 assert(intercept(nullptr,WM_KEYDOWN,VK_F9,0)==0&&forwarded==1);
 assert(intercept(nullptr,WM_KEYDOWN,VK_F9,1L<<30)==0&&forwarded==1);
 assert(intercept(nullptr,WM_KEYUP,VK_F9,0)==0&&swallowedGameKey==0);
 intercept(nullptr,WM_KEYDOWN,VK_F9,0);intercept(nullptr,WM_KILLFOCUS,0,0);assert(swallowedGameKey==0);
 editingBindings=true;assert(intercept(nullptr,WM_KEYDOWN,VK_F9,0)==17);editingBindings=false;
 assert(saveBindings(path,(4<<8)|VK_F9,(2<<8)|VK_F6));mapBinding=0;refreshBinding=0;loadBindings(path);assert(mapBinding==((4<<8)|VK_F9)&&refreshBinding==((2<<8)|VK_F6));
 auto bad=path+L".invalid";WritePrivateProfileStringW(L"Keys",L"Toggle",L"27",bad.c_str());loadBindings(bad);assert(mapBinding==VK_F8&&refreshBinding==VK_F5);DeleteFileW(bad.c_str());
 std::cout<<"PASS native persisted shortcuts, shared encoding, duplicate/invalid rejection, typing, key forwarding/repeat/release/focus and editor isolation\n";
}
