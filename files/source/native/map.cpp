#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <shellapi.h>
#include <dxgi.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <thread>
#include <atomic>
#include <memory>
#include <filesystem>

namespace sm {
struct Rect{int l;double x,y,w,h;};
struct Tile{int l;double x,y,z;};
struct Mark{int l;double x,y,z;int state;std::wstring id,key,name,area,countKey;};
struct Snapshot{std::vector<Rect> open;std::vector<Tile> tiles;std::vector<Mark> marks;bool player=false;Tile hero{};std::wstring error;bool allUnfinished=false;int savedSolved=-1;std::vector<Tile> visiblePoints;};
HWND wnd=nullptr,gameWnd=nullptr,searchBox=nullptr,profileBox=nullptr;
HMODULE module=nullptr;
WNDPROC gameProc=nullptr;
Snapshot data;
std::wstring root,gameDir,saveRoot,query,status=L"选择档案后读取地图";
std::vector<std::wstring> profiles;
int layer=0,selected=-1,areaPage=0;
unsigned generation=0;
bool onlyUnsolved=true,dragging=false,moved=false;
bool showAllUnfinished=false,savedNormalFilter=true;
double zoom=6,cx=80,cy=76;POINT drag{};
std::atomic<bool> busy=false,stopping=false;
std::thread worker;
bool refreshQueued=false;
int clientW=1200,clientH=800;
double uiScale=1;
HFONT controlFont=nullptr;
std::wstring uiFontFace=L"SimSun",gameFontFile;
bool gameFontLoaded=false;
int hoverX=-1,hoverY=-1;

int physicalW=1200,physicalH=800;
const int side=352,top=112;
struct Surface{HBITMAP bitmap=nullptr;int w=0,h=0;~Surface(){if(bitmap)DeleteObject(bitmap);}};
std::map<int,std::unique_ptr<Surface>> terrain;
ULONG_PTR gdiplusToken=0;
void discardTerrain(){terrain.clear();}
std::unique_ptr<Surface> sanitizeTerrain(Gdiplus::Bitmap&raw,int l,const Snapshot&s){
 const int width=(int)raw.GetWidth(),height=(int)raw.GetHeight();
 if(width!=8192||height!=8192)return nullptr;
 auto safe=std::make_unique<Surface>();safe->w=width;safe->h=height;
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
 void*pixels=nullptr;safe->bitmap=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&pixels,nullptr,0);if(!safe->bitmap)return nullptr;
 // Mask BEFORE any interpolation or scaling. Unknown source pixels never reach the sanitized bitmap.
 std::fill_n((DWORD*)pixels,(size_t)width*height,0x000a1017);
 HDC dc=CreateCompatibleDC(nullptr);auto old=SelectObject(dc,safe->bitmap);HRGN allowed=CreateRectRgn(0,0,0,0);
 const double scale=width/190.;
 for(const auto&r:s.open)if(r.l==l){int left=(int)std::ceil((r.x+24.5)*scale),right=(int)std::floor((r.x+r.w+24.5)*scale),upper=(int)std::ceil((170.5-r.y-r.h)*scale),bottom=(int)std::floor((170.5-r.y)*scale);if(left>=right||upper>=bottom)continue;HRGN part=CreateRectRgn(left,upper,right,bottom);CombineRgn(allowed,allowed,part,RGN_OR);DeleteObject(part);}
 SelectClipRgn(dc,allowed);
 {Gdiplus::Graphics g(dc);Gdiplus::Region mask(allowed);g.SetClip(&mask,Gdiplus::CombineModeReplace);g.SetSmoothingMode(Gdiplus::SmoothingModeNone);g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);g.DrawImage(&raw,0,0,width,height);}
 SelectClipRgn(dc,nullptr);DeleteObject(allowed);SelectObject(dc,old);DeleteDC(dc);return safe;
}
// Packed assets prevent accidental image previews; this is packaging, not DRM.
std::unique_ptr<Gdiplus::Bitmap> readTerrain(const std::wstring&file,IStream**keep){
 *keep=nullptr;std::ifstream input(std::filesystem::path(file),std::ios::binary|std::ios::ate);if(!input)return nullptr;
 auto length=input.tellg();if(length<12||length>64*1024*1024)return nullptr;
 input.seekg(0);char header[8]{};DWORD size=0;input.read(header,8);input.read((char*)&size,4);
 if(std::string(header,7)!="SMTERR1"||size!=(DWORD)length-12)return nullptr;
 HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,size);if(!memory)return nullptr;
 auto bytes=(unsigned char*)GlobalLock(memory);if(!bytes){GlobalFree(memory);return nullptr;}
 input.read((char*)bytes,size);bool complete=!!input;
 for(DWORD i=0;i<size;i++)bytes[i]^=(unsigned char)(0xA7u+31u*(i%251u));
 GlobalUnlock(memory);if(!complete){GlobalFree(memory);return nullptr;}
 if(FAILED(CreateStreamOnHGlobal(memory,TRUE,keep))){GlobalFree(memory);return nullptr;}
 auto raw=std::make_unique<Gdiplus::Bitmap>(*keep,FALSE);
 if(raw->GetLastStatus()!=Gdiplus::Ok){raw.reset();(*keep)->Release();*keep=nullptr;return nullptr;}
 return raw;
}
Surface*terrainFor(int l){
 auto found=terrain.find(l);if(found!=terrain.end())return found->second.get();
 terrain.clear();
 bool shown=false;for(const auto&r:data.open)if(r.l==l)shown=true;if(!shown||l<0||l>2)return nullptr;
 std::wstring file=root+L"\\terrain\\map-"+std::to_wstring(l)+L".smt";
 IStream*stream=nullptr;auto raw=readTerrain(file,&stream);std::unique_ptr<Surface>safe;
 if(raw)safe=sanitizeTerrain(*raw,l,data);
 raw.reset();if(stream)stream->Release();
 auto ptr=safe.get();terrain.emplace(l,std::move(safe));return ptr;
}
std::wstring wide(const std::string&s){if(s.empty())return L"";int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0);if(!n)throw 1;std::wstring w(n,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),w.data(),n);return w;}
std::wstring dirOf(std::wstring s){auto p=s.find_last_of(L"\\/");return p==s.npos?L".":s.substr(0,p);}
bool fileExists(const std::wstring&s){DWORD a=GetFileAttributesW(s.c_str());return a!=INVALID_FILE_ATTRIBUTES&&!(a&FILE_ATTRIBUTE_DIRECTORY);}
// Refactoring UI §4.2: privately load the game's own Chinese serif, without installing fonts.
void loadGameFont(){
 gameFontFile=gameDir+L"\\data\\fonts\\SourceHanSerifCN-SemiBold.otf";
 if(fileExists(gameFontFile)&&AddFontResourceExW(gameFontFile.c_str(),FR_PRIVATE,nullptr)>0){
  gameFontLoaded=true;uiFontFace=L"Source Han Serif CN SemiBold";
 }
}
#include "keybindings.hpp"
std::wstring quote(const std::wstring&s){std::wstring r=L"\"";int slash=0;for(wchar_t c:s){if(c==L'\\'){slash++;continue;}if(c==L'\"')r.append(slash*2+1,L'\\');else r.append(slash,L'\\');slash=0;r+=c;}r.append(slash*2,L'\\');return r+L"\"";}
std::vector<std::string> split(const std::string&s){std::vector<std::string> v;size_t i=0,j;while((j=s.find('\t',i))!=s.npos){v.push_back(s.substr(i,j-i));i=j+1;}v.push_back(s.substr(i));return v;}
double number(const std::string&s){size_t n=0;double v=std::stod(s,&n);if(n!=s.size()||!std::isfinite(v)||std::abs(v)>100000)throw 1;return v;}
Snapshot parse(const std::string&s){
 Snapshot out;std::istringstream in(s);std::string line;bool end=false,modeSeen=false;
 if(!std::getline(in,line)||line!="SMAP1")throw 1;
 while(std::getline(in,line)){
  if(line=="END"){end=true;break;}auto v=split(line);
  if(v[0]=="ERROR"){if(v.size()!=2)throw 1;out.error=wide(v[1]);continue;}
  if(v[0]=="M"&&v.size()==2&&v[1]=="ALL_UNFINISHED"&&!modeSeen){out.allUnfinished=true;modeSeen=true;}
  else if(v[0]=="G"&&v.size()==2&&out.savedSolved<0){double n=number(v[1]);if(n<0||n!=std::floor(n))throw 1;out.savedSolved=(int)n;}
  else if(v[0]=="O"&&v.size()==6)out.open.push_back({(int)number(v[1]),number(v[2]),number(v[3]),number(v[4]),number(v[5])});
  else if(v[0]=="T"&&v.size()==5)out.tiles.push_back({(int)number(v[1]),number(v[2]),number(v[3]),number(v[4])});
  else if(v[0]=="V"&&v.size()==5){double l=number(v[1]);if(l<0||l>2||l!=std::floor(l))throw 1;out.visiblePoints.push_back({(int)l,number(v[2]),number(v[3]),number(v[4])});}
  else if(v[0]=="P"&&v.size()==11)out.marks.push_back({(int)number(v[1]),number(v[2]),number(v[3]),number(v[4]),(int)number(v[5]),wide(v[6]),wide(v[7]),wide(v[8]),wide(v[9]),wide(v[10])});
  else if(v[0]=="H"&&v.size()==5){out.player=true;out.hero={(int)number(v[1]),number(v[2]),number(v[3]),number(v[4])};}
  else throw 1;
 }
 if(!end||std::getline(in,line))throw 1;
 if(!out.error.empty())return Snapshot{{},{},{},false,{},out.error};
 if(out.open.size()>10000||out.tiles.size()>200000||out.marks.size()>10000||out.visiblePoints.size()>10000)throw 1;
 for(const auto&p:out.visiblePoints){bool found=false;for(const auto&m:out.marks)if(m.l==p.l&&m.x==p.x&&m.y==p.y&&m.z==p.z){found=true;break;}if(!found)throw 1;}
 if(out.allUnfinished)for(const auto&m:out.marks)if(m.state!=0)throw 1;
 return out;
}
bool covers(const Snapshot&s,int l,double x,double y){for(auto&r:s.open)if(r.l==l&&x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h)return true;return false;}
bool coversMarker(const Snapshot&s,const Mark&m){if(covers(s,m.l,m.x,m.y))return true;for(const auto&p:s.visiblePoints)if(p.l==m.l&&p.x==m.x&&p.y==m.y&&p.z==m.z)return true;return false;}
POINT screen(double x,double y){return {(LONG)std::lround((clientW-side)/2.+(x-cx)*zoom),(LONG)std::lround(top+(clientH-top)/2.-(y-cy)*zoom)};}
bool match(const Mark&m){return m.l==layer&&((!showAllUnfinished&&!onlyUnsolved)||m.state==0)&&(query.empty()||m.name.find(query)!=m.name.npos||m.id.find(query)!=m.id.npos||m.key.find(query)!=m.key.npos||m.area.find(query)!=m.area.npos);}
void reset(){
 bool first=true;double a=0,b=0,c=0,d=0;
 for(auto&r:data.open)if(r.l==layer){if(first){a=r.x;b=r.y;c=r.x+r.w;d=r.y+r.h;first=false;}else{a=std::min(a,r.x);b=std::min(b,r.y);c=std::max(c,r.x+r.w);d=std::max(d,r.y+r.h);}}
 if(!first){cx=(a+c)/2;cy=(b+d)/2;zoom=std::clamp(std::min((clientW-side-70)/std::max(1.,c-a),(clientH-top-70)/std::max(1.,d-b)),1.5,40.);}
}
void centerPlayer(){if(data.player){layer=data.hero.l;cx=data.hero.x;cy=data.hero.y;zoom=std::max(zoom,10.);status=L"定位到最近一次保存的位置";}else status=L"当前位置无法可靠确认；仅在保存于主世界时提供定位";}
void resetScope(){
 refreshQueued=false;
 ++generation;
 if(showAllUnfinished)onlyUnsolved=savedNormalFilter;
 showAllUnfinished=false;data=Snapshot{};discardTerrain();selected=-1;areaPage=0;query.clear();
 if(searchBox)SetWindowTextW(searchBox,L"");
}
#include "worker-reader.hpp"
void refresh(){
 if(busy){refreshQueued=true;return;}
 refreshQueued=false;
 data=Snapshot{};discardTerrain();selected=-1;areaPage=0;status=L"正在只读解析；地图已清空…";InvalidateRect(wnd,nullptr,FALSE);
 int n=(int)SendMessageW(profileBox,CB_GETCURSEL,0,0);if(n<0||n>=(int)profiles.size()){status=L"未找到存档档案";return;}
 std::wstring campaign=saveRoot+L"\\"+profiles[n];
 busy=true;EnableWindow(profileBox,FALSE);unsigned request=++generation;bool requestedAll=showAllUnfinished;if(worker.joinable())worker.join();
 worker=std::thread([campaign,request,requestedAll]{
  auto result=std::make_unique<Snapshot>();
  try{
   std::wstring exe=root+L"\\runtime\\node.exe",command=quote(exe)+L" --disable-warning=ExperimentalWarning "+quote(root+L"\\worker\\main.mjs")+L" "+quote(gameDir)+L" "+quote(campaign)+(requestedAll?L" --all-unfinished":L"");
   *result=readSnapshotProcess(exe,command,root,requestedAll);
  }catch(const WorkerReadFailure&failure){*result=Snapshot{};result->error=failure.message;}
  catch(...){*result=Snapshot{};result->error=L"读取未能完成；地图已清空，请刷新重试。";}
  if(!stopping&&PostMessageW(wnd,WM_APP+1,request,(LPARAM)result.get()))result.release();
 });
}
void toggleAllUnfinished(){
 ++generation;data=Snapshot{};discardTerrain();selected=-1;areaPage=0;query.clear();if(searchBox)SetWindowTextW(searchBox,L"");
 if(!showAllUnfinished){savedNormalFilter=onlyUnsolved;showAllUnfinished=true;onlyUnsolved=true;}
 else{showAllUnfinished=false;onlyUnsolved=savedNormalFilter;}
 status=showAllUnfinished?L"已开启剧透模式，正在读取全地图未完成谜题…":L"已恢复探索范围限制，正在重新读取…";
 if(wnd){refresh();InvalidateRect(wnd,nullptr,FALSE);}
}
void fill(HDC dc,RECT r,COLORREF color){HBRUSH b=CreateSolidBrush(color);FillRect(dc,&r,b);DeleteObject(b);}
void text(HDC dc,std::wstring s,RECT r,COLORREF color,int size=16,UINT flags=DT_LEFT|DT_TOP|DT_WORDBREAK){
 HFONT font=CreateFontW(-size,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,uiFontFace.c_str());auto old=SelectObject(dc,font);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);DrawTextW(dc,s.c_str(),(int)s.size(),&r,flags);SelectObject(dc,old);DeleteObject(font);
}
// Warm ink and champagne, sampled in spirit from the supplied title artwork.
const COLORREF fg=RGB(241,233,213),muted=RGB(184,180,160),amber=RGB(222,191,126),green=RGB(140,199,166),panel=RGB(22,23,18),fog=RGB(10,16,23);
const COLORREF unfinished=RGB(255,61,135);
const int unfinishedRadius=12,unfinishedHitRadius=16;
void drawMarker(HDC dc,const Mark&m,POINT p,bool active){
 auto saved=SaveDC(dc);
 if(m.state){
  HPEN outline=CreatePen(PS_SOLID,2,fog);HBRUSH brush=CreateSolidBrush(green);SelectObject(dc,outline);SelectObject(dc,brush);int r=active?7:4;Ellipse(dc,p.x-r,p.y-r,p.x+r+1,p.y+r+1);RestoreDC(dc,saved);DeleteObject(outline);DeleteObject(brush);return;
 }
 const int r=active?15:unfinishedRadius;
 HBRUSH brush=CreateSolidBrush(unfinished);SelectObject(dc,brush);
 HPEN shadow=CreatePen(PS_SOLID,7,fog);SelectObject(dc,shadow);Ellipse(dc,p.x-r,p.y-r,p.x+r+1,p.y+r+1);
 HPEN border=CreatePen(PS_SOLID,2,RGB(255,255,255));SelectObject(dc,border);Ellipse(dc,p.x-r,p.y-r,p.x+r+1,p.y+r+1);
 // The exclamation shape differs from the game's gold pads and remains legible over any terrain colour.
 fill(dc,{p.x-2,p.y-7,p.x+3,p.y+2},RGB(255,255,255));fill(dc,{p.x-2,p.y+5,p.x+3,p.y+9},RGB(255,255,255));
 RestoreDC(dc,saved);DeleteObject(shadow);DeleteObject(border);DeleteObject(brush);
}
#include "map-ui.hpp"
bool menuOpen=false;
enum MenuAction{DefaultView=501,OpenSaveFolder,LocateSaved};
void toolbarAction(int id){
 if(id==DefaultView)reset();
 else if(id==OpenSaveFolder)ShellExecuteW(wnd,L"open",saveRoot.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
 else if(id==LocateSaved)centerPlayer();
 InvalidateRect(wnd,nullptr,FALSE);
}
HMENU createMoreMenu(){
 HMENU menu=CreatePopupMenu();
 AppendMenuW(menu,MF_OWNERDRAW,DefaultView,L"恢复默认视图");AppendMenuW(menu,MF_OWNERDRAW,LocateSaved,L"定位最近存档位置");AppendMenuW(menu,MF_OWNERDRAW,OpenSaveFolder,L"打开存档文件夹");
 return menu;
}
void moreMenu(){
 HMENU menu=createMoreMenu();
 POINT p{(LONG)((clientW-side-184)*uiScale),(LONG)(50*uiScale)};ClientToScreen(wnd,&p);menuOpen=true;
 int result=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_LEFTALIGN|TPM_TOPALIGN,p.x,p.y,0,wnd,nullptr);menuOpen=false;DestroyMenu(menu);if(result)toolbarAction(result);
}
BOOL CALLBACK findWindow(HWND w,LPARAM){DWORD p=0;GetWindowThreadProcessId(w,&p);if(p==GetCurrentProcessId()&&IsWindowVisible(w)&&GetWindow(w,GW_OWNER)==nullptr&&w!=wnd){gameWnd=w;return FALSE;}return TRUE;}
void layout(){RECT r;GetClientRect(wnd,&r);physicalW=r.right;physicalH=r.bottom;uiScale=std::max(1.,std::min(physicalH/820.,physicalW/1080.));clientW=(int)(physicalW/uiScale);clientH=(int)(physicalH/uiScale);MoveWindow(searchBox,(int)((clientW-side+18)*uiScale),(int)((top+46)*uiScale),(int)((side-36)*uiScale),(int)(34*uiScale),TRUE);MoveWindow(profileBox,(int)((clientW-side+18)*uiScale),(int)(40*uiScale),(int)((side-36)*uiScale),(int)(220*uiScale),TRUE);HFONT old=controlFont;controlFont=CreateFontW((int)(-17*uiScale),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,uiFontFace.c_str());SendMessage(searchBox,WM_SETFONT,(WPARAM)controlFont,TRUE);SendMessage(profileBox,WM_SETFONT,(WPARAM)controlFont,TRUE);if(old)DeleteObject(old);}
void openMap(){
 if(!bindingPath.empty())loadBindings(bindingPath);
 resetScope();
 if(gameWnd){RECT r;GetClientRect(gameWnd,&r);POINT p{};ClientToScreen(gameWnd,&p);SetWindowPos(wnd,HWND_TOP,p.x,p.y,std::max(980L,r.right),std::max(620L,r.bottom),SWP_SHOWWINDOW);}
 data=Snapshot{};discardTerrain();selected=-1;ShowWindow(wnd,SW_SHOW);SetForegroundWindow(wnd);SetFocus(wnd);refresh();
}
void hideMap(){resetScope();ShowWindow(wnd,SW_HIDE);if(gameWnd)SetForegroundWindow(gameWnd);}
LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 switch(msg){
 case WM_NCCREATE:wnd=w;return TRUE;
 case WM_CREATE:{
  searchBox=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,0,0,100,25,w,(HMENU)101,module,nullptr);
  profileBox=CreateWindowExW(0,L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,0,0,100,200,w,(HMENU)102,module,nullptr);
  SendMessageW(searchBox,EM_SETCUEBANNER,TRUE,(LPARAM)L"名称 / 编号 / 区域");SendMessageW(searchBox,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(8,8));
  auto font=(HFONT)GetStockObject(DEFAULT_GUI_FONT);SendMessage(searchBox,WM_SETFONT,(WPARAM)font,TRUE);SendMessage(profileBox,WM_SETFONT,(WPARAM)font,TRUE);
  for(auto&p:profiles)SendMessageW(profileBox,CB_ADDSTRING,0,(LPARAM)p.c_str());
  SendMessage(profileBox,CB_SETCURSEL,profiles.size()==1?0:-1,0);SetTimer(w,1,80,nullptr);return 0;
 }
 case WM_SIZE:layout();return 0;
 case WM_ERASEBKGND:return 1;
 case WM_CTLCOLOREDIT:case WM_CTLCOLORSTATIC:case WM_CTLCOLORLISTBOX:{HDC dc=(HDC)wp;SetTextColor(dc,fg);SetBkColor(dc,RGB(39,48,43));static HBRUSH brush=CreateSolidBrush(RGB(39,48,43));return (LRESULT)brush;}
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps),mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,physicalW,physicalH);auto old=SelectObject(mem,bmp);SetGraphicsMode(mem,GM_ADVANCED);XFORM xf{(FLOAT)uiScale,0,0,(FLOAT)uiScale,0,0};SetWorldTransform(mem,&xf);paint(mem);ModifyWorldTransform(mem,nullptr,MWT_IDENTITY);BitBlt(dc,0,0,physicalW,physicalH,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);EndPaint(w,&ps);return 0;}
 case WM_APP+1:{std::unique_ptr<Snapshot>s((Snapshot*)lp);busy=false;EnableWindow(profileBox,TRUE);bool again=refreshQueued;refreshQueued=false;if(wp!=generation||again){if(again||IsWindowVisible(w))refresh();return 0;}data=std::move(*s);selected=-1;
  if(data.error.empty()&&data.allUnfinished!=showAllUnfinished){data=Snapshot{};data.error=L"显示范围校验失败，请重新刷新";}
  status=data.error.empty()?L"已刷新。进度及位置以最近一次保存为准。":data.error;
  bool found=false;for(auto&r:data.open)if(r.l==layer)found=true;if(!found&&!data.open.empty())layer=data.open[0].l;reset();InvalidateRect(w,nullptr,FALSE);return 0;}
 case WM_APP+2:if(IsWindowVisible(w))hideMap();else openMap();return 0;
 case WM_MEASUREITEM:{auto m=(MEASUREITEMSTRUCT*)lp;if(m->CtlType==ODT_MENU){m->itemWidth=(UINT)(272*uiScale);m->itemHeight=(UINT)(36*uiScale);return TRUE;}break;}
 case WM_DRAWITEM:{auto d=(DRAWITEMSTRUCT*)lp;if(d->CtlType==ODT_MENU){fill(d->hDC,d->rcItem,(d->itemState&ODS_SELECTED)?card:panel);RECT r=d->rcItem;r.left+=(int)(30*uiScale);r.right-=(int)(24*uiScale);label(d->hDC,(const wchar_t*)d->itemData,r,fg,(int)(15*uiScale));if(wcsstr((const wchar_t*)d->itemData,L"显示范围")){RECT arrow=d->rcItem;arrow.left=arrow.right-(int)(24*uiScale);label(d->hDC,L"›",arrow,muted,(int)(18*uiScale));}if(d->itemState&ODS_CHECKED){RECT mark=d->rcItem;mark.right=mark.left+(int)(26*uiScale);label(d->hDC,L"●",mark,amber,(int)(12*uiScale),false,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}return TRUE;}break;}
 case WM_COMMAND:
  if(LOWORD(wp)==101&&HIWORD(wp)==EN_CHANGE){wchar_t b[256]{};GetWindowTextW(searchBox,b,256);query=b;selected=-1;InvalidateRect(w,nullptr,FALSE);}
  if(LOWORD(wp)==102&&HIWORD(wp)==CBN_SELCHANGE){resetScope();status=L"档案已切换，请刷新";if(!busy)refresh();InvalidateRect(w,nullptr,FALSE);}return 0;
 case WM_MOUSEWHEEL:{POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(w,&p);p.x=(LONG)(p.x/uiScale);p.y=(LONG)(p.y/uiScale);if(p.x>=clientW-side){areaPage=std::max(0,areaPage+(GET_WHEEL_DELTA_WPARAM(wp)<0?1:-1));}else if(p.y>=top){double x=cx+(p.x-(clientW-side)/2.)/zoom,y=cy-(p.y-top-(clientH-top)/2.)/zoom;zoom=std::clamp(zoom*std::pow(1.15,GET_WHEEL_DELTA_WPARAM(wp)/120.),1.5,48.);cx=x-(p.x-(clientW-side)/2.)/zoom;cy=y+(p.y-top-(clientH-top)/2.)/zoom;}InvalidateRect(w,nullptr,FALSE);return 0;}
 case WM_LBUTTONDOWN:{int x=(int)(GET_X_LPARAM(lp)/uiScale),y=(int)(GET_Y_LPARAM(lp)/uiScale);SetFocus(w);
  if(y>=14&&y<46){if(x>=clientW-side-104&&x<clientW-side-16)openBindingSettings();else if(x>=clientW-side-184&&x<clientW-side-120)moreMenu();return 0;}
  if(y>=66&&y<98){
   if(x>=244&&x<308&&!showAllUnfinished){onlyUnsolved=false;selected=-1;}
   else if(x>=308&&x<396){onlyUnsolved=true;selected=-1;}
   else if(x>=412&&x<524)toggleAllUnfinished();
   else if(x>=clientW-side-88&&x<clientW-side-16)refresh();
   else {int bx=18;for(int i=0;i<3;i++){bool known=false;for(auto&r:data.open)if(r.l==i)known=true;if(known){if(x>=bx&&x<bx+58){layer=i;selected=-1;reset();}bx+=66;}}}
   InvalidateRect(w,nullptr,FALSE);return 0;
  }
  if(y<top)return 0;
  if(y>=top&&x<clientW-side){dragging=true;moved=false;drag={x,y};SetCapture(w);}return 0;}
 case WM_MOUSELEAVE:hoverX=hoverY=-1;InvalidateRect(w,nullptr,FALSE);return 0;
 case WM_MOUSEMOVE:{int hx=(int)(GET_X_LPARAM(lp)/uiScale),hy=(int)(GET_Y_LPARAM(lp)/uiScale);if(hx!=hoverX||hy!=hoverY){bool header=hy<top||hoverY<top;hoverX=hx;hoverY=hy;if(header)InvalidateRect(w,nullptr,FALSE);TRACKMOUSEEVENT leave{sizeof(leave),TME_LEAVE,w,0};TrackMouseEvent(&leave);}}if(dragging){int x=(int)(GET_X_LPARAM(lp)/uiScale),y=(int)(GET_Y_LPARAM(lp)/uiScale);if(std::abs(x-drag.x)+std::abs(y-drag.y)>2)moved=true;cx-=(x-drag.x)/zoom;cy+=(y-drag.y)/zoom;drag={x,y};InvalidateRect(w,nullptr,FALSE);}return 0;
 case WM_LBUTTONUP:if(dragging){ReleaseCapture();dragging=false;if(!moved){selected=-1;double best=unfinishedHitRadius*unfinishedHitRadius;for(size_t i=0;i<data.marks.size();i++){auto&m=data.marks[i];if(!match(m))continue;auto p=screen(m.x,m.y);double dd=(p.x-drag.x)*(p.x-drag.x)+(p.y-drag.y)*(p.y-drag.y);if(dd<best&&covers(data,layer,m.x,m.y)){selected=(int)i;best=dd;}}InvalidateRect(w,nullptr,FALSE);}}return 0;
 case WM_CAPTURECHANGED:dragging=false;return 0;
 case WM_TIMER:{
  static ULONGLONG lastKeyLoad=0;auto now=GetTickCount64();if(!editingBindings&&!bindingPath.empty()&&now-lastKeyLoad>1500){loadBindings(bindingPath);lastKeyLoad=now;}
  HWND front=GetForegroundWindow();bool ours=front==wnd||front==bindingWnd||IsChild(wnd,front),game=front==gameWnd;
  if(IsWindowVisible(w)&&gameWnd&&!ours&&!game&&!menuOpen){resetScope();ShowWindow(w,SW_HIDE);}
  return 0;}
 case WM_CLOSE:hideMap();return 0;
 case WM_DESTROY:stopping=true;PostQuitMessage(0);return 0;
 }return DefWindowProcW(w,msg,wp,lp);
}
LRESULT CALLBACK intercept(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==WM_KILLFOCUS)swallowedGameKey=0;
 if(!editingBindings&&(msg==WM_KEYDOWN||msg==WM_SYSKEYDOWN)){
  if(swallowedGameKey==(UINT)wp)return 0;
  if(sameBinding(mapBinding,(UINT)wp,keyModifiers())){swallowedGameKey=(UINT)wp;if(!(lp&(1L<<30))&&wnd)PostMessageW(wnd,WM_APP+2,0,0);return 0;}
 }
 if((msg==WM_KEYUP||msg==WM_SYSKEYUP)&&swallowedGameKey==(UINT)wp){swallowedGameKey=0;return 0;}
 return CallWindowProcW(gameProc,w,msg,wp,lp);
}
void run(bool standalone){
 Gdiplus::GdiplusStartupInput gdiplusInput;if(Gdiplus::GdiplusStartup(&gdiplusToken,&gdiplusInput,nullptr)!=Gdiplus::Ok)return;
 wchar_t path[32768];GetModuleFileNameW(module,path,32768);root=dirOf(path);
 if(!standalone)root+=L"\\SinkingMap";
 gameDir=standalone?L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Order of the Sinking Star":dirOf(root);
 if(standalone){wchar_t b[32768];if(GetEnvironmentVariableW(L"SINKING_MAP_GAME",b,32768))gameDir=b;}
 loadGameFont();initBindings();
 PWSTR saved=nullptr;if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_SavedGames,0,nullptr,&saved))){saveRoot=std::wstring(saved)+L"\\Order of the Sinking Star";CoTaskMemFree(saved);}
 WIN32_FIND_DATAW fd{};HANDLE h=FindFirstFileW((saveRoot+L"\\*").c_str(),&fd);
 if(h!=INVALID_HANDLE_VALUE){do{if((fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&fd.cFileName[0]!=L'.'&&fileExists(saveRoot+L"\\"+fd.cFileName+L"\\Status.sokoban_status"))profiles.emplace_back(fd.cFileName);}while(FindNextFileW(h,&fd));FindClose(h);}std::sort(profiles.begin(),profiles.end());
 if(!standalone){for(int i=0;i<300&&!gameWnd;i++){EnumWindows(findWindow,0);Sleep(100);}if(!gameWnd)return;}
 WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=module;wc.lpszClassName=L"SinkingMapFogSafe";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
 wnd=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"沉星之序地图 · 存档地图",WS_POPUP|WS_CLIPCHILDREN,100,100,1200,800,gameWnd,nullptr,module,nullptr);
 if(!wnd)return;
 if(gameWnd){gameProc=(WNDPROC)GetWindowLongPtrW(gameWnd,GWLP_WNDPROC);SetWindowLongPtrW(gameWnd,GWLP_WNDPROC,(LONG_PTR)intercept);}
 if(standalone)openMap();
 MSG m;while(GetMessageW(&m,nullptr,0,0)>0){
  if(bindingWnd){
   if(m.message==WM_KEYDOWN&&m.wParam==VK_ESCAPE){DestroyWindow(bindingWnd);continue;}
   if(IsDialogMessageW(bindingWnd,&m))continue;
  }else if(m.message==WM_KEYDOWN||m.message==WM_SYSKEYDOWN){
   int mods=keyModifiers();bool textInput=(GetFocus()==searchBox||GetFocus()==profileBox);
   bool toggle=useMapBinding(mapBinding,(UINT)m.wParam,mods,textInput);
   bool reload=useMapBinding(refreshBinding,(UINT)m.wParam,mods,textInput);
   if(toggle||reload||m.wParam==VK_ESCAPE){if(!(m.lParam&(1L<<30))){if(reload)refresh();else hideMap();}continue;}
  }
  TranslateMessage(&m);DispatchMessageW(&m);
 }stopping=true;if(worker.joinable())worker.join();
 if(gameFontLoaded)RemoveFontResourceExW(gameFontFile.c_str(),FR_PRIVATE,nullptr);
}
}

#ifdef STANDALONE
int WINAPI wWinMain(HINSTANCE h,HINSTANCE,LPWSTR,int){sm::module=h;sm::run(true);return 0;}
#else
static HMODULE systemDxgi=nullptr;
static INIT_ONCE init=INIT_ONCE_STATIC_INIT;
static BOOL CALLBACK initialize(PINIT_ONCE,PVOID,PVOID*){
 wchar_t path[MAX_PATH];GetSystemDirectoryW(path,MAX_PATH);std::wstring dll=std::wstring(path)+L"\\dxgi.dll";systemDxgi=LoadLibraryW(dll.c_str());
 wchar_t host[32768];GetModuleFileNameW(nullptr,host,32768);std::wstring name=host;auto p=name.find_last_of(L"\\/");name=name.substr(p+1);
 if(_wcsicmp(name.c_str(),L"sinking_star.exe")==0)std::thread([]{sm::run(false);}).detach();return TRUE;
}
static FARPROC real(const char*name){InitOnceExecuteOnce(&init,initialize,nullptr,nullptr);return systemDxgi?GetProcAddress(systemDxgi,name):nullptr;}
extern "C" __declspec(dllexport) HRESULT WINAPI CreateDXGIFactory2(UINT f,REFIID i,void**p){using Fn=HRESULT(WINAPI*)(UINT,REFIID,void**);auto fn=(Fn)real("CreateDXGIFactory2");return fn?fn(f,i,p):E_FAIL;}
extern "C" __declspec(dllexport) HRESULT WINAPI CreateDXGIFactory1(REFIID i,void**p){using Fn=HRESULT(WINAPI*)(REFIID,void**);auto fn=(Fn)real("CreateDXGIFactory1");return fn?fn(i,p):E_FAIL;}
extern "C" __declspec(dllexport) HRESULT WINAPI CreateDXGIFactory(REFIID i,void**p){using Fn=HRESULT(WINAPI*)(REFIID,void**);auto fn=(Fn)real("CreateDXGIFactory");return fn?fn(i,p):E_FAIL;}
extern "C" __declspec(dllexport) HRESULT WINAPI DXGIGetDebugInterface1(UINT f,REFIID i,void**p){using Fn=HRESULT(WINAPI*)(UINT,REFIID,void**);auto fn=(Fn)real("DXGIGetDebugInterface1");return fn?fn(f,i,p):E_FAIL;}
extern "C" __declspec(dllexport) HRESULT WINAPI DXGIDeclareAdapterRemovalSupport(){using Fn=HRESULT(WINAPI*)();auto fn=(Fn)real("DXGIDeclareAdapterRemovalSupport");return fn?fn():E_FAIL;}
BOOL WINAPI DllMain(HINSTANCE h,DWORD why,LPVOID){if(why==DLL_PROCESS_ATTACH){sm::module=h;DisableThreadLibraryCalls(h);}return TRUE;}
#endif

