#ifndef LAYOUT_H
#define LAYOUT_H

#include "Font.h"
#include <hb.h>
#include <unicode/unistr.h>
#include <unicode/ubidi.h>
#include <vector>
#include <cstdint>

class Layout
{
public:
    Layout(const Font& font);
    Layout(const Font& font, const std::u16string& text, uint32_t width, uint32_t height);
    ~Layout();

    void setText(const std::u16string& text);
    void setWidth(uint32_t width);
    void setHeight(uint32_t height);
    void setGeometry(uint32_t width, uint32_t height);

    void addText(const std::u16string& text);

    size_t runCount() const { return mRunCount; }
    size_t glyphCount() const { return mGlyphCount; }

    void dump();
    void dumppre();

    struct Run
    {
        int32_t start, length;
        UBiDiDirection direction;
        Rect rect;

        hb_buffer_t* buffer;

        bool empty() const { return start == length; };
    };
    struct Line
    {
        std::vector<Run> runs;

        bool empty() const { return runs.empty(); };
    };
    const std::vector<Line>& lines() const { return mLines; }

private:
    void parse(const icu::UnicodeString& input, int32_t lastLineBreak = 0);
    void relayout();
    void reshape();

private:
    struct Item
    {
        enum Type { Linebreak, Text } type;
        int32_t start, length, trim;
        UBiDiDirection direction;
        Rect rect, trimmed;
    };
    std::vector<Item> mPreLayout;

    Font mFont;
    icu::UnicodeString mText;
    uint32_t mWidth, mHeight;
    size_t mRunCount, mGlyphCount;
    std::vector<Line> mLines;

private:
    void insertItem(const Item& item, float& currentWidth, bool& skipNextLinebreak);
    void trimStart(Line* line, double& curwidth);
    void newLine(bool force = false);
    void clearBuffers();
};

#endif // LAYOUT_H
