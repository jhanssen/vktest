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
    : font(f), width(std::numeric_limits<uint32_t>::max()), height(std::numeric_limits<uint32_t>::max())
{
}

Layout::Layout(const Font& f, const std::u16string& t, uint32_t w, uint32_t h)
    : font(f), width(w), height(h)
{
    text.setTo(&t[0], t.size());
    newLine();
    parse(text);
}

Layout::~Layout()
{
    clearBuffers();
}

void Layout::clearBuffers()
{
    for (const auto& line : lines) {
        for (const auto& run : line.runs) {
            if (run.buffer) {
                hb_buffer_destroy(run.buffer);
            }
        }
    }
}

void Layout::setText(const std::u16string& t)
{
    prelayout.clear();
    text.setTo(&t[0], t.size());
    parse(text);
    relayout();
}

void Layout::setWidth(uint32_t w)
{
    width = w;
    relayout();
}

void Layout::setHeight(uint32_t h)
{
    height = h;
    relayout();
}

void Layout::setGeometry(uint32_t w, uint32_t h)
{
    width = w;
    height = h;
    relayout();
}

void Layout::addText(const std::u16string& t)
{
    if (prelayout.empty()) {
        setText(t);
        return;
    }

    icu::UnicodeString newtext;
    newtext.setTo(&t[0], t.size());

    // append the new text to our main text
    text.append(newtext);

    // merge this text with the last item
    auto& item = prelayout.back();
    const int32_t lastLineBreak = item.start;
    auto str = cloneString(text.tempSubString(item.start, item.length));
    prelayout.pop_back();

    str.append(newtext);

    // and then we need to parse all this
    parse(str, lastLineBreak);
}

void Layout::newLine(bool force)
{
    if (!force && !lines.empty()) {
        if (lines.back().runs.empty())
            return;
        //finalizeLastLine();
    }
    lines.push_back({});
}

void Layout::insertItem(const Item& item, float& currentWidth, bool& skipNextLinebreak)
{
    auto addItem = [](Line* line, const Item& item, int32_t trim = 0) {
        if (line->runs.empty()) {
            line->runs.push_back({ item.start, item.length - trim, item.direction, trim > 0 ? item.trimmed : item.rect, nullptr });
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
            }
        }
    };

    Line* line = &lines.back();
    if (item.type == Item::Linebreak) {
        if (!skipNextLinebreak) {
            newLine(true);
            line = &lines.back();
            currentWidth = 0.;
        }
        // either we're here because skipNextLinebreak == false, in which case we just added a new line
        // or we're here because our last added run needed to be trimmed and thus we explicitly added a new line there.
        assert(line->runs.empty());
        skipNextLinebreak = false;
    } else if (currentWidth + item.rect.width <= width) {
        // we are go
        addItem(line, item);
        currentWidth += item.rect.width;
        skipNextLinebreak = false;
    } else if (item.trim > 0 && currentWidth + item.trimmed.width <= width) {
        addItem(line, item, item.trim);
        newLine();
        line = &lines.back();
        currentWidth = 0.;
        skipNextLinebreak = true;
    } else if (item.rect.width <= width) {
        // we are also go
        newLine();
        line = &lines.back();
        addItem(line, item);
        currentWidth = item.rect.width;
        skipNextLinebreak = false;
    } else if (item.trim > 0 && item.trimmed.width <= width) {
        // and go
        newLine();
        line = &lines.back();
        addItem(line, item, item.trim);
        line = &lines.back();
        currentWidth = 0.;
        skipNextLinebreak = true;
    } else {
        // we are not go, we need to measure character by character
        int len = item.length - item.trim;
        while (len > 0) {
            const auto txt = text.tempSubString(item.start, len - 1);
            const auto r = font.measure(txt);
            if (r.width <= width) {
                // we fit now
                newLine();
                line = &lines.back();
                addItem(line, { Item::Text, item.start, len - 1, 0, item.direction, r, r });
                line = &lines.back();
                currentWidth = 0.;
                skipNextLinebreak = false;

                const auto rest = text.tempSubString(item.start + (len - 1), item.length - (len - 1));
                const auto rr = font.measure(rest);
                insertItem({ Item::Text, item.start + (len - 1), item.length - (len - 1), 0, item.direction, rr, rr }, currentWidth, skipNextLinebreak);

                return;
            }
            --len;
        }
    }
}

void Layout::reshape()
{
    const auto txt = text.getBuffer();
    for (auto& line : lines) {
        for (auto& run : line.runs) {
            if (run.buffer) {
                hb_buffer_destroy(run.buffer);
            }
            run.buffer = hb_buffer_create();
            hb_buffer_add_utf16(run.buffer, reinterpret_cast<const uint16_t*>(txt) + run.start, run.length, 0, -1);
            hb_buffer_guess_segment_properties(run.buffer);
            hb_shape(font.font(), run.buffer, nullptr, 0);
        }
    }
}

void Layout::relayout()
{
    clearBuffers();
    lines.clear();
    newLine();

    float currentWidth = 0.;
    bool skipNextLinebreak = false;
    for (const auto& item : prelayout) {
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
            const auto txt = text.tempSubString(cur + lastLineBreak, it->first - cur);
            const auto len = txt.length();
            // check if the last character is a newline
            const auto last = txt[len - 1];
            if (ISNEWLINE(last)) {
                if (len > 1) {
                    // this item is more than just a newline
                    const Rect r = font.measure(text.tempSubString(cur + lastLineBreak, len - 1));
                    const auto trim = spacesAtEnd(txt) - 1; // - 1 accounts for the newline
                    const Rect tr = trim > 0 ? font.measure(text.tempSubString(cur + lastLineBreak, len - 1 - trim)) : r;
                    prelayout.push_back({ Item::Text, cur + lastLineBreak, len - 1, trim, static_cast<UBiDiDirection>(dir), r, tr });
                }
                prelayout.push_back({ Item::Linebreak, cur + lastLineBreak + (len - 1), 1, 0, static_cast<UBiDiDirection>(dir), { 0 }, { 0 } });
            } else {
                const Rect r = font.measure(txt);
                const auto trim = spacesAtEnd(txt);
                const Rect tr = trim > 0 ? font.measure(text.tempSubString(cur + lastLineBreak, len - trim)) : r;
                prelayout.push_back({ Item::Text, cur + lastLineBreak, len, trim, static_cast<UBiDiDirection>(dir), r, tr });
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
    printf("%zu items\n", prelayout.size());
    for (const auto& item : prelayout) {
        if (item.type == Item::Text) {
            std::string str;
            text.tempSubString(item.start, item.length).toUTF8String(str);
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
            assert(ISNEWLINE(text.tempSubString(item.start, item.length)[0]));
        }
    }
}

void Layout::dump()
{
    printf("%zu lines\n", lines.size());
    for (const auto& line : lines) {
        for (const auto& run : line.runs) {
            std::string str;
            text.tempSubString(run.start, run.length).toUTF8String(str);
            printf("run: '%s' ", str.c_str());
        }
        printf("\n");
    }
}
