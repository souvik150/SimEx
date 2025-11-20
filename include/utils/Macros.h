//
// Created by souvik on 11/12/25.
//

#ifndef ORDERMATCHINGSYSTEM_MACROS_H
#define ORDERMATCHINGSYSTEM_MACROS_H
#include <iostream>

#include "CompilerHints.h"

inline auto ASSERT(bool cond, const std::string &msg) noexcept {
    if (UNLIKELY(!cond)) {
        std::cerr << "ASSERT : " << msg << std::endl;

        exit(EXIT_FAILURE);
    }
}

inline auto FATAL(const std::string &msg) noexcept {
    std::cerr << "FATAL : " << msg << std::endl;

    exit(EXIT_FAILURE);
}

#endif //ORDERMATCHINGSYSTEM_MACROS_H