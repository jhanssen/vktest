#ifndef FONT_H
#define FONT_H

#include <Rect.h>
#include <string>
#include <hb.h>
#include <unicode/unistr.h>

class Font
{
public:
    Font();
    Font(const std::string& path, uint32_t size);
    Font(const Font& other);
    ~Font();

    Font& operator=(const Font& other);

    Rect measure(const icu::UnicodeString& str) const;
    bool isValid() const { return mFace != nullptr; }

    hb_face_t* face() const { return mFace; }
    hb_font_t* font() const { return mFont; }

private:
    hb_face_t* mFace;
    hb_font_t* mFont;
};

#endif // FONT_H
