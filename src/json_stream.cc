// -*- mode: c++; c-basic-offset: 4 -*-

#include <assert.h>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "json_stream.h"

namespace treecreeper {

    static std::string quote_json_string (const char* const value);

    JSONStream::JSONStream (const char* const filename)
    {
        assert (filename);

        stream.exceptions (std::ofstream::failbit | std::ofstream::badbit);
        stream.open (filename, std::ofstream::trunc);
    } // JSONStream::JSONStream

    void JSONStream::close ()
    {
        assert (context () == InRoot);
        if (state != NewStream)
            stream << std::endl;
        stream.close ();
    } // JSONStream::close

    void JSONStream::new_item ()
    {
        auto ctx = context ();

        assert ((ctx == InObject && (state == AfterBrace || state == AfterColon
                                     || state == AfterValue))
                || (ctx == InArray
                    && (state == AfterBracket || state == AfterValue))
                || (ctx == InRoot && state == NewStream)
                || (ctx != InRoot && state != NewStream));

        if (state == NewStream)
            return; // Don't add extra spaces in the beginning of file
        else if (state == AfterValue) {
            // Separate values/fields by commas in arrays and objects
            stream << ',';
        } // if

        // Put space or newline after comma, bracket or brace
        if (state == AfterColon || compact ())
            stream << ' ';
        else
            newline_and_indent ();
    } // JSONStream::new_item

    void JSONStream::close_block (char c)
    {
        assert (state == AfterBrace || state == AfterBracket || state == AfterValue);

        contexts.pop_back ();

        if (state == AfterBrace || state == AfterBracket || compact ())
            stream << ' ';
        else
            newline_and_indent ();

        stream << c;
        compactness.pop_back ();
        state = AfterValue;
    } // JSONStream::close_block

    void JSONStream::newline_and_indent ()
    {
        stream << std::endl;
        for (auto j = (contexts.size () - 1) * indentation; j > 0; j--)
            stream << ' ';
    } // JSONStream::newline_and_indent

    JSONStream& JSONStream::operator<< (const char* const value)
    {
        if (value)
            return write_raw_value (quote_json_string (value).c_str ());
        else
            return write_raw_value ("null");
    } // JSONStream::operator<<

    JSONStream& JSONStream::operator<< (const std::string& value)
    {
        return *this << value.c_str();
    } // JSONStream::operator<<

    JSONStream& JSONStream::operator<< (const std::string&& value)
    {
        return *this << value.c_str();
    } // JSONStream::operator<<
  
    JSONStream& JSONStream::operator<< (bool value)
    {
        return write_raw_value (value ? "true" : "false");
    } // JSONStream::operator<<

    JSONStream& JSONStream::operator<< (const JSONRawString& value)
    {
        return write_raw_value (value.get_str_ref ().c_str ());
    } // JSONStream::operator<<

    JSONStream& JSONStream::operator<< (const JSONRawString&& value)
    {
        return *this << value;
    } // JSONStream::operator<<

    JSONStream& JSONStream::operator[] (const char* const name)
    {
        assert (name);
        assert (context () == InObject
                && (state == AfterBrace || state == AfterValue));
        new_item ();
        stream << quote_json_string (name) << ':';
        state = AfterColon;
        return *this;
    } // JSONStream::operator[]

    JSONStream& JSONStream::new_object (bool compact)
    {
        assert (context () != InObject || state != AfterBrace);
        new_item ();
        stream << '{';
        state = AfterBrace;
        contexts.push_back (InObject);
        compactness.push_back (compact);
        return *this;
    } // JSONStream::new_object

    JSONStream& JSONStream::new_array (bool compact)
    {
        assert (context () != InObject || state != AfterBrace);
        new_item ();
        stream << '[';
        state = AfterBracket;
        contexts.push_back (InArray);
        compactness.push_back (compact);
        return *this;
    } // JSONStream::new_array
  
    JSONStream& JSONStream::end_array ()
    {
        assert (context () == InArray
                && (state == AfterValue || state == AfterBracket));
        close_block (']');
        return *this;
    } // JSONStream::end_array

    JSONStream& JSONStream::end_object ()
    {
        assert (context () == InObject
                && (state == AfterValue || state == AfterBrace));
        close_block ('}');
        return *this;
    } // JSONStream::end_object

    std::string quote_json_string (const char* const value)
    {
        if (!value)
            std::abort ();

        std::stringstream ss;
        ss << std::setfill('0') << std::hex << '"';

        for (int j = 0; value[j] != 0; j++)
            {
                char c = value[j];

                if (c < 0x20) { // Control characters
                    switch (c) {
                    case '\a':
                        ss << "\\a";
                        break;
                    case '\b':
                        ss << "\\b";
                        break;
                    case '\t':
                        ss << "\\t";
                        break;
                    case '\n':
                        ss << "\\n";
                        break;
                    case '\f':
                        ss << "\\f";
                        break;
                    case '\r':
                        ss << "\\r";
                        break;
                    default:
                        // Use unicode escape
                        ss << "\\u" << std::setw (4) << int (c);
                    } // switch
                } else if (c == '\\')
                    ss << "\\\\";
                else if (c == '"')
                    ss << "\\\"";
                else
                    ss << c;
            } // for
        ss << '"';
        return ss.str ();
    } // json_quote_string

    const JSONRawString Null ("null");

} // namespace treecreeper
