#include "Framework/DictParser.h"
#include "Framework/Files.h"
#include <fstream>

void DictParser::load_from_memory(const uint8_t* ptr, int length, const char* name) {
    
    assert(!buffer);

    buffer = ptr;
    buffer_size = length;
    read_ptr = 0;
    line = 1;
    allocated = false;

    this->filename = name;
}

void DictParser::load_from_file(IFile* file)
{
    assert(!buffer);


    buffer = new uint8_t[file->size()];
    buffer_size = file->size();
    allocated = true;

    file->read((uint8_t*)buffer, buffer_size);

    read_ptr = 0;
    line = 1;

    this->filename = filename;
}

// convenience functions

 bool DictParser::read_int(int& i) {
    StringView view;
    bool ret = read_next_token(view);
    if (!ret) return false;
    auto cstr = view.to_stack_string();
    i = atoi(cstr.c_str());
    return true;
}

 bool DictParser::read_float(float& f) {
    StringView view;
    bool ret = read_next_token(view);
    if (!ret) return false;
    auto cstr = view.to_stack_string();
    f = atof(cstr.c_str());
    return true;
}

 bool DictParser::read_float2(float& f1, float& f2) {
    StringView view;
    bool ret = read_next_token(view);
    if (!ret) return false;
    auto cstr = view.to_stack_string();
    return sscanf_s(cstr.c_str(), "%f %f", &f1, &f2) == 2;
}

 bool DictParser::read_float3(float& f1, float& f2, float& f3) {
    StringView view;
    bool ret = read_next_token(view);
    if (!ret) return false;
    auto cstr = view.to_stack_string();
    return sscanf_s(cstr.c_str(), "%f %f %f", &f1, &f2, &f3) == 3;
}

StringView DictParser::get_line_str(int line_to_get) {
    int line_so_far = 1;
    int i = 0;
    while (i < buffer_size) {
        if (get_character(i++) == '\n') line_so_far++;
        if (line_so_far == line_to_get) {
            break;
        }
    }
    int count = 0;
    int start = i;
    while (i < buffer_size) {
        if (get_character(i++) == '\n') {
            break;
        }
    }
    return StringView(get_c_str(start), count);
}

bool DictParser::expect_no_more_tokens() {
    StringView tok;
    bool ret = read_next_token(tok);
    if (ret) {
        raise_error("got tokens but expected eof");
    }
    else if (!ret) {
        had_error = false;
        error_msg = "";
    }
    return !ret;
}

// low level

bool DictParser::read_next_token(StringView& token) {
    bool is_quote = false;

    skip_whitespace();

    if (is_eof()) {
        raise_error("tried to read past end");
        return false;
    }

    char c = get_character(read_ptr++);
    // skip commented lines
    while (c == ';') {
        skip_to_next_line();
        skip_whitespace();

        if (is_eof()) {
            raise_error("tried to read past end");
            return false;
        }

        c = get_character(read_ptr++);
    }

    if (c == '\"') {
        is_quote = true;
    }
    else if (break_a_token(c)) {
        token = StringView(get_c_str(read_ptr - 1), 1);
        skip_whitespace();
        return true;
    }

    int start = read_ptr - 1;
    int count = 1;
    while (read_ptr < size()) {

        c = get_character(read_ptr++);
        if (is_quote && c == '\"') {
            assert(start + 1 < buffer_size);
            start = start + 1;
            count = count - 1;
            break;
        }
        else if (!is_quote && break_a_token(c)) {
            break;
        }
        count++;
    }
    token = StringView(get_c_str(start), count);

    skip_whitespace();

    return true;
}

char DictParser::get_next_character_but_dont_fail(char default_) {
    if (read_ptr < buffer_size) {
        return buffer[read_ptr];
    }
    else
        return default_;
}

bool DictParser::read_line(StringView& line, char delimiter)
{
    if (is_eof()) {
        return false;
    }
    int start = read_ptr;
    while (read_ptr < size()) {
        if (get_character(read_ptr) == delimiter) {
            line.str_start = (char*)&buffer[start];
            line.str_len = read_ptr - start;
            read_ptr++;
            return true;
        }
        read_ptr++;
    }

    line.str_start = (char*)&buffer[start];
    line.str_len = read_ptr - start;
    return true;
}

void DictParser::skip_to_next_line() {
    while (read_ptr < size()) {
        char c = get_character(read_ptr++);
        if (c == '\n') {
            line++;
            break;
        }
    }
}

void DictParser::skip_whitespace() {
    while (read_ptr < size()) {
        char c = get_character(read_ptr++);
        if (c == ' ' || c == '\t' || c== '\r') continue;
        else if (c == '\n') {
            line++;
            continue;
        }
        else {
            read_ptr--;
            break;
        }
    }
}
