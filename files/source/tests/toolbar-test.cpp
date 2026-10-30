#define STANDALONE
#include "../native/map.cpp"
#include <cassert>
#include <iostream>
int main(){using namespace sm;
 clientW=980;clientH=620;uiScale=1;showAllUnfinished=false;onlyUnsolved=true;
 proc(nullptr,WM_LBUTTONDOWN,0,MAKELPARAM(275,80));assert(!onlyUnsolved);
 proc(nullptr,WM_LBUTTONDOWN,0,MAKELPARAM(352,80));assert(onlyUnsolved);
 onlyUnsolved=false;toolbarAction(AllScope);assert(showAllUnfinished&&onlyUnsolved);
 proc(nullptr,WM_LBUTTONDOWN,0,MAKELPARAM(275,80));assert(onlyUnsolved);
 toolbarAction(AllScope);assert(showAllUnfinished);toolbarAction(KnownScope);assert(!showAllUnfinished&&!onlyUnsolved);
 data.open={{0,0,0,10,10},{2,0,0,5,5}};layer=0;
 proc(nullptr,WM_LBUTTONDOWN,0,MAKELPARAM(100,80));assert(layer==2);
 toolbarAction(DefaultView);assert(layer==2);
 MEASUREITEMSTRUCT m{};m.CtlType=ODT_MENU;proc(nullptr,WM_MEASUREITEM,0,(LPARAM)&m);assert(m.itemHeight==36&&m.itemWidth==272);
 BITMAPINFO info{};info.bmiHeader.biSize=40;info.bmiHeader.biWidth=300;info.bmiHeader.biHeight=-40;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void*ptr;auto bmp=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&ptr,nullptr,0);auto dc=CreateCompatibleDC(nullptr);auto old=SelectObject(dc,bmp);
 DRAWITEMSTRUCT d{};d.CtlType=ODT_MENU;d.hDC=dc;d.rcItem={0,0,300,40};d.itemData=(ULONG_PTR)L"显示范围";proc(nullptr,WM_DRAWITEM,0,(LPARAM)&d);assert(GetPixel(dc,1,1)==panel);
 d.itemState=ODS_SELECTED|ODS_CHECKED;proc(nullptr,WM_DRAWITEM,0,(LPARAM)&d);assert(GetPixel(dc,1,1)==card);
 SelectObject(dc,old);DeleteObject(bmp);DeleteDC(dc);
 std::cout<<"PASS filter hit regions, known layer tabs, explicit scope selection/restoration, menu sizing and dark owner drawing without windows\n";
}
