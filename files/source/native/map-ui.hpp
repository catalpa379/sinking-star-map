// Refactoring UI §2.1/2.8/3.6/4.1: map is primary, progress and existing area names secondary.
// Visual-only refinement: warm ink/champagne palette, 8/16/24 spacing, 12/14/18/24/28 type; no coordinate UI.
const COLORREF card=RGB(40,35,26),edge=RGB(89,76,49),well=RGB(17,19,16);
// Refactoring UI 2.1/2.5.4/4.1: state text has consistent roles; map marker colours are unchanged.
const COLORREF progressPending=RGB(214,139,165);
void rounded(HDC dc,RECT r,COLORREF color,int radius=4){auto oldPen=SelectObject(dc,GetStockObject(NULL_PEN));HBRUSH brush=CreateSolidBrush(color);auto old=SelectObject(dc,brush);RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);SelectObject(dc,old);SelectObject(dc,oldPen);DeleteObject(brush);}
// The reference-derived raster ornament is below all controls and text, never map data.
std::unique_ptr<Gdiplus::Bitmap> starOrnament;
std::wstring starOrnamentPath;
void ornament(HDC dc,RECT clip,int x,int y,int width,float opacity,bool topHalf=false){
 std::wstring path=root+L"\\ui\\star-rosette.png";
 if(path!=starOrnamentPath){starOrnamentPath=path;starOrnament=std::make_unique<Gdiplus::Bitmap>(path.c_str());}
 if(!starOrnament||starOrnament->GetLastStatus()!=Gdiplus::Ok)return;
 // Refactoring UI 7.2/2.4: decoration stays below the controls. Convert once to device pixels.
 int height=(int)(width*(double)starOrnament->GetHeight()/starOrnament->GetWidth());
 if(topHalf)y=-height/2;
 if(clip.right<=clip.left||clip.bottom<=clip.top)return;
 POINT p[]={{clip.left,clip.top},{clip.right,clip.bottom},{x,y},{x+width,y+height}};LPtoDP(dc,p,4);
 int saved=SaveDC(dc);SetGraphicsMode(dc,GM_ADVANCED);ModifyWorldTransform(dc,nullptr,MWT_IDENTITY);
 {
  Gdiplus::Graphics g(dc);g.SetPageUnit(Gdiplus::UnitPixel);g.ResetTransform();
  g.SetClip(Gdiplus::Rect(p[0].x,p[0].y,p[1].x-p[0].x,p[1].y-p[0].y));
  Gdiplus::ColorMatrix matrix={{{1,0,0,0,0},{0,1,0,0,0},{0,0,1,0,0},{0,0,0,opacity,0},{0,0,0,0,1}}};
  Gdiplus::ImageAttributes attributes;attributes.SetColorMatrix(&matrix);
  g.DrawImage(starOrnament.get(),Gdiplus::Rect(p[2].x,p[2].y,p[3].x-p[2].x,p[3].y-p[2].y),0,0,starOrnament->GetWidth(),starOrnament->GetHeight(),Gdiplus::UnitPixel,&attributes);
 }
 RestoreDC(dc,saved);
}
void trim(HDC dc,RECT r,COLORREF color){
 HPEN pen=CreatePen(PS_SOLID,1,color);auto old=SelectObject(dc,pen);auto brush=SelectObject(dc,GetStockObject(NULL_BRUSH));
 Rectangle(dc,r.left,r.top,r.right,r.bottom);SelectObject(dc,brush);SelectObject(dc,old);DeleteObject(pen);
}
void joint(HDC dc,int x,int y){
 POINT p[]={{x,y-3},{x+3,y},{x,y+3},{x-3,y}};auto old=SelectObject(dc,GetStockObject(NULL_PEN));HBRUSH b=CreateSolidBrush(amber);auto ob=SelectObject(dc,b);Polygon(dc,p,4);SelectObject(dc,ob);SelectObject(dc,old);DeleteObject(b);
}
void label(HDC dc,const std::wstring&s,RECT r,COLORREF color=fg,int size=14,bool bold=false,UINT flags=DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS){
 HFONT font=CreateFontW(-size,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,uiFontFace.c_str());auto old=SelectObject(dc,font);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);DrawTextW(dc,s.c_str(),(int)s.size(),&r,flags);SelectObject(dc,old);DeleteObject(font);
}
void button(HDC dc,const wchar_t*caption,int x,int y,int width,bool active=false){
 bool hover=hoverX>=x&&hoverX<x+width&&hoverY>=y&&hoverY<y+32;
 rounded(dc,{x,y,x+width,y+32},active?amber:(hover?edge:card),2);
 trim(dc,{x,y,x+width,y+32},active?amber:edge);
 label(dc,caption,{x+8,y,x+width-8,y+32},active?well:fg,14,active,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
}
void paint(HDC dc){
 fill(dc,{0,0,clientW,clientH},fog);
 HRGN clip=CreateRectRgn(0,0,0,0);
 for(const auto&r:data.open)if(r.l==layer){POINT p[]={screen(r.x,r.y+r.h),screen(r.x+r.w,r.y)};LPtoDP(dc,p,2);HRGN part=CreateRectRgn(p[0].x,p[0].y,p[1].x,p[1].y);CombineRgn(clip,clip,part,RGN_OR);DeleteObject(part);}
 POINT boundary[]={{0,top},{clientW-side,clientH}};LPtoDP(dc,boundary,2);HRGN bounds=CreateRectRgn(boundary[0].x,boundary[0].y,boundary[1].x,boundary[1].y);CombineRgn(clip,clip,bounds,RGN_AND);DeleteObject(bounds);SelectClipRgn(dc,clip);
 fill(dc,{0,top,clientW-side,clientH},RGB(24,41,52));
 Surface*base=terrainFor(layer);
 if(base){HDC src=CreateCompatibleDC(dc);auto old=SelectObject(src,base->bitmap);auto a=screen(-24.5,170.5),b=screen(165.5,-19.5);SetStretchBltMode(dc,HALFTONE);SetBrushOrgEx(dc,0,0,nullptr);StretchBlt(dc,a.x,a.y,b.x-a.x,b.y-a.y,src,0,0,base->w,base->h,SRCCOPY);SelectObject(src,old);DeleteDC(src);}
 else for(const auto&t:data.tiles)if(t.l==layer){auto a=screen(t.x-.5,t.y+.5),b=screen(t.x+.5,t.y-.5);int tint=std::clamp(65+(int)t.z,45,110);fill(dc,{a.x,a.y,b.x,b.y},RGB(tint-12,tint+10,tint+13));}
 // Point visibility has its own height-aware grants; never punch holes in the raster mask.
 HRGN markerBounds=CreateRectRgn(boundary[0].x,boundary[0].y,boundary[1].x,boundary[1].y);SelectClipRgn(dc,markerBounds);DeleteObject(markerBounds);
 // Completed pads first. Unfinished markers always stay on top at a fixed screen size, even when zoomed out.
 for(int pass=1;pass>=0;pass--)for(size_t i=0;i<data.marks.size();i++){const auto&m=data.marks[i];if(m.state!=pass||!match(m)||!coversMarker(data,m))continue;auto p=screen(m.x,m.y);if(p.x<0||p.x>clientW-side||p.y<top||p.y>clientH)continue;drawMarker(dc,m,p,(int)i==selected);}
 SelectClipRgn(dc,clip);
 if(data.player&&data.hero.l==layer){auto p=screen(data.hero.x,data.hero.y);HPEN pen=CreatePen(PS_SOLID,3,RGB(120,205,255));auto old=SelectObject(dc,pen);MoveToEx(dc,p.x-10,p.y,nullptr);LineTo(dc,p.x+11,p.y);MoveToEx(dc,p.x,p.y-10,nullptr);LineTo(dc,p.x,p.y+11);SelectObject(dc,old);DeleteObject(pen);}
 SelectClipRgn(dc,nullptr);DeleteObject(clip);
 fill(dc,{0,0,clientW,top},panel);fill(dc,{clientW-side,0,clientW,clientH},panel);
 // Decorative layer is deliberately confined to chrome; hidden map pixels remain pure fog.
 // Refactoring UI 2.4/7.2: two bounded accents balance the header and sidebar.
 // The title, profile/search controls and fog are outside the ornament clips.
 // Anchor the visible rosette, not the padded image canvas: source centre (768,480), right 1174, bottom 884.
 int mapRight=clientW-side;
 const double motifScale=top/(884.-480.)*1.8;
 int headerSize=(int)std::lround(1536*motifScale),motifRight=mapRight-184-8;
 int headerX=(int)std::lround(motifRight-1174*motifScale),headerY=(int)std::lround(-480*motifScale);
 ornament(dc,{196,0,motifRight,top},headerX,headerY,headerSize,mapRight>=900?.16f:.12f);
 int ornamentTop=std::max(top+256,clientH-512);
 ornament(dc,{mapRight+16,ornamentTop,clientW-16,clientH-48},mapRight+(side-640)/2,clientH-464,640,.12f);
 fill(dc,{18,top-1,clientW-side-18,top},edge);fill(dc,{clientW-side,16,clientW-side+1,clientH-16},edge);joint(dc,clientW-side,top);joint(dc,18,top-1);joint(dc,clientW-side-18,top-1);
 label(dc,L"沉星之序地图",{24,8,244,46},amber,28,true);
 button(dc,L"更多",mapRight-184,14,64);button(dc,L"按键设置",mapRight-104,14,88);
 bool known[3]={};for(auto&r:data.open)if(r.l>=0&&r.l<3)known[r.l]=true;const wchar_t*names[]={L"地面",L"地下",L"空中"};int bx=18;
 for(int i=0;i<3;i++)if(known[i]){label(dc,names[i],{bx,66,bx+58,98},layer==i?amber:muted,16,false,DT_CENTER|DT_VCENTER|DT_SINGLELINE);if(layer==i)fill(dc,{bx+12,100,bx+46,102},amber);bx+=66;}
 fill(dc,{224,74,225,90},edge);
 button(dc,L"全部",244,66,64,!onlyUnsolved&&!showAllUnfinished);button(dc,L"未完成",308,66,88,onlyUnsolved||showAllUnfinished);
 if(showAllUnfinished){label(dc,L"全部",{252,66,300,98},muted,14,false,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
 // Frequent scope control sits beside the completion filters; text and fill both indicate its state.
 button(dc,showAllUnfinished?L"剧透：开":L"剧透：关",412,66,112,showAllUnfinished);
 button(dc,L"刷新",mapRight-88,66,72);
 int sx=clientW-side+24,right=clientW-24;
 label(dc,L"当前档案",{sx,14,right,35},muted,12);label(dc,L"须与游戏使用的档案一致",{sx,80,right,104},muted,12);
 label(dc,showAllUnfinished?L"搜索全地图未完成地点":L"搜索已揭露的地点",{sx,top+16,right,top+38},fg,16,true);
 rounded(dc,{sx-6,top+44,right+6,top+82},edge);
 std::set<std::wstring> counted;int solved=0,unsolved=0;std::map<std::wstring,std::pair<int,int>> areas;
 for(const auto&m:data.marks){if(!counted.insert(m.countKey).second)continue;if(m.state)solved++;else unsolved++;auto&a=areas[m.area];if(m.state)a.first++;else a.second++;}
 bool compact=clientH<720;
 // Keep save-wide progress primary and map coverage secondary, using existing type/color tokens.
 int y=top+(compact?88:104);int progressHeight=showAllUnfinished?88:116;
 rounded(dc,{sx-8,y,right+8,y+progressHeight},well);
 label(dc,showAllUnfinished?L"全地图未完成 · 含未探索":L"存档进度 · 已揭露地图",{sx+8,y+8,right-8,y+30},fg,14);
 if(busy||!data.error.empty())label(dc,busy?L"读取中…":L"状态无法确认",{sx+8,y+35,right-8,y+75},muted,20);
 else{
 label(dc,std::to_wstring(unsolved),{sx+8,y+30,sx+140,y+64},fg,28,true);label(dc,showAllUnfinished?L"未完成":L"地图内未完成",{sx+8,y+64,sx+140,y+82},progressPending,14);
 if(!showAllUnfinished){fill(dc,{sx+148,y+38,sx+149,y+74},edge);label(dc,data.savedSolved>=0?std::to_wstring(data.savedSolved):L"—",{sx+172,y+32,right-8,y+64},fg,24);label(dc,L"存档总完成",{sx+172,y+64,right-8,y+82},green,14);
 label(dc,L"当前地图内已完成："+std::to_wstring(solved),{sx+8,y+88,right-8,y+108},muted,13);}}
 y+=progressHeight+(compact?8:16);
 if(selected>=0&&selected<(int)data.marks.size()&&match(data.marks[selected])){
  const auto&m=data.marks[selected];rounded(dc,{sx-8,y,right+8,y+100},card);fill(dc,{sx-8,y,sx-6,y+100},amber);
  label(dc,m.name,{sx+8,y+5,right-8,y+31},amber,18,true);
  label(dc,L"区域："+m.area,{sx+8,y+32,right-8,y+52},muted,12);
  label(dc,m.id,{sx+8,y+53,right-96,y+73},muted,14);
  label(dc,m.state?L"已完成":L"未完成",{right-88,y+53,right-8,y+73},m.state?green:progressPending,14,false,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
  label(dc,L"坐标："+std::to_wstring((int)m.x)+L", "+std::to_wstring((int)m.y)+L", "+std::to_wstring((int)m.z),{sx+8,y+74,right-8,y+94},muted,12);y+=compact?108:112;
 }
 label(dc,L"区域汇总",{sx,y,right,y+24},amber,18,true);label(dc,L"滚轮翻阅",{right-100,y,right,y+24},muted,14,false,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);y+=28;
 label(dc,showAllUnfinished?L"全地图未完成 · 按谜题去重":L"已揭露范围 · 按谜题去重",{sx,y,right,y+20},fg,14);y+=28;
 std::vector<std::pair<std::wstring,std::pair<int,int>>> rows(areas.begin(),areas.end());std::stable_sort(rows.begin(),rows.end(),[](const auto&a,const auto&b){return a.second.second>b.second.second;});
 int listY=y;int slots=std::max(0,(clientH-56-y)/64),idx=0;areaPage=std::clamp(areaPage,0,std::max(0,(int)rows.size()-std::max(1,slots)));
 for(const auto&[name,n]:rows){if(idx>=areaPage&&idx<areaPage+slots){
  label(dc,name,{sx,y+2,right,y+29},fg,16,true);
  label(dc,L"未完成",{sx,y+30,sx+72,y+54},progressPending,14);
  label(dc,std::to_wstring(n.second),{sx+72,y+30,sx+136,y+54},progressPending,14,false,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
  if(!showAllUnfinished){
   label(dc,L"已完成",{sx+156,y+30,sx+228,y+54},green,14);
   label(dc,std::to_wstring(n.first),{right-64,y+30,right,y+54},green,14,false,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
  }
  fill(dc,{sx,y+60,right,y+61},edge);y+=64;
 }idx++;}
 if(slots>0&&(int)rows.size()>slots){int track=slots*64,thumb=std::max(16,track*slots/(int)rows.size());int offset=(track-thumb)*areaPage/std::max(1,(int)rows.size()-slots);fill(dc,{clientW-10,listY+offset,clientW-8,listY+offset+thumb},muted);}
 if(rows.empty())label(dc,busy?L"正在读取区域…":L"暂无可确认的区域",{sx,y,right,y+36},muted,13);
 label(dc,status,{sx,clientH-48,right,clientH-6},fg,14,false,DT_LEFT|DT_TOP|DT_WORDBREAK|DT_END_ELLIPSIS);
 fill(dc,{0,clientH-32,clientW-side,clientH},panel);
 label(dc,showAllUnfinished?(clientW-side<800?L"剧透已开启":L"剧透已开启，关闭开关恢复迷雾。"):L"未揭露区域保留迷雾",{18,clientH-32,clientW-side-320,clientH-4},fg,14);
 label(dc,L"! 未完成   ● 已完成   + 存档位置",{clientW-side-310,clientH-32,clientW-side-16,clientH-4},muted,14,false,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
}
