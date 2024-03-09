// Copyright ?019 Domagoj "oberth" Pandža
// MIT license | Read LICENSE.txt for details.

#pragma once

#include <string>
#include <Windows.h>
#include <locale>
#include <codecvt>
#include <string>

namespace utility {

    inline auto convert_utf8_to_utf16(std::string const& string) {

        auto new_size = MultiByteToWideChar(CP_UTF8, 0, string.data(), static_cast<int>(string.size()), nullptr, 0);
        
        std::wstring utf16_string; utf16_string.resize(new_size);
        MultiByteToWideChar(CP_UTF8, 0, string.data(), static_cast<int>(string.size()), utf16_string.data(), new_size);

        return utf16_string;

    }

    inline  std::wstring  utf8toUtf16(const std::string& str)
    {
        if (str.empty())
            return std::wstring();

        size_t charsNeeded = ::MultiByteToWideChar(CP_UTF8, 0,
            str.data(), (int)str.size(), NULL, 0);
        if (charsNeeded == 0)
            throw std:: runtime_error("Failed converting UTF-8 string to UTF-16");

        std::vector<wchar_t> buffer(charsNeeded);
        int charsConverted = ::MultiByteToWideChar(CP_UTF8, 0,
            str.data(), (int)str.size(), &buffer[0], buffer.size());
        if (charsConverted == 0)
            throw std::runtime_error("Failed converting UTF-8 string to UTF-16");

        return std::wstring(&buffer[0], charsConverted);
    }

    inline auto utf8test(const std::string& narrow_utf8_source_string) {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        //std::string narrow = converter.to_bytes(wide_utf16_source_string);
        std::wstring wide = converter.from_bytes(narrow_utf8_source_string);
        return wide;
    }

}