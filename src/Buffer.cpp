#include "Buffer.h"
#include <stdio.h>

Buffer Buffer::readFile(const std::string& path)
{
    FILE* f = fopen(path.c_str(), "r");
    if (!f)
        return Buffer();
    fseek(f, 0, SEEK_END);
    const auto sz = ftell(f);
    if (sz == 0) {
        fclose(f);
        return Buffer();
    }
    fseek(f, 0, SEEK_SET);
    Buffer buf(sz);
    fread(buf.data(), 1, sz, f);
    fclose(f);
    return buf;
}
