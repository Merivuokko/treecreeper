// -*- mode: c++; c-basic-offset: 4 -*-

#ifndef TRAVERSE_H
#define TRAVERSE_H

#include <string>

#include "gcc-plugin.h"
#include "tree.h"

struct plugin_gcc_version;

namespace treecreeper {

    struct OPTIONS {
        std::string output_file;
        bool builtins;
    };

    extern OPTIONS options;

    void print_whole_tree (plugin_gcc_version* version);
    void visit_tree (const_tree tree);

} // namespace treecreeper

#endif // TRAVERSE_H
