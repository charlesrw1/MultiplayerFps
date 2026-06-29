#include "AnimSidecarFile.h"
#include "Animation/AnimEvent.h"
#include "Animation/SkeletonData.h"
#include "Render/Model.h"
#include "Framework/Files.h"
#include "Framework/Util.h"
#include "AssetCompile/Someutils.h"
#include <json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace AnimSidecarFile {

std::string amd_path_for_model(const std::string& cmdl_game_path) {
    return strip_extension(cmdl_game_path) + ".amd";
}

void apply_to_model(Model* model) {
    ASSERT(model);
    auto* skel = model->get_skel();
    if (!skel) return;

    std::string amd_path = amd_path_for_model(model->get_name());
    auto file = FileSys::open_read_game(amd_path.c_str());
    if (!file) return;

    std::string text;
    {
        size_t sz = file->size();
        text.resize(sz);
        file->read(text.data(), sz);
    }

    json root;
    try { root = json::parse(text); }
    catch (...) {
        sys_print(Warning, "AnimSidecarFile: failed to parse %s\n", amd_path.c_str());
        return;
    }

    auto anims_it = root.find("animations");
    if (anims_it == root.end() || !anims_it->is_object()) return;

    for (auto& [clip_name, clip_json] : anims_it->items()) {
        AnimationSeq* seq = skel->find_clip(clip_name);
        if (!seq) continue;

        if (auto it = clip_json.find("is_additive"); it != clip_json.end() && it->is_boolean())
            seq->is_additive_clip = it->get<bool>();

        seq->anim_events.clear();
        auto events_it = clip_json.find("events");
        if (events_it == clip_json.end() || !events_it->is_array()) continue;

        for (auto& ev_json : *events_it) {
            AnimEvent ev;
            ev.name        = ev_json.value("name",        std::string{});
            ev.payload     = ev_json.value("payload",     std::string{});
            ev.time_start  = ev_json.value("time_start",  0.f);
            ev.time_end    = ev_json.value("time_end",    0.f);
            ev.is_duration = ev_json.value("is_duration", false);
            if (!ev.name.empty())
                seq->anim_events.push_back(std::move(ev));
        }
    }
}

bool save_from_model(Model* model) {
    ASSERT(model);
    auto* skel = model->get_skel();
    if (!skel) return false;

    json anims = json::object();
    for (auto& [clip_name, refed] : skel->get_clips_hashmap()) {
        const AnimationSeq* seq = refed.ptr;
        if (!seq) continue;

        json clip_json = json::object();
        clip_json["is_additive"] = seq->is_additive_clip;

        json events_arr = json::array();
        for (auto& ev : seq->anim_events) {
            json ej;
            ej["name"]        = ev.name;
            ej["payload"]     = ev.payload;
            ej["time_start"]  = ev.time_start;
            ej["time_end"]    = ev.time_end;
            ej["is_duration"] = ev.is_duration;
            events_arr.push_back(std::move(ej));
        }
        clip_json["events"] = std::move(events_arr);
        anims[clip_name] = std::move(clip_json);
    }

    json root;
    root["animations"] = std::move(anims);

    std::string text = root.dump(2);
    std::string amd_path = amd_path_for_model(model->get_name());
    auto f = FileSys::open_write_game(amd_path);
    if (!f) {
        sys_print(Error, "AnimSidecarFile: could not write %s\n", amd_path.c_str());
        return false;
    }
    f->write(text.data(), text.size());
    f->close();
    return true;
}

} // namespace AnimSidecarFile
