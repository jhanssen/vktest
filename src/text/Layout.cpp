#include "Layout.h"
#include <unicode/brkiter.h>
#include <unicode/locid.h>

#define ISNEWLINE(c)                                    \
    (((c) >= 0xa && (c) <= 0xd) ||                      \
     (c) == 0x85 || (c) == 0x2028 || (c) == 0x2029)
#define ISSPACE(c)                              \
    (u_isspace((c)) ||                          \
     u_charType((c)) == U_CONTROL_CHAR ||       \
     u_charType((c)) == U_NON_SPACING_MARK)

static inline icu::UnicodeString cloneString(const icu::UnicodeString& str)
{
    icu::UnicodeString nstr;
    nstr.setTo(str.getBuffer(), str.length());
    return nstr;
}

static int32_t spacesAtEnd(const icu::UnicodeString& str)
{
    auto off = str.length();
    if (off == 0)
        return 0;
    while (off > 0 && ISSPACE(str[off - 1]))
        --off;
    return str.length() - off;
}

static const char* directionToString(UBiDiDirection dir)
{
    switch (dir) {
    case UBIDI_LTR:
        return "ltr";
    case UBIDI_RTL:
        return "rtl";
    case UBIDI_MIXED:
        return "mixed";
    case UBIDI_NEUTRAL:
        return "neutral";
    }
    return "<invalid direction>";
}

Layout::Layout(const Font& f)
    : mFont(f), mWidth(std::numeric_limits<uint32_t>::max()), mHeight(std::numeric_limits<uint32_t>::max()), mRunCount(0), mGlyphCount(0)
{
}

Layout::Layout(const Font& f, const std::u16string& t, uint32_t w, uint32_t h)
    : mFont(f), mWidth(w), mHeight(h), mRunCount(0), mGlyphCount(0)
{
    mText.setTo(&t[0], t.size());
    newLine();
    parse(mText);
}

Layout::~Layout()
{
    clearBuffers();
}

void Layout::clearBuffers()
{
    for (const auto& line : mLines) {
        for (const auto& run : line.runs) {
            if (run.buffer) {
                hb_buffer_destroy(run.buffer);
            }
        }
    }
}

void Layout::setText(const std::u16string& t)
{
    mPreLayout.clear();
    mText.setTo(&t[0], t.size());
    parse(mText);
    relayout();
}

void Layout::setWidth(uint32_t w)
{
    mWidth = w;
    relayout();
}

void Layout::setHeight(uint32_t h)
{
    mHeight = h;
    relayout();
}

void Layout::setGeometry(uint32_t w, uint32_t h)
{
    mWidth = w;
    mHeight = h;
    relayout();
}

void Layout::addText(const std::u16string& t)
{
    if (mPreLayout.empty()) {
        setText(t);
        return;
    }

    icu::UnicodeString newtext;
    newtext.setTo(&t[0], t.size());

    // append the new text to our main text
    mText.append(newtext);

    // merge this text with the last item
    auto& item = mPreLayout.back();
    const int32_t lastLineBreak = item.start;
    auto str = cloneString(mText.tempSubString(item.start, item.length));
    mPreLayout.pop_back();

    str.append(newtext);

    // and then we need to parse all this
    parse(str, lastLineBreak);
}

void Layout::newLine(bool force)
{
    if (!force && !mLines.empty()) {
        if (mLines.back().runs.empty())
            return;
        //finalizeLastLine();
    }
    mLines.push_back({});
}

void Layout::insertItem(const Item& item, float& currentWidth, bool& skipNextLinebreak)
{
    auto addItem = [this](Line* line, const Item& item, int32_t trim = 0) {
        if (line->runs.empty()) {
            line->runs.push_back({ item.start, item.length - trim, item.direction, trim > 0 ? item.trimmed : item.rect, nullptr });
            ++mRunCount;
        } else {
            auto& prev = line->runs.back();
            if (prev.direction == item.direction) {
                // merge
                assert(item.start == prev.start + prev.length);
                prev.length += item.length - trim;
                prev.rect.unite(trim > 0 ? item.trimmed : item.rect);
            } else {
                // add
                line->runs.push_back({ item.start, item.length - trim, item.direction, trim > 0 ? item.trimmed : item.rect, nullptr });
                ++mRunCount;
            }
        }
    };

    Line* line = &mLines.back();
    if (item.type == Item::Linebreak) {
        if (!skipNextLinebreak) {
            newLine(true);
            line = &mLines.back();
            currentWidth = 0.;
        }
        // either we're here because skipNextLinebreak == false, in which case we just added a new line
        // or we're here because our last added run needed to be trimmed and thus we explicitly added a new line there.
        assert(line->runs.empty());
        skipNextLinebreak = false;
    } else if (currentWidth + item.rect.width <= mWidth) {
        // we are go
        addItem(line, item);
        currentWidth += item.rect.width;
        skipNextLinebreak = false;
    } else if (item.trim > 0 && currentWidth + item.trimmed.width <= mWidth) {
        addItem(line, item, item.trim);
        newLine();
        line = &mLines.back();
        currentWidth = 0.;
        skipNextLinebreak = true;
    } else if (item.rect.width <= mWidth) {
        // we are also go
        newLine();
        line = &mLines.back();
        addItem(line, item);
        currentWidth = item.rect.width;
        skipNextLinebreak = false;
    } else if (item.trim > 0 && item.trimmed.width <= mWidth) {
        // and go
        newLine();
        line = &mLines.back();
        addItem(line, item, item.trim);
        line = &mLines.back();
        currentWidth = 0.;
        skipNextLinebreak = true;
    } else {
        // we are not go, we need to measure character by character
        int len = item.length - item.trim;
        while (len > 0) {
            const auto txt = mText.tempSubString(item.start, len - 1);
            const auto r = mFont.measure(txt);
            if (r.width <= mWidth) {
                // we fit now
                newLine();
                line = &mLines.back();
                addItem(line, { Item::Text, item.start, len - 1, 0, item.direction, r, r });
                line = &mLines.back();
                currentWidth = 0.;
                skipNextLinebreak = false;

                const auto rest = mText.tempSubString(item.start + (len - 1), item.length - (len - 1));
                const auto rr = mFont.measure(rest);
                insertItem({ Item::Text, item.start + (len - 1), item.length - (len - 1), 0, item.direction, rr, rr }, currentWidth, skipNextLinebreak);

                return;
            }
            --len;
        }
    }
}

void Layout::reshape()
{
    mGlyphCount = 0;

    const auto txt = mText.getBuffer();
    for (auto& line : mLines) {
        for (auto& run : line.runs) {
            if (run.buffer) {
                hb_buffer_destroy(run.buffer);
            }
            run.buffer = hb_buffer_create();
            hb_buffer_add_utf16(run.buffer, reinterpret_cast<const uint16_t*>(txt) + run.start, run.length, 0, -1);
            hb_buffer_guess_segment_properties(run.buffer);
            hb_shape(mFont.font(), run.buffer, nullptr, 0);

            mGlyphCount += hb_buffer_get_length(run.buffer);
        }
    }
}

void Layout::relayout()
{
    clearBuffers();
    mLines.clear();
    mRunCount = 0;
    newLine();

    float currentWidth = 0.;
    bool skipNextLinebreak = false;
    for (const auto& item : mPreLayout) {
        insertItem(item, currentWidth, skipNextLinebreak);
    }
    reshape();
}

void Layout::parse(const icu::UnicodeString& input, int32_t lastLineBreak)
{
    auto bidi = ubidi_open();
    UErrorCode errorCode = U_ZERO_ERROR;

    // std::string ball;
    // input.toUTF8String(ball);
    // printf("parsing '%s' (len %d) at %d\n", ball.c_str(), input.length(), lastLineBreak);

    // run the bidi algorithm
    UBiDiLevel paraLevel = UBIDI_DEFAULT_LTR;
    ubidi_setPara(bidi, input.getBuffer(), input.length(), paraLevel, nullptr, &errorCode);
    if (U_SUCCESS(errorCode)) {
        typedef std::pair<int32_t, int32_t> LBPair;

        // now find the line breaks
        std::vector<LBPair> lineBreaks;
        icu::Locale locale;
        auto bi = icu::BreakIterator::createLineInstance(locale, errorCode);
        if (U_FAILURE(errorCode)) {
            delete bi;
            return;
        }
        bi->setText(input);
        for (;;) {
            const int32_t b = bi->next();
            if (b == icu::BreakIterator::DONE)
                break;
            lineBreaks.push_back(std::make_pair(b, -1));
        }
        delete bi;

        // two passes through the visual runs, one to add the logical start
        // of each run as a potential line break and one to process the visual runs
        // walk the runs and break where needed
        const int32_t count = ubidi_countRuns(bidi, &errorCode);
        int32_t logicalStart, length;

        for (int32_t i = 0; i < count; ++i) {
            const auto dir = ubidi_getVisualRun(bidi, i, &logicalStart, &length);
            lineBreaks.push_back(std::make_pair(logicalStart, dir));
        }

        // sort the linebreaks
        std::sort(lineBreaks.begin(), lineBreaks.end(),
                  [](const LBPair& a, const LBPair& b)
                  {
                      if (a.first == b.first) {
                          if (a.second == b.second)
                              return false;
                          if (a.second != -1)
                              return true;
                      }
                      return a.first < b.first;
                  });

        // postprocess, assign the correct directionality
        int32_t currentDir = UBIDI_LTR;
        auto it = lineBreaks.begin();
        while (it != lineBreaks.end()) {
            if (it->second != -1) {
                currentDir = it->second;
            } else {
                it->second = currentDir;
            }
            ++it;
        }

        // remove duplicates
        lineBreaks.erase(std::unique(lineBreaks.begin(), lineBreaks.end(),
                                     [](const LBPair& a, const LBPair& b) {
                                         return a.first == b.first;
                                     }), lineBreaks.end());

        if (lineBreaks.size() < 2)
            return;

        it = lineBreaks.begin();
        auto cur = it->first;
        auto dir = it->second;
        assert(cur == 0);
        ++it;
        while (it != lineBreaks.end()) {
            if (it->first == cur) {
                // nothing
                dir = it->second;
                ++it;
                continue;
            }
            //assert(cur + lastLineBreak + (it->first - cur) <= text.length() + 1);
            const auto txt = mText.tempSubString(cur + lastLineBreak, it->first - cur);
            const auto len = txt.length();
            // check if the last character is a newline
            const auto last = txt[len - 1];
            if (ISNEWLINE(last)) {
                if (len > 1) {
                    // this item is more than just a newline
                    const Rect r = mFont.measure(mText.tempSubString(cur + lastLineBreak, len - 1));
                    const auto trim = spacesAtEnd(txt) - 1; // - 1 accounts for the newline
                    const Rect tr = trim > 0 ? mFont.measure(mText.tempSubString(cur + lastLineBreak, len - 1 - trim)) : r;
                    mPreLayout.push_back({ Item::Text, cur + lastLineBreak, len - 1, trim, static_cast<UBiDiDirection>(dir), r, tr });
                }
                mPreLayout.push_back({ Item::Linebreak, cur + lastLineBreak + (len - 1), 1, 0, static_cast<UBiDiDirection>(dir), { 0 }, { 0 } });
            } else {
                const Rect r = mFont.measure(txt);
                const auto trim = spacesAtEnd(txt);
                const Rect tr = trim > 0 ? mFont.measure(mText.tempSubString(cur + lastLineBreak, len - trim)) : r;
                mPreLayout.push_back({ Item::Text, cur + lastLineBreak, len, trim, static_cast<UBiDiDirection>(dir), r, tr });
            }
            cur = it->first;
            dir = it->second;

            ++it;
        }
    }

    ubidi_close(bidi);
}

void Layout::dumppre()
{
    printf("%zu items\n", mPreLayout.size());
    for (const auto& item : mPreLayout) {
        if (item.type == Item::Text) {
            std::string str;
            mText.tempSubString(item.start, item.length).toUTF8String(str);
            const Rect r = item.rect.integralized();
            printf("item: start %d length %d trim %d direction %s text '%s' rect %.0f,%.0f+%.0fx%.0f",
                   item.start, item.length, item.trim, directionToString(item.direction), str.c_str(),
                   r.x, r.y, r.width, r.height);
            if (item.trim > 0) {
                const Rect r = item.trimmed.integralized();
                printf(" trimmed %.0f,%.0f+%.0fx%.0f\n", r.x, r.y, r.width, r.height);
            } else {
                printf("\n");
            }
        } else {
            printf("item: newline\n");
            assert(item.length == 1);
            assert(ISNEWLINE(mText.tempSubString(item.start, item.length)[0]));
        }
    }
}

void Layout::dump()
{
    printf("%zu lines\n", mLines.size());
    for (const auto& line : mLines) {
        for (const auto& run : line.runs) {
            std::string str;
            mText.tempSubString(run.start, run.length).toUTF8String(str);
            printf("run: '%s' ", str.c_str());
        }
        printf("\n");
    }
}
