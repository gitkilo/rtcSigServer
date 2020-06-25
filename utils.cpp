//
//  utils.cpp
//  sign_server
//
//  Created by chifl on 2020/6/25.
//  Copyright © 2020 chifl. All rights reserved.
//

#include "utils.hpp"

template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];

#define arraysize(array) (sizeof(ArraySizeHelper(array)))

std::string int2str(int i)
{
    return ToString(i);
}

std::string size_t2str(size_t i)
{
    return ToString(i);
}

std::string ToString(const int s)
{
    char buf[32];
    const int len = std::snprintf(&buf[0], arraysize(buf), "%d", s);
    return std::string(&buf[0], len);
}

std::string ToString(const unsigned long s)
{
    char buf[32];
    const int len = std::snprintf(&buf[0], arraysize(buf), "%lu", s);
    return std::string(&buf[0], len);
}
