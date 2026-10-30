#define STANDALONE
#include "../native/map.cpp"
#include <cassert>
#include <iostream>
#include <iterator>
std::wstring ornamentRoot;
struct Frame {int w,h;std::vector<DWORD> pixels;};
Frame draw(int w,int h,bool decor){using namespace sm;
 physicalW=w;physicalH=h;uiScale=std::max(1.,std::min(h/820.,w/1080.));clientW=(int)(w/uiScale);clientH=(int)(h/uiScale);
 root=decor?ornamentRoot:ornamentRoot+L"/missing";
 BITMAPINFO info{};info.bmiHeader.biSize=40;info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
 void*ptr=nullptr;HBITMAP bmp=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&ptr,nullptr,0);assert(bmp);HDC dc=CreateCompatibleDC(nullptr);auto old=SelectObject(dc,bmp);
 SetGraphicsMode(dc,GM_ADVANCED);XFORM xf{(FLOAT)uiScale,0,0,(FLOAT)uiScale,0,0};SetWorldTransform(dc,&xf);paint(dc);GdiFlush();
 Frame out{w,h,std::vector<DWORD>((DWORD*)ptr,(DWORD*)ptr+(size_t)w*h)};SelectObject(dc,old);DeleteObject(bmp);DeleteDC(dc);return out;
}
int main(int argc,char**argv){using namespace sm;assert(argc>=2);ornamentRoot=wide(argv[1]);Gdiplus::GdiplusStartupInput startup;assert(Gdiplus::GdiplusStartup(&gdiplusToken,&startup,nullptr)==Gdiplus::Ok);

 for(auto [w,h]:{std::pair{980,620},std::pair{1440,900},std::pair{1920,1080},std::pair{2560,1440},std::pair{3840,2160}}){
  data=Snapshot{};auto a=draw(w,h,true),b=draw(w,h,false);int changed=0;
  int left=(int)std::floor((clientW-side+16)*uiScale)-1,right=(int)std::ceil((clientW-16)*uiScale)+1;
  int topY=(int)std::floor(std::max(top+256,clientH-512)*uiScale)-1,bottom=(int)std::ceil((clientH-48)*uiScale)+1;
  int headerLeft=(int)std::floor(std::max(196,clientW-side-960)*uiScale)-1,headerRight=(int)std::ceil((clientW-side)*uiScale)+1,headerBottom=(int)std::ceil(top*uiScale)+1;
  int headerChanged=0,sideChanged=0;
  for(int y=0;y<h;y++)for(int x=0;x<w;x++)if(a.pixels[(size_t)y*w+x]!=b.pixels[(size_t)y*w+x]){
   bool inHeader=x>=headerLeft&&x<=headerRight&&y<=headerBottom;
   bool inSide=x>=left&&x<=right&&y>=topY&&y<=bottom;
   assert(inHeader||inSide);if(inHeader)++headerChanged;if(inSide)++sideChanged;++changed;
  }
  assert(headerChanged>100&&sideChanged>100);std::cout<<"PASS both ornaments and protected controls/fog "<<w<<"x"<<h<<std::endl;
 }
 std::cout<<"PASS production paint header/sidebar clipping at five layout scales; no windows or disk image writes"<<std::endl;
}
