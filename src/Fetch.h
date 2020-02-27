#ifndef FETCH_H
#define FETCH_H

#include "Buffer.h"
#include <string>

struct Fetch
{
    static Buffer fetch(const std::string& uri);
};

#endif
