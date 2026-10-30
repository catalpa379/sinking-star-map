#define STANDALONE
#include "../native/map.cpp"
#include <cassert>
#include <iostream>
#include <cstring>
int main(int argc,char**argv){
 using namespace sm;
 Gdiplus::GdiplusStartupInput startup;assert(Gdiplus::GdiplusStartup(&gdiplusToken,&startup,nullptr)==Gdiplus::Ok);
 auto s=parse("SMAP1\nO\t0\t0\t0\t10\t10\nP\t0\t2\t2\t0\t0\t#1\t\tknown\tarea\t#1\nEND");
 assert(covers(s,0,2,2));assert(!covers(s,0,12,2));assert(!covers(s,1,2,2));
 for(auto bad:{"", "SMAP1\n", "SMAP1\nO\t0\tnan\t0\t1\t1\nEND", "SMAP1\nEND\nP\t0"}){bool rejected=false;try{parse(bad);}catch(...){rejected=true;}assert(rejected);}
 assert(!s.allUnfinished);assert(parse("SMAP1\nM\tALL_UNFINISHED\nEND").allUnfinished);
 for(auto bad:{"SMAP1\nM\tUNKNOWN\nEND","SMAP1\nM\tALL_UNFINISHED\nM\tALL_UNFINISHED\nEND","SMAP1\nM\tALL_UNFINISHED\nP\t0\t2\t2\t0\t1\t#1\t\tknown\tarea\t#1\nEND"}){bool rejected=false;try{parse(bad);}catch(...){rejected=true;}assert(rejected);}
 // Exercise the actual toggle/reset logic without opening a window or launching a worker.
 onlyUnsolved=false;data=s;query=L"hidden search";selected=0;unsigned oldGeneration=generation;
 toggleAllUnfinished();assert(showAllUnfinished&&onlyUnsolved&&data.marks.empty()&&query.empty()&&selected==-1&&generation>oldGeneration);
 data=parse("SMAP1\nM\tALL_UNFINISHED\nEND");query=L"spoiler";toggleAllUnfinished();assert(!showAllUnfinished&&!onlyUnsolved&&data.open.empty()&&query.empty());
 toggleAllUnfinished();resetScope();assert(!showAllUnfinished&&!onlyUnsolved&&data.marks.empty());onlyUnsolved=true;
 auto stale=std::make_unique<Snapshot>(parse("SMAP1\nM\tALL_UNFINISHED\nO\t1\t0\t0\t10\t10\nEND"));
 proc(nullptr,WM_APP+1,generation-1,(LPARAM)stale.release());assert(!showAllUnfinished&&data.open.empty());
 data=s;clientW=1200;clientH=800;layer=0;cx=5;cy=5;zoom=15;
 // Render the actual native paint path into a bitmap. Even malicious extra geometry is clipped by fog.
 HDC screenDC=GetDC(nullptr),dc=CreateCompatibleDC(screenDC);HBITMAP bmp=CreateCompatibleBitmap(screenDC,1200,800);auto old=SelectObject(dc,bmp);
 data.tiles.push_back({0,13,5,1});paint(dc);POINT hidden=screen(13,5);assert(GetPixel(dc,hidden.x,hidden.y)==fog);
 POINT visible=screen(5,5);assert(GetPixel(dc,visible.x,visible.y)!=fog);
 // Unfinished marks keep a large, distinct silhouette over a background matching their fill colour.
 Mark pin{0,0,0,0,0,L"#pin",L"",L"pin",L"area",L"#pin"};
 fill(dc,{300,300,400,400},unfinished);drawMarker(dc,pin,{350,350},false);
 assert(GetPixel(dc,350,343)==RGB(255,255,255));assert(GetPixel(dc,356,350)==unfinished);
 assert(GetPixel(dc,350,336)==fog); // dark outer silhouette cannot disappear into a similarly coloured map.
 // Navigation follows only already-projected, currently-filtered entries on the selected layer.
 data.marks.push_back({1,2,2,-21,0,L"#under",L"",L"under",L"area",L"#under"});
 // Underground rendering uses the same strict mask and marker code; no underground source map is shown here.
 Snapshot ground=data;data=Snapshot{};data.open.push_back({1,0,0,10,10});data.marks.push_back(pin);data.marks[0].l=1;
 layer=1;cx=5;cy=5;zoom=15;paint(dc);POINT undergroundHidden=screen(13,5);assert(GetPixel(dc,undergroundHidden.x,undergroundHidden.y)==fog);
 data=ground;layer=0;
 query=L"unknown";assert(!match(data.marks[0]));query=L"known";assert(match(data.marks[0]));onlyUnsolved=false;data.marks[0].state=1;assert(match(data.marks[0]));onlyUnsolved=true;assert(!match(data.marks[0]));
 // Zoom and pan over the same hidden point through the production drawing function.
 for(double z:{1.5,3.,12.,48.}){zoom=z;cx=10;paint(dc);auto p=screen(13,5);assert(GetPixel(dc,p.x,p.y)==fog);}
 SelectObject(dc,old);DeleteObject(bmp);DeleteDC(dc);ReleaseDC(nullptr,screenDC);
 // Alter EVERY hidden texel in a full detailed background. The sanitized output must stay byte-identical.
 Gdiplus::Bitmap raw(8192,8192,PixelFormat32bppRGB);
 {Gdiplus::Graphics g(&raw);g.Clear(Gdiplus::Color(255,200,40,30));}
 Snapshot mask;mask.open.push_back({0,0,0,10,10});auto before=sanitizeTerrain(raw,0,mask);assert(before);
 {Gdiplus::Graphics g(&raw);g.Clear(Gdiplus::Color(255,0,255,0));Gdiplus::SolidBrush brush(Gdiplus::Color(255,200,40,30));double scale=8192/190.;int left=(int)std::ceil(24.5*scale),right=(int)std::floor(34.5*scale),upper=(int)std::ceil(160.5*scale),bottom=(int)std::floor(170.5*scale);g.FillRectangle(&brush,left,upper,right-left,bottom-upper);}
 auto after=sanitizeTerrain(raw,0,mask);assert(after);
 BITMAP x{},y{};GetObject(before->bitmap,sizeof(x),&x);GetObject(after->bitmap,sizeof(y),&y);
 assert(std::memcmp(x.bmBits,y.bmBits,(size_t)8192*8192*4)==0);
 auto px=(DWORD*)x.bmBits;assert(px[0]==0x000a1017);
 before.reset();after.reset();
 if(argc==2){
  std::wstring dir=wide(argv[1]);
  for(const auto&name:{L"map-0",L"map-1",L"map-2"}){
   IStream*stream=nullptr;auto actual=readTerrain(dir+L"/"+name+L".smt",&stream);assert(actual);assert(actual->GetWidth()==8192&&actual->GetHeight()==8192);
   Snapshot under;under.open.push_back({1,0,0,10,10});auto masked=sanitizeTerrain(*actual,1,under);assert(masked);BITMAP bitmap{};GetObject(masked->bitmap,sizeof(bitmap),&bitmap);assert(((DWORD*)bitmap.bmBits)[0]==0x000a1017);actual.reset();stream->Release();
  }
  // The real underground image must not even load into the visible cache for an unrevealed layer.
  terrain.clear();data=Snapshot{};root=dirOf(dir);assert(!terrainFor(1));assert(terrain.empty());
  std::cout<<"PASS detailed ground/underground/sky decoding and underground masking without displaying hidden art\n";
 }
 IStream*invalid=nullptr;assert(!readTerrain(L"does-not-exist.smt",&invalid)&&invalid==nullptr);
 std::cout<<"PASS packed texture decoding and no disk image extraction\n";
 std::cout<<"PASS native parser, fog pixels, pan/zoom, search/filter and detailed-texture noninterference\n";
}
