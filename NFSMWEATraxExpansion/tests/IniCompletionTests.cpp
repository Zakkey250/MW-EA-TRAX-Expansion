#include "IniCompletion.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

void Check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
std::string Read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void Write(const std::filesystem::path& p,const std::string& data){std::ofstream f(p,std::ios::binary);f.write(data.data(),data.size());}
int wmain(int argc,wchar_t** argv) {
    try {
    using namespace eatrax::ini;
    if(argc!=2)return 2;
    const std::filesystem::path root(argv[1]);std::filesystem::create_directories(root);
    size_t added=0;
    const auto text=CompleteText(L"; keep\n[mAiN]\nLogging=\nMyOption=hello\n[MAIN]\nStreamerMode=true",L"[Main]\n; log comment\nLogging=false\n; swap comment\nSwapArtistAlbum=false\nStreamerMode=false\n[Pursuit]\n; heat comment\nHeat1To3EATraxKeeping=false\n",added);
    Check(added==2,"missing key count / case / blank-value preservation");
    Check(text.find(L"Logging=\nMyOption=hello\n[MAIN]\nStreamerMode=true")!=std::wstring::npos,"existing values or duplicate sections changed");
    Check(text.find(L"; swap comment\nSwapArtistAlbum=false\n")!=std::wstring::npos,"description or LF style lost");
    Check(text.find(L"[Pursuit]")!=std::wstring::npos,"missing section not created");
    Check(CompleteText(text,L"[Main]\nLogging=false\nSwapArtistAlbum=false\nStreamerMode=false\n[Pursuit]\nHeat1To3EATraxKeeping=false\n",added)==text && added==0,"text completion not idempotent");
    const auto module=GetModuleHandleW(nullptr);
    for(auto encoding:{Encoding::Utf8,Encoding::Utf8Bom,Encoding::Utf16LE,Encoding::Utf16BE,Encoding::Ansi}) {
        const auto p=root/(L"encoding-"+std::to_wstring(static_cast<int>(encoding))+L".ini");
        std::wstring original=L"; 日本語のコメントを保持\r\n[Main]\r\nLogging = true\r\nStreamerMode = true\r\nJapaneseMetadata = 1\r\nUnknown = そのまま\r\n[Pursuit]\r\nEnabled = false\r\n";
        if(encoding==Encoding::Ansi) {
            std::wstring label=GetACP()==932?L"日本語":GetACP()==1252?L"café":L"Legacy comment";
            original=L"; "+label+L"\r\n[Main]\r\nLogging = true\r\nStreamerMode = true\r\nJapaneseMetadata = 1\r\nUnknown = "+label+L"\r\n[Pursuit]\r\nEnabled = false\r\n";
        }
        std::string bytes;Check(Encode(original,encoding,bytes),"fixture encoding failed");Write(p,bytes);
        const auto result=EnsureDefaults(p,module);Check(result.ok && result.added>0,"actual INI completion failed");
        std::wstring completed;Encoding actual;Check(Decode(Read(p),completed,actual),"completed file cannot decode");
        // ASCII-only ANSI fixtures are also valid UTF-8; byte preservation is the contract.
        Check(actual==encoding || encoding==Encoding::Ansi,"encoding/BOM changed");
        for(const auto& line:Lines(original))Check(completed.find(line.text+line.ending)!=std::wstring::npos,"original line changed");
        Check(completed.find(L"SwapArtistAlbum = false")!=std::wstring::npos,"new Main setting missing");
        Check(completed.find(L"Heat1To3EATraxKeeping = false")!=std::wstring::npos,"new Pursuit setting missing");
        const auto first=Read(p);const auto time=std::filesystem::last_write_time(p);
        const auto again=EnsureDefaults(p,module);Check(again.ok && again.added==0 && Read(p)==first && std::filesystem::last_write_time(p)==time,"repeat startup rewrites file");
        if(encoding==Encoding::Utf8 || encoding==Encoding::Utf16LE) {
            wchar_t value[32]{};GetPrivateProfileStringW(L"Main",L"SwapArtistAlbum",L"missing",value,32,p.c_str());
            Check(std::wstring(value)==L"false","native INI API cannot read added key");
            GetPrivateProfileStringW(L"Main",L"Logging",L"missing",value,32,p.c_str());Check(std::wstring(value)==L"true","existing logging value overwritten");
            GetPrivateProfileStringW(L"Pursuit",L"Heat1To3EATraxKeeping",L"missing",value,32,p.c_str());Check(std::wstring(value)==L"false","new intermediate section captured tail-section keys");
        }
    }
    const auto missing=root/L"missing.ini";const auto fresh=EnsureDefaults(missing,module);Check(fresh.ok && fresh.added==27,"missing-file defaults incomplete");
    const auto empty=root/L"empty.ini";Write(empty,"");Check(EnsureDefaults(empty,module).ok && Read(empty)==Read(missing),"empty-file defaults differ");
    const auto bad=root/L"truncated.ini";Write(bad,"\xFF\xFE\x41");Check(!EnsureDefaults(bad,module).ok && Read(bad)=="\xFF\xFE\x41","malformed Unicode overwritten");
    const auto badUtf8=root/L"invalid-utf8-bom.ini";Write(badUtf8,"\xEF\xBB\xBF\xFF");Check(!EnsureDefaults(badUtf8,module).ok && Read(badUtf8)=="\xEF\xBB\xBF\xFF","malformed UTF-8 BOM overwritten");
    const auto readonly=root/L"readonly.ini";Write(readonly,"[Main]\r\nEnabled=0\r\n");const auto before=Read(readonly);
    SetFileAttributesW(readonly.c_str(),FILE_ATTRIBUTE_READONLY);Check(!EnsureDefaults(readonly,module).ok && Read(readonly)==before,"read-only original overwritten");SetFileAttributesW(readonly.c_str(),FILE_ATTRIBUTE_NORMAL);
    const auto locked=root/L"locked.ini";Write(locked,"[Main]\r\nEnabled=0\r\n");
    Handle held;held.value=CreateFileW(locked.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    Check(held.value!=INVALID_HANDLE_VALUE && !EnsureDefaults(locked,module).ok && Read(locked)==before,"failed atomic replacement altered original");
    for(const auto& item:std::filesystem::directory_iterator(root))Check(item.path().extension()!=L".tmp","temporary file leaked");
    std::cout<<"PASS INI completion: missing keys/sections, case and duplicate sections, blank/custom values, comments, newline/BOM/encoding, native reads, unchanged repeat startup, missing/empty file, malformed Unicode, read-only/locked rollback\n";
    return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
