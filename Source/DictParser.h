#pragma once

#include "StringUtil.h"
#include <cassert>

class DictParser
{
public:
    bool load_from_file(const char* filename);
    void load_from_memory(const uint8_t* ptr, int length, const char* name);
    ~DictParser() {
        if (allocated) {
            delete buffer;
        }
    }

    // high level dict functions

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
        bool ret = read_next_token(tok);
        bool cmp = tok.cmp("[");
        if (!cmp) {
            raise_error("expected list start '['");
        }
        return cmp && ret;
    }
    bool check_list_end(const StringView& tok) {
        return tok.cmp("]");
    }

    bool read_string(StringView& str) {
        return read_next_token(str);
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
    void skip_to_next_line();
    void skip_whitespace();
    bool break_a_token(char c) {
        return (c == ' ' || c == '[' || c == ']' || c == '{' || c == '}' || c == '\t' || c == '\n');
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