// User preferences live outside the game and saves; both installer and map share this format.
std::atomic<int> mapBinding=VK_F8,refreshBinding=VK_F5;
std::atomic<bool> editingBindings=false;
std::atomic<UINT> swallowedGameKey=0;
std::wstring bindingPath;
HWND bindingWnd=nullptr,bindingEdit[2]{},bindingNotice=nullptr;
int pendingBinding[2]{VK_F8,VK_F5};
WNDPROC bindingEditProc=nullptr;
int keyModifiers(){return ((GetKeyState(VK_MENU)&0x8000)?1:0)|((GetKeyState(VK_CONTROL)&0x8000)?2:0)|((GetKeyState(VK_SHIFT)&0x8000)?4:0);}
bool validBinding(int code){
 int key=code&255,mods=code>>8;
 bool allowed=(key>=VK_F1&&key<=VK_F24)||(key>='A'&&key<='Z')||(key>='0'&&key<='9')||(key>=VK_PRIOR&&key<=VK_DOWN)||key==VK_INSERT||key==VK_DELETE||key==VK_SPACE;
 return allowed&&mods>=0&&mods<=7&&!((mods&1)&&key==VK_F4);
}
bool sameBinding(int code,UINT key,int mods){return code==((mods<<8)|(int)key);}
bool textBinding(int code){int key=code&255;return (code>>8)==0&&((key>='A'&&key<='Z')||(key>='0'&&key<='9')||key==VK_SPACE||(key>=VK_PRIOR&&key<=VK_DOWN)||key==VK_DELETE);}
bool useMapBinding(int code,UINT key,int mods,bool textInput){return sameBinding(code,key,mods)&&!(textInput&&textBinding(code));}
std::wstring bindingName(int code){
 std::wstring name;int mods=code>>8,key=code&255;
 if(mods&2)name+=L"Ctrl + ";if(mods&1)name+=L"Alt + ";if(mods&4)name+=L"Shift + ";
 if(key>=VK_F1&&key<=VK_F24)return name+L"F"+std::to_wstring(key-VK_F1+1);
 wchar_t text[80]{};LONG scan=MapVirtualKeyW(key,MAPVK_VK_TO_VSC)<<16;
 if(key==VK_INSERT||key==VK_DELETE||(key>=VK_PRIOR&&key<=VK_DOWN))scan|=1<<24;
 if(GetKeyNameTextW(scan,text,80))return name+text;
 return name+std::wstring(1,(wchar_t)key);
}
void loadBindings(const std::wstring&file){
 int a=GetPrivateProfileIntW(L"Keys",L"Toggle",VK_F8,file.c_str()),b=GetPrivateProfileIntW(L"Keys",L"Refresh",VK_F5,file.c_str());
 if(!validBinding(a)||!validBinding(b)||a==b){a=VK_F8;b=VK_F5;}
 mapBinding=a;refreshBinding=b;
}
void initBindings(){
 PWSTR local=nullptr;if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local))){
  std::wstring folder=std::wstring(local)+L"\\SinkingMap";CoTaskMemFree(local);
  CreateDirectoryW(folder.c_str(),nullptr);bindingPath=folder+L"\\controls.ini";loadBindings(bindingPath);
 }
}
bool saveBindings(const std::wstring&file,int a,int b){
 if(file.empty()||!validBinding(a)||!validBinding(b)||a==b)return false;
 std::wstring temporary=file+L".tmp-"+std::to_wstring(GetCurrentProcessId());
 if(!WritePrivateProfileStringW(L"Keys",L"Toggle",std::to_wstring(a).c_str(),temporary.c_str())||!WritePrivateProfileStringW(L"Keys",L"Refresh",std::to_wstring(b).c_str(),temporary.c_str())){DeleteFileW(temporary.c_str());return false;}
 WritePrivateProfileStringW(nullptr,nullptr,nullptr,temporary.c_str());
 if(!MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(temporary.c_str());return false;}
 WritePrivateProfileStringW(nullptr,nullptr,nullptr,file.c_str());mapBinding=a;refreshBinding=b;return true;
}
LRESULT CALLBACK captureBinding(HWND w,UINT message,WPARAM key,LPARAM lp){
 if(message==WM_KEYDOWN||message==WM_SYSKEYDOWN){
  if(key==VK_ESCAPE){PostMessageW(bindingWnd,WM_CLOSE,0,0);return 0;}
  if(key==VK_TAB)return CallWindowProcW(bindingEditProc,w,message,key,lp);
  if(key==VK_RETURN)return 0;
  int code=(keyModifiers()<<8)|(int)key;
  if(validBinding(code)){int index=w==bindingEdit[1]?1:0;pendingBinding[index]=code;SetWindowTextW(w,bindingName(code).c_str());SetWindowTextW(bindingNotice,L"设置后点击保存。两个操作不能使用相同组合键。");}
  return 0;
 }
 if(message==WM_GETDLGCODE)return DLGC_WANTALLKEYS;
 if(message==WM_CHAR||message==WM_SYSCHAR||message==WM_KEYUP||message==WM_SYSKEYUP)return 0;
 return CallWindowProcW(bindingEditProc,w,message,key,lp);
}
LRESULT CALLBACK bindingProc(HWND w,UINT message,WPARAM wp,LPARAM lp){
 if(message==WM_ERASEBKGND){RECT r;GetClientRect(w,&r);HBRUSH brush=CreateSolidBrush(RGB(24,32,30));FillRect((HDC)wp,&r,brush);DeleteObject(brush);return 1;}
 if(message==WM_CTLCOLORSTATIC||message==WM_CTLCOLOREDIT){
  HDC dc=(HDC)wp;bool input=(HWND)lp==bindingEdit[0]||(HWND)lp==bindingEdit[1];
  SetTextColor(dc,RGB(241,233,213));SetBkColor(dc,input?RGB(39,48,43):RGB(24,32,30));
  static HBRUSH background=CreateSolidBrush(RGB(24,32,30)),field=CreateSolidBrush(RGB(39,48,43));return (LRESULT)(input?field:background);
 }
 if(message==WM_DRAWITEM){
  auto item=(DRAWITEMSTRUCT*)lp;bool save=item->CtlID==IDOK;
  COLORREF color=save?RGB(222,191,126):RGB(39,48,43);if(item->itemState&ODS_SELECTED)color=save?RGB(195,160,94):RGB(59,68,56);
  HBRUSH brush=CreateSolidBrush(color);FillRect(item->hDC,&item->rcItem,brush);DeleteObject(brush);
  auto old=SelectObject(item->hDC,controlFont);SetBkMode(item->hDC,TRANSPARENT);SetTextColor(item->hDC,save?RGB(17,25,23):RGB(241,233,213));
  wchar_t caption[64]{};GetWindowTextW(item->hwndItem,caption,64);RECT r=item->rcItem;DrawTextW(item->hDC,caption,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
  if(item->itemState&ODS_FOCUS){InflateRect(&r,-3,-3);DrawFocusRect(item->hDC,&r);}SelectObject(item->hDC,old);return TRUE;
 }
 if(message==WM_COMMAND){
  int action=LOWORD(wp);
  if(action==202){pendingBinding[0]=VK_F8;pendingBinding[1]=VK_F5;for(int i=0;i<2;i++)SetWindowTextW(bindingEdit[i],bindingName(pendingBinding[i]).c_str());return 0;}
  if(action==IDOK){
   if(pendingBinding[0]==pendingBinding[1]){SetWindowTextW(bindingNotice,L"按键重复，请为两个操作设置不同组合键。");return 0;}
   if(!saveBindings(bindingPath,pendingBinding[0],pendingBinding[1])){SetWindowTextW(bindingNotice,L"保存失败，原设置未改变。请检查用户目录权限。");return 0;}
   status=L"按键设置已保存";DestroyWindow(w);return 0;
  }
  if(action==IDCANCEL){DestroyWindow(w);return 0;}
 }
 if(message==WM_CLOSE){DestroyWindow(w);return 0;}
 if(message==WM_DESTROY){bindingWnd=nullptr;editingBindings=false;EnableWindow(wnd,TRUE);SetForegroundWindow(wnd);SetFocus(wnd);InvalidateRect(wnd,nullptr,FALSE);return 0;}
 return DefWindowProcW(w,message,wp,lp);
}
void openBindingSettings(){
 if(bindingWnd)return;
 pendingBinding[0]=mapBinding;pendingBinding[1]=refreshBinding;editingBindings=true;
 WNDCLASSW c{};c.hInstance=module;c.lpfnWndProc=bindingProc;c.lpszClassName=L"SinkingMapKeys";c.hCursor=LoadCursorW(nullptr,IDC_ARROW);c.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassW(&c);
 RECT owner{};GetWindowRect(wnd,&owner);int width=(int)(480*uiScale),height=(int)(272*uiScale);
 bindingWnd=CreateWindowExW(WS_EX_DLGMODALFRAME,c.lpszClassName,L"按键设置",WS_CAPTION|WS_SYSMENU|WS_POPUP,owner.left+(owner.right-owner.left-width)/2,owner.top+(owner.bottom-owner.top-height)/2,width,height,wnd,nullptr,module,nullptr);
 if(!bindingWnd){editingBindings=false;return;}
 auto control=[&](const wchar_t*type,const wchar_t*text,int id,DWORD style,int x,int y,int cw,int ch){if(std::wstring(type)==L"BUTTON")style=(style&~15)|BS_OWNERDRAW;HWND h=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|style,(int)(x*uiScale),(int)(y*uiScale),(int)(cw*uiScale),(int)(ch*uiScale),bindingWnd,(HMENU)(INT_PTR)id,module,nullptr);SendMessageW(h,WM_SETFONT,(WPARAM)controlFont,TRUE);return h;};
 control(L"STATIC",L"点击输入框，按下新的按键或组合键。",0,0,24,18,428,26);
 for(int i=0;i<2;i++){
  control(L"STATIC",i?L"刷新地图":L"打开 / 关闭地图",0,0,24,62+i*48,160,28);
  bindingEdit[i]=control(L"EDIT",bindingName(pendingBinding[i]).c_str(),210+i,WS_TABSTOP|WS_BORDER|ES_READONLY|ES_CENTER,192,58+i*48,254,32);
  auto old=(WNDPROC)SetWindowLongPtrW(bindingEdit[i],GWLP_WNDPROC,(LONG_PTR)captureBinding);if(i==0)bindingEditProc=old;
 }
 bindingNotice=control(L"STATIC",L"两个操作不能使用相同组合键。",0,0,24,154,424,28);
 control(L"BUTTON",L"恢复默认",202,WS_TABSTOP,24,192,110,32);control(L"BUTTON",L"取消",IDCANCEL,WS_TABSTOP,240,192,94,32);control(L"BUTTON",L"保存",IDOK,WS_TABSTOP|BS_DEFPUSHBUTTON,344,192,102,32);
 EnableWindow(wnd,FALSE);ShowWindow(bindingWnd,SW_SHOW);SetForegroundWindow(bindingWnd);SetFocus(bindingEdit[0]);
}
