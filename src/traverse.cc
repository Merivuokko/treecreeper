// -*- mode: c++; c-basic-offset: 4 -*-

#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <gmp.h>

#include "interface.h"
#include "json_stream.h"
#include "traverse.h"

#include "gcc-plugin.h"
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tree.h"
#include "intl.h"
#include "tm.h"
#include "c-family/c-common.h"
#include "c-family/c-pragma.h"
//#include "cp/cp-tree.h"
#include "diagnostic.h"
#include "cpplib.h"
#include "cpp-id-data.h"
#include "stor-layout.h"
#include "tree-pretty-print.h"
#include "wide-int.h"
#include "stringpool.h" //For get_identifier

namespace treecreeper {

    typedef void(*tree_printer_func)(JSONStream&, const_tree);

    // Mapping from tree nodes to unique numeric IDs.
    std::unordered_map<const_tree, int> tree_id_map;

    // Compare ..._DECL nodes by their source location. When node is not a declaration, order by pointer.
    class DeclLocationComparator {
    public:
        bool operator() (const_tree a, const_tree b) const
        {
            if (TREE_CODE_CLASS (TREE_CODE (a)) == tcc_declaration
                && TREE_CODE_CLASS (TREE_CODE (b)) == tcc_declaration)
                return DECL_SOURCE_LOCATION (a) < DECL_SOURCE_LOCATION (b);
            else
                return a < b;
        } // operator()
    }; // class DeclLocationComparator

    // Set of nodes ordered by their source location or pointer value.
    typedef std::set<const_tree, DeclLocationComparator> node_set;

    // Sets for storing all visited and seen tree nodes. This is necessary,
    // because C frontend only makes some tree nodes accessible through
    // per-node callbacks, whereas in C++ everything is reachable from within
    // the global namespace node.
    std::unordered_set<const_tree> visited_nodes;
    node_set all_nodes;

    // Map for collecting all CONST_DECLnodes. This is useful to unify
    // differences between C and C++ frotnends' enumeration type handling. See
    // print_enumeral_type for details.
    typedef std::multimap<const_tree, const_tree> const_decl_map;
    const_decl_map const_decl_nodes;

    static void call_printer (JSONStream& stream, tree_printer_func func, const_tree node);
    static const_tree find_const_decl (const_tree type, const_tree node);
    static JSONRawString get_int_value (const_tree cst);
    static const char* get_tree_name_ptr (const_tree node);
    static std::string make_description (const_tree node);
    static int make_tree_id (const_tree node);
    static void print_all_line_maps (JSONStream& stream);
    static void print_all_macros (JSONStream& stream);
    static void print_all_translation_units (JSONStream& stream);
    static void print_array_type (JSONStream& stream, const_tree type);
    static void print_block (JSONStream& stream, const_tree block);
    static void print_block_list (JSONStream& stream, const_tree block);
    static void print_common_constant (JSONStream& stream, const_tree cst);
    static void print_common_declaration (JSONStream& stream, const_tree decl);
    static void print_common_description (JSONStream& stream, const_tree node);
    static void print_common_precision (JSONStream& stream, const_tree type);
    static void print_common_tree (JSONStream& stream, const_tree node, bool supported = true);
    static void print_common_type (JSONStream& stream, const_tree type);
    static void print_common_visibility (JSONStream& stream, const_tree decl);
    static void print_complex_constant (JSONStream& stream, const_tree cst);
    static void print_complex_type (JSONStream& stream, const_tree type);
    static void print_const_decl (JSONStream& stream, const_tree decl);
    static void print_enumeral_type (JSONStream& stream, const_tree type);
    static void print_field_decl (JSONStream& stream, const_tree decl);
    static void print_fixed_point_constant (JSONStream& stream, const_tree cst);
    static void print_fixed_point_type (JSONStream& stream, const_tree type);
    static void print_function_decl (JSONStream& stream, const_tree decl);
    static void print_function_type (JSONStream& stream, const_tree type);
    static void print_identifier (JSONStream& stream, const_tree id);
    static void print_integer_constant (JSONStream& stream, const_tree cst);
    static void print_integer_type (JSONStream& stream, const_tree type);
    static void print_line_map (JSONStream& stream, line_map_ordinary* map);
    static void print_line_map_location (JSONStream& stream, line_map_ordinary* map);
    static void print_location (JSONStream& stream, source_location loc);
    static int print_macro (cpp_reader*, cpp_hashnode* node, void* stream_ptr);
    static void print_metadata (JSONStream& stream, plugin_gcc_version* version);
    static void print_namespace (JSONStream& stream, const_tree ns);
    static void print_pointer_type (JSONStream& stream, const_tree type);
    static void print_precisioned_type (JSONStream& stream, const_tree type);
    static void print_real_constant (JSONStream& stream, const_tree cst);
    static void print_record_type (JSONStream& stream, const_tree type);
    static void print_reference  (JSONStream& stream, const_tree node);
    static void print_root (JSONStream& stream, plugin_gcc_version* version);
    static void print_simple_type (JSONStream& stream, const_tree type);
    static void print_string_constant (JSONStream& stream, const_tree cst);
    static void print_unsupported_node (JSONStream& stream, const_tree node);
    static void print_template_decl (JSONStream& stream, const_tree decl);
    static void print_translation_unit_decl (JSONStream& stream, const_tree decl);
    static void print_type_decl (JSONStream& stream, const_tree decl);
    static void print_var_decl (JSONStream& stream, const_tree decl);
    static void print_vector_constant (JSONStream& stream, const_tree cst);
    static void print_vector_type (JSONStream& stream, const_tree type);
    static void remember_node (const_tree node);
    static bool should_only_reference (const_tree node);

    static JSONStream& operator<< (JSONStream& stream, const_tree node);
    static JSONStream& operator<< (JSONStream& stream, signop op);
    static std::ostream& operator<< (std::ostream& stream, const_tree node);

    OPTIONS options;

    const std::unordered_map<int, const tree_printer_func> tree_printer_map = {
        // Declarations
        { CONST_DECL, print_const_decl },
        { FIELD_DECL, print_field_decl },
        { FUNCTION_DECL, print_function_decl },
        { NAMESPACE_DECL, print_namespace },
        { TRANSLATION_UNIT_DECL, print_translation_unit_decl },
        { TYPE_DECL, print_type_decl },
        { PARM_DECL, print_var_decl },
        { RESULT_DECL, print_var_decl },
        { TEMPLATE_DECL, print_template_decl },
        { VAR_DECL, print_var_decl },

        // Types
        { ARRAY_TYPE, print_array_type },
        { BOOLEAN_TYPE, print_simple_type },
        { LANG_TYPE, print_simple_type },
        { VOID_TYPE, print_simple_type },
        { COMPLEX_TYPE, print_complex_type },
        { ENUMERAL_TYPE, print_enumeral_type },
        { FIXED_POINT_TYPE, print_fixed_point_type },
        { FUNCTION_TYPE, print_function_type },
        { METHOD_TYPE, print_function_type },
        { INTEGER_TYPE, print_integer_type },
        { NULLPTR_TYPE, print_pointer_type },
        { POINTER_TYPE, print_pointer_type },
        { POINTER_BOUNDS_TYPE, print_precisioned_type },
        { REFERENCE_TYPE, print_pointer_type },
        { REAL_TYPE, print_precisioned_type },
        { RECORD_TYPE, print_record_type },
        { QUAL_UNION_TYPE, print_record_type },
        { UNION_TYPE, print_record_type },
        { VECTOR_TYPE, print_vector_type },
        // Constants
        { COMPLEX_CST, print_complex_constant },
        { INTEGER_CST, print_integer_constant },
        { FIXED_CST, print_fixed_point_constant },
        { REAL_CST, print_real_constant },
        { STRING_CST, print_string_constant },
        { VECTOR_CST, print_vector_constant },
        // Exceptional nodes
        { BLOCK, print_block_list },
        { IDENTIFIER_NODE, print_identifier }
    }; // tree_printer_map

    static JSONStream& operator<< (JSONStream& stream, const_tree node)
    {
        if (!node)
            return stream << Null;

        try {
            const auto& function = tree_printer_map.at (TREE_CODE (node));
            call_printer (stream, function, node);
        } catch (std::out_of_range& e) {
            std::cerr << "treecreeper: Unsupported tree node " << node
                      << ". Output will be incomplete.\n";
            print_unsupported_node (stream, node);
        } // try...catch
        return stream;
    } // operator<<

    static bool
    should_only_reference (const_tree node)
    {
        int code = TREE_CODE (node);
        if (code == IDENTIFIER_NODE)
            return false;

        if (visited_nodes.count (node))
            return true;

        return false;
    } // should_only_reference

    static void
    call_printer (JSONStream& stream, tree_printer_func func, const_tree node)
    {
        if (!tree_id_map.count (node)) {
            tree_id_map[node] = make_tree_id (node);
            all_nodes.insert (node);
        } // if

        if (should_only_reference (node))
            print_reference (stream, node);
        else {
            visited_nodes.insert (node);
            func (stream, node);
        } // if
    } // accept

    static JSONStream&
    operator<< (JSONStream& stream, signop op)
    {
        switch (op) {
        case SIGNED:
            stream << "signed";
            break;

        case UNSIGNED:
            stream << "unsigned";
            break;
        } // switch
        return stream;
    } // operator<<

    static const_tree
    find_const_decl (const_tree type, const_tree node)
    {
        auto name = TREE_PURPOSE (node);
        auto value = TREE_VALUE (node);
        auto iterators = const_decl_nodes.equal_range (type);
        for (auto it = iterators.first; it != iterators.second; it++) {
            auto decl = it->second;
            if (DECL_NAME (decl) == name && DECL_INITIAL (decl) == value)
                return decl;
        } // for

        // CONST_DECL nodes are only present for the main variant of the type.
        // So look them up there also.
        if (type != TYPE_MAIN_VARIANT (type))
            return find_const_decl (TYPE_MAIN_VARIANT (type), node);

        // Nothing found, bail out!
        std::stringstream err;
        err << "Could not find CONST_DECL for " << name << ":"
            << type << "=" << value;
        throw std::logic_error (err.str ());
    } // find_const_decl

    static JSONRawString
    get_int_value (const_tree cst)
    {
        if (!cst)
            return Null;
        wide_int value (cst);
        mpz_t n;

        mpz_init (n);
        wi::to_mpz (value, n, TYPE_SIGN (TREE_TYPE (cst)));
        char* str = mpz_get_str (nullptr, 10, n);
        mpz_clear (n);
        if (!str)
            throw std::bad_alloc ();

        JSONRawString result (str);
        free (str);
        return result;
    } // get_int_value

    static const char*
    get_tree_name_ptr (const_tree node)
    {
        if (!node)
            return nullptr;

        auto code = TREE_CODE (node);
        auto klass = TREE_CODE_CLASS (code);
        const_tree id = nullptr;

        if (klass == tcc_declaration) {
            id = DECL_NAME (node);
        } else if (klass == tcc_type) {
            const_tree type_name = TYPE_NAME (node);
            if (!type_name)
                id = nullptr;
            else if (TREE_CODE (type_name) == IDENTIFIER_NODE)
                id = type_name;
            else if (TREE_CODE (type_name))
                id = DECL_NAME (type_name);
            else
                id = nullptr;
        } else if (code == IDENTIFIER_NODE)
            id = node;
        else
            id = nullptr;

        if (id && TREE_CODE (id) != IDENTIFIER_NODE)
            throw std::logic_error ("Bad identifier!");

        if (id)
            return IDENTIFIER_POINTER (id);
        else
            return nullptr;
    } // get_tree_name_ptr

    static std::string
    make_description (const_tree node)
    {
        if (!node)
            return "<Null tree>";

        char* buffer;
        size_t size;
        FILE* memstream = open_memstream (&buffer, &size);

        pretty_printer pp;
        pp_translate_identifiers (&pp) = false;
        pp.buffer->stream = memstream;

        if (TREE_CODE_CLASS (TREE_CODE (node)) == tcc_declaration
            && TREE_CODE (node) != TRANSLATION_UNIT_DECL)
            print_declaration (&pp, const_cast<tree> (node), 0, 0);
        else
            dump_generic_node (&pp, const_cast<tree> (node), 0, 0, false);

        pp_flush (&pp);
        fclose (memstream);

        if (!buffer)
            throw std::bad_alloc ();

        // Remove unwanted trailing characters
        char* printable_buffer = buffer;
        while (size > 0) {
            size--;
            char c = buffer[size];
            if (c <= ' ' || c == ';')
                continue;
            else {
                buffer[size + 1] = '\0';
                break;
            } // if
        } // while

        // Remove preceding whitespace (this occurs in the name of the global namespace)
        while (*printable_buffer == ' ') printable_buffer++;

        std::string result = printable_buffer;
        free (buffer);
        return result;
    } // make_description

    static int
    make_tree_id (const_tree node __attribute__ ((unused)))
    {
        static int id = 1;
        return id++;
    } // make_tree_id

    static void
    print_common_constant (JSONStream& stream, const_tree cst)
    {
        print_common_tree (stream, cst);
        stream["type"] << TREE_TYPE (cst);
    } // print_common_constant

    static void
    print_common_declaration (JSONStream& stream, const_tree decl)
    {
        // anonymous_namespace_name from gcc is static, so redefine it here.
        static const_tree anonymous_namespace_name = get_identifier ("_GLOBAL__N_1");

        print_common_tree (stream, decl);

        auto name = DECL_NAME (decl);
        if (name == anonymous_namespace_name)
            name = NULL_TREE;
        stream["name"] << name;

        stream["language"];
        if (in_cxx)
            stream << language_to_string (DECL_LANGUAGE (decl));
        else
            stream << "C";

        auto code = TREE_CODE (decl);
        const bool is_const = code == CONST_DECL;
        const bool is_field = code == FIELD_DECL;
        const bool is_func = code == FUNCTION_DECL;
        const bool is_parm = code == PARM_DECL;
        const bool is_result = code == RESULT_DECL;
        const bool is_type = code == TYPE_DECL;
        const bool is_var = code == VAR_DECL;

        if (is_func || is_var)
            stream["assembler name"] << DECL_ASSEMBLER_NAME (const_cast<tree> (decl));

        stream["context"] << DECL_CONTEXT (decl);
        if (DECL_ABSTRACT_ORIGIN (decl))
            stream["abstract origin"] << DECL_ABSTRACT_ORIGIN (decl);
        stream["artificial"] << bool (DECL_ARTIFICIAL (decl));
        stream["built-in"] << bool (DECL_IS_BUILTIN (decl));

        print_location (stream["location"], DECL_SOURCE_LOCATION (decl));

        const bool has_size_info =
            (is_field || is_parm || is_result || is_var);

        if (has_size_info) {
            stream["size"] << get_int_value (DECL_SIZE (decl));
            stream["alignment"] << DECL_ALIGN (decl);
        } // if

        const bool has_qualifiers = has_size_info || is_func || is_result;
        const bool has_static_extern = is_func || is_parm || is_var;

        // Qualifiers
        if (has_qualifiers) {
            stream["qualifiers"].new_array (true);
            if (has_static_extern && DECL_THIS_STATIC (decl))
                stream << "static";
            if (has_static_extern && DECL_THIS_EXTERN (decl))
                stream << "extern";
            if (TREE_THIS_VOLATILE (decl))
                stream << (is_func ? "no-return" : "volatile");
            if (is_func && DECL_DECLARED_INLINE_P (decl))
                stream << "inline";
            if (TREE_READONLY (decl))
                stream << "const";
            stream.end_array ();
        } // if

        const bool has_access_info = (has_qualifiers || is_const || is_type
                                      || code == NAMESPACE_DECL);

        if (has_access_info) {
            stream["access"];
            if (TREE_PRIVATE (decl))
                stream << "private";
            else if (TREE_PROTECTED (decl))
                stream << "protected";
            else if (TREE_PUBLIC (decl))
                stream << "public";
            else
                stream << "local";
        } // if

        if (is_func || is_var) {
            print_common_visibility (stream, decl);
        } // if
    } // print_common_declaration

    static void
    print_common_description (JSONStream& stream, const_tree node)
    {
        stream["description"];
        std::string data = make_description (node);
        if (!data.empty ())
            stream << data;
        else
            stream << Null;
    } // print_common_description

    static void
    print_common_precision (JSONStream& stream, const_tree type)
    {
        print_common_type (stream, type);
        stream["precison"] << TYPE_PRECISION (type);
    } // print_common_precision

    static void
    print_common_tree (JSONStream& stream, const_tree node, bool supported)
    {
        stream["kind"];
        if (supported)
            stream << "gcc_tree";
        else
            stream << "unsupported_gcc_tree";

        stream["id"] << tree_id_map[node];
        stream["node type"] << get_tree_code_name (TREE_CODE (node));
        print_common_description (stream, node);
    } // print_common_tree

    static void
    print_common_type (JSONStream& stream, const_tree type)
    {
        print_common_tree (stream, type);

        stream["name"] << get_tree_name_ptr (type);
        stream["context"] << TYPE_CONTEXT (type);

        const_tree decl = TYPE_NAME (type);
        stream["declaration"];
        if (decl && TREE_CODE (decl) != IDENTIFIER_NODE)
            stream << decl;
        else
            stream << Null;

        stream["complete"] << bool (COMPLETE_TYPE_P (type));
        stream["size"] << get_int_value (TYPE_SIZE (type));
        stream["alignment"] << TYPE_ALIGN (type);
        stream["user alignment"] << bool (TYPE_USER_ALIGN (type));

        // Qualifiers
        stream["qualifiers"].new_array (true);
        auto qualifiers = TYPE_QUALS (type);

        if (qualifiers & TYPE_QUAL_ATOMIC)
            stream << "atomic";

        if (qualifiers & TYPE_QUAL_CONST)
            stream << "const";

        if (qualifiers & TYPE_QUAL_RESTRICT)
            stream << "restrict";

        if (qualifiers & TYPE_QUAL_VOLATILE)
            stream << "volatile";

        stream.end_array ();

        stream["needs constuccting"] << bool (TYPE_NEEDS_CONSTRUCTING (type));

        stream["main variant"] << TYPE_MAIN_VARIANT (type);

        // Don't print NEXT_VARIANT, because its mostly useless and makes the
        // output hard to read. However, remember it so that we can pritnt it
        // at the end of traversal.
        remember_node (TYPE_NEXT_VARIANT (type));
    } // print_common_type

    static void
    print_common_visibility (JSONStream& stream, const_tree decl)
    {
        stream["weak linkage"] << bool (DECL_WEAK (decl));
        stream["visibility"];
        switch (DECL_VISIBILITY (decl)) {
        case VISIBILITY_DEFAULT:
            stream << "default";
            break;
        case VISIBILITY_PROTECTED:
            stream << "protected";
            break;
        case VISIBILITY_HIDDEN:
            stream << "hidden";
            break;
        case VISIBILITY_INTERNAL:
            stream << "internal";
            break;
        default:
            assert (false);
        } // switch
    } // print_common_visibility

    static void
    print_complex_constant (JSONStream& stream, const_tree cst)
    {
        stream.new_object ();
        print_common_constant (stream, cst);
        stream["real part"] << TREE_REALPART (cst);
        stream["imaginary part"] << TREE_IMAGPART (cst);
        stream.end_object ();
    } // print_complex_constant

    static void
    print_all_macros (JSONStream& stream)
    {
        stream.new_array ();
        cpp_forall_identifiers
            (parse_in, print_macro, &stream);
        stream.end_array ();
    } // print_all_macros

    static void
    print_all_translation_units (JSONStream& stream)
    {
        // NOTE: We assume that stream is in array state!
        if (!all_translation_units) {
            return;
        }

        unsigned int length = all_translation_units->length ();
        for (unsigned int j = 0; j < length; j++)
            stream << (*all_translation_units)[j];
    } // print_all_translation_units

    static void
    print_array_type (JSONStream& stream, const_tree type)
    {
        stream.new_object ();
        print_common_type (stream, type);

        stream["element type"] << TREE_TYPE (type);
        stream["index type"] << TYPE_DOMAIN (type);
        stream["is string"] << bool (TYPE_STRING_FLAG (type));
        stream["aliased components"] << !bool (TYPE_NONALIASED_COMPONENT (type));
        stream.end_object ();
    } // print_array_type

    static void
    print_block (JSONStream& stream, const_tree block)
    {
        stream.new_object ();
        print_common_tree (stream, block);

        // Harvest all declarations and order them by source location
        node_set decls;
        for (const_tree node = BLOCK_VARS (block); node; node = TREE_CHAIN (node)) {
            if (options.builtins || !DECL_IS_BUILTIN (node)) {
                remember_node (node);
                decls.insert (node);
            } // if
        } // for

        stream["declarations"].new_array ();
        for (auto decl : decls)
            stream << decl;
        stream.end_array ();

        stream["context"] << BLOCK_SUPERCONTEXT (block);

        stream["subblocks"].new_array ();
        for (tree node = BLOCK_SUBBLOCKS (block); node; node = BLOCK_CHAIN (node)) {
            stream << node;
        } // for
        stream.end_array ();
        stream.end_object ();
    } // print_block

    static void
    print_block_list (JSONStream& stream, const_tree block)
    {
        // This function must not use << or call_printer to print blocks.
        stream.new_array ();
        for (const_tree node = block; node; node = BLOCK_CHAIN (node)) {
            print_block (stream, node);
        } // for
        stream.end_array ();
    } // print_block_list

    static void
    print_complex_type (JSONStream& stream, const_tree type)
    {
        stream.new_object ();
        print_common_type (stream, type);

        stream["component type"] << TREE_TYPE (type);
        stream.end_object ();
    } // print_complex_type

    static void
    print_const_decl (JSONStream& stream, const_tree decl)
    {
        stream.new_object ();
        print_common_declaration (stream, decl);

        stream["type"] << TREE_TYPE (decl);
        stream["value"] << DECL_INITIAL (decl);
        stream.end_object ();
    } // print_const_decl

    static void
    print_enumeral_type (JSONStream& stream, const_tree type)
    {
        stream.new_object ();
        print_common_precision (stream, type);

        stream["scpoped"] << bool (ENUM_IS_SCOPED (type));
        stream["sign"] << TYPE_SIGN (type);
        stream["minimum value"] << TYPE_MIN_VALUE (type);
        stream["maximum value"] << TYPE_MAX_VALUE (type);

        stream["values"].new_array ();
        // Here C and C++ deviate: in C TREE_VALUE gives the enumeration value
        // as a integer constant, whereas in C++ it is a CONST_DECL node. We
        // unify this behaviour by collecting all CONST_DECL nodes beforehand
        // and looking them up here.
        for (auto elem = TYPE_VALUES (type); elem; elem = TREE_CHAIN (elem)) {
            auto value = TREE_VALUE (elem);
            if (TREE_CODE (value) == CONST_DECL)
                stream << value;
            else
                stream << find_const_decl (type, elem);
        } // for
        stream.end_array ();
        stream.end_object ();
    } // print_enumeral_type

    static void
    print_field_decl (JSONStream& stream, const_tree decl)
    {
        stream.new_object ();
        print_common_declaration (stream, decl);

        stream["type"] << TREE_TYPE (decl);
        stream["declaring class"] << DECL_FIELD_CONTEXT (decl);

        stream["unit offset"] << DECL_FIELD_OFFSET (decl);
        stream["unit size"] << DECL_OFFSET_ALIGN (decl);
        stream["bit offset"] << get_int_value (DECL_FIELD_BIT_OFFSET (decl));
        stream["bit-field"] << bool (DECL_C_BIT_FIELD (decl));
        if (DECL_C_BIT_FIELD (decl))
            stream["bit-field type"] << DECL_BIT_FIELD_TYPE (decl);

        stream["packed"] << bool (DECL_PACKED (decl));
        stream["mutable"] << bool (DECL_MUTABLE_P (decl));
        stream.end_object ();
    } // print_field_decl

    static void
    print_fixed_point_constant (JSONStream& stream, const_tree cst)
    {
        stream.new_object ();
        print_common_constant (stream, cst);

	char str[100];
	fixed_to_decimal (str, TREE_FIXED_CST_PTR (cst), sizeof (str));
	stream["value"] << str;
        stream.end_object ();
    } // print_fixed_point_constant

    static void
    print_fixed_point_type (JSONStream& stream, const_tree type)
    {
        stream.new_object ();
        print_common_precision (stream, type);

        stream["sign"] << TYPE_SIGN (type);
        stream["fractional bits"] << TYPE_FBIT (type);
        stream["integral bits"] << TYPE_IBIT (type);
        stream["saturating"] << bool (TYPE_SATURATING (type));

        stream.end_object ();
    } // print_fixed_point_type

    static void
    print_function_decl (JSONStream& stream, const_tree decl)
    {
        stream.new_object ();
        print_common_declaration (stream, decl);

        stream["function type"] << TREE_TYPE (decl);
        stream["result"] << DECL_RESULT (decl);

        stream["arguments"].new_array ();
        for (tree arg = DECL_ARGUMENTS (decl); arg; arg = TREE_CHAIN (arg))
            stream <<  arg;
        stream.end_array ();

        stream["defined"] << bool (TREE_STATIC (decl));
        stream["pure"] << bool (DECL_PURE_P (decl));
        stream["read globals"] << bool (!DECL_IS_NOVOPS (decl));
        stream["virtual"] << bool (DECL_VIRTUAL_P (decl));
        if (DECL_VIRTUAL_P (decl)) {
            stream["final"] << bool (DECL_FINAL_P (decl));
            stream["vtable index"] << DECL_VINDEX (decl);
        } // if

        if (DECL_CONV_FN_P (decl))
            stream["conversion target type"] << DECL_CONV_FN_TYPE (decl);

        stream["construction role"];
        if (DECL_STATIC_CONSTRUCTOR (decl))
            stream << "static constructor";
        else if (DECL_STATIC_DESTRUCTOR (decl))
            stream << "static destructor";
        else if (DECL_CONSTRUCTOR_P (decl))
            stream << "constructor";
        else if (DECL_CXX_DESTRUCTOR_P (decl))
            stream << "destructor";
        else
            stream << Null;

        if (DECL_CLONED_FUNCTION_P (decl))
            stream["cloned function"] << DECL_CLONED_FUNCTION (decl);
        stream.end_object ();
    } // print_function_decl

    static void
    print_function_type (JSONStream& stream, const_tree type)
    {
        stream.new_object ();
        print_common_type (stream, type);

        stream["result type"] << TREE_TYPE (type);

        if (TREE_CODE (type) == METHOD_TYPE)
            stream["class type"] << TYPE_METHOD_BASETYPE (type);

        stream["argument types"].new_array (true);
        bool variadic = true;
        for (tree arg = TYPE_ARG_TYPES (type); arg; arg = TREE_CHAIN (arg)) {
            const_tree arg_value = TREE_VALUE (arg);

            // Check the varidic argument case and omit the last void node
            if (arg_value == void_type_node && !TREE_CHAIN (arg)) {
                variadic = false;
                break;
            } // if

            stream << arg_value;
        } // for
        stream.end_array ();
        stream["variadic"] << variadic;

        stream.end_object ();
    } // print_function_type

    static void
    print_identifier (JSONStream& stream, const_tree id)
    {
        if (IDENTIFIER_TRANSPARENT_ALIAS (id)) {
            stream.new_array (true);
            for (auto& node = id; node; node = TREE_CHAIN (id))
                stream << IDENTIFIER_POINTER (node);
            stream.end_array ();
        } else if (IDENTIFIER_TYPENAME_P (id)) {
            stream.new_object ();
            print_common_tree (stream, id);
            stream["conversion operator"] << true;
            stream["target type"] << TREE_TYPE (id);
        } else {
            auto name = IDENTIFIER_POINTER (id);
            std::string result;
            if (IDENTIFIER_OPNAME_P (id)) {
                result += "operator";
                if (name)
                    result += ' ';
            } // if

            result += name;
            stream << result;
        } // if
    } // print_identifier

    static void
    print_integer_constant (JSONStream& stream, const_tree cst)
    {
        stream.new_object ();
        print_common_constant (stream, cst);
        stream["value"] << get_int_value (cst);
        stream["overflow"] << bool (TREE_OVERFLOW (cst));
        stream.end_object ();
    } // print_integer_constant

    static void
    print_integer_type (JSONStream& stream, const_tree type)
    {
        stream.new_object ();
        print_common_precision (stream, type);

        stream["sign"] << TYPE_SIGN (type);
        stream["minimum value"] << TYPE_MIN_VALUE (type);
        stream["maximum value"] << TYPE_MAX_VALUE (type);
        stream["is character"] << bool (TYPE_STRING_FLAG (type));

        stream.end_object ();
    } // print_integer_type


    static void
    print_all_line_maps (JSONStream& stream)
    {
        stream.new_array ();
        for (unsigned int j = 0; j < LINEMAPS_ORDINARY_USED (line_table); j++) {
            line_map_ordinary* map = LINEMAPS_ORDINARY_MAP_AT (line_table, j);
            print_line_map (stream, map);
        } // for
        // Note that the first linemap does not have a corresponding LC_LEAVE
        // entry, so we have to close an array and object explicitly.
        stream.end_array ();
        stream.end_object ();

        // Now end the array that we started ourselves.
        stream.end_array ();
    } // print_all_line_maps

    static void
    print_line_map (JSONStream& stream, line_map_ordinary* map)
    {
        switch (map->reason) {
        case LC_ENTER:
            stream.new_object ();
            stream["kind"] << "gcc_include";
            stream["include file"] << ORDINARY_MAP_FILE_NAME (map);
            print_line_map_location (stream, map);
            stream["includes"].new_array ();
            break;
        case LC_RENAME: case LC_RENAME_VERBATIM:
            stream.new_object ();
            stream["kind"] << "gcc_include";
            stream["rename"].new_object (true);
            stream["file"] << ORDINARY_MAP_FILE_NAME (map);
            stream["line"] << ORDINARY_MAP_STARTING_LINE_NUMBER (map);
            stream.end_object ();

            print_line_map_location (stream, map);
            stream.end_object ();
            break;
        case LC_LEAVE:
            stream.end_array (); // End of actions array
            stream.end_object (); // End of include file object
            break;
        default:
            ; // Don't care ???
        } // switch
    } // print_line_map

    static void
    print_line_map_location (JSONStream& stream, line_map_ordinary* map)
    {
        unsigned int from_idx
            = ORDINARY_MAP_INCLUDER_FILE_INDEX (map);
        line_map_ordinary* from_map =
            (from_idx < LINEMAPS_ORDINARY_USED (line_table)
             ? LINEMAPS_ORDINARY_MAP_AT (line_table, from_idx) : nullptr);
        stream["location"];
        if (from_map)
            print_location (stream, from_map->start_location);
        else
            stream << Null;
    } // print_line_map_from

    static void
    print_location (JSONStream& stream, const source_location loc)
    {
        if (loc == BUILTINS_LOCATION) {
            stream << "built-in";
            return;
        } else if (loc < RESERVED_LOCATION_COUNT) {
            stream << Null;
            return;
        } // if

        expanded_location locx = expand_location (loc);
        if (!locx.file) {
            stream << Null;
            return;
        } // if

        stream.new_object (true);
        stream["kind"] << "source_location";
        stream["file"] << locx.file;
        stream["line"] << locx.line;
        stream["column"] << locx.column;
        stream["system header"] << bool (locx.sysp);
        stream.end_object ();
    } // print_location

    static int
    print_macro (cpp_reader*, cpp_hashnode* node, void* stream_ptr)
    {
        if (node->type != NT_MACRO || (node->flags & NODE_BUILTIN))
            return 1;

        auto& stream = *static_cast<JSONStream*> (stream_ptr);
        stream.new_object ();
        stream["kind"] << "gcc_macro";
        const cpp_macro* macro = node->value.macro;

        stream["name"] << NODE_NAME (node);
        print_location (stream["location"], macro->line);

        stream["arguments"];
        if (!macro->fun_like)
            stream << Null;
        else {
            stream.new_array (true);
            for (int j = 0; j < macro->paramc; j++) {
                cpp_hashnode* arg = macro->params[j];
                stream << NODE_NAME (arg);
            } // for
            stream.end_array ();

            stream["variadic"] << macro->variadic;
        } // if

        stream["tokens"].new_array ();
        //unsigned int count = macro_real_token_count (macro); ???
        for (unsigned int j = 0; j < macro->count; j++) {
            cpp_token& token = macro->exp.tokens[j];

            stream.new_object (true);
            stream["kind"] << "gcc_macro_token";
            stream["type"] << cpp_type2name (token.type, token.flags);

            // Flags
            stream["flags"].new_array (true);
            if (token.flags & PREV_WHITE)
                stream << "previous whitespace";
            if (token.flags & DIGRAPH)
                stream << "digraph";
            if (token.flags & STRINGIFY_ARG)
                stream << "stringify";
            if (token.flags & PASTE_LEFT)
                stream << "paste left";
            if (token.flags & NAMED_OP)
                stream << "named operator";
            if (token.flags & NO_EXPAND)
                stream << "no expansion";
            if (token.flags & BOL)
                stream << "beginning of line";
            if (token.flags & PURE_ZERO)
                stream << "pure zero";
            if (token.flags & SP_DIGRAPH)
                stream << "sp digraph";
            if (token.flags & SP_PREV_WHITE)
                stream << "sp previous whitespace";
            stream.end_array ();

            stream["text"];
            if (token.type == CPP_MACRO_ARG)
                stream << NODE_NAME (macro->params[token.val.macro_arg.arg_no - 1]);
            else
                stream << cpp_token_as_text (parse_in, &token);

            stream.end_object ();
        } // for
        stream.end_array ();
        stream.end_object ();

        return 1;
    } // print_macro

    static void
    print_metadata (JSONStream& stream, plugin_gcc_version* version)
    {
        stream["metadata"].new_object ();
        stream["kind"] << "metadata_root";

        stream["format"].new_object ();
        stream["kind"] << "format_info";
        stream["creator"] << "Treecreeper GCC plugin";
        stream["version"] << "treecreeper-0";
        stream.end_object ();

        stream["compiler"].new_object ();
        stream["kind"] << "comipler_info";
        stream["name"] << "GCC";
        stream["version"] << version->basever;
        stream["revision"] << version->revision;
        stream["build date"] << version->datestamp;
        stream.end_object ();
        stream.end_object ();
    } // print_metadata

    static void
    print_namespace (JSONStream& stream, const_tree ns)
    {
        stream.new_object ();
        print_common_declaration (stream, ns);

        auto alias = DECL_NAMESPACE_ALIAS (ns);
        stream["alias for"] << alias;

        if (!alias) {
            // Harvest all declarations and order them by source location
            node_set decls;

            // Handle non-nmespace members first
            cp_binding_level* level = NAMESPACE_LEVEL (ns);
            for (const_tree decl = level->names; decl; decl = TREE_CHAIN (decl)) {
                if (options.builtins || !DECL_IS_BUILTIN (decl)) {
                    remember_node (decl);
                    decls.insert (decl);
                } // if
            } // for

            // Process subnamespaces
            for (auto decl = level->namespaces; decl; decl = TREE_CHAIN (decl)) {
                remember_node (decl);
                decls.insert (decl);
            } // for

            stream["declarations"].new_array ();
            for (auto decl : decls)
                stream << decl;
            stream.end_array ();
        } else
            stream["declarations"] << Null;
        stream.end_object ();
    } // print_namespace

    static void
    print_pointer_type (JSONStream& stream, const_tree type)
    {
        stream.new_object ();
        print_common_type (stream, type);

        stream["referred type"] << TREE_TYPE (type);
        if (TREE_CODE (type) == REFERENCE_TYPE)
            stream["rvalue reference"] << bool (TYPE_REF_IS_RVALUE (type));

        stream["member pointer"] << bool (TYPE_PTRDATAMEM_P (type));
        if (TYPE_PTRDATAMEM_P (type)) {
            stream["class type"] << TYPE_PTRMEM_CLASS_TYPE (type);
            stream["member type"] << TYPE_PTRMEM_POINTED_TO_TYPE (type);
        } //if
        stream.end_object ();
    } // print_pointer_type

    static void
    print_precisioned_type (JSONStream& stream, const_tree type)
    {
        stream.new_object ();
        print_common_precision (stream, type);
        stream.end_object ();
    } // print_precisioned_type

    static void
    print_real_constant (JSONStream& stream, const_tree cst)
    {
        stream.new_object ();
        print_common_constant (stream, cst);

	REAL_VALUE_TYPE d;
        stream["value"];
	d = TREE_REAL_CST (cst);
	if (REAL_VALUE_ISINF (d))
            stream << (REAL_VALUE_NEGATIVE (d) ? "-Inf" : "Inf");
	else if (REAL_VALUE_ISNAN (d))
            stream << "Nan";
        else {
	    char str[100];
	    real_to_decimal (str, &d, sizeof (str), 0, 1);
	    stream << str;
        } // if
        stream["overflow"] << bool (TREE_OVERFLOW (cst));
        stream.end_object ();
    } // print_reaal_constant

    static void
    print_record_type (JSONStream& stream, const_tree type)
    {
        // Special case:record_type node may be actually a holder for a
        // pointer-to-member-function type. Handle this case separately from
        // struct types.
        if (TYPE_PTRMEMFUNC_P (type)) {
            stream << TYPE_PTRMEMFUNC_FN_TYPE (type);
            return;
        } // if

        stream.new_object ();
        print_common_type (stream, type);

        const_tree base_info = TYPE_BINFO (type);
        if (base_info) {
            stream["base types"].new_array ();
            const size_t num_bases = BINFO_N_BASE_BINFOS (base_info);
            bool has_access_infos = bool (BINFO_BASE_ACCESSES (base_info));

            for (size_t n = 0; n < num_bases; n++) {
                const_tree base = BINFO_BASE_BINFO (base_info, n);

                stream.new_object ();
                stream["type"] << BINFO_TYPE (base);

                stream["access"];
                const_tree access;
                if (has_access_infos)
                    access = BINFO_BASE_ACCESS (base_info, n);
                else
                    access = access_public_node;

                if (access == access_private_node)
                    stream << "private";
                else if (access == access_protected_node)
                    stream << "protected";
                else
                    stream << "public";

                stream["virtual"] << bool (BINFO_VIRTUAL_P (base));
                stream.end_object ();
            } // for
            stream.end_array ();
        } // if

        // Handle members
        stream["fields"].new_array ();
        for (tree field = TYPE_FIELDS (type); field; field = TREE_CHAIN (field))
            stream << field;
        stream.end_array ();

        stream["methods"].new_array ();
        for (tree method = TYPE_METHODS (type); method; method = TREE_CHAIN (method))
            stream << method;
        stream.end_array ();

        stream.end_object ();
    } // print_record_type

    static void
    print_reference (JSONStream& stream, const_tree node)
    {
        stream.new_object (true);
        stream["kind"] << "reference";
        stream["referred id"] << tree_id_map[node];
        stream.end_object ();
    } // print_reference

    static void
    print_root (JSONStream& stream, plugin_gcc_version* version)
    {
        stream.new_object ();
        stream["kind"] << "root";

        // Put meta info in place
        print_metadata (stream, version);

        stream["declarations"].new_array ();
        print_all_translation_units (stream);
        if (global_namespace)
            stream << global_namespace;
        stream.end_array ();

        // Make sure that we did not miss a single declaration
        for (auto& elem : all_nodes) {
            if (!visited_nodes.count (elem)) {
                stream << elem;
            } // if
        } // for

        print_all_macros (stream["macros"]);
        print_all_line_maps (stream["includes"]);
        stream.end_object ();
    } // print_root

    static void
    print_simple_type (JSONStream& stream, const_tree type)
    {
        stream.new_object ();
        print_common_type (stream, type);
        stream.end_object ();
    } // print_simple_type

    static void
    print_string_constant (JSONStream& stream, const_tree cst)
    {
        stream.new_object ();
        print_common_constant (stream, cst);
        stream["value"] << TREE_STRING_POINTER (cst);
        stream.end_object ();
    } // print_string_constant

    static void
    print_unsupported_node (JSONStream& stream, const_tree node)
    {
        stream.new_object ();
        print_common_tree (stream, node, false);
        stream.end_object ();
    } // print_unsupported_node

    static void
    print_template_decl (JSONStream& stream, const_tree decl)
    {
        stream.new_object ();
        print_common_declaration (stream, decl);

        stream["parameters"].new_array ();
        auto& params = DECL_TEMPLATE_PARMS (decl);
        for (auto& param = params; param; param = TREE_CHAIN (param)) {
            stream.new_object ();
            stream["level"] << TREE_INT_CST_LOW (TREE_PURPOSE (param));
            stream["parameters"].new_array ();
            auto vec = TREE_VALUE (param);
            auto length = TREE_VEC_LENGTH (vec);
            for (auto j = 0; j < length; j++) {
                auto elt = TREE_VEC_ELT (vec, j);
                stream.new_object ();
                stream["parameter"] << TREE_VALUE (elt);
                stream["default"] << TREE_PURPOSE (elt);
                stream.end_object ();
            } // for
            stream.end_array ();
            stream.end_object ();
        } // for
        stream.end_array ();

        stream["result"] << DECL_TEMPLATE_RESULT (decl);
        stream.end_object ();
    } // print_template_decl

    static void
    print_translation_unit_decl (JSONStream& stream, const_tree decl)
    {
        stream.new_object ();
        print_common_declaration (stream, decl);

        stream["language standard"] << TRANSLATION_UNIT_LANGUAGE (decl);
        stream["blocks"] << DECL_INITIAL (decl);
        stream.end_object ();
    } // print_translation_unit

    static void
    print_type_decl (JSONStream& stream, const_tree decl)
    {
        stream.new_object ();
        print_common_declaration (stream, decl);

        const_tree type = TREE_TYPE (decl);
        if (!type) {
            std::stringstream err;
            err << "Found a type declaration without type: " << decl;
            throw std::invalid_argument (err.str ());
        } // if

        stream["type"] << type;
        stream.end_object ();
    } // print_type_decl

    static void
    print_var_decl (JSONStream& stream, const_tree decl)
    {
        int code = TREE_CODE (decl);
        stream.new_object ();
        print_common_declaration (stream, decl);

        if (code == PARM_DECL) {
            stream["type"] << TREE_TYPE (decl);
            stream["passing type"] << DECL_ARG_TYPE (decl);
        } else if (code == RESULT_DECL) {
            stream["return type"] << TREE_TYPE (decl);
        } else if (code == VAR_DECL) {
            stream["type"] << TREE_TYPE (decl);
            stream["thread local"] << bool (DECL_THREAD_LOCAL_P (decl));
            stream["vtable"] << bool (DECL_VIRTUAL_P (decl));
        } else if (code == FIELD_DECL) {
            stream["type"] << TREE_TYPE (decl);
            stream["vtable pointer"] << bool (DECL_VIRTUAL_P (decl));
        } // if

        if (code == PARM_DECL || code == RESULT_DECL) {
            stream["passing style"];
            if (DECL_BY_REFERENCE (decl))
                stream << "reference";
            else
                stream << "copy";
        } // if

        stream.end_object ();
    } // print_var_decl

    static void
    print_vector_constant (JSONStream& stream, const_tree cst)
    {
        stream.new_object ();
        print_common_constant (stream, cst);

        stream["values"].new_array ();
	for (unsigned int j = 0; j < VECTOR_CST_NELTS (cst); j++) {
	    stream << VECTOR_CST_ELT (cst, j);
        } // for
        stream.end_array ();
        stream.end_object ();
    } // print_vector_constant

    static void
    print_vector_type (JSONStream& stream, const_tree type)
    {
        stream.new_object ();
        print_common_type (stream, type);

        stream["element type"] << TREE_TYPE (type);
        stream["element count"] << TYPE_PRECISION (type);
        stream.end_object ();
    } // print_vector_type

    static std::ostream&
    operator<< (std::ostream& stream, const_tree node)
    {
        if (!node) {
            stream << "<Null tree>";
            return stream;
        } // if

        stream << "<Tree:";

        // Location
        expanded_location loc;
        loc.file = nullptr;

        auto klass = TREE_CODE_CLASS (TREE_CODE (node));
        if (klass == tcc_declaration)
            loc = expand_location (DECL_SOURCE_LOCATION (node));
        else if (EXPR_HAS_LOCATION (node))
            loc = expand_location (EXPR_LOCATION (node));

        if (loc.file) {
            stream << " loc=" << loc.file << ":" << loc.line << ":" << loc.column;
        } // if

        stream << ": code=" << get_tree_code_name (TREE_CODE (node)) << " ";
        stream << "class="
               << tree_code_class_strings[klass];

        const char* name = get_tree_name_ptr (node);
        if (name)
            stream << " name=" << name;
        stream << " descr=" << make_description (node);
        if (klass == tcc_declaration)
            stream << " context=" << DECL_CONTEXT (node);
        else if (klass == tcc_type)
            stream << " context=" << TYPE_CONTEXT (node);
        stream << ">";

        return stream;
    } // operator<<

    void
    print_whole_tree (plugin_gcc_version* version)
    {
        if (errorcount || sorrycount) {
            std::cerr << "Treecreeper: Errors occured while compiling, will not write output.\n";
            return;
        } // if

        JSONStream stream (options.output_file.c_str ());
        print_root (stream, version);
        stream.close ();
    } // print_whole_tree

    static void
    remember_node (const_tree node)
    {
        if (!node)
            return;

        all_nodes.insert (node);
        // Collect CONST_DECL nodes, see print_enumeral_type for details.
        if (TREE_CODE (node) == CONST_DECL) {
            const_decl_nodes.insert (std::make_pair (TREE_TYPE (node), node));
        } // if
    } // remember_node

    void
    visit_tree (const_tree node)
    {
        std::cout << "Visiting " << node << std::endl;
        remember_node (node);
    } // visit_tree

} // namespace treecreeper
