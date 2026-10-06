#include "UpdateNotice.h"
#include "UpdateRelease.h"
#include "ModUpdateDialog.h"
#include "RuntimeInventory.h"
#include "StartupText.h"
#include "Logging.h"
#include <winhttp.h>
#include <atomic>
#include <memory>
#pragma comment(lib,"winhttp.lib")
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"advapi32.lib")
namespace eatrax { namespace {
constexpr char repository[]="Zakkey250/MW-EA-TRAX-Expansion";
constexpr char assetPrefix[]="NFSMWEATraxExpansion-";
constexpr wchar_t apiPath[]=L"/repos/Zakkey250/MW-EA-TRAX-Expansion/releases?per_page=100";
constexpr wchar_t releasesUrl[]=L"https://github.com/Zakkey250/MW-EA-TRAX-Expansion/releases";
std::atomic<bool> checked{false};
mod_update::KernelHandle checkedEvent;
bool StartupNoticeAllowed() noexcept {
    unsigned flow=6; SIZE_T count=0;
    const auto address=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))+0x525E90;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&flow,sizeof(flow),&count)&&count==sizeof(flow)&&flow<6;
}
struct NoticeContext {HMODULE module;std::wstring problem;bool japanese;std::filesystem::path ini;};
struct InternetHandle {
    HINTERNET value;
    explicit InternetHandle(HINTERNET v):value(v){}
    ~InternetHandle(){if(value)WinHttpCloseHandle(value);}
    InternetHandle(const InternetHandle&)=delete;
    InternetHandle& operator=(const InternetHandle&)=delete;
};
bool FetchReleases(std::string& output) {
    const auto deadline=GetTickCount64()+10000;
    InternetHandle session(WinHttpOpen(L"MW-EA-TRAX-Expansion-UpdateNotice/1",WINHTTP_ACCESS_TYPE_NO_PROXY,
                                     WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session.value||!WinHttpSetTimeouts(session.value,1500,2000,2000,2000))return false;
    InternetHandle connection(WinHttpConnect(session.value,L"api.github.com",INTERNET_DEFAULT_HTTPS_PORT,0));
    if(!connection.value)return false;
    InternetHandle request(WinHttpOpenRequest(connection.value,L"GET",apiPath,nullptr,WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    if(!request.value)return false;
    DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER,auth=WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
    DWORD disabled=WINHTTP_DISABLE_COOKIES;
    if(!WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects))||
       !WinHttpSetOption(request.value,WINHTTP_OPTION_AUTOLOGON_POLICY,&auth,sizeof(auth))||
       !WinHttpSetOption(request.value,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled)))return false;
    constexpr wchar_t headers[]=L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
    if(!WinHttpSendRequest(request.value,headers,static_cast<DWORD>(-1),WINHTTP_NO_REQUEST_DATA,0,0,0)||
       GetTickCount64()>=deadline||!WinHttpReceiveResponse(request.value,nullptr))return false;
    DWORD status=0,size=sizeof(status);
    if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,
                            &status,&size,WINHTTP_NO_HEADER_INDEX)||status!=200)return false;
    output.clear();char buffer[8192];
    while(GetTickCount64()<deadline) {
        DWORD read=0;
        if(!WinHttpReadData(request.value,buffer,sizeof(buffer),&read))return false;
        if(!read)return !output.empty();
        if(output.size()+read>2*1024*1024)return false;
        output.append(buffer,read);
    }
    return false;
}

DWORD WINAPI NoticeWorker(void* raw) noexcept {
    std::unique_ptr<NoticeContext> context(static_cast<NoticeContext*>(raw));
    try {
        const auto name=mod_update::QueueName(GetCurrentProcessId())+L".Checked.MW-EA-TRAX-Expansion";
        checkedEvent.value=CreateEventW(nullptr,TRUE,TRUE,name.c_str());
        if(!checkedEvent.value||GetLastError()==ERROR_ALREADY_EXISTS)return 0;
        const bool ja=context->japanese;
        if(!context->problem.empty()) {
            std::wstring text=ja?
                L"追加ファイルが不足しているか、対応版ではありません。\r\nGitHubから0.5.0のRuntime一式を導入してください。\r\n今回は標準曲を使用します。導入後は再起動してください。\r\n\r\n確認対象: ":
                L"Required files are missing or incompatible.\r\nInstall the complete 0.5.0 Runtime package from GitHub.\r\nThis launch uses stock music. Restart after installation.\r\n\r\nCheck: ";
            text+=context->problem.substr(0,100);
            text+=L"\r\ngithub.com/Zakkey250/MW-EA-TRAX-Expansion/releases";
            const bool shown=mod_update::ShowSerializedNotice(context->module,
                ja?L"EA TRAX Expansion — ランタイムが必要です":L"EA TRAX Expansion — Runtime required",
                text.c_str(),ja?L"標準曲で続行":L"Continue with stock music",StartupNoticeAllowed,
                90000,60000,releasesUrl,ja?L"GitHubを開く":L"Open GitHub");
            Log(LogLevel::Info,"RUNTIME_NOTICE shown=%u downloads=0",unsigned(shown));
            return 0; // Do not stack an update notice over a missing-runtime notice.
        }
        if(!GetPrivateProfileIntW(L"Updates",L"Enabled",1,context->ini.c_str()))return 0;
        if(!StartupNoticeAllowed())return 0;
        std::string payload;
        if(!FetchReleases(payload)){Log(LogLevel::Info,"UPDATE_NOTICE unavailable; gameplay unaffected");return 0;}
        const auto update=mod_update::FindUpdate(payload,kReleaseVersion,repository,assetPrefix);
        if(!update || !StartupNoticeAllowed())return 0;
        const std::wstring latest(update->tag.begin(),update->tag.end());
        std::wstring text=ja?L"新しいバージョンが公開されています。\r\n現在: 0.5.0\r\n公開版: ":
            L"A newer version is available.\r\nInstalled: 0.5.0\r\nAvailable: ";
        text+=latest;
        text+=ja?L"\r\n\r\n更新はゲーム終了後に行ってください。\r\n自動ダウンロード・インストールは行いません。":
            L"\r\n\r\nUpdate after closing the game.\r\nNo automatic download or installation.";
        text+=L"\r\ngithub.com/Zakkey250/MW-EA-TRAX-Expansion/releases";
        mod_update::ShowSerializedNotice(context->module,ja?L"EA TRAX Expansion — 更新通知":L"EA TRAX Expansion — Update available",
            text.c_str(),ja?L"閉じて続行":L"Close and continue",StartupNoticeAllowed,90000,60000,
            releasesUrl,ja?L"GitHubを開く":L"Open GitHub");
        Log(LogLevel::Info,"UPDATE_NOTICE available=%s downloads=0",update->tag.c_str());
    }catch(...){Log(LogLevel::Info,"NOTICE skipped after internal error");}
    return 0;
}
} // namespace
std::wstring RuntimeProblem(const std::filesystem::path& root) noexcept {
    try{return CheckRuntimeInventory(root);}catch(...){return L"RuntimeRequired.json (invalid)";}
}
void StartNotices(HMODULE module,const std::wstring& problem) noexcept {
    if(checked.exchange(true))return;
    try {
        wchar_t path[32768]{};if(!GetModuleFileNameW(module,path,32768))return;
        auto scripts=std::filesystem::path(path).parent_path();
        auto root=scripts.filename()==L"NFSMWEATraxExpansion"?scripts:scripts/L"NFSMWEATraxExpansion";
        const auto ini=root/L"NFSMWEATraxExpansion.ini";
        wchar_t language[256]{};
        GetPrivateProfileStringW(L"Updates",L"Language",L"auto",language,256,ini.c_str());
        const bool ja=_wcsicmp(language,L"auto")==0?startup::currentText==&startup::translations[1]:mod_update::Japanese(language);
        auto context=std::make_unique<NoticeContext>(NoticeContext{module,problem,ja,ini});
        HANDLE thread=CreateThread(nullptr,0,NoticeWorker,context.get(),0,nullptr);
        if(thread){context.release();CloseHandle(thread);}
    }catch(...){Log(LogLevel::Info,"NOTICE could not start");}
}
} // namespace eatrax
