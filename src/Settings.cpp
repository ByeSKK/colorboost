#include "Settings.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "Utils.h"

namespace fs = std::filesystem;
using nlohmann::json;

void ColorParams::Clamp() {
    auto fix = [](float& v, float lo, float hi, float def) {
        if (!std::isfinite(v)) v = def;
        v = std::clamp(v, lo, hi);
        };
    fix(gamma, kGammaMin, kGammaMax, 1.0f);
    fix(temperature, -100.0f, 100.0f, 0.0f);
    fix(shadows, -100.0f, 100.0f, 0.0f);
    fix(saturation, -100.0f, 100.0f, 0.0f);
    fix(vibrance, -100.0f, 100.0f, 0.0f);
    fix(brightness, -100.0f, 100.0f, 0.0f);
    fix(contrast, -100.0f, 100.0f, 0.0f);
    fix(highlights, -100.0f, 100.0f, 0.0f);
}

namespace {

    ColorParams P(float gamma, float temperature, float shadows, float saturation, float vibrance,
        float brightness, float contrast, float highlights) {
        ColorParams p;
        p.gamma = gamma;
        p.temperature = temperature;
        p.shadows = shadows;
        p.saturation = saturation;
        p.vibrance = vibrance;
        p.brightness = brightness;
        p.contrast = contrast;
        p.highlights = highlights;
        return p;
    }

    float GetFloat(const json& j, const char* key, float def) {
        const auto it = j.find(key);
        if (it != j.end() && it->is_number()) return it->get<float>();
        return def;
    }
    int GetInt(const json& j, const char* key, int def) {
        const auto it = j.find(key);
        if (it != j.end() && it->is_number()) return it->get<int>();
        return def;
    }
    bool GetBool(const json& j, const char* key, bool def) {
        const auto it = j.find(key);
        if (it != j.end() && it->is_boolean()) return it->get<bool>();
        return def;
    }
    std::string GetString(const json& j, const char* key, const std::string& def) {
        const auto it = j.find(key);
        if (it != j.end() && it->is_string()) return it->get<std::string>();
        return def;
    }

    json ParamsToJson(const ColorParams& p) {
        return json{ {"gamma", p.gamma},           {"temperature", p.temperature}, {"shadows", p.shadows},
                    {"saturation", p.saturation}, {"vibrance", p.vibrance},       {"brightness", p.brightness},
                    {"contrast", p.contrast},     {"highlights", p.highlights} };
    }

    ColorParams ParamsFromJson(const json& j) {
        ColorParams p;
        if (!j.is_object()) return p;
        p.gamma = GetFloat(j, "gamma", p.gamma);
        p.temperature = GetFloat(j, "temperature", p.temperature);
        p.shadows = GetFloat(j, "shadows", p.shadows);
        p.saturation = GetFloat(j, "saturation", p.saturation);
        p.vibrance = GetFloat(j, "vibrance", p.vibrance);
        p.brightness = GetFloat(j, "brightness", p.brightness);
        p.contrast = GetFloat(j, "contrast", p.contrast);
        p.highlights = GetFloat(j, "highlights", p.highlights);
        p.Clamp();
        return p;
    }

}  // namespace

const std::vector<BuiltinPreset>& BuiltinPresets() {
    //                                         gamma temp  shad  sat   vib   bri  con   high
    static const std::vector<BuiltinPreset> presets = {
        {"По умолчанию", "Default", P(1.00f, 0, 0, 0, 0, 0, 0, 0)},
        {"Яркие цвета", "Vivid", P(1.00f, 0, 0, 12, 40, 0, 12, -5)},
        {"Видимость в тёмных сценах", "Dark scene visibility", P(1.20f, 0, 45, 5, 20, 0, 15, -10)},
        {"Соревновательный", "Competitive", P(1.10f, -5, 30, 10, 25, 4, 25, -15)},
        {"Максимальный контраст", "Maximum contrast", P(1.05f, 0, 15, 15, 35, 0, 40, -10)},
        {"Кино", "Cinematic", P(1.00f, 10, -10, -8, 15, 0, 22, -15)},
        {"Холодный", "Cool", P(1.00f, -25, 0, 5, 15, 0, 8, 0)},
        {"Тёплый", "Warm", P(1.00f, 25, 0, 5, 15, 0, 8, 0)},
        {"Чёрно-белый", "Black and white", P(1.00f, 0, 0, -100, 0, 0, 15, 0)},
    };
    return presets;
}

bool LoadConfig(Config& config) {
    try {
        const fs::path path = util::ConfigFilePath();
        std::error_code ec;
        if (!fs::exists(path, ec)) return false;

        std::ifstream in(path, std::ios::binary);
        if (!in) return false;
        std::stringstream buffer;
        buffer << in.rdbuf();

        const json root = json::parse(buffer.str(), nullptr, false);
        if (root.is_discarded() || !root.is_object()) return false;

        config.language = GetInt(root, "language", 0) == 1 ? Language::English : Language::Russian;
        config.uiScale = GetFloat(root, "uiScale", 0.0f);
        config.effectEnabled = GetBool(root, "effectEnabled", true);
        config.dither = GetBool(root, "dither", true);
        config.onlyForProfiles = GetBool(root, "onlyForProfiles", false);
        config.monitor = GetString(root, "monitor", "");

        if (const auto it = root.find("global"); it != root.end()) config.global = ParamsFromJson(*it);

        config.userPresets.clear();
        if (const auto it = root.find("userPresets"); it != root.end() && it->is_array()) {
            for (const auto& e : *it) {
                if (!e.is_object()) continue;
                UserPreset preset;
                preset.name = GetString(e, "name", "");
                if (preset.name.empty()) continue;
                if (const auto pi = e.find("params"); pi != e.end()) preset.params = ParamsFromJson(*pi);
                config.userPresets.push_back(std::move(preset));
            }
        }

        config.profiles.clear();
        if (const auto it = root.find("profiles"); it != root.end() && it->is_array()) {
            for (const auto& e : *it) {
                if (!e.is_object()) continue;
                AppProfile profile;
                profile.exe = util::ToLowerAscii(GetString(e, "exe", ""));
                if (profile.exe.empty()) continue;
                if (const auto pi = e.find("params"); pi != e.end()) profile.params = ParamsFromJson(*pi);
                config.profiles.push_back(std::move(profile));
            }
        }

        if (const auto it = root.find("panelRect"); it != root.end() && it->is_array() && it->size() == 4) {
            bool ok = true;
            for (const auto& v : *it) ok = ok && v.is_number();
            if (ok) {
                config.hasPanelRect = true;
                config.panelX = (*it)[0].get<int>();
                config.panelY = (*it)[1].get<int>();
                config.panelW = (*it)[2].get<int>();
                config.panelH = (*it)[3].get<int>();
            }
        }
        return true;
    }
    catch (...) {
        return false;
    }
}

bool SaveConfig(const Config& config) {
    try {
        json root;
        root["version"] = 1;
        root["language"] = config.language == Language::English ? 1 : 0;
        root["uiScale"] = config.uiScale;
        root["effectEnabled"] = config.effectEnabled;
        root["dither"] = config.dither;
        root["onlyForProfiles"] = config.onlyForProfiles;
        root["monitor"] = config.monitor;
        root["global"] = ParamsToJson(config.global);

        json presets = json::array();
        for (const auto& p : config.userPresets) {
            presets.push_back(json{ {"name", p.name}, {"params", ParamsToJson(p.params)} });
        }
        root["userPresets"] = presets;

        json profiles = json::array();
        for (const auto& p : config.profiles) {
            profiles.push_back(json{ {"exe", p.exe}, {"params", ParamsToJson(p.params)} });
        }
        root["profiles"] = profiles;

        if (config.hasPanelRect) {
            root["panelRect"] = json::array({ config.panelX, config.panelY, config.panelW, config.panelH });
        }

        const fs::path path = util::ConfigFilePath();
        fs::path tmp = path;
        tmp += L".tmp";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if (!out) return false;
            out << root.dump(2, ' ', false, json::error_handler_t::replace);
            if (!out.good()) return false;
        }
        std::error_code ec;
        fs::rename(tmp, path, ec);
        if (ec) {
            fs::copy_file(tmp, path, fs::copy_options::overwrite_existing, ec);
            std::error_code ignored;
            fs::remove(tmp, ignored);
            return !ec;
        }
        return true;
    }
    catch (...) {
        return false;
    }
}