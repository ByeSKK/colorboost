#pragma once
#include "Settings.h"

// X-macro: a single table keeps the enum and both languages in sync.
#define CB_STRINGS(X)                                                                                          \
    X(TabColor, "Цвет", "Color")                                                                               \
    X(TabProfiles, "Профили", "Profiles")                                                                      \
    X(TabSettings, "Настройки", "Settings")                                                                    \
    X(EffectEnabled, "Эффект включён", "Effect enabled")                                                       \
    X(StatusActive, "Работает", "Running")                                                                     \
    X(StatusOff, "Выключено", "Off")                                                                           \
    X(StatusWaiting, "Ожидание приложения с профилем", "Waiting for an app with a profile")                    \
    X(StatusStarting, "Запуск захвата...", "Starting capture...")                                              \
    X(Status, "Статус", "Status")                                                                              \
    X(EditingGlobal, "Редактируются общие настройки", "Editing global settings")                               \
    X(EditingProfile, "Редактируется профиль", "Editing profile")                                              \
    X(Preset, "Пресет", "Preset")                                                                              \
    X(PresetCustom, "Свои настройки", "Custom")                                                                \
    X(SaveNew, "Сохранить как новый", "Save as new")                                                           \
    X(Overwrite, "Перезаписать", "Overwrite")                                                                  \
    X(Delete, "Удалить", "Delete")                                                                             \
    X(Adjustments, "Параметры", "Adjustments")                                                                 \
    X(Gamma, "Гамма", "Gamma")                                                                                 \
    X(Temperature, "Температура", "Temperature")                                                               \
    X(Shadows, "Тени", "Shadows")                                                                              \
    X(Saturation, "Насыщенность", "Saturation")                                                                \
    X(Vibrance, "Сочность (Vibrance)", "Vibrance")                                                             \
    X(Brightness, "Яркость", "Brightness")                                                                     \
    X(Contrast, "Контраст", "Contrast")                                                                        \
    X(Highlights, "Света", "Highlights")                                                                       \
    X(ResetParam, "Сбросить параметр", "Reset parameter")                                                      \
    X(ResetAll, "Сбросить всё", "Reset all")                                                                   \
    X(HotkeysTitle, "Горячие клавиши", "Hotkeys")                                                              \
    X(HkPanel, "Ctrl+Alt+F10 - показать/скрыть панель", "Ctrl+Alt+F10 - show/hide this panel")                 \
    X(HkEffect, "Ctrl+Alt+F9 - эффект вкл/выкл", "Ctrl+Alt+F9 - effect on/off")                                \
    X(HkNext, "Ctrl+Alt+F11 - следующий пресет", "Ctrl+Alt+F11 - next preset")                                 \
    X(HkReset, "Ctrl+Alt+F8 - сбросить настройки", "Ctrl+Alt+F8 - reset settings")                             \
    X(ProfilesHelp,                                                                                            \
      "Профиль хранит отдельные настройки для приложения и применяется, пока окно этого приложения активно. "   \
      "Запустите игру, откройте панель (Ctrl+Alt+F10) и создайте профиль.",                                    \
      "A profile keeps separate settings for an application and is applied while that app is in the "          \
      "foreground. Start the game, open this panel (Ctrl+Alt+F10) and create a profile.")                      \
    X(CurrentApp, "Активное приложение", "Foreground app")                                                     \
    X(NoApp, "не определено", "unknown")                                                                       \
    X(CreateProfile, "Создать профиль для этого приложения", "Create a profile for this app")                  \
    X(DeleteProfile, "Удалить профиль этого приложения", "Delete this app's profile")                          \
    X(OnlyProfiles, "Применять эффект только в приложениях с профилем", "Apply the effect only in apps with a profile") \
    X(ProfileList, "Список профилей", "Profiles")                                                              \
    X(NoProfiles, "Профилей пока нет", "No profiles yet")                                                      \
    X(Language, "Язык", "Language")                                                                            \
    X(Monitor, "Монитор", "Monitor")                                                                           \
    X(Refresh, "Обновить список", "Refresh list")                                                              \
    X(UiScale, "Масштаб интерфейса (применяется после отпускания)", "UI scale (applied on release)")           \
    X(Dither, "Дизеринг (убирает полосы на градиентах)", "Dithering (reduces banding)")                        \
    X(Autostart, "Запускать вместе с Windows", "Start with Windows")                                           \
    X(HidePanel, "Скрыть панель", "Hide panel")                                                                \
    X(Exit, "Выход", "Exit")                                                                                   \
    X(Diagnostics, "Диагностика", "Diagnostics")                                                               \
    X(CaptureInfo, "Захват", "Capture")                                                                        \
    X(Fps, "Кадров обработки в секунду", "Processed frames per second")                                        \
    X(HotkeyFail, "Не удалось занять горячую клавишу (она используется другим приложением)",                   \
      "Could not register a hotkey (it is used by another application)")                                       \
    X(TrayShow, "Показать/скрыть панель", "Show/hide panel")                                                   \
    X(TrayEffect, "Эффект включён", "Effect enabled")                                                          \
    X(TrayExit, "Выход", "Exit")                                                                               \
    X(NewPresetName, "Мой пресет", "My preset")

enum class Str {
#define CB_ENUM(id, ru, en) id,
    CB_STRINGS(CB_ENUM)
#undef CB_ENUM
    Count_
};

inline const char* Loc(Language language, Str id) {
    static const char* const kRu[] = {
#define CB_RU(id, ru, en) ru,
        CB_STRINGS(CB_RU)
#undef CB_RU
    };
    static const char* const kEn[] = {
#define CB_EN(id, ru, en) en,
        CB_STRINGS(CB_EN)
#undef CB_EN
    };
    const size_t index = static_cast<size_t>(id);
    return language == Language::English ? kEn[index] : kRu[index];
}