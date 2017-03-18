// -*- mode: c++; c-basic-offset: 4 -*-

#ifndef JSON_STREAM_H
#define JSON_STREAM_H

#include <assert.h>
#include <fstream>
#include <string>
#include <type_traits>
#include <vector>

namespace treecreeper {

    class JSONRawString;

    class JSONStream final {

    private:

        enum StreamState {
            NewStream,
            AfterBrace,
            AfterBracket,
            AfterColon,
            AfterValue
        };

        enum StreamContext {
            InRoot,
            InObject,
            InField,
            InArray
        };

        StreamState state = NewStream;
        std::vector<StreamContext> contexts = { InRoot };
        std::vector<bool> compactness;
        std::ofstream stream;
        int indentation = 4;

        StreamContext context () const
        { return contexts.back (); }

        bool compact () const
        { return compactness.back (); }

        void new_item ();
        void close_block (char c);
        void newline_and_indent ();

        template <typename T> JSONStream&
            write_raw_value (T value)
        {
            assert (context () != InObject || state == AfterColon);
            new_item ();
            stream << value;
            state = AfterValue;
            return *this;
        } // write_raw_value

    public:
        JSONStream (const char* const filename);
        JSONStream (const JSONStream&) = delete;
        void close ();

        JSONStream& operator<< (const char* const value);
        JSONStream& operator<< (const unsigned char* const value)
        { return *this << reinterpret_cast<const char* const> (value); }

        JSONStream& operator<< (const std::string& value);
        JSONStream& operator<< (const std::string&& value);
        JSONStream& operator<< (bool value);
        JSONStream& operator<< (const JSONRawString& value);
        JSONStream& operator<< (const JSONRawString&& value);
        JSONStream& operator[] (const char* const name);

        JSONStream& operator<< (int value)
        { return write_raw_value (value); }
        JSONStream& operator<< (unsigned int value)
        { return write_raw_value (value); }

        JSONStream& operator<< (long value)
        { return write_raw_value (value); }
        JSONStream& operator<< (unsigned long value)
        { return write_raw_value (value); }

        JSONStream& operator<< (long long value)
        { return write_raw_value (value); }
        JSONStream& operator<< (unsigned long long value)
        { return write_raw_value (value); }

        JSONStream& new_object (bool compact = false);
        JSONStream& new_array (bool compact = false);
        JSONStream& end_array ();
        JSONStream& end_object ();
    }; // class JSONStream

    class JSONRawString final {
    private:
        std::string str;

    public:
        JSONRawString (const std::string& value)
            : str (value)
            { }

        JSONRawString (const char* const value)
            : str (value)
            { }

        JSONRawString (const JSONRawString& value)
            : str (value.str)
            { }
                
        const std::string& get_str_ref () const
        { return str; }

    }; // class JSONRawString

    extern const JSONRawString Null;

} // namespace treecreeper

#endif // JSON_STREAM_H
