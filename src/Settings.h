#pragma once
#include <string>
#include <vector>

enum class Language { Russian = 0, English = 1 };

constexpr float kGammaMin = 0.30f;
constexpr float kGammaMax = 3.00f;

// All parameters except gamma are in the range -100..+100 (0 = no change).
struct ColorParams {
    float gamma = 1.0f;  // 0.30 .. 3.00 (1.0 = no change)
    float temperature = 0.0f;
    float shadows = 0.0f;
    float saturation = 0.0f;
    float vibrance = 0.0f;
    float brightness = 0.0f;
    float contrast = 0.0f;
    float highlights = 0.0f;

    void Clamp();
    bool operator==(const ColorParams&) const = default;
};

struct UserPreset {
    std::string name;  // UTF-8
    ColorParams params;
};

struct AppProfile {
    std::string exe;  // lower case, UTF-8, e.g. "game.exe"
    ColorParams params;
};

struct BuiltinPreset {
    const char* nameRu;
    const char* nameEn;
    ColorParams params;
};

struct Config {
    Language language = Language::Russian;
    float uiScale = 0.0f;  // 0 = determine from the monitor DPI
    bool effectEnabled = true;
    bool dither = true;
    bool onlyForProfiles = false;
    std::string monitor;  // DXGI device name, e.g. \\.\DISPLAY1
    ColorParams global;
    std::vector<UserPreset> userPresets;
    std::vector<AppProfile> profiles;

    bool hasPanelRect = false;
    int panelX = 0, panelY = 0, panelW = 0, panelH = 0;
};

const std::vector<BuiltinPreset>& BuiltinPresets();

bool LoadConfig(Config& config);
bool SaveConfig(const Config& config);