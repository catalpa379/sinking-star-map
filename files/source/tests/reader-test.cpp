#define STANDALONE
#include "../native/map.cpp"
#include <cassert>
#include <iostream>
std::string payload(bool all){return std::string("SMAP1\n")+(all?"M\tALL_UNFINISHED\n":"")+"O\t0\t0\t0\t10\t10\nP\t0\t2\t2\t0\t0\t#test\t\tknown\tknown\t#test\nEND";}
int testMain(int argc,char**argv){using namespace sm;
 if(argc==2&&std::string(argv[1]).starts_with("--child-")){
  std::string mode=argv[1],bytes=mode=="--child-invalid"?"SMAP1\n":mode=="--child-error"?"SMAP1\nERROR\tTEST_READ_ERROR\nEND":payload(mode=="--child-all");DWORD written=0;HANDLE out=GetStdHandle(STD_OUTPUT_HANDLE);WriteFile(out,bytes.data(),(DWORD)bytes.size(),&written,nullptr);CloseHandle(out);Sleep(mode=="--child-timeout"?2000:90);ExitProcess(mode=="--child-fail"?3:mode=="--child-error"?1:0);
 }
 wchar_t modulePath[32768];GetModuleFileNameW(nullptr,modulePath,32768);std::wstring exe=modulePath;
 for(int i=0;i<8;i++){bool all=i%2;auto s=readSnapshotProcess(exe,quote(exe)+(all?L" --child-all":L" --child-normal"),L"",all,10000);assert(s.error.empty()&&s.allUnfinished==all&&s.marks.size()==1);std::cout<<"PASS early EOF iteration "<<i+1<<std::endl;}
 auto failure=[&](const wchar_t*mode,DWORD timeout){bool rejected=false;try{readSnapshotProcess(exe,quote(exe)+L" "+mode,L"",false,timeout);}catch(const WorkerReadFailure&){rejected=true;}assert(rejected);};
 failure(L"--child-invalid",2000);failure(L"--child-fail",2000);failure(L"--child-timeout",40);failure(L"--child-all",2000);
 auto error=readSnapshotProcess(exe,quote(exe)+L" --child-error",L"",false,2000);assert(error.error==L"TEST_READ_ERROR"&&error.marks.empty());
 std::cout<<"PASS production pipe reader: repeated early stdout EOF, delayed exit, truncated output, nonzero exit, timeout, wrong mode and preserved worker errors\n";
 if(argc==4){
  gameDir=wide(argv[1]);std::wstring campaign=wide(argv[2]);saveRoot=dirOf(campaign);profiles={campaign.substr(campaign.find_last_of(L"\\/")+1)};root=wide(argv[3]);module=GetModuleHandleW(nullptr);
  WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=module;cls.lpszClassName=L"SMReadOnlyRegression";RegisterClassW(&cls);
  HWND hidden=CreateWindowExW(0,cls.lpszClassName,L"read-only regression",WS_POPUP,0,0,1200,800,nullptr,nullptr,module,nullptr);assert(hidden&&!IsWindowVisible(hidden));
  auto pump=[&](){ULONGLONG end=GetTickCount64()+60000;do{MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}if(!busy)break;Sleep(5);}while(GetTickCount64()<end);assert(!busy);if(worker.joinable())worker.join();};
  auto valid=[&](bool all){if(!data.error.empty())throw WorkerReadFailure{data.error};assert(data.allUnfinished==all&&!data.open.empty());};
  refresh();pump();valid(false);
  for(int i=0;i<6;i++){toggleAllUnfinished();pump();valid(i%2==0);std::cout<<"PASS real async completed toggle "<<i+1<<std::endl;}
  toggleAllUnfinished();assert(busy);toggleAllUnfinished();toggleAllUnfinished();toggleAllUnfinished();assert(refreshQueued&&!showAllUnfinished);pump();valid(false);
  std::cout<<"PASS rapid on/off/on/off during an active read resolves to latest normal scope\n";
  toggleAllUnfinished();assert(busy);resetScope();pump();assert(!showAllUnfinished&&data.open.empty()&&data.marks.empty()&&!refreshQueued);
  refresh();refresh();refresh();assert(refreshQueued);pump();valid(false);
  std::cout<<"PASS close/reset discards stale spoiler results; queued F5 refresh recovers normal scope\n";
  stopping=true;DestroyWindow(hidden);
 }
 return 0;
}
int main(int argc,char**argv){try{return testMain(argc,argv);}catch(const sm::WorkerReadFailure&error){int n=WideCharToMultiByte(CP_UTF8,0,error.message.data(),(int)error.message.size(),nullptr,0,nullptr,nullptr);std::string msg(n,0);WideCharToMultiByte(CP_UTF8,0,error.message.data(),(int)error.message.size(),msg.data(),n,nullptr,nullptr);std::cerr<<msg<<std::endl;return 1;}}
