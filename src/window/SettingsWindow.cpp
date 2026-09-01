#include "window/SettingsWindow.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>

#include <algorithm>

#include "core/Logger.h"
#include "core/StringConvert.h"

namespace sveta::window {

namespace {

constexpr wchar_t kWindowClassName[] = L"SvetaAssistantSettingsWindow";
constexpr wchar_t kFontFamilyName[] = L"Segoe UI";

// Dark, "modern tech panel" palette (the ask was explicitly "UEFI, not
// BIOS") -- a near-black purple-navy background, a slightly lighter field
// color for inputs, and one accent violet used for headers/focus/the
// primary action button.
constexpr COLORREF kColorBackground = RGB(0x17, 0x17, 0x20);
constexpr COLORREF kColorFieldBg = RGB(0x23, 0x23, 0x30);
constexpr COLORREF kColorText = RGB(0xEC, 0xEC, 0xF3);
constexpr COLORREF kColorAccent = RGB(0x9B, 0x87, 0xFF);
constexpr COLORREF kColorAccentText = RGB(0xFF, 0xFF, 0xFF);

// Raw DWM attribute IDs (not pulled from dwmapi.h's enum, which gates
// Windows-11-era values behind an NTDDI_VERSION this project doesn't
// target) -- these numeric IDs are stable regardless of SDK version.
constexpr DWORD kDwmUseImmersiveDarkMode = 20;
constexpr DWORD kDwmWindowCornerPreference = 33;
constexpr DWORD kDwmwcpRound = 2;

constexpr int kWindowWidth = 480;
constexpr int kMargin = 20;
constexpr int kLabelWidth = 140;
constexpr int kFieldX = kMargin + kLabelWidth + 12;
constexpr int kFieldWidth = kWindowWidth - kFieldX - kMargin;
constexpr int kRowHeight = 22;
constexpr int kControlHeight = 22;
constexpr int kRowSpacing = 8;
constexpr int kSectionSpacing = 20;
constexpr int kSectionHeaderGap = 8;

constexpr int kIdProviderGoogle = 101;
constexpr int kIdProviderSapi = 102;
constexpr int kIdResetPosition = 103;
constexpr int kIdSave = 104;
constexpr int kIdCancel = 105;

enum class StringId {
    Title,
    SectionPersonalization,
    LabelUserName,
    LabelRelationship,
    SectionLanguage,
    LabelPrimaryLanguage,
    LabelUiLanguage,
    SectionVoice,
    LabelProvider,
    RadioGoogle,
    RadioSapi,
    LabelVolume,
    SectionLanguageVoices,
    SectionAutomation,
    CheckProactive,
    CheckGameDetection,
    SectionCharacter,
    ButtonResetPosition,
    ButtonSave,
    ButtonCancel,
    LangKorean,
    LangEnglish,
    LangJapanese,
    LangChinese,
    LangSpanish,
    LangGerman,
    LangRussian,
    Count,
};

// [StringId][0]=Korean, [1]=English. Scoped to just this window -- the
// rest of the app (tray menu, chat bubble) stays Korean-only for now.
constexpr const wchar_t* kStrings[static_cast<size_t>(StringId::Count)][2] = {
    {L"Sveta 설정", L"Sveta Settings"},
    {L"개인화", L"Personalization"},
    {L"Sveta가 부를 이름", L"What Sveta calls you"},
    {L"어떻게 대해줬으면", L"How she should treat you"},
    {L"언어", L"Language"},
    {L"Sveta 주 언어", L"Sveta's main language"},
    {L"UI 언어", L"UI language"},
    {L"음성", L"Voice"},
    {L"제공자", L"Provider"},
    {L"Google Cloud TTS (Chirp 3: HD)", L"Google Cloud TTS (Chirp 3: HD)"},
    {L"Windows 기본 (SAPI)", L"Windows default (SAPI)"},
    {L"볼륨", L"Volume"},
    {L"언어별 음성 (Chirp 3: HD)", L"Per-language voice (Chirp 3: HD)"},
    {L"자동 동작", L"Automatic behavior"},
    {L"프로액티브 음성 (같은 오류 반복 시 먼저 말 걸기)", L"Proactive speech (speaks up on repeated errors)"},
    {L"게임 감지 (플레이 중인 게임 인식)", L"Game detection (recognizes active games)"},
    {L"캐릭터", L"Character"},
    {L"위치 초기화", L"Reset position"},
    {L"저장", L"Save"},
    {L"취소", L"Cancel"},
    {L"한국어", L"Korean"},
    {L"영어", L"English"},
    {L"일본어", L"Japanese"},
    {L"중국어", L"Chinese"},
    {L"스페인어", L"Spanish"},
    {L"독일어", L"German"},
    {L"러시아어", L"Russian"},
};

const wchar_t* Str(StringId id, bool english) {
    return kStrings[static_cast<size_t>(id)][english ? 1 : 0];
}

// language display name (matches tts_config.json's "google_voices" keys /
// audio::ToString(Language)) paired with its Cloud TTS locale code
// (audio::GoogleLanguageCode) and the StringId for its localized label --
// duplicated here rather than pulling in audio/LanguageDetection.h since
// this window only needs the string pairs.
struct LanguageInfo {
    const wchar_t* canonicalName; // stored value; matches audio::ToString(Language)
    const wchar_t* localeCode;
    StringId label;
};
constexpr LanguageInfo kLanguages[] = {
    {L"Korean", L"ko-KR", StringId::LangKorean},     {L"English", L"en-US", StringId::LangEnglish},
    {L"Japanese", L"ja-JP", StringId::LangJapanese}, {L"Chinese", L"cmn-CN", StringId::LangChinese},
    {L"Spanish", L"es-ES", StringId::LangSpanish},   {L"German", L"de-DE", StringId::LangGerman},
    {L"Russian", L"ru-RU", StringId::LangRussian},
};

// The 30 Chirp 3: HD voice personas, confirmed against Cloud TTS's live
// voices.list endpoint (GET .../v1/voices?languageCode=ko-KR) -- the same
// persona names are shared across every locale, only the locale prefix
// changes (e.g. ko-KR-Chirp3-HD-Leda / en-US-Chirp3-HD-Leda).
struct VoicePersona {
    const wchar_t* name;
    bool female;
};
constexpr VoicePersona kVoicePersonas[] = {
    {L"Achernar", true},    {L"Achird", false},     {L"Algenib", false},   {L"Algieba", false},
    {L"Alnilam", false},    {L"Aoede", true},       {L"Autonoe", true},    {L"Callirrhoe", true},
    {L"Charon", false},     {L"Despina", true},     {L"Enceladus", false}, {L"Erinome", true},
    {L"Fenrir", false},     {L"Gacrux", true},      {L"Iapetus", false},   {L"Kore", true},
    {L"Laomedeia", true},   {L"Leda", true},        {L"Orus", false},      {L"Puck", false},
    {L"Pulcherrima", true}, {L"Rasalgethi", false}, {L"Sadachbia", false}, {L"Sadaltager", false},
    {L"Schedar", false},    {L"Sulafat", true},     {L"Umbriel", false},   {L"Vindemiatrix", true},
    {L"Zephyr", true},      {L"Zubenelgenubi", false},
};

} // namespace

std::unique_ptr<SettingsWindow> SettingsWindow::Create(
    HINSTANCE instance, SaveCallback onSave, ResetPositionCallback onResetPosition) {
    INITCOMMONCONTROLSEX icc{sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES};
    InitCommonControlsEx(&icc); // registers the trackbar (volume slider) window class

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = &SettingsWindow::WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(kColorBackground);
    wc.lpszClassName = kWindowClassName;
    RegisterClassExW(&wc); // ignore failure: benign if already registered

    // Placeholder size -- BuildControls lays out the real content and the
    // window is resized to fit exactly once that's known (see below), so
    // there's no need to hand-calculate a height here.
    const HWND hwnd = CreateWindowExW(
        0, kWindowClassName, L"Sveta 설정", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, kWindowWidth, 400, nullptr, nullptr, instance, nullptr);
    if (!hwnd) {
        core::Logger::Error("Failed to create settings window");
        return nullptr;
    }

    auto window = std::unique_ptr<SettingsWindow>(new SettingsWindow(hwnd));
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window.get()));
    window->onSave_ = std::move(onSave);
    window->onResetPosition_ = std::move(onResetPosition);
    window->ApplyDarkChrome();
    window->BuildControls(instance);
    window->RelabelForCurrentUiLanguage();

    return window;
}

SettingsWindow::SettingsWindow(HWND hwnd) : hwnd_(hwnd) {}

SettingsWindow::~SettingsWindow() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
    }
    if (font_) {
        DeleteObject(font_);
    }
    if (boldFont_) {
        DeleteObject(boldFont_);
    }
    if (backgroundBrush_) {
        DeleteObject(backgroundBrush_);
    }
    if (fieldBrush_) {
        DeleteObject(fieldBrush_);
    }
}

void SettingsWindow::ApplyDarkChrome() {
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd_, kDwmUseImmersiveDarkMode, &dark, sizeof(dark));
    const DWORD corner = kDwmwcpRound;
    DwmSetWindowAttribute(hwnd_, kDwmWindowCornerPreference, &corner, sizeof(corner));
}

void SettingsWindow::BuildControls(HINSTANCE instance) {
    font_ = CreateFontW(
        -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, kFontFamilyName);
    boldFont_ = CreateFontW(
        -16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, kFontFamilyName);
    backgroundBrush_ = CreateSolidBrush(kColorBackground);
    fieldBrush_ = CreateSolidBrush(kColorFieldBg);

    int y = 20;

    const auto addRaw = [&](const wchar_t* className, const wchar_t* text, DWORD style, int x, int yPos, int width,
                             int controlHeight, int id, HFONT withFont) {
        const HWND control = CreateWindowExW(
            0, className, text, WS_CHILD | WS_VISIBLE | style, x, yPos, width, controlHeight, hwnd_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(withFont), TRUE);
        return control;
    };
    const auto addLabel = [&](int x, int yPos, int width, StringId id, bool bold) {
        const HWND h = addRaw(L"STATIC", L"", 0, x, yPos, width, kRowHeight, 0, bold ? boldFont_ : font_);
        relabelable_.push_back({h, static_cast<int>(id)});
        return h;
    };
    const auto addControl = [&](const wchar_t* className, StringId id, DWORD style, int x, int width,
                                 int controlHeight, int ctrlId) {
        const HWND h = addRaw(className, L"", style, x, y - 1, width, controlHeight, ctrlId, font_);
        relabelable_.push_back({h, static_cast<int>(id)});
        return h;
    };
    const auto sectionHeader = [&](StringId id) {
        addLabel(kMargin, y, 300, id, true);
        y += kRowHeight + kSectionHeaderGap;
    };
    const auto darkCombo = [&](HWND combo) { SetWindowTheme(combo, L"DarkMode_CFD", nullptr); };
    const auto darkTrackbar = [&](HWND trackbar) { SetWindowTheme(trackbar, L"DarkMode_Explorer", nullptr); };
    // Themed (visual-styles) checkboxes/radio buttons ignore WM_CTLCOLORBTN's
    // text color entirely and always draw black label text -- unreadable on
    // a dark background. An empty theme name reverts just this control to
    // classic (unthemed) drawing, which does respect it.
    const auto darkCheckable = [&](HWND control) { SetWindowTheme(control, L"", L""); };

    // 개인화
    sectionHeader(StringId::SectionPersonalization);
    addLabel(kMargin, y, kLabelWidth, StringId::LabelUserName, false);
    userNameEdit_ = addRaw(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, kFieldX, y - 1, kFieldWidth, kControlHeight, 0,
                            font_);
    y += kRowHeight + kRowSpacing;

    addLabel(kMargin, y, kLabelWidth, StringId::LabelRelationship, false);
    constexpr int kRelationshipNoteHeight = 50;
    relationshipNoteEdit_ = addRaw(
        L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL | ES_MULTILINE, kFieldX, y - 1, kFieldWidth, kRelationshipNoteHeight,
        0, font_);
    y += kRelationshipNoteHeight + kSectionSpacing;

    // 언어
    sectionHeader(StringId::SectionLanguage);
    addLabel(kMargin, y, kLabelWidth, StringId::LabelPrimaryLanguage, false);
    primaryLanguageCombo_ = addRaw(
        L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, kFieldX, y - 1, kFieldWidth,
        kControlHeight * 8, 0, font_);
    // Items are populated by RelabelForCurrentUiLanguage() (called right
    // after BuildControls, and again on every UI-language change) rather
    // than here, since their display text is localized.
    darkCombo(primaryLanguageCombo_);
    y += kRowHeight + 4;

    addLabel(kMargin, y, kLabelWidth, StringId::LabelUiLanguage, false);
    uiLanguageCombo_ = addRaw(
        L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, kFieldX, y - 1, kFieldWidth, kControlHeight * 3,
        0, font_);
    SendMessageW(uiLanguageCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"한국어"));
    SendMessageW(uiLanguageCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
    darkCombo(uiLanguageCombo_);
    y += kRowHeight + kSectionSpacing;

    // 음성
    sectionHeader(StringId::SectionVoice);
    addLabel(kMargin, y, kLabelWidth, StringId::LabelProvider, false);
    providerGoogleRadio_ =
        addControl(L"BUTTON", StringId::RadioGoogle, WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP, kFieldX,
                    kFieldWidth, 20, kIdProviderGoogle);
    darkCheckable(providerGoogleRadio_);
    y += kRowHeight + 4;
    providerSapiRadio_ = addControl(
        L"BUTTON", StringId::RadioSapi, WS_TABSTOP | BS_AUTORADIOBUTTON, kFieldX, kFieldWidth, 20, kIdProviderSapi);
    darkCheckable(providerSapiRadio_);
    y += kRowHeight + kRowSpacing;

    addLabel(kMargin, y, kLabelWidth, StringId::LabelVolume, false);
    constexpr int kVolumeLabelWidth = 46;
    volumeTrackbar_ = CreateWindowExW(
        0, TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, kFieldX, y - 1,
        kFieldWidth - kVolumeLabelWidth - 8, 26, hwnd_, nullptr, instance, nullptr);
    SendMessageW(volumeTrackbar_, TBM_SETRANGE, TRUE, MAKELONG(1, 100));
    SendMessageW(volumeTrackbar_, TBM_SETPAGESIZE, 0, 10);
    darkTrackbar(volumeTrackbar_);
    volumeValueLabel_ = addRaw(
        L"STATIC", L"100%", 0, kFieldX + kFieldWidth - kVolumeLabelWidth, y + 2, kVolumeLabelWidth, kRowHeight, 0,
        font_);
    y += 26 + kSectionSpacing;

    // 언어별 음성
    sectionHeader(StringId::SectionLanguageVoices);
    for (const LanguageInfo& language : kLanguages) {
        addLabel(kMargin + 8, y, kLabelWidth - 8, language.label, false);
        const HWND combo = addRaw(
            L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, kFieldX, y - 1, kFieldWidth,
            kControlHeight * 8, 0, font_);
        for (const VoicePersona& persona : kVoicePersonas) {
            const std::wstring item = std::wstring(persona.name) + (persona.female ? L" ♀" : L" ♂");
            SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
        }
        darkCombo(combo);
        languageRows_.push_back(
            {core::WideToUtf8(language.canonicalName), core::WideToUtf8(language.localeCode), combo});
        y += kRowHeight + 4;
    }
    y += kSectionSpacing - 4;

    // 자동 동작
    sectionHeader(StringId::SectionAutomation);
    proactiveCheckbox_ = addControl(
        L"BUTTON", StringId::CheckProactive, WS_TABSTOP | BS_AUTOCHECKBOX, kMargin, kWindowWidth - kMargin * 2, 20,
        0);
    darkCheckable(proactiveCheckbox_);
    y += kRowHeight + 4;
    gameDetectionCheckbox_ = addControl(
        L"BUTTON", StringId::CheckGameDetection, WS_TABSTOP | BS_AUTOCHECKBOX, kMargin, kWindowWidth - kMargin * 2,
        20, 0);
    darkCheckable(gameDetectionCheckbox_);
    y += kRowHeight + kSectionSpacing;

    // 캐릭터
    sectionHeader(StringId::SectionCharacter);
    addControl(L"BUTTON", StringId::ButtonResetPosition, WS_TABSTOP | BS_OWNERDRAW, kMargin, 170, 30,
               kIdResetPosition);
    y += 30 + kSectionSpacing;

    // 저장 / 취소
    addControl(L"BUTTON", StringId::ButtonSave, WS_TABSTOP | BS_OWNERDRAW, kWindowWidth - 210, 90, 32, kIdSave);
    addControl(L"BUTTON", StringId::ButtonCancel, WS_TABSTOP | BS_OWNERDRAW, kWindowWidth - 110, 90, 32, kIdCancel);
    y += 32 + 20;

    // Resize the window to fit exactly what was just laid out, and center
    // it on screen instead of leaving it at CW_USEDEFAULT's placement.
    RECT windowRect{0, 0, kWindowWidth, y};
    AdjustWindowRectEx(&windowRect, static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_STYLE)), FALSE, 0);
    const int windowWidth = windowRect.right - windowRect.left;
    const int windowHeight = windowRect.bottom - windowRect.top;
    const int screenX = (GetSystemMetrics(SM_CXSCREEN) - windowWidth) / 2;
    const int screenY = (GetSystemMetrics(SM_CYSCREEN) - windowHeight) / 2;
    SetWindowPos(hwnd_, nullptr, screenX, screenY, windowWidth, windowHeight, SWP_NOZORDER);
}

void SettingsWindow::RelabelForCurrentUiLanguage() {
    SetWindowTextW(hwnd_, Str(StringId::Title, uiLanguageIsEnglish_));
    for (const auto& [control, id] : relabelable_) {
        SetWindowTextW(control, Str(static_cast<StringId>(id), uiLanguageIsEnglish_));
    }

    // The primary-language combo's items are language *names*, which are
    // themselves localized -- repopulate rather than SetWindowTextW,
    // preserving whichever language was selected.
    if (primaryLanguageCombo_) {
        const int selected = static_cast<int>(SendMessageW(primaryLanguageCombo_, CB_GETCURSEL, 0, 0));
        SendMessageW(primaryLanguageCombo_, CB_RESETCONTENT, 0, 0);
        for (const LanguageInfo& language : kLanguages) {
            SendMessageW(
                primaryLanguageCombo_, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(Str(language.label, uiLanguageIsEnglish_)));
        }
        SendMessageW(primaryLanguageCombo_, CB_SETCURSEL, selected < 0 ? 0 : selected, 0);
    }
}

void SettingsWindow::ApplyValuesToControls(const SettingsValues& values) {
    uiLanguageIsEnglish_ = values.uiLanguage == "en";
    SendMessageW(uiLanguageCombo_, CB_SETCURSEL, uiLanguageIsEnglish_ ? 1 : 0, 0);
    RelabelForCurrentUiLanguage();

    SetWindowTextW(userNameEdit_, core::Utf8ToWide(values.userName).c_str());
    SetWindowTextW(relationshipNoteEdit_, core::Utf8ToWide(values.relationshipNote).c_str());

    int primaryIndex = 0;
    for (size_t i = 0; i < std::size(kLanguages); ++i) {
        if (core::WideToUtf8(kLanguages[i].canonicalName) == values.primaryLanguage) {
            primaryIndex = static_cast<int>(i);
            break;
        }
    }
    SendMessageW(primaryLanguageCombo_, CB_SETCURSEL, static_cast<WPARAM>(primaryIndex), 0);

    const bool isGoogle = values.ttsProvider == "google";
    SendMessageW(providerGoogleRadio_, BM_SETCHECK, isGoogle ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(providerSapiRadio_, BM_SETCHECK, isGoogle ? BST_UNCHECKED : BST_CHECKED, 0);

    SendMessageW(volumeTrackbar_, TBM_SETPOS, TRUE, std::clamp(values.volumePercent, 1, 100));
    SetWindowTextW(volumeValueLabel_, (std::to_wstring(values.volumePercent) + L"%").c_str());

    for (auto& row : languageRows_) {
        const auto it = values.voicesByLanguage.find(row.language);
        int selectIndex = 0; // first persona as a harmless fallback if nothing matches
        if (it != values.voicesByLanguage.end()) {
            // "ko-KR-Chirp3-HD-Leda" -> "Leda": the persona name is
            // whatever follows the last '-'.
            const std::string& voiceId = it->second;
            const size_t lastDash = voiceId.find_last_of('-');
            const std::string personaName = lastDash == std::string::npos ? voiceId : voiceId.substr(lastDash + 1);
            for (size_t i = 0; i < std::size(kVoicePersonas); ++i) {
                if (core::WideToUtf8(kVoicePersonas[i].name) == personaName) {
                    selectIndex = static_cast<int>(i);
                    break;
                }
            }
        }
        SendMessageW(row.combo, CB_SETCURSEL, static_cast<WPARAM>(selectIndex), 0);
    }

    SendMessageW(proactiveCheckbox_, BM_SETCHECK, values.proactiveSpeechEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(gameDetectionCheckbox_, BM_SETCHECK, values.gameDetectionEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
}

SettingsValues SettingsWindow::ReadValuesFromControls() const {
    SettingsValues values;

    const auto readEditText = [](HWND edit) {
        const int len = GetWindowTextLengthW(edit);
        std::wstring text(static_cast<size_t>(len), L'\0');
        if (len > 0) {
            GetWindowTextW(edit, text.data(), len + 1);
        }
        return text;
    };
    values.userName = core::WideToUtf8(readEditText(userNameEdit_));
    values.relationshipNote = core::WideToUtf8(readEditText(relationshipNoteEdit_));
    values.uiLanguage = SendMessageW(uiLanguageCombo_, CB_GETCURSEL, 0, 0) == 1 ? "en" : "ko";

    const int primaryIndex = static_cast<int>(SendMessageW(primaryLanguageCombo_, CB_GETCURSEL, 0, 0));
    if (primaryIndex >= 0 && static_cast<size_t>(primaryIndex) < std::size(kLanguages)) {
        values.primaryLanguage = core::WideToUtf8(kLanguages[primaryIndex].canonicalName);
    }

    values.ttsProvider = SendMessageW(providerGoogleRadio_, BM_GETCHECK, 0, 0) == BST_CHECKED ? "google" : "sapi";
    values.volumePercent = static_cast<int>(SendMessageW(volumeTrackbar_, TBM_GETPOS, 0, 0));

    for (const auto& row : languageRows_) {
        const int selected = static_cast<int>(SendMessageW(row.combo, CB_GETCURSEL, 0, 0));
        if (selected >= 0 && static_cast<size_t>(selected) < std::size(kVoicePersonas)) {
            values.voicesByLanguage[row.language] =
                row.localeCode + "-Chirp3-HD-" + core::WideToUtf8(kVoicePersonas[selected].name);
        }
    }

    values.proactiveSpeechEnabled = SendMessageW(proactiveCheckbox_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    values.gameDetectionEnabled = SendMessageW(gameDetectionCheckbox_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    return values;
}

void SettingsWindow::HandleSaveClicked() {
    if (onSave_) {
        onSave_(ReadValuesFromControls());
    }
    ShowWindow(hwnd_, SW_HIDE);
}

void SettingsWindow::HandleVolumeSliderMoved() {
    const int pos = static_cast<int>(SendMessageW(volumeTrackbar_, TBM_GETPOS, 0, 0));
    SetWindowTextW(volumeValueLabel_, (std::to_wstring(pos) + L"%").c_str());
}

void SettingsWindow::DrawOwnerButton(LPDRAWITEMSTRUCT item) const {
    const bool primary = item->CtlID == kIdSave;
    const HDC hdc = item->hDC;
    const RECT rect = item->rcItem;

    const HBRUSH fillBrush = CreateSolidBrush(primary ? kColorAccent : kColorBackground);
    const HPEN borderPen = CreatePen(PS_SOLID, 1, kColorAccent);
    const HGDIOBJ oldBrush = SelectObject(hdc, fillBrush);
    const HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 8, 8);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(fillBrush);
    DeleteObject(borderPen);

    wchar_t text[64]{};
    GetWindowTextW(item->hwndItem, text, static_cast<int>(std::size(text)));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, primary ? kColorAccentText : kColorAccent);
    SelectObject(hdc, font_);
    RECT textRect = rect;
    DrawTextW(hdc, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void SettingsWindow::ShowWithValues(const SettingsValues& values) {
    ApplyValuesToControls(values);
    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
}

LRESULT CALLBACK SettingsWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) {
        return self->HandleMessage(message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT SettingsWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_COMMAND:
            if (HIWORD(wParam) == CBN_SELCHANGE && reinterpret_cast<HWND>(lParam) == uiLanguageCombo_) {
                uiLanguageIsEnglish_ = SendMessageW(uiLanguageCombo_, CB_GETCURSEL, 0, 0) == 1;
                RelabelForCurrentUiLanguage();
                return 0;
            }
            switch (LOWORD(wParam)) {
                case kIdSave:
                    HandleSaveClicked();
                    return 0;
                case kIdCancel:
                    ShowWindow(hwnd_, SW_HIDE);
                    return 0;
                case kIdResetPosition:
                    if (onResetPosition_) {
                        onResetPosition_();
                    }
                    return 0;
                default:
                    return 0;
            }
        case WM_HSCROLL:
            if (reinterpret_cast<HWND>(lParam) == volumeTrackbar_) {
                HandleVolumeSliderMoved();
            }
            return 0;
        case WM_DRAWITEM: {
            const LPDRAWITEMSTRUCT drawItem = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
            if (drawItem->CtlID == kIdSave || drawItem->CtlID == kIdCancel || drawItem->CtlID == kIdResetPosition) {
                DrawOwnerButton(drawItem);
                return TRUE;
            }
            return FALSE;
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            const HDC hdc = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdc, kColorText);
            SetBkColor(hdc, kColorFieldBg);
            return reinterpret_cast<LRESULT>(fieldBrush_);
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            const HDC hdc = reinterpret_cast<HDC>(wParam);
            const HWND control = reinterpret_cast<HWND>(lParam);
            const bool isHeader = std::any_of(relabelable_.begin(), relabelable_.end(), [&](const auto& entry) {
                return entry.first == control &&
                    (entry.second == static_cast<int>(StringId::SectionPersonalization) ||
                     entry.second == static_cast<int>(StringId::SectionLanguage) ||
                     entry.second == static_cast<int>(StringId::SectionVoice) ||
                     entry.second == static_cast<int>(StringId::SectionLanguageVoices) ||
                     entry.second == static_cast<int>(StringId::SectionAutomation) ||
                     entry.second == static_cast<int>(StringId::SectionCharacter));
            });
            SetTextColor(hdc, isHeader ? kColorAccent : kColorText);
            SetBkColor(hdc, kColorBackground);
            SetBkMode(hdc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(backgroundBrush_);
        }
        case WM_CLOSE:
            // Hide, don't destroy: this window is reused across opens (see
            // class comment), and the real teardown happens in ~SettingsWindow.
            ShowWindow(hwnd_, SW_HIDE);
            return 0;
        default:
            return DefWindowProcW(hwnd_, message, wParam, lParam);
    }
}

} // namespace sveta::window
