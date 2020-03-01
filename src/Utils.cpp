#include "Utils.h"
#include <vector>
#include <cstdint>

std::u16string utf8_to_utf16(const std::string& utf8)
{
    std::vector<uint32_t> unicode;
    unicode.reserve(utf8.size() / 4);
    size_t i = 0;
    while (i < utf8.size())
    {
        uint32_t uni;
        size_t todo;
        uint8_t ch = utf8[i++];
        if (ch <= 0x7F)
        {
            uni = ch;
            todo = 0;
        }
        else if (ch <= 0xBF)
        {
            return std::u16string();
        }
        else if (ch <= 0xDF)
        {
            uni = ch&0x1F;
            todo = 1;
        }
        else if (ch <= 0xEF)
        {
            uni = ch&0x0F;
            todo = 2;
        }
        else if (ch <= 0xF7)
        {
            uni = ch&0x07;
            todo = 3;
        }
        else
        {
            return std::u16string();
        }
        for (size_t j = 0; j < todo; ++j)
        {
            if (i == utf8.size())
                return std::u16string();
            uint8_t ch = utf8[i++];
            if (ch < 0x80 || ch > 0xBF)
                return std::u16string();
            uni <<= 6;
            uni += ch & 0x3F;
        }
        if (uni >= 0xD800 && uni <= 0xDFFF)
            return std::u16string();
        if (uni > 0x10FFFF)
            return std::u16string();
        unicode.push_back(uni);
    }
    std::u16string utf16;
    utf16.reserve(unicode.size());
    for (size_t i = 0; i < unicode.size(); ++i)
    {
        uint32_t uni = unicode[i];
        if (uni <= 0xFFFF)
        {
            utf16 += (char16_t)uni;
        }
        else
        {
            uni -= 0x10000;
            utf16 += (char16_t)((uni >> 10) + 0xD800);
            utf16 += (char16_t)((uni & 0x3FF) + 0xDC00);
        }
    }
    return utf16;
}
