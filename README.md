# Tree Creeper

Tree Creeper (with a short package name treecreeper) is a GCC plugin which dumps GCC's Tree data structures together with C preprocessor macro definitions to a JSON format file. This may be useful for creating (for example) automatic binding generators from C and C++ to other languages. Some code or API analysis tools may have use for this information too.

The output contains detailed information about all declarations, types and CPP macros. Code is not dumped, so there's no access to function bodies, statements, expressions, variable initializers, etc. Most kinds of C declarations are supported. However, there is not yet support for C++ templates or many other features only present in C++11 and beyond.

The Tree Creeper plugin works with C and C++ frontends. I have tested it on GCC 6 and 7 on x86_64-pc-linux-gnu architecture.

Tree Creeper does not work on a recent GCC (such as GCC 13 as of this writing). I have switched to using LLVM's API as it is a bit cleaner. Porting to GCC 13 probably would not be very difficult – there did not seem to be too many API changes.

# WARNING

This tool is functional, but alpha quality and not really documented yet. Don't be surprised if it segfaults for you. There is no schema or any other kind of documentation for the output format.

# License

This software is licensed under the Terms of the GNU General Public License version 3.

# Usage

To build Tree Creeper edit its Makefile to suit your needs. If you want to debug Tree Creeper, you need to build your own GCC passing --enable-checking=tree to configure.

To build Tree Creeper, run "make all". Check out the output for errors and generated files' locations.

To test it, create a test file in C or C++ and then run gcc or g++ like this:

```sh
    g++ -S -fplugin=(path-to-treecreeper.so) -fplugin-arg-treecreeper-output=test.cc.json test.cc
```

Note that if you want to try Tree Creeper on a C++ header file, you'd better use the "-X c++" option to gcc so that it doesn't try to create a precompiled header for you.

Note also that Tree Creeper disables assembler output from GCC. This may change in the future.

To clean up the build dir, run `make clean` or `make distclean`.
