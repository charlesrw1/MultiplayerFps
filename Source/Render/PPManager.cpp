#include "PPManager.h"
#include "Framework/Util.h"

PPManager* PPManager::inst = nullptr;

PPManager::PPManager() {
    ASSERT(!inst);
    inst = this;
}
PPManager::~PPManager() {
    inst = nullptr;
}

PPManager::Handle PPManager::register_settings(int priority) {
    for (int i = 0; i < (int)entries_.size(); i++) {
        if (!entries_[i].valid) {
            entries_[i] = {.priority = priority, .valid = true};
            return {i};
        }
    }
    entries_.push_back({.priority = priority, .valid = true});
    return {(int)entries_.size() - 1};
}

void PPManager::update_settings(Handle h, const PostProcessParams& p) {
    ASSERT(h.is_valid() && h.id < (int)entries_.size());
    entries_[h.id].params = p;
}

void PPManager::remove_settings(Handle& h) {
    if (!h.is_valid()) return;
    ASSERT(h.id < (int)entries_.size());
    entries_[h.id].valid = false;
    h.id = -1;
}

PPManager::Handle PPManager::push_override(const PostProcessParams& p, int priority) {
    auto h = register_settings(priority);
    update_settings(h, p);
    return h;
}

void PPManager::pop_override(Handle& h) {
    remove_settings(h);
}

PostProcessParams PPManager::get_active() const {
    const Entry* best = nullptr;
    for (auto& e : entries_) {
        if (!e.valid) continue;
        if (!best || e.priority > best->priority)
            best = &e;
    }
    return best ? best->params : defaults_;
}
