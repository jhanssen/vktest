#ifndef BUFFER_H
#define BUFFER_H

#include <cstdint>
#include <cstddef>
#include <cstring>

class Buffer
{
public:
    Buffer() : mData(nullptr), mSize(0) { }
    Buffer(size_t size);
    Buffer(const uint8_t* data, size_t size);
    Buffer(Buffer&& buf) noexcept;
    ~Buffer();

    Buffer& operator=(Buffer&& buf) noexcept;

    void assign(const uint8_t* data, size_t size); // copies
    void clear();

    const uint8_t* data() const { return mData; }
    size_t size() const { return mSize; }

private:
    uint8_t* mData;
    size_t mSize;
};

Buffer::Buffer(size_t size)
{
    mData = new uint8_t[size];
    mSize = size;
}

Buffer::Buffer(Buffer&& buf) noexcept
    : mData(buf.mData), mSize(buf.mSize)
{
    buf.mData = nullptr;
    buf.mSize = 0;
}

Buffer::Buffer(const uint8_t* data, size_t size)
    : mData(nullptr), mSize(0)
{
    assign(data, size);
}

Buffer::~Buffer()
{
    clear();
}

Buffer& Buffer::operator=(Buffer&& buf) noexcept
{
    if (mData) {
        delete[] mData;
    }
    mData = buf.mData;
    mSize = buf.mSize;
    buf.mData = nullptr;
    buf.mSize = 0;
    return *this;
}

void Buffer::assign(const uint8_t* data, size_t size)
{
    if (mData) {
        delete[] mData;
    }
    mData = new uint8_t[size];
    mSize = size;
    memcpy(mData, data, size);
}

void Buffer::clear()
{
    if (mData) {
        delete[] mData;
        mData = nullptr;
        mSize = 0;
    }
}

#endif // BUFFER_H
