#ifndef BUFFER_H
#define BUFFER_H

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <string>

class Buffer
{
public:
    Buffer() : mData(nullptr), mSize(0) { }
    Buffer(size_t size);
    Buffer(const uint8_t* data, size_t size);
    Buffer(Buffer&& buf) noexcept;
    ~Buffer();

    Buffer& operator=(Buffer&& buf) noexcept;

    void resize(size_t size);
    void assign(const uint8_t* data, size_t size); // copies
    void append(const uint8_t* data, size_t size); // copies
    void clear();

    uint8_t* data() { return mData; }
    const uint8_t* data() const { return mData; }
    size_t size() const { return mSize; }
    bool empty() const { return !mSize; }

    static Buffer readFile(const std::string& path);
    void writeFile(const std::string& path);

private:
    uint8_t* mData;
    size_t mSize;
};

inline Buffer::Buffer(size_t size)
{
    mData = static_cast<uint8_t*>(malloc(size));
    mSize = size;
}

inline Buffer::Buffer(Buffer&& buf) noexcept
    : mData(buf.mData), mSize(buf.mSize)
{
    buf.mData = nullptr;
    buf.mSize = 0;
}

inline Buffer::Buffer(const uint8_t* data, size_t size)
    : mData(nullptr), mSize(0)
{
    assign(data, size);
}

inline Buffer::~Buffer()
{
    clear();
}

inline Buffer& Buffer::operator=(Buffer&& buf) noexcept
{
    if (mData) {
        free(mData);
    }
    mData = buf.mData;
    mSize = buf.mSize;
    buf.mData = nullptr;
    buf.mSize = 0;
    return *this;
}

inline void Buffer::assign(const uint8_t* data, size_t size)
{
    if (mData) {
        free(mData);
    }
    mData = static_cast<uint8_t*>(malloc(size));
    mSize = size;
    memcpy(mData, data, size);
}

inline void Buffer::append(const uint8_t* data, size_t size)
{
    mData = static_cast<uint8_t*>(realloc(mData, mSize + size));
    memcpy(mData + mSize, data, size);
    mSize += size;
}

inline void Buffer::resize(size_t size)
{
    mData = static_cast<uint8_t*>(realloc(mData, size));
    mSize = size;
}

inline void Buffer::clear()
{
    if (mData) {
        free(mData);
        mData = nullptr;
        mSize = 0;
    }
}

#endif // BUFFER_H
