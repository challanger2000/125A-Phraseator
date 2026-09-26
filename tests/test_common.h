#pragma once

#include <iostream>

#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "CHECK failed: " #expr \
                      << " at " << __FILE__ << ":" << __LINE__ << '\n'; \
            return 1; \
        } \
    } while (false)
