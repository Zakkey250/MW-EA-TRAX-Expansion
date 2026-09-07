#include "StartupWait.h"
#include "GameImports.h"
#include <array>
#include <cstdio>
#include <cstdlib>
using namespace eatrax::startup;
int checks=0;
void Check(bool v){++checks;if(!v){puts("FAIL");exit(1);}}
struct Context {DWORD thread;HANDLE ready;bool cancel;bool observed=false;};
BOOL CALLBACK Inspect(HWND h,LPARAM param){
 auto* c=reinterpret_cast<Context*>(param);wchar_t title[256]{};GetWindowTextW(h,title,256);
 if(std::wstring(title).find(L"EA TRAX Expansion")!=0)return TRUE;
 Check(IsWindowVisible(h)!=FALSE);
 Check(GetDlgItem(h,100)!=nullptr && GetDlgItem(h,101)!=nullptr && GetDlgItem(h,IDCANCEL)!=nullptr);
 wchar_t caption[256]{};GetWindowTextW(GetDlgItem(h,IDCANCEL),caption,256);Check(std::wstring(caption)==currentText->cancel);
 RECT parent{},button{};GetClientRect(h,&parent);GetWindowRect(GetDlgItem(h,IDCANCEL),&button);
 MapWindowPoints(nullptr,h,reinterpret_cast<POINT*>(&button),2);
 Check(button.left>=0&&button.right<=parent.right&&button.bottom<=parent.bottom);
 c->observed=true;
 if(c->cancel)PostMessageW(h,WM_COMMAND,IDCANCEL,0);
 return TRUE;
}
DWORD WINAPI Worker(void* p){auto* c=static_cast<Context*>(p);Sleep(350);EnumThreadWindows(c->thread,Inspect,reinterpret_cast<LPARAM>(c));Sleep(300);SetEvent(c->ready);return 0;}
int main(){
 std::array<unsigned char,4096> image{};auto* base=image.data();
 auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=128;
 auto* nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(base+128);nt->Signature=IMAGE_NT_SIGNATURE;nt->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;
 nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]={512,40};
 auto* entry=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+512);entry->Name=768;entry->OriginalFirstThunk=1024;entry->FirstThunk=1536;
 strcpy_s(reinterpret_cast<char*>(base+768),32,"KERNEL32.dll");
 auto* names=reinterpret_cast<IMAGE_THUNK_DATA32*>(base+1024);names[0].u1.Ordinal=IMAGE_ORDINAL_FLAG32|12;names[1].u1.AddressOfData=1280;
 auto* name=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+1280);strcpy_s(reinterpret_cast<char*>(name->Name),32,"CreateFileA");
 auto** slot=reinterpret_cast<void**>(base+1540);*slot=reinterpret_cast<void*>(0x12345678);
 Check(eatrax::FindGameImport(reinterpret_cast<HMODULE>(base),"kernel32.DLL","CreateFileA")==slot);
 *slot=reinterpret_cast<void*>(0x76543210); // An earlier MOD may already replace the import.
 Check(eatrax::FindGameImport(reinterpret_cast<HMODULE>(base),"KERNEL32.dll","CreateFileA")==slot);
 Check(eatrax::FindGameImport(reinterpret_cast<HMODULE>(base),"KERNEL32.dll","Missing")==nullptr);
 Check(eatrax::FindGameImport(reinterpret_cast<HMODULE>(base),"user32.dll","CreateFileA")==nullptr);
 entry->OriginalFirstThunk=0;Check(eatrax::FindGameImport(reinterpret_cast<HMODULE>(base),"KERNEL32.dll","CreateFileA")==nullptr);
 dos->e_magic=0;Check(eatrax::FindGameImport(reinterpret_cast<HMODULE>(base),"KERNEL32.dll","CreateFileA")==nullptr);

 for(const auto& text:translations){Check(&Lookup(text.language)==&text);Check(wcslen(text.restartBody)>30);Check(wcslen(text.cancel)>4);}
 Check(&Lookup(L"English UK")==&translations[0]);Check(&Lookup(L" Japanese ; note ")==&translations[1]);
 HANDLE ready=CreateEventW(nullptr,TRUE,FALSE,nullptr),cancel=CreateEventW(nullptr,TRUE,FALSE,nullptr);
 for(int i=0;i<2;++i){currentText=&translations[i];Update(Stage::Generating,L"2/27");ResetEvent(ready);ResetEvent(cancel);
  Context context{GetCurrentThreadId(),ready,i==1};HANDLE worker=CreateThread(nullptr,0,Worker,&context,0,nullptr);
  Check(Wait(GetModuleHandleW(nullptr),ready,cancel)==(i==0));WaitForSingleObject(worker,5000);CloseHandle(worker);
  Check(context.observed);Check((WaitForSingleObject(cancel,0)==WAIT_OBJECT_0)==(i==1));
 }
 CloseHandle(ready);CloseHandle(cancel);printf("PASS %d checks: locale lookup, live dialog controls/layout, completed wait, cancellation\n",checks);
}
