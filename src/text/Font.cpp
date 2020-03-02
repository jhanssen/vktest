#include "Font.h"
#include <hb-ot.h>

Font::Font()
    : mFace(nullptr), mFont(nullptr), mSize(0)
{
}

Font::Font(const std::string& path, uint32_t size)
    : mFace(nullptr), mFont(nullptr), mPath(path), mSize(size)
{
    hb_blob_t* blob = hb_blob_create_from_file(path.c_str());
    if (!blob) {
        return;
    }
    mFace = hb_face_create(blob, 0);
    hb_blob_destroy(blob);
    mFont = hb_font_create(mFace);
    hb_ot_font_set_funcs(mFont);
    hb_font_set_scale(mFont, size * 64, size * 64);
}

Font::Font(const Font& other)
{
    if (!other.isValid())
        return;
    mFace = hb_face_reference(other.mFace);
    mFont = hb_font_reference(other.mFont);
    mPath = other.mPath;
    mSize = other.mSize;
}

Font::~Font()
{
    if (mFont)
        hb_font_destroy(mFont);
    if (mFace)
        hb_face_destroy(mFace);
}

Font& Font::operator=(const Font& other)
{
    if (mFont)
        hb_font_destroy(mFont);
    if (mFace)
        hb_face_destroy(mFace);
    if (!other.isValid()) {
        mFont = nullptr;
        mFace = nullptr;
        mPath.clear();
        mSize = 0;
        return *this;
    }
    mFace = hb_face_reference(other.mFace);
    mFont = hb_font_reference(other.mFont);
    mPath = other.mPath;
    mSize = other.mSize;
    return *this;
}

Rect Font::measure(const icu::UnicodeString& str) const
{
    if (!mFace) {
        return { 0 };
    }

    hb_buffer_t* buffer = hb_buffer_create();
    hb_buffer_add_utf16(buffer, reinterpret_cast<const uint16_t*>(str.getBuffer()), str.length(), 0, -1);
    hb_buffer_guess_segment_properties(buffer);

    hb_shape(mFont, buffer, nullptr, 0);

    hb_font_extents_t fontExtents;
    hb_font_get_extents_for_direction(mFont, HB_DIRECTION_LTR, &fontExtents);

    Rect r = Rect::fromRect(0, -fontExtents.ascender, 0, fontExtents.ascender - fontExtents.descender);

    const unsigned int len = hb_buffer_get_length(buffer);
    //hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buffer, nullptr);
    hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(buffer, nullptr);
    for (unsigned int i = 0; i < len; ++i) {
        r.width += (1 / 64.) * pos[i].x_advance;
    }

    hb_buffer_destroy(buffer);

    return r;
}
