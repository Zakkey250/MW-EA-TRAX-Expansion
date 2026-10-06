#pragma once
#include <Windows.h>
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace eatrax::ini {
constexpr unsigned kDefaultsResource = 110;
struct Completion { bool ok = true; size_t added = 0; DWORD error = 0; };
inline std::wstring Trim(std::wstring s) {
    const auto space=[](wchar_t c){return c==L' ' || c==L'\t';};
    while(!s.empty() && space(s.back()))s.pop_back();
    size_t i=0;while(i<s.size() && space(s[i]))++i;
    return s.substr(i);
}
inline std::wstring Fold(std::wstring s) {
    s=Trim(s);for(auto& c:s)c=static_cast<wchar_t>(std::towupper(c));return s;
}
struct Line { std::wstring text, ending; };
inline std::vector<Line> Lines(const std::wstring& s) {
    std::vector<Line> lines;
    for(size_t pos=0;pos<s.size();) {
        const auto end=s.find_first_of(L"\r\n",pos);
        if(end==std::wstring::npos){lines.push_back({s.substr(pos),L""});break;}
        const size_t n=s[end]==L'\r' && end+1<s.size() && s[end+1]==L'\n'?2:1;
        lines.push_back({s.substr(pos,end-pos),s.substr(end,n)});pos=end+n;
    }
    return lines;
}
inline bool Section(const std::wstring& text,std::wstring& name) {
    const auto s=Trim(text);const auto close=s.find(L']');
    if(s.empty() || s[0]!=L'[' || close==std::wstring::npos)return false;
    name=Trim(s.substr(1,close-1));return !name.empty();
}
inline std::wstring Key(const std::wstring& text) {
    const auto s=Trim(text);
    if(s.empty() || s[0]==L';' || s[0]==L'#' || s[0]==L'[')return {};
    return Fold(s.substr(0,s.find(L'=')));
}
// Retain each original line verbatim, including blank/invalid values and duplicate
// sections. Only missing keys are inserted into the last matching section.
inline std::wstring CompleteText(const std::wstring& original,const std::wstring& defaults,size_t& added) {
    added=0;const auto lines=Lines(original);
    std::wstring eol=L"\r\n";
    for(const auto& line:lines)if(!line.ending.empty()){eol=line.ending;break;}
    std::map<std::wstring,std::set<std::wstring>> keys;
    std::map<std::wstring,size_t> ends;
    std::wstring section,name;
    for(size_t i=0;i<lines.size();++i) {
        if(Section(lines[i].text,name)) {
            if(!section.empty())ends[section]=i;
            section=Fold(name);keys[section];
        }else if(!section.empty()) {
            const auto key=Key(lines[i].text);if(!key.empty())keys[section].insert(key);
        }
    }
    if(!section.empty())ends[section]=lines.size();
    std::map<size_t,std::wstring> insertions;
    std::map<std::wstring,std::wstring> newSections;
    std::vector<std::wstring> newOrder;
    std::wstring prefix;
    section.clear();
    for(const auto& line:Lines(defaults)) {
        if(Section(line.text,name)){section=Fold(name);prefix.clear();continue;}
        if(section.empty())continue;
        const auto key=Key(line.text);
        if(key.empty()){prefix+=line.text+eol;continue;}
        if(!keys[section].count(key)) {
            if(!ends.count(section)) {
                if(!newSections.count(section)) {
                    newOrder.push_back(section);
                    newSections[section]=eol+L"["+name+L"]"+eol;
                }
                newSections[section]+=prefix+line.text+eol;
            }else insertions[ends[section]]+=prefix+line.text+eol;
            keys[section].insert(key);++added;
        }
        prefix.clear();
    }
    if(!added)return original;
    std::wstring result;
    for(size_t i=0;i<=lines.size();++i) {
        const auto found=insertions.find(i);
        if(found!=insertions.end()) {
            if(!result.empty() && result.back()!=L'\r' && result.back()!=L'\n')result+=eol;
            result+=found->second;
        }
        if(i<lines.size())result+=lines[i].text+lines[i].ending;
    }
    for(const auto& key:newOrder) {
        if(!result.empty() && result.back()!=L'\r' && result.back()!=L'\n')result+=eol;
        result+=newSections[key];
    }
    return result;
}
enum class Encoding { Utf8, Utf8Bom, Utf16LE, Utf16BE, Ansi };
inline bool Decode(const std::string& bytes,std::wstring& text,Encoding& encoding) {
    text.clear();size_t offset=0;
    if(bytes.size()>=2 && (static_cast<unsigned char>(bytes[0])==0xFF && static_cast<unsigned char>(bytes[1])==0xFE || static_cast<unsigned char>(bytes[0])==0xFE && static_cast<unsigned char>(bytes[1])==0xFF)) {
        encoding=static_cast<unsigned char>(bytes[0])==0xFF?Encoding::Utf16LE:Encoding::Utf16BE;
        if(bytes.size()%2)return false;
        for(size_t i=2;i<bytes.size();i+=2) {
            const auto a=static_cast<unsigned char>(bytes[i]),b=static_cast<unsigned char>(bytes[i+1]);
            text.push_back(static_cast<wchar_t>(encoding==Encoding::Utf16LE?a|(b<<8):(a<<8)|b));
        }
        return text.find(L'\0')==std::wstring::npos;
    }
    encoding=Encoding::Utf8;
    if(bytes.compare(0,3,"\xEF\xBB\xBF")==0){encoding=Encoding::Utf8Bom;offset=3;}
    if(bytes.size()==offset)return true;
    UINT page=CP_UTF8;
    int n=MultiByteToWideChar(page,MB_ERR_INVALID_CHARS,bytes.data()+offset,static_cast<int>(bytes.size()-offset),nullptr,0);
    if(!n && !offset){page=CP_ACP;encoding=Encoding::Ansi;n=MultiByteToWideChar(page,MB_ERR_INVALID_CHARS,bytes.data(),static_cast<int>(bytes.size()),nullptr,0);}
    if(!n)return false;
    text.resize(n);
    if(!MultiByteToWideChar(page,MB_ERR_INVALID_CHARS,bytes.data()+offset,static_cast<int>(bytes.size()-offset),text.data(),n))return false;
    return text.find(L'\0')==std::wstring::npos;
}
inline bool Encode(const std::wstring& text,Encoding encoding,std::string& bytes) {
    bytes.clear();
    if(encoding==Encoding::Utf16LE || encoding==Encoding::Utf16BE) {
        bytes=encoding==Encoding::Utf16LE?"\xFF\xFE":"\xFE\xFF";
        for(auto c:text){const char lo=static_cast<char>(c&255),hi=static_cast<char>((c>>8)&255);bytes+=encoding==Encoding::Utf16LE?lo:hi;bytes+=encoding==Encoding::Utf16LE?hi:lo;}
        return true;
    }
    const UINT page=encoding==Encoding::Ansi?GetACP():CP_UTF8;
    BOOL replaced=FALSE;auto* used=page==CP_UTF8?nullptr:&replaced;
    const DWORD flags=page==CP_UTF8?WC_ERR_INVALID_CHARS:WC_NO_BEST_FIT_CHARS;
    const int n=WideCharToMultiByte(page,flags,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,used);
    if(!n && !text.empty())return false;
    bytes.resize(n);
    if(n && !WideCharToMultiByte(page,flags,text.data(),static_cast<int>(text.size()),bytes.data(),n,nullptr,used))return false;
    if(replaced)return false;
    if(encoding==Encoding::Utf8Bom)bytes.insert(0,"\xEF\xBB\xBF");
    return true;
}
struct Handle {
    HANDLE value=INVALID_HANDLE_VALUE;
    ~Handle(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
};
inline Completion EnsureDefaults(const std::filesystem::path& path,HMODULE module) {
    Completion result;
    const auto fail=[&](DWORD error){result.ok=false;result.error=error;return result;};
    const auto resource=FindResourceW(module,MAKEINTRESOURCEW(kDefaultsResource),MAKEINTRESOURCEW(10));
    if(!resource)return fail(ERROR_RESOURCE_DATA_NOT_FOUND);
    const auto memory=LoadResource(module,resource);
    const auto data=static_cast<const char*>(LockResource(memory));
    if(!data)return fail(ERROR_RESOURCE_DATA_NOT_FOUND);
    std::wstring defaults;Encoding templateEncoding;
    if(!Decode(std::string(data,SizeofResource(module,resource)),defaults,templateEncoding))return fail(ERROR_NO_UNICODE_TRANSLATION);
    Handle input;
    input.value=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    const bool exists=input.value!=INVALID_HANDLE_VALUE;
    if(!exists && GetLastError()!=ERROR_FILE_NOT_FOUND)return fail(GetLastError());
    std::string original;
    BY_HANDLE_FILE_INFORMATION info{};
    if(exists) {
        if(!GetFileInformationByHandle(input.value,&info))return fail(GetLastError());
        if(info.dwFileAttributes&FILE_ATTRIBUTE_READONLY)return fail(ERROR_ACCESS_DENIED);
        if(info.nFileSizeHigh || info.nFileSizeLow>4*1024*1024)return fail(ERROR_FILE_TOO_LARGE);
        original.resize(info.nFileSizeLow);DWORD read=0;
        if(!ReadFile(input.value,original.data(),static_cast<DWORD>(original.size()),&read,nullptr) || read!=original.size())return fail(ERROR_READ_FAULT);
    }
    std::wstring text;Encoding encoding;
    if(!Decode(original,text,encoding))return fail(ERROR_NO_UNICODE_TRANSLATION);
    std::string roundtrip;
    if(!Encode(text,encoding,roundtrip) || roundtrip!=original)return fail(ERROR_NO_UNICODE_TRANSLATION);
    const auto completed=CompleteText(text,defaults,result.added);
    if(!result.added)return result;
    std::string output;
    // Legacy code pages may not represent translated comments. Preserve their
    // bytes/encoding and retain the template's English explanations instead.
    if(encoding==Encoding::Ansi && !Encode(completed,encoding,output)) {
        std::wstring asciiTemplate;
        for(const auto& line:Lines(defaults))if(std::all_of(line.text.begin(),line.text.end(),[](wchar_t c){return c<128;}))asciiTemplate+=line.text+line.ending;
        const auto fallback=CompleteText(text,asciiTemplate,result.added);
        if(!Encode(fallback,encoding,output))return fail(ERROR_NO_UNICODE_TRANSLATION);
    }else if(!Encode(completed,encoding,output))return fail(ERROR_NO_UNICODE_TRANSLATION);
    const auto temporary=std::filesystem::path(path.wstring()+L".complete-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".tmp");
    Handle temp;
    temp.value=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(temp.value==INVALID_HANDLE_VALUE)return fail(GetLastError());
    DWORD written=0;
    const bool saved=WriteFile(temp.value,output.data(),static_cast<DWORD>(output.size()),&written,nullptr) && written==output.size() && FlushFileBuffers(temp.value);
    const auto saveError=GetLastError();CloseHandle(temp.value);temp.value=INVALID_HANDLE_VALUE;
    if(!saved){DeleteFileW(temporary.c_str());return fail(saveError?saveError:ERROR_WRITE_FAULT);}
    bool unchanged=true;
    if(exists) {
        Handle current;current.value=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        BY_HANDLE_FILE_INFORMATION now{};
        unchanged=current.value!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(current.value,&now) && now.dwVolumeSerialNumber==info.dwVolumeSerialNumber && now.nFileIndexHigh==info.nFileIndexHigh && now.nFileIndexLow==info.nFileIndexLow;
    }
    const bool committed=unchanged && (exists?ReplaceFileW(path.c_str(),temporary.c_str(),nullptr,0,nullptr,nullptr):MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH));
    const auto commitError=GetLastError();
    if(!committed){DeleteFileW(temporary.c_str());return fail(unchanged?commitError:ERROR_SHARING_VIOLATION);}
    return result;
}
} // namespace eatrax::ini
