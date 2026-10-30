// stdout EOF and process termination are separate events. An EOF never proves a crash.
struct WorkerReadFailure {std::wstring message;};
struct WorkerHandles {
 HANDLE read=nullptr,write=nullptr,nul=nullptr;PROCESS_INFORMATION process{};
 ~WorkerHandles(){if(read)CloseHandle(read);if(write)CloseHandle(write);if(nul)CloseHandle(nul);if(process.hProcess)CloseHandle(process.hProcess);if(process.hThread)CloseHandle(process.hThread);}
};
Snapshot readSnapshotProcess(const std::wstring&exe,std::wstring command,const std::wstring&directory,bool requestedAll,DWORD timeoutMs=30000){
 WorkerHandles h;const std::wstring cleared=L"；地图已清空，请刷新重试。";
 try{
  SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
  if(!CreatePipe(&h.read,&h.write,&sa,0)||!SetHandleInformation(h.read,HANDLE_FLAG_INHERIT,0))throw WorkerReadFailure{L"无法建立读取通道"+cleared};
  h.nul=CreateFileW(L"NUL",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);if(h.nul==INVALID_HANDLE_VALUE){h.nul=nullptr;throw WorkerReadFailure{L"无法准备读取程序"+cleared};}
  STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdOutput=h.write;si.hStdInput=h.nul;si.hStdError=h.nul;
  BOOL ok=CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,directory.empty()?nullptr:directory.c_str(),&si,&h.process);
  DWORD startError=GetLastError();CloseHandle(h.write);h.write=nullptr;CloseHandle(h.nul);h.nul=nullptr;
  if(!ok)throw WorkerReadFailure{L"无法启动读取程序（系统错误 "+std::to_wstring(startError)+L"）"+cleared};
  std::string output;char bytes[8192];ULONGLONG deadline=GetTickCount64()+timeoutMs;
  auto remaining=[&](){ULONGLONG now=GetTickCount64();return now<deadline?(DWORD)std::min<ULONGLONG>(deadline-now,10):0;};
  auto append=[&](DWORD available){DWORD got=0;if(!ReadFile(h.read,bytes,std::min<DWORD>(sizeof(bytes),available),&got,nullptr))throw WorkerReadFailure{L"读取通道中断"+cleared};output.append(bytes,got);if(output.size()>32*1024*1024)throw WorkerReadFailure{L"读取结果超过支持大小"+cleared};};
  while(!stopping&&remaining()){
   DWORD available=0;
   if(!PeekNamedPipe(h.read,nullptr,0,nullptr,&available,nullptr)){
    if(GetLastError()==ERROR_BROKEN_PIPE)break;
    throw WorkerReadFailure{L"无法读取进度通道"+cleared};
   }
   if(available){append(available);continue;}
   DWORD wait=WaitForSingleObject(h.process.hProcess,remaining());if(wait==WAIT_OBJECT_0)break;if(wait==WAIT_FAILED)throw WorkerReadFailure{L"无法等待读取程序"+cleared};
  }
  // The child may close stdout before completing normal shutdown. Wait within the same deadline.
  DWORD exit=WAIT_TIMEOUT;
  while(!stopping&&(exit=WaitForSingleObject(h.process.hProcess,remaining()))==WAIT_TIMEOUT&&remaining()){}
  if(stopping)throw WorkerReadFailure{L"读取已取消"+cleared};
  if(exit!=WAIT_OBJECT_0)throw WorkerReadFailure{L"读取超时"+cleared};
  for(;;){DWORD available=0;if(!PeekNamedPipe(h.read,nullptr,0,nullptr,&available,nullptr)){if(GetLastError()==ERROR_BROKEN_PIPE)break;throw WorkerReadFailure{L"读取通道中断"+cleared};}if(!available)break;append(available);}
  DWORD code=1;if(!GetExitCodeProcess(h.process.hProcess,&code))throw WorkerReadFailure{L"无法确认读取程序状态"+cleared};
  Snapshot result;try{result=parse(output);}catch(...){throw WorkerReadFailure{L"读取结果不完整（程序退出码 "+std::to_wstring(code)+L"）"+cleared};}
  if(!result.error.empty())return result;
  if(code!=0)throw WorkerReadFailure{L"读取程序异常结束（退出码 "+std::to_wstring(code)+L"）"+cleared};
  if(result.allUnfinished!=requestedAll)throw WorkerReadFailure{L"读取结果的显示模式不匹配"+cleared};
  for(const auto&m:result.marks)if(!covers(result,m.l,m.x,m.y)||m.state<0||m.state>1)throw WorkerReadFailure{L"读取结果未通过可见性校验"+cleared};
  if(result.player&&!covers(result,result.hero.l,result.hero.x,result.hero.y))throw WorkerReadFailure{L"存档位置未通过可见性校验"+cleared};
  return result;
 }catch(...){if(h.process.hProcess&&WaitForSingleObject(h.process.hProcess,0)==WAIT_TIMEOUT){TerminateProcess(h.process.hProcess,1);WaitForSingleObject(h.process.hProcess,1000);}throw;}
}
