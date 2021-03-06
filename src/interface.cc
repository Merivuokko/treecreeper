// -*- mode: c++; c-basic-offset: 4 -*-

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>

#include "interface.h"
#include "traverse.h"

#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "options.h"

int plugin_is_GPL_compatible;

static void traverse_callback (void*, void* version);
static void visitor_callback (void* t, void* phase);

namespace treecreeper {
    bool in_cxx;
} // namespace treecreeper

static void
visitor_callback (void* t, void* phase)
{
    const char* phase_name = static_cast<const char*> (phase);
    std::cout << phase_name << ": ";
    treecreeper::visit_tree (static_cast<const_tree> (t));
} // visitor_callback

static void
traverse_callback (void*, void* version)
{
    std::cout << "Finished unit!\n";
    treecreeper::print_whole_tree (static_cast<plugin_gcc_version*> (version));
} // traverse_callback

extern "C" int
plugin_init (plugin_name_args* args,
             plugin_gcc_version* version)
{
    const char *base_name = args->base_name;
    // C++ functions declared with weak linkage in plugin.h will not be
    // available if we are called from the C frontend. If that is the case
    // enable restricted mode.
    treecreeper::in_cxx = global_namespace != nullptr;

    // Make standard C++ streams throw exceptions by
    // default. Useful for debugging.
    std::cout.exceptions (std::ostream::failbit | std::ostream::badbit);
    std::cerr.exceptions (std::ostream::failbit | std::ostream::badbit);

#ifndef DEBUG
    if (!plugin_default_version_check (version, &gcc_version))
        return 1;
#endif // !DEBUG

    treecreeper::options.builtins = false;

    for (int j = 0; j < args->argc; j++)
        {
            plugin_argument& arg = args->argv[j];
            if (!std::strcmp (arg.key, "output") && arg.value)
                treecreeper::options.output_file = arg.value;
            else if (!std::strcmp (arg.key, "builtins")
                     && (!arg.value || std::strcmp (arg.value, "true")))
                treecreeper::options.builtins = true;
            else
                std::cerr << "treecreeper: Unknown argument " << arg.key << "=" << arg.value << "\n";
        } // for

    if (treecreeper::options.output_file.empty ()) {
        std::cerr << "treecreeper: Output file not defined (use -fplugin-arg-"
                  << args->base_name << "-output=file)\n";
        std::exit (1);
    } // if

    // Disable assembly output.
    asm_file_name = HOST_BIT_BUCKET;

    plugin_info info = { TREECREEPER_VERSION, "Tree Creeper" };
    register_callback (args->base_name, PLUGIN_INFO, NULL, &info);
    register_callback (base_name,
                       PLUGIN_FINISH_UNIT,
                       traverse_callback,
                       version);

    register_callback (base_name,
                       PLUGIN_PRE_GENERICIZE,
                       visitor_callback,
                       (void*) "PRE_GENERICIZE");

    register_callback (base_name,
                       PLUGIN_FINISH_DECL,
                       visitor_callback,
                       (void*) "FINISH_DECL");

    register_callback (base_name,
                       PLUGIN_FINISH_TYPE,
                       visitor_callback,
                       (void*) "FINISH_TYPE");
    return 0;
} // plugin_init
