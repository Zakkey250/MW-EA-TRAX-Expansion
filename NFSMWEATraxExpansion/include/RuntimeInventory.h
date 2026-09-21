#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#include "../third_party/nlohmann/json.hpp"
namespace eatrax {
inline std::wstring CheckRuntimeInventory(const std::filesystem::path& root) {
    std::ifstream input(root/L"RuntimeRequired.json",std::ios::binary);
    if(!input)return L"RuntimeRequired.json";
    const auto data=nlohmann::json::parse(input);
    if(data.at("schema")!=1 || data.at("version")!="0.4.8")return L"RuntimeRequired.json (version)";
    const auto& files=data.at("files");
    if(!files.is_array() || files.empty() || files.size()>2000)return L"RuntimeRequired.json (files)";
    for(const auto& item:files) {
        const auto name=item.at("path").get<std::string>();
        const auto path=std::filesystem::u8path(name);
        if(name.empty() || path.is_absolute() || name.find(':')!=std::string::npos || name.find("..")!=std::string::npos ||
           name.rfind("Runtime/",0)!=0)return L"RuntimeRequired.json (path)";
        std::error_code ec;
        const auto actual=std::filesystem::file_size(root/path,ec);
        if(ec || actual!=item.at("bytes").get<std::uintmax_t>())return path.wstring();
    }
    return {};
}
}
