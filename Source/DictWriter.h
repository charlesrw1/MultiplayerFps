#pragma once

#include <string>

class DictWriter
{
public:
    void write_key_value(const char* str1, const char* str2) {
        add_indents();
        this->str += str1;
        this->str += " \"";
        this->str += str2;
        this->str += "\"\n";
    }

    void write_key(const char* str1) {
        add_indents();
        this->str += str1;
        this->str += ' ';
    }

    void write_value_quoted(const char* str) {
        this->str += " \"";
        this->str += str;
        this->str += "\"\n";
    }

    void write_value_no_ln(const char* str) {
        this->str += ' ';
        this->str += str;
        this->str += ' ';
    }

    void write_value(const char* str1) {
        this->str += ' ';
        this->str += str1;
        this->str += '\n';
    }
    void write_key_list_start(const char* str) {
        add_indents();
        this->str += str;
        this->str += ' ';
        this->str += "[\n";
        tabs++;
    }
    void write_list_start() {
        add_indents();
        this->str += ' ';
        this->str += "[\n";
        tabs++;
    }
    void write_list_end() {
        tabs--;
        add_indents();
        this->str += "]\n";
    }
    void write_item_start() {
        add_indents();
        this->str += "{\n";
        tabs++;
    }
    void write_item_end() {
        tabs--;
        add_indents();
        this->str += "}\n";
    }
    void write_comment(const char* comment) {
        add_indents();
        this->str += "; ";
        this->str += comment;
        this->str += '\n';
    }

    std::string& get_output() {
        return str;
    }
    void set_should_add_indents(bool b) { should_add_indents = b; }
private:
    void add_indents() {
        if(should_add_indents)
            for (int i = 0; i < tabs; i++)
                this->str += '\t';
    }
    bool should_add_indents = false;
    int tabs = 0;
    std::string str;
};
