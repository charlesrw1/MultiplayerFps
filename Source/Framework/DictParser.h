#pragma once

#include "Framework/StringUtil.h"
#include <cassert>

class IFile;
class DictParser
{
public:
    void load_from_memory(const uint8_t* ptr, int length, const char* name);
    void load_from_file(IFile* file);
    ~DictParser() {
        if (allocated) {
            delete buffer;
        }
    }

    // high level dict functions

    bool expect_string(const char* str) {
        StringView tok;
        read_string(tok);
        return tok.cmp(str);
    }

    bool expect_item_start() {
        StringView tok;
        read_string(tok);
        return check_item_start(tok);
    }
    bool expect_item_end() {
        StringView tok;
        read_string(tok);
        return check_item_end(tok);
    }
    bool check_item_start(const StringView& tok) {
        return tok.cmp("{");
    }
    bool check_item_end(const StringView& tok) {
        return tok.cmp("}");
    }

    bool check_list_start(const StringView& tok) {
        return tok.cmp("[");
    }
    bool expect_list_start() {
        StringView tok;
        read_string(tok);
        return check_list_start(tok);
    }
    bool check_list_end(const StringView& tok) {
        return tok.cmp("]");
    }
    bool expect_list_end() {
        StringView tok;
        read_string(tok);
        return check_list_end(tok);
    }

    bool read_string(StringView& str) {
        return read_next_token(str);
    }
    void return_string(StringView view) {
        int index = (uint8_t*)view.str_start - buffer;
        assert(index >= 0 && index < buffer_size);
        if (index > 0 && index + 1 < buffer_size) {
            if (view.str_start[-1] == '\"' && view.str_start[view.str_len] == '\"') {
                index -= 1;
            }
        }
        read_ptr = index;
    }

    // convenience functions
    bool read_int(int& i);
    bool read_float(float& f);
    bool read_float2(float& f1, float& f2);
    bool read_float3(float& f1, float& f2, float& f3);


    int get_last_line() {
        return line;
    }
    StringView get_line_str(int line_to_get);

    bool is_eof() {
        return read_ptr >= buffer_size;
    }

    bool has_read_error() {
        return had_error;
    }

    bool expect_no_more_tokens();

    template<typename FUNCTOR>
    bool read_list_and_apply_functor(FUNCTOR&& f) {

        StringView start;
        read_string(start);
        while (!check_list_end(start) && !is_eof()) {
            bool out = f(start);
            if (!out) 
                return false;
            read_string(start);
        }

        return check_list_end(start);

    }

    bool read_line(StringView& line, char delimiter = '\n');

    const char* get_filename() const { return filename.c_str(); }

    int get_marker() {
        return read_ptr;
    }
    bool get_string_view_for_marker(StringView& sv, int start, int end) {
        if (start < buffer_size && end <= buffer_size) {
            sv = StringView((const char*)&buffer[start], end - start);
            return true;
        }
        return false;
    }
    void skip_to_next_line();
    void skip_whitespace();

    bool double_slash_comments = false;
private:
    void raise_error(const char* msg) {
        had_error = true;
        error_msg = msg;
    }

    // low level
    bool read_next_token(StringView& token);

    Stack_String<256> filename;

    int size() {
        return buffer_size;
    }

    char get_next_character_but_dont_fail(char default_ = '\0');

    char get_character(int where_) {
        assert(where_ < buffer_size);
        return buffer[where_];
    }
    bool break_a_token(char c) {
        return (c == ' ' || c == '[' || c == ']' || c == '{' || c == '}' || c == '\t' || c == '\n' || c=='\r');
    }
    const char* get_c_str(int ofs) {
        assert(ofs < buffer_size);
        return (char*)&buffer[ofs];
    }

    const uint8_t* buffer = nullptr;
    int buffer_size = 0;
    bool allocated = false;

    int read_ptr = 0;
    bool had_error = false;
    const char* error_msg = "";

    int line = 1;
};