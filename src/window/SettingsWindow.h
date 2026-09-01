#pragma once

#include <windows.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sveta::window {

struct SettingsValues {
    std::string userName;         // what Sveta calls the user; see ai/UserProfile.h
    std::string relationshipNote; // how Sveta should treat the user; see ai/UserProfile.h
    std::string primaryLanguage = "Korean"; // Sveta's main conversational language
    std::string uiLanguage = "ko";          // this window's own display language: "ko" or "en"
    std::string ttsProvider = "google";     // "google" or "sapi"
    int volumePercent = 100;
    // Keyed by audio::ToString(Language), e.g. "Korean" -> voice name.
    std::unordered_map<std::string, std::string> voicesByLanguage;
    bool proactiveSpeechEnabled = true;
    bool gameDetectionEnabled = true;
};

// A dark-themed, owner-drawn settings window -- unlike the rest of the
// app's popups, this behaves like an ordinary window (title bar,
// draggable, closable) since it's a utility dialog, not part of the
// character's on-screen presence. Modeless: created once and reused via
// ShowWithValues() rather than recreated per open, so its messages are
// dispatched by MainWindow's ordinary GetMessageW loop like any other
// window owned by the thread.
class SettingsWindow {
public:
    using SaveCallback = std::function<void(const SettingsValues&)>;
    using ResetPositionCallback = std::function<void()>;

    static std::unique_ptr<SettingsWindow> Create(
        HINSTANCE instance, SaveCallback onSave, ResetPositionCallback onResetPosition);
    ~SettingsWindow();

    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;

    // Repopulates the controls from `values` and brings the window to the
    // foreground -- called every time the tray menu's "설정" item is
    // clicked, so it always reflects current state even if it changed
    // elsewhere since the window was last shown.
    void ShowWithValues(const SettingsValues& values);

private:
    explicit SettingsWindow(HWND hwnd);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void ApplyDarkChrome();
    void BuildControls(HINSTANCE instance);
    void ApplyValuesToControls(const SettingsValues& values);
    SettingsValues ReadValuesFromControls() const;
    void HandleSaveClicked();
    void HandleVolumeSliderMoved();
    void DrawOwnerButton(LPDRAWITEMSTRUCT item) const;

    // Re-applies every static/button label's text for the currently
    // selected UI language (see StringId in the .cpp) -- called both right
    // after building controls and live whenever the UI-language combo
    // changes, so the window never shows a stale mix of languages.
    void RelabelForCurrentUiLanguage();

    HWND hwnd_;
    HFONT font_ = nullptr;
    HFONT boldFont_ = nullptr;
    HBRUSH backgroundBrush_ = nullptr;
    HBRUSH fieldBrush_ = nullptr;

    // Index into the StringId enum this control displays, so
    // RelabelForCurrentUiLanguage() knows what to write into it. Only
    // holds label/button/checkbox/radio controls -- not edit boxes or
    // combo boxes, which hold user data rather than fixed labels.
    std::vector<std::pair<HWND, int>> relabelable_;
    bool uiLanguageIsEnglish_ = false;

    HWND userNameEdit_ = nullptr;
    HWND relationshipNoteEdit_ = nullptr;
    HWND primaryLanguageCombo_ = nullptr;
    HWND uiLanguageCombo_ = nullptr;
    HWND providerGoogleRadio_ = nullptr;
    HWND providerSapiRadio_ = nullptr;
    HWND volumeTrackbar_ = nullptr;
    HWND volumeValueLabel_ = nullptr;

    struct LanguageRow {
        std::string language;   // e.g. "Korean" -- audio::ToString(Language)
        std::string localeCode; // e.g. "ko-KR" -- audio::GoogleLanguageCode(Language)
        HWND combo = nullptr;   // CBS_DROPDOWNLIST of the 30 Chirp3 HD voice personas
    };
    std::vector<LanguageRow> languageRows_;

    HWND proactiveCheckbox_ = nullptr;
    HWND gameDetectionCheckbox_ = nullptr;

    SaveCallback onSave_;
    ResetPositionCallback onResetPosition_;
};

} // namespace sveta::window
