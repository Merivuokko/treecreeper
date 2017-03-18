// -*- mode: c++; c-basic-offset: 4 -*-

#ifndef INTERFACE_H
#define INTERFACE_H

#include "gcc-plugin.h"
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "intl.h"
#include "tm.h"
#include "cp/cp-tree.h"

// Some c++ specific variables or functiont, so link them weakly.
decltype (global_namespace) global_namespace __attribute__ ((weak));
decltype (cp_type_quals) cp_type_quals __attribute__ ((weak));
decltype (cp_build_qualified_type_real) cp_build_qualified_type_real __attribute__ ((weak));
decltype (language_to_string) language_to_string __attribute__ ((weak));
decltype (lang_check_failed) lang_check_failed __attribute__ ((weak));

namespace treecreeper {
    // True if we are running in C++ mode, false otherwise.
    extern bool in_cxx;
} // namespace treecreeper

#endif // !INTERFACE_H
