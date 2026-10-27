#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <string>

#include "App.h"
#include "Utils.h"

// ============================================================================
// Modern dark theme — shared constants + drawing helpers
// ============================================================================

namespace {

    constexpr ImVec4 kAccent = ImVec4(0.31f, 0.65f, 1.00f, 1.00f);  // #4FA6FF
    constexpr ImVec4 kAccentHover = ImVec4(0.45f, 0.75f, 1.00f, 1.00f);
    constexpr ImVec4 kSuccess = ImVec4(0.34f, 0.85f, 0.55f, 1.00f);  // #57D98C
    constexpr ImVec4 kWarning = ImVec4(1.00f, 0.75f, 0.35f, 1.00f);  // #FFBF59
    constexpr ImVec4 kDanger = ImVec4(1.00f, 0.42f, 0.42f, 1.00f);  // #FF6B6B
    constexpr ImVec4 kMuted = ImVec4(0.55f, 0.60f, 0.67f, 1.00f);
    constexpr ImVec4 kMutedDim = ImVec4(0.38f, 0.42f, 0.48f, 1.00f);

    // ---- small text helpers ----

    void SectionLabel(const char* text) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
    }

    void MutedText(const char* text) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
    }

    // ---- modern buttons ----

    bool AccentButton(const char* label, const ImVec2& size = ImVec2(0.0f, 0.0f)) {
        ImGui::PushStyleColor(ImGuiCol_Button, kAccent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccentHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccent);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.04f, 0.06f, 0.09f, 1.00f));
        const bool pressed = ImGui::Button(label, size);
        ImGui::PopStyleColor(4);
        return pressed;
    }

    bool GhostButton(const char* label, const ImVec2& size = ImVec2(0.0f, 0.0f)) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.20f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.23f, 0.28f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.26f, 0.30f, 0.36f, 1.00f));
        const bool pressed = ImGui::Button(label, size);
        ImGui::PopStyleColor(3);
        return pressed;
    }

    bool DangerButton(const char* label, const ImVec2& size = ImVec2(0.0f, 0.0f)) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.13f, 0.15f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.42f, 0.17f, 0.19f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kDanger);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.86f, 0.86f, 1.00f));
        const bool pressed = ImGui::Button(label, size);
        ImGui::PopStyleColor(4);
        return pressed;
    }

    // ---- status pill ----

    void DrawPill(const char* text, const ImVec4& color) {
        const ImGuiStyle& style = ImGui::GetStyle();
        const ImVec2 textSize = ImGui::CalcTextSize(text);
        const ImVec2 padding(style.FramePadding.x * 1.1f, style.FramePadding.y * 0.55f);
        const ImVec2 size(textSize.x + padding.x * 2.0f, textSize.y + padding.y * 2.0f);
        const ImVec2 pos = ImGui::GetCursorScreenPos();

        ImVec4 bg = color;
        bg.w = 0.16f;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::GetColorU32(bg), size.y * 0.5f);
        dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::GetColorU32(color), size.y * 0.5f);
        dl->AddText(ImVec2(pos.x + padding.x, pos.y + padding.y),
            ImGui::GetColorU32(color), text);
        ImGui::Dummy(size);
    }

    // ---- card primitive ----

    void BeginCard(const char* id) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.075f, 0.085f, 0.105f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.14f, 0.16f, 0.20f, 1.00f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::BeginChild(id, ImVec2(0.0f, 0.0f),
            ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    }

    void EndCard() {
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }

}  // namespace

// ============================================================================
// ParamSlider — modernised row: muted label | slider | reset
// ============================================================================

bool App::ParamSlider(const char* id, Str label, float& value, float minValue, float maxValue, const char* format,
    float defaultValue) {
    bool changed = false;
    const float scale = appliedScale_;
    const float labelWidth = 160.0f * scale;
    const float resetWidth = ImGui::CalcTextSize("R").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;

    ImGui::PushID(id);

    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::TextUnformatted(Tr(label));
    ImGui::PopStyleColor();
    ImGui::SameLine(labelWidth);

    const float avail = ImGui::GetContentRegionAvail().x;
    const float sliderWidth = std::max(120.0f * scale, avail - resetWidth - spacing);
    ImGui::SetNextItemWidth(sliderWidth);
    if (ImGui::SliderFloat("##slider", &value, minValue, maxValue, format, ImGuiSliderFlags_AlwaysClamp)) {
        changed = true;
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("R")) {
        value = defaultValue;
        changed = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", Tr(Str::ResetParam));

    ImGui::PopID();
    return changed;
}

// ============================================================================
// DrawPanel — top-level frame with header + tabs
// ============================================================================

void App::DrawPanel() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));

    if (ImGui::Begin("##ColorBoostMain", nullptr, flags)) {
        // ---- header: title + status pill
        {
            ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
            ImGui::TextUnformatted("ColorBoost");
            ImGui::PopStyleColor();

            ImGui::SameLine();

            ImVec4 statusColor;
            if (!cfg_.effectEnabled) statusColor = kMutedDim;
            else if (cfg_.onlyForProfiles && FindProfile(lastExe_) == nullptr) statusColor = kWarning;
            else if (!capture_.Valid() || !overlayShown_) statusColor = kAccent;
            else statusColor = kSuccess;

            DrawPill(StatusText(), statusColor);

            // gradient accent underline
            ImGui::Spacing();
            {
                const ImVec2 p = ImGui::GetCursorScreenPos();
                const float w = ImGui::GetContentRegionAvail().x;
                const float h = 2.0f * appliedScale_;
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddRectFilledMultiColor(
                    p, ImVec2(p.x + w, p.y + h),
                    ImGui::GetColorU32(kAccent),
                    ImGui::GetColorU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.0f)),
                    ImGui::GetColorU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.0f)),
                    ImGui::GetColorU32(kAccent));
                ImGui::Dummy(ImVec2(w, h));
            }
            ImGui::Spacing();
        }

        // ---- tabs
        if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
            if (ImGui::BeginTabItem(Tr(Str::TabColor))) {
                DrawColorTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(Tr(Str::TabProfiles))) {
                DrawProfilesTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(Tr(Str::TabSettings))) {
                DrawSettingsTab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

// ============================================================================
// DrawColorTab — effect / preset / adjustments / hotkeys as cards
// ============================================================================

void App::DrawColorTab() {
    // The edit target changes when the foreground app (with / without a profile) changes.
    {
        const std::string key = ActiveProfile() ? lastExe_ : std::string();
        if (key != lastEditKey_) {
            lastEditKey_ = key;
            selectedPreset_ = -1;
        }
    }

    // ---- Effect card
    BeginCard("##card_effect");
    {
        if (ImGui::Checkbox(Tr(Str::EffectEnabled), &cfg_.effectEnabled)) MarkParamsEdited();

        ImGui::Spacing();
        if (AppProfile* profile = ActiveProfile()) {
            ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
            ImGui::Text("%s: %s", Tr(Str::EditingProfile), profile->exe.c_str());
            ImGui::PopStyleColor();
        }
        else {
            MutedText(Tr(Str::EditingGlobal));
        }
    }
    EndCard();

    // ---- Preset card
    ImGui::Spacing();
    BeginCard("##card_preset");
    {
        SectionLabel(Tr(Str::Preset));

        const int builtinCount = static_cast<int>(BuiltinPresets().size());
        const int presetCount = PresetCount();

        std::string preview = Tr(Str::PresetCustom);
        if (selectedPreset_ >= 0 && selectedPreset_ < presetCount) {
            preview = PresetName(selectedPreset_);
            if (!(PresetParams(selectedPreset_) == EditTarget())) preview += " *";
        }

        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##preset", preview.c_str())) {
            for (int i = 0; i < presetCount; ++i) {
                ImGui::PushID(i);
                const bool selected = (i == selectedPreset_);
                const std::string name = PresetName(i);
                if (ImGui::Selectable(name.c_str(), selected)) ApplyPreset(i);
                if (selected) ImGui::SetItemDefaultFocus();
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();

        const bool userSelected = selectedPreset_ >= builtinCount && selectedPreset_ < presetCount;

        if (AccentButton(Tr(Str::SaveNew))) SaveCurrentAsNewPreset();
        ImGui::SameLine();

        ImGui::BeginDisabled(!userSelected);
        if (GhostButton(Tr(Str::Overwrite))) {
            cfg_.userPresets[static_cast<size_t>(selectedPreset_ - builtinCount)].params = EditTarget();
            MarkConfigDirty();
        }
        ImGui::SameLine();
        if (DangerButton(Tr(Str::Delete))) {
            cfg_.userPresets.erase(cfg_.userPresets.begin() + (selectedPreset_ - builtinCount));
            selectedPreset_ = -1;
            MarkConfigDirty();
        }
        ImGui::EndDisabled();
    }
    EndCard();

    // ---- Adjustments card
    ImGui::Spacing();
    BeginCard("##card_adjust");
    {
        SectionLabel(Tr(Str::Adjustments));
        ImGui::Spacing();

        ColorParams& target = EditTarget();
        bool changed = false;
        changed |= ParamSlider("gamma", Str::Gamma, target.gamma, kGammaMin, kGammaMax, "%.2f", 1.0f);
        changed |= ParamSlider("temperature", Str::Temperature, target.temperature, -100.0f, 100.0f, "%+.0f", 0.0f);
        changed |= ParamSlider("shadows", Str::Shadows, target.shadows, -100.0f, 100.0f, "%+.0f", 0.0f);
        changed |= ParamSlider("saturation", Str::Saturation, target.saturation, -100.0f, 100.0f, "%+.0f", 0.0f);
        changed |= ParamSlider("vibrance", Str::Vibrance, target.vibrance, -100.0f, 100.0f, "%+.0f", 0.0f);
        changed |= ParamSlider("brightness", Str::Brightness, target.brightness, -100.0f, 100.0f, "%+.0f", 0.0f);
        changed |= ParamSlider("contrast", Str::Contrast, target.contrast, -100.0f, 100.0f, "%+.0f", 0.0f);
        changed |= ParamSlider("highlights", Str::Highlights, target.highlights, -100.0f, 100.0f, "%+.0f", 0.0f);
        if (changed) MarkParamsEdited();
    }
    EndCard();

    ImGui::Spacing();
    if (GhostButton(Tr(Str::ResetAll))) ResetCurrent();

    // ---- Hotkeys card
    ImGui::Spacing();
    BeginCard("##card_hotkeys");
    {
        SectionLabel(Tr(Str::HotkeysTitle));
        ImGui::Spacing();
        MutedText(Tr(Str::HkPanel));
        MutedText(Tr(Str::HkEffect));
        MutedText(Tr(Str::HkNext));
        MutedText(Tr(Str::HkReset));
        if (!hotkeyError_.empty()) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, kWarning);
            ImGui::TextWrapped("%s", hotkeyError_.c_str());
            ImGui::PopStyleColor();
        }
    }
    EndCard();
}

// ============================================================================
// DrawProfilesTab — help / current app / options / list
// ============================================================================

void App::DrawProfilesTab() {
    // ---- Help card
    BeginCard("##card_help");
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
        ImGui::TextWrapped("%s", Tr(Str::ProfilesHelp));
        ImGui::PopStyleColor();
    }
    EndCard();

    // ---- Current app card
    ImGui::Spacing();
    BeginCard("##card_current");
    {
        SectionLabel(Tr(Str::CurrentApp));
        ImGui::Spacing();

        if (!lastExe_.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
            ImGui::TextUnformatted(lastExe_.c_str());
            ImGui::PopStyleColor();
        }
        else {
            MutedText(Tr(Str::NoApp));
        }

        ImGui::Spacing();

        if (!lastExe_.empty()) {
            if (ActiveProfile() == nullptr) {
                if (AccentButton(Tr(Str::CreateProfile))) {
                    AppProfile profile;
                    profile.exe = lastExe_;
                    profile.params = cfg_.global;
                    cfg_.profiles.push_back(std::move(profile));
                    selectedPreset_ = -1;
                    MarkParamsEdited();
                }
            }
            else if (DangerButton(Tr(Str::DeleteProfile))) {
                const std::string exe = lastExe_;
                cfg_.profiles.erase(std::remove_if(cfg_.profiles.begin(), cfg_.profiles.end(),
                    [&](const AppProfile& p) { return p.exe == exe; }),
                    cfg_.profiles.end());
                selectedPreset_ = -1;
                MarkParamsEdited();
            }
        }
    }
    EndCard();

    // ---- Options card
    ImGui::Spacing();
    BeginCard("##card_options");
    {
        if (ImGui::Checkbox(Tr(Str::OnlyProfiles), &cfg_.onlyForProfiles)) MarkParamsEdited();
    }
    EndCard();

    // ---- Profile list card
    ImGui::Spacing();
    BeginCard("##card_list");
    {
        SectionLabel(Tr(Str::ProfileList));
        ImGui::Spacing();

        if (cfg_.profiles.empty()) {
            MutedText(Tr(Str::NoProfiles));
        }
        else {
            int removeIndex = -1;
            for (int i = 0; i < static_cast<int>(cfg_.profiles.size()); ++i) {
                ImGui::PushID(i);
                if (ImGui::SmallButton("X")) removeIndex = i;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", Tr(Str::Delete));
                ImGui::SameLine();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(cfg_.profiles[static_cast<size_t>(i)].exe.c_str());
                ImGui::PopID();
            }
            if (removeIndex >= 0) {
                cfg_.profiles.erase(cfg_.profiles.begin() + removeIndex);
                selectedPreset_ = -1;
                MarkParamsEdited();
            }
        }
    }
    EndCard();
}

// ============================================================================
// DrawSettingsTab — language / monitor / ui / diagnostics
// ============================================================================

void App::DrawSettingsTab() {
    // ---- Language card
    BeginCard("##card_language");
    {
        SectionLabel(Tr(Str::Language));
        ImGui::Spacing();

        static const char* const kLanguages[] = { "Русский", "English" };
        const int languageIndex = cfg_.language == Language::English ? 1 : 0;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##language", kLanguages[languageIndex])) {
            for (int i = 0; i < 2; ++i) {
                if (ImGui::Selectable(kLanguages[i], i == languageIndex)) {
                    cfg_.language = (i == 1) ? Language::English : Language::Russian;
                    MarkConfigDirty();
                }
            }
            ImGui::EndCombo();
        }
    }
    EndCard();

    // ---- Monitor card
    ImGui::Spacing();
    BeginCard("##card_monitor");
    {
        SectionLabel(Tr(Str::Monitor));
        ImGui::Spacing();

        const std::string currentMonitor = monitors_.empty()
            ? std::string("-")
            : monitors_[static_cast<size_t>(activeMonitor_)].label;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##monitor", currentMonitor.c_str())) {
            for (int i = 0; i < static_cast<int>(monitors_.size()); ++i) {
                const MonitorInfo& m = monitors_[static_cast<size_t>(i)];
                if (ImGui::Selectable(m.label.c_str(), i == activeMonitor_) && i != activeMonitor_) {
                    cfg_.monitor = m.name;
                    MarkConfigDirty();
                    needRebuild_ = true;
                    nextRebuildAt_ = 0;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();
        if (GhostButton(Tr(Str::Refresh))) {
            needRebuild_ = true;
            nextRebuildAt_ = 0;
        }
    }
    EndCard();

    // ---- Interface card
    ImGui::Spacing();
    BeginCard("##card_ui");
    {
        SectionLabel(Tr(Str::UiScale));
        ImGui::Spacing();

        float scale = cfg_.uiScale;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderFloat("##scale", &scale, 0.75f, 3.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
            cfg_.uiScale = scale;
            MarkConfigDirty();
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) fontsDirty_ = true;

        ImGui::Spacing();
        if (ImGui::Checkbox(Tr(Str::Dither), &cfg_.dither)) MarkParamsEdited();

        bool autostart = autostart_;
        if (ImGui::Checkbox(Tr(Str::Autostart), &autostart)) {
            if (util::SetAutostartEnabled(autostart)) autostart_ = autostart;
        }
    }
    EndCard();

    // ---- Diagnostics card
    ImGui::Spacing();
    BeginCard("##card_diag");
    {
        SectionLabel(Tr(Str::Diagnostics));
        ImGui::Spacing();

        if (capture_.Valid()) {
            ImGui::Text("%s: %ux%u", Tr(Str::CaptureInfo), capture_.Width(), capture_.Height());
        }
        else {
            ImGui::Text("%s: -", Tr(Str::CaptureInfo));
        }
        ImGui::Text("%s: %.0f", Tr(Str::Fps), fps_);

        if (!captureError_.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, kWarning);
            ImGui::TextWrapped("%s", captureError_.c_str());
            ImGui::PopStyleColor();
        }
        if (!statusError_.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, kDanger);
            ImGui::TextWrapped("%s", statusError_.c_str());
            ImGui::PopStyleColor();
        }
    }
    EndCard();

    // ---- Actions
    ImGui::Spacing();
    if (GhostButton(Tr(Str::HidePanel))) ShowPanel(false);
    ImGui::SameLine();
    if (DangerButton(Tr(Str::Exit))) PostMessageW(ctl_, WM_CLOSE, 0, 0);
}