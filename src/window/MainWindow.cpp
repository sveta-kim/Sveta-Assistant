#include "window/MainWindow.h"

#include <windowsx.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <string>
#include <thread>

#include "ai/Persona.h"
#include "ai/UserProfile.h"
#include "audio/GoogleTtsConfig.h"
#include "audio/SpeakableText.h"
#include "audio/TextToSpeechFactory.h"
#include "context/LeagueLiveClient.h"
#include "context/PrivacyConfig.h"
#include "core/Logger.h"
#include "core/StringConvert.h"
#include "interaction/HeadHitbox.h"
#include "rendering/SpriteOverlay.h"
#include "window/WindowPosition.h"

namespace sveta::window {

namespace {
constexpr wchar_t kWindowClassName[] = L"SvetaAssistantWindowClass";
constexpr int kFallbackSize = 300;
constexpr int kScreenMargin = 40;
// On-screen size cap for character art; source art (e.g. 1254x1254) is
// downscaled to fit, never upscaled.
constexpr uint32_t kMaxCharacterDimension = 240;

constexpr UINT_PTR kCharacterTickTimerId = 1;
constexpr UINT kCharacterTickIntervalMs = 1000;

// Posted by the AI worker thread with an AiResponsePayload* in lParam.
constexpr UINT kAiResponseMessage = WM_APP + 1;
// Posted by SAPI itself (see ISpVoice::SetNotifyWindowMessage) when speech
// starts/ends; no payload, just a signal to call TextToSpeech::PumpEvents().
constexpr UINT kTtsEventMessage = WM_APP + 2;
// Posted by ContextEngine's background UI Automation read with a
// ContextSnapshot* in lParam.
constexpr UINT kContextSnapshotMessage = WM_APP + 3;
// Posted by Shell_NotifyIcon on tray icon mouse events; lParam carries the
// mouse message (WM_RBUTTONUP, WM_CONTEXTMENU, etc.), see TrayIcon.h.
constexpr UINT kTrayIconMessage = WM_APP + 4;

constexpr UINT_PTR kMenuIdTogglePause = 1;
constexpr UINT_PTR kMenuIdExit = 2;
constexpr UINT_PTR kMenuIdSettings = 3;

constexpr size_t kMaxHistoryMessages = 20;

constexpr UINT_PTR kMouthAnimationTimerId = 2;
constexpr UINT kMouthAnimationIntervalMs = 180;

// How long the response bubble lingers after real TTS playback actually
// finishes (see MainWindow::HandleTtsEvent), so it doesn't vanish the
// instant the voice stops.
constexpr int kPostSpeechGraceMs = 2500;

// Floor between proactive speeches (project plan section 18) so a
// still-broken build doesn't get commented on every single rerun.
// Placeholder pending real UX tuning, same as the idle-to-sleep timeout.
constexpr std::chrono::minutes kProactiveCooldown{10};

// TODO(Phase 9 - Item System / Content Platform): replace with the real
// character package loader (character.json -> assets/). SVETA_CONTENT_DIR
// points at the repo's content/ directory for local development only.
std::filesystem::path SpritePathForFile(const std::string& fileName) {
    return std::filesystem::path(SVETA_CONTENT_DIR) / L"characters" / L"sveta" / L"assets" / fileName;
}

std::filesystem::path SpritePathForEmotion(character::Emotion emotion) {
    return SpritePathForFile(std::string(character::SpriteFileName(emotion)));
}

POINT DefaultPosition(int width, int height) {
    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    return POINT{
        screenWidth - width - kScreenMargin,
        screenHeight - height - kScreenMargin,
    };
}

// A saved position can go stale if a monitor gets disconnected or the
// display arrangement changes since it was last saved (e.g. an old
// right-monitor position with nothing there anymore) -- restoring it
// blindly would put the whole window somewhere with no screen at all,
// making the app look like it silently failed to launch.
bool IsPositionOnAnyMonitor(POINT position, int width, int height) {
    RECT rect{position.x, position.y, position.x + width, position.y + height};
    return MonitorFromRect(&rect, MONITOR_DEFAULTTONULL) != nullptr;
}

} // namespace

std::unique_ptr<MainWindow> MainWindow::Create(HINSTANCE instance) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = &MainWindow::WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kWindowClassName;

    if (!RegisterClassExW(&wc)) {
        core::Logger::Error("Failed to register window class");
        return nullptr;
    }

    auto sprite = rendering::Sprite::LoadFromFile(SpritePathForEmotion(character::Emotion::Calm), kMaxCharacterDimension);
    if (!sprite) {
        core::Logger::Warn("No character sprite loaded; falling back to a blank placeholder window");
    }

    const int width = sprite ? static_cast<int>(sprite->Width()) : kFallbackSize;
    const int height = sprite ? static_cast<int>(sprite->Height()) : kFallbackSize;

    POINT position = DefaultPosition(width, height);
    if (const auto savedPosition = LoadWindowPosition();
        savedPosition && IsPositionOnAnyMonitor(*savedPosition, width, height)) {
        position = *savedPosition;
    } else if (savedPosition) {
        core::Logger::Warn("Saved window position is off every current monitor; using the default position instead");
    }

    // WS_POPUP: borderless. WS_EX_LAYERED: per-pixel transparency support.
    // WS_EX_TOPMOST: always on top. WS_EX_TOOLWINDOW: hide from taskbar/alt-tab.
    const HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kWindowClassName,
        L"Sveta Assistant",
        WS_POPUP,
        position.x, position.y,
        width, height,
        nullptr, nullptr, instance, nullptr);

    if (!hwnd) {
        core::Logger::Error("Failed to create main window");
        return nullptr;
    }

    auto window = std::unique_ptr<MainWindow>(new MainWindow(hwnd, std::move(sprite)));
    window->ApplySpriteToWindow();

    window->chatBubble_ = ChatBubble::Create(instance);
    window->aiConfig_ = ai::AiConfig::Load();
    if (!window->aiConfig_ || !window->aiConfig_->IsUsable()) {
        core::Logger::Warn("AI chat is not configured yet; double-click will show a placeholder reply");
    }
    {
        const ai::UserProfile profile = ai::UserProfile::Load();
        window->userName_ = profile.userName;
        window->relationshipNote_ = profile.relationshipNote;
        window->primaryLanguage_ = profile.primaryLanguage;
    }

    window->textToSpeech_ = audio::CreateTextToSpeech(hwnd, kTtsEventMessage);
    if (!window->textToSpeech_) {
        core::Logger::Warn("Text-to-speech unavailable; replies will be text-only");
    }

    window->contextEngine_ = context::ContextEngine::Create(hwnd, kContextSnapshotMessage);
    if (!window->contextEngine_) {
        core::Logger::Warn("Desktop awareness unavailable; chat won't know what's on screen");
    }

    window->memoryEngine_ = memory::MemoryEngine::Create();
    window->memoryEngine_->RecordAppStart();

    window->trayIcon_ = TrayIcon::Create(
        hwnd, kTrayIconMessage, std::filesystem::path(SVETA_CONTENT_DIR) / L"face.png", L"Sveta Assistant");

    MainWindow* windowPtr = window.get();
    window->settingsWindow_ = SettingsWindow::Create(
        instance, [windowPtr](const SettingsValues& values) { windowPtr->OnSettingsSaved(values); },
        [windowPtr]() { windowPtr->ResetCharacterPosition(); });

    ShowWindow(hwnd, SW_SHOW);
    SetTimer(hwnd, kCharacterTickTimerId, kCharacterTickIntervalMs, nullptr);

    core::Logger::Info("Main window created");
    return window;
}

MainWindow::MainWindow(HWND hwnd, std::optional<rendering::Sprite> sprite)
    : hwnd_(hwnd), sprite_(std::move(sprite)) {
    if (sprite_) {
        headHitbox_ = interaction::ComputeHeadHitbox(sprite_->Width(), sprite_->Height());
    }
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
}

MainWindow::~MainWindow() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
    }
}

void MainWindow::ApplySpriteToWindow() {
    if (!sprite_) {
        // No PNG available yet: fall back to a fully opaque rectangle so the
        // window is at least visible during early development.
        SetLayeredWindowAttributes(hwnd_, 0, 255, LWA_ALPHA);
        return;
    }
    ApplyPixelsToWindow(sprite_->PremultipliedBgra(), sprite_->Width(), sprite_->Height());
}

void MainWindow::ApplyPixelsToWindow(const std::vector<uint8_t>& pixels, uint32_t width, uint32_t height) {
    RECT windowRect{};
    GetWindowRect(hwnd_, &windowRect);

    const HDC screenDc = GetDC(nullptr);
    const HDC memDc = CreateCompatibleDC(screenDc);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(width);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(height); // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    const HBITMAP dib = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (dib && bits) {
        memcpy(bits, pixels.data(), pixels.size());

        const HGDIOBJ oldBitmap = SelectObject(memDc, dib);

        POINT srcPoint{0, 0};
        POINT dstPoint{windowRect.left, windowRect.top};
        SIZE size{static_cast<LONG>(width), static_cast<LONG>(height)};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

        if (!UpdateLayeredWindow(hwnd_, screenDc, &dstPoint, &size, memDc, &srcPoint, 0, &blend, ULW_ALPHA)) {
            core::Logger::Error(std::format("UpdateLayeredWindow failed (error={})", GetLastError()));
        }

        SelectObject(memDc, oldBitmap);
        DeleteObject(dib);
    } else {
        core::Logger::Error("Failed to create DIB section for sprite");
    }

    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);
}

void MainWindow::ReloadSprite(const std::string& fileName) {
    auto sprite = rendering::Sprite::LoadFromFile(SpritePathForFile(fileName), kMaxCharacterDimension);
    if (!sprite) {
        core::Logger::Warn(std::format("No sprite file '{}'; falling back to calm", fileName));
        sprite = rendering::Sprite::LoadFromFile(SpritePathForEmotion(character::Emotion::Calm), kMaxCharacterDimension);
    }
    if (!sprite) {
        return; // keep whatever is currently displayed
    }

    sprite_ = std::move(sprite);
    headHitbox_ = interaction::ComputeHeadHitbox(sprite_->Width(), sprite_->Height());
    ApplySpriteToWindow();
}

void MainWindow::SyncSpriteToEmotion() {
    const character::Emotion emotion = characterState_.CurrentEmotion();
    // PlayingGame has its own dedicated art (a gamepad overlay) that takes
    // priority over whatever emotion-based sprite would otherwise apply.
    const bool isPlayingGame = characterState_.CurrentAction() == character::Action::PlayingGame;
    if (emotion == lastAppliedEmotion_ && isPlayingGame == lastAppliedIsPlayingGame_) {
        return;
    }
    lastAppliedEmotion_ = emotion;
    lastAppliedIsPlayingGame_ = isPlayingGame;
    ReloadSprite(isPlayingGame ? "playing_game.png" : std::string(character::SpriteFileName(emotion)));
}

void MainWindow::SaveCurrentPosition() {
    RECT rect{};
    if (GetWindowRect(hwnd_, &rect)) {
        SaveWindowPosition(POINT{rect.left, rect.top});
    }
}

void MainWindow::HandleMouseMove(LPARAM lParam) {
    const auto now = std::chrono::steady_clock::now();

    if (!isHovering_) {
        isHovering_ = true;
        core::Logger::Info("CharacterHovered");
        characterState_.OnHoverStart(now);

        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd_;
        TrackMouseEvent(&tme);
    }

    const POINT localPos{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    if (PtInRect(&headHitbox_, localPos)) {
        if (pettingDetector_.OnCursorMove(localPos, now)) {
            core::Logger::Info("CharacterPetted");
            characterState_.OnPetted(now);
        }
    } else {
        pettingDetector_.Reset();
    }

    SyncSpriteToEmotion();
}

void MainWindow::HandleMouseLeave() {
    isHovering_ = false;
    pettingDetector_.Reset();
    characterState_.OnHoverEnd();
    core::Logger::Info("Character hover ended");
    SyncSpriteToEmotion();
}

void MainWindow::HandleTick() {
    const auto now = std::chrono::steady_clock::now();
    if (contextEngine_) {
        characterState_.SetGamingContext(contextEngine_->IsGaming(), now);

        if (now - lastProactiveSpeechTime_ >= kProactiveCooldown) {
            if (const auto situation = contextEngine_->ConsumeSameErrorRepeatedEvent()) {
                lastProactiveSpeechTime_ = now;
                StartProactiveSpeech(*situation);
                if (memoryEngine_) {
                    memoryEngine_->RecordSameErrorRepeated();
                }
            }
        }
    }
    characterState_.Tick(now, isHovering_);
    SyncSpriteToEmotion();

    if (memoryEngine_ && contextEngine_) {
        memoryEngine_->Tick(
            contextEngine_->IsGaming(), contextEngine_->CurrentProcessNameForMemory(),
            characterState_.CurrentAction() == character::Action::Sleeping);
    }
}

POINT MainWindow::ComputeBubbleAnchor() const {
    RECT rect{};
    GetWindowRect(hwnd_, &rect);
    return POINT{(rect.left + rect.right) / 2, rect.top};
}

void MainWindow::StartChat() {
    if (!chatBubble_ || conversationInFlight_ || chatBubble_->IsVisible()) {
        return;
    }

    chatBubble_->OpenForInput(
        ComputeBubbleAnchor(),
        [this](const std::wstring& message) { OnMessageSubmitted(message); },
        [this]() { OnChatDismissed(); });
}

void MainWindow::OnMessageSubmitted(const std::wstring& message) {
    // A new message pre-empts whatever the previous reply was doing —
    // stop it being read aloud and don't let a stale "speech ended" event
    // reschedule dismissal on the bubble we're about to repurpose.
    awaitingSpeechEndForDismiss_ = false;
    if (textToSpeech_) {
        textToSpeech_->Stop();
    }
    if (isSpeaking_) {
        isSpeaking_ = false;
        KillTimer(hwnd_, kMouthAnimationTimerId);
        ApplySpriteToWindow();
    }

    const auto now = std::chrono::steady_clock::now();
    characterState_.OnConversationStart(now);
    SyncSpriteToEmotion();

    conversationHistory_.push_back({"user", core::WideToUtf8(message)});

    characterState_.OnThinking();
    SyncSpriteToEmotion();
    chatBubble_->ShowThinking(ComputeBubbleAnchor());

    if (!aiConfig_ || !aiConfig_->IsUsable()) {
        // Still flow through Thinking -> Talking so the UI is consistent,
        // just without a real network round trip.
        AiResponsePayload payload{
            false,
            L"(AI가 아직 설정되지 않았어요 — config/ai_config.json, config/secrets.local.json을 확인해주세요)"};
        OnAiResponse(payload);
        return;
    }

    std::vector<ai::ChatMessage> requestHistory;
    requestHistory.push_back({"system", ai::BuildSystemPrompt(characterState_.GetPersonality(), userName_, relationshipNote_, primaryLanguage_)});
    if (contextEngine_) {
        const std::wstring contextLine = contextEngine_->BuildContextLine();
        if (!contextLine.empty()) {
            requestHistory.push_back({"system", core::WideToUtf8(contextLine)});
        }
    }
    if (memoryEngine_) {
        const std::string memoryLine = memoryEngine_->BuildMemoryDigestLine();
        if (!memoryLine.empty()) {
            requestHistory.push_back({"system", memoryLine});
        }
    }
    requestHistory.insert(requestHistory.end(), conversationHistory_.begin(), conversationHistory_.end());
    SendChatRequestAsync(std::move(requestHistory));
}

void MainWindow::SendChatRequestAsync(std::vector<ai::ChatMessage> requestHistory) {
    conversationInFlight_ = true;

    const ai::AiConfig config = *aiConfig_;
    const HWND hwnd = hwnd_;

    std::thread([config, requestHistory = std::move(requestHistory), hwnd]() mutable {
        // Safe to always attempt: FetchLeagueLiveMatchState itself checks
        // the foreground process before touching the network, so this is
        // a cheap no-op for every message except while actually in a
        // League of Legends match.
        if (const auto leagueState = context::FetchLeagueLiveMatchState()) {
            requestHistory.insert(
                requestHistory.begin() + 1,
                ai::ChatMessage{"system", core::WideToUtf8(context::BuildLeagueContextLine(*leagueState))});
        }

        const ai::ChatClient client(config);
        const ai::ChatResult result = client.Send(requestHistory);

        auto payload = std::make_unique<AiResponsePayload>();
        payload->success = result.success;
        payload->text = core::Utf8ToWide(result.text);

        PostMessageW(hwnd, kAiResponseMessage, 0, reinterpret_cast<LPARAM>(payload.release()));
    }).detach();
}

void MainWindow::StartProactiveSpeech(const std::wstring& situationDescription) {
    // Same guards as StartChat, plus: no point speaking up about the
    // screen if the AI isn't even configured.
    if (!chatBubble_ || conversationInFlight_ || chatBubble_->IsVisible() || !aiConfig_ || !aiConfig_->IsUsable()) {
        return;
    }

    core::Logger::Info("Proactive speech triggered (same error repeated)");

    const auto now = std::chrono::steady_clock::now();
    characterState_.OnConversationStart(now);
    SyncSpriteToEmotion();

    characterState_.OnThinking();
    SyncSpriteToEmotion();
    chatBubble_->ShowThinking(ComputeBubbleAnchor());

    std::vector<ai::ChatMessage> requestHistory;
    requestHistory.push_back({"system", ai::BuildSystemPrompt(characterState_.GetPersonality(), userName_, relationshipNote_, primaryLanguage_)});
    requestHistory.push_back({"system", core::WideToUtf8(situationDescription)});
    if (memoryEngine_) {
        const std::string memoryLine = memoryEngine_->BuildMemoryDigestLine();
        if (!memoryLine.empty()) {
            requestHistory.push_back({"system", memoryLine});
        }
    }
    requestHistory.insert(requestHistory.end(), conversationHistory_.begin(), conversationHistory_.end());
    SendChatRequestAsync(std::move(requestHistory));
}

void MainWindow::OnAiResponse(const AiResponsePayload& payload) {
    conversationInFlight_ = false;

    characterState_.OnTalking(std::chrono::steady_clock::now());
    SyncSpriteToEmotion();

    bool willSpeak = false;
    if (payload.success) {
        conversationHistory_.push_back({"assistant", core::WideToUtf8(payload.text)});
        // Cap history length; Phase 8 (Memory System) replaces this with
        // real session/long-term memory.
        if (conversationHistory_.size() > kMaxHistoryMessages) {
            conversationHistory_.erase(
                conversationHistory_.begin(),
                conversationHistory_.begin() + (conversationHistory_.size() - kMaxHistoryMessages));
        }
        if (textToSpeech_) {
            // What's spoken gets Markdown/emoji stripped (and tildes eased
            // into pauses). A reply that's nothing but emoji/symbols can
            // strip down to empty — Speak() no-ops on that and never posts
            // a started/ended event, so don't claim we'll speak
            // (ShowResponse would then wait out the long TTS-safety-net
            // timer for nothing).
            const std::wstring speakable = audio::MakeSpeakable(payload.text);
            if (!speakable.empty()) {
                core::Logger::Info("TTS: Speak() called");
                textToSpeech_->Speak(speakable);
                awaitingSpeechEndForDismiss_ = true;
                willSpeak = true;
            }
        }
    }

    // The bubble keeps Markdown as typed (harmless to display) but still
    // needs emoji stripped: GDI+'s "Segoe UI" font has no color-emoji
    // glyphs, so any emoji would otherwise draw as a broken "tofu" box.
    // conversationHistory_ above keeps the untouched original text.
    const std::wstring displayText = audio::StripUnrenderableSymbols(payload.text);
    chatBubble_->ShowResponse(ComputeBubbleAnchor(), displayText, [this]() { OnChatDismissed(); }, willSpeak);
}

void MainWindow::OnChatDismissed() {
    // Dismissing the bubble (manually, or via its own timer) also cuts off
    // any reply still being read, rather than leaving the voice going with
    // nothing on screen to show for it.
    awaitingSpeechEndForDismiss_ = false;
    if (textToSpeech_) {
        textToSpeech_->Stop();
    }
    if (isSpeaking_) {
        isSpeaking_ = false;
        KillTimer(hwnd_, kMouthAnimationTimerId);
        ApplySpriteToWindow();
    }

    characterState_.OnConversationEnd(isHovering_);
    SyncSpriteToEmotion();
}

void MainWindow::HandleTtsEvent() {
    if (!textToSpeech_) {
        return;
    }
    const audio::ITextToSpeech::EventResult result = textToSpeech_->PumpEvents();
    core::Logger::Info(std::format("TTS event: started={} ended={}", result.started, result.ended));

    if (result.started && !isSpeaking_) {
        isSpeaking_ = true;
        mouthFrameToggle_ = false;
        SetTimer(hwnd_, kMouthAnimationTimerId, kMouthAnimationIntervalMs, nullptr);
    }
    if (result.ended && isSpeaking_) {
        isSpeaking_ = false;
        KillTimer(hwnd_, kMouthAnimationTimerId);
        ApplySpriteToWindow(); // restore the plain (non-indicator) frame

        // Now that speech has genuinely finished, replace whatever guess
        // ChatBubble::ShowResponse's own timer made with a short, precise
        // grace period — long replies were disappearing mid-sentence
        // before this, since that guess topped out well under real
        // reading time.
        if (awaitingSpeechEndForDismiss_ && chatBubble_) {
            awaitingSpeechEndForDismiss_ = false;
            chatBubble_->RescheduleDismiss(kPostSpeechGraceMs, [this]() { OnChatDismissed(); });
        }
    }
}

void MainWindow::ShowTrayMenu() {
    POINT cursor{};
    GetCursorPos(&cursor);

    const HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, kMenuIdSettings, L"설정");
    AppendMenuW(menu, MF_STRING, kMenuIdTogglePause, isPaused_ ? L"다시 보이기" : L"일시정지");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuIdExit, L"종료");

    // Standard TrackPopupMenu idiom: the window must be foreground or the
    // menu won't dismiss on an outside click, and a trailing WM_NULL works
    // around a well-known Windows quirk where the menu can otherwise stick.
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, hwnd_, nullptr);
    PostMessageW(hwnd_, WM_NULL, 0, 0);

    DestroyMenu(menu);
}

void MainWindow::TogglePause() {
    isPaused_ = !isPaused_;
    if (isPaused_) {
        if (chatBubble_) {
            chatBubble_->Hide();
        }
        if (textToSpeech_) {
            textToSpeech_->Stop();
        }
        KillTimer(hwnd_, kCharacterTickTimerId);
        ShowWindow(hwnd_, SW_HIDE);
        core::Logger::Info("MainWindow: paused via tray menu");
    } else {
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        SetTimer(hwnd_, kCharacterTickTimerId, kCharacterTickIntervalMs, nullptr);
        SyncSpriteToEmotion();
        core::Logger::Info("MainWindow: resumed via tray menu");
    }
}

SettingsValues MainWindow::BuildCurrentSettingsValues() const {
    SettingsValues values;
    values.userName = userName_;
    values.relationshipNote = relationshipNote_;
    values.primaryLanguage = primaryLanguage_;
    // uiLanguage isn't cached on MainWindow (nothing else needs it), so
    // read it fresh here rather than adding a member just to shuttle it.
    values.uiLanguage = ai::UserProfile::Load().uiLanguage;
    const audio::GoogleTtsConfig ttsConfig = audio::GoogleTtsConfig::Load();
    values.ttsProvider = ttsConfig.provider;
    values.volumePercent = ttsConfig.volumePercent;
    values.voicesByLanguage = ttsConfig.voicesByLanguage;

    const context::PrivacyConfig privacyConfig = context::PrivacyConfig::Load();
    values.proactiveSpeechEnabled = privacyConfig.proactiveSpeechEnabled;
    values.gameDetectionEnabled = privacyConfig.gameDetectionEnabled;
    values.memoryEnabled = privacyConfig.memoryEnabled;
    return values;
}

void MainWindow::OpenSettings() {
    if (settingsWindow_) {
        settingsWindow_->ShowWithValues(BuildCurrentSettingsValues());
    }
}

void MainWindow::OnSettingsSaved(const SettingsValues& values) {
    userName_ = values.userName;
    relationshipNote_ = values.relationshipNote;
    primaryLanguage_ = values.primaryLanguage;
    ai::UserProfile{values.userName, values.relationshipNote, values.primaryLanguage, values.uiLanguage}.Save();

    // Reloaded fresh (not just re-using BuildCurrentSettingsValues' result)
    // so fields the Settings window doesn't expose -- excluded_processes,
    // screen_awareness_enabled -- survive the write instead of getting
    // clobbered with defaults.
    audio::GoogleTtsConfig ttsConfig = audio::GoogleTtsConfig::Load();
    ttsConfig.provider = values.ttsProvider;
    ttsConfig.volumePercent = values.volumePercent;
    for (const auto& [language, voice] : values.voicesByLanguage) {
        ttsConfig.voicesByLanguage[language] = voice;
    }
    ttsConfig.Save();

    context::PrivacyConfig privacyConfig = context::PrivacyConfig::Load();
    privacyConfig.proactiveSpeechEnabled = values.proactiveSpeechEnabled;
    privacyConfig.gameDetectionEnabled = values.gameDetectionEnabled;
    privacyConfig.memoryEnabled = values.memoryEnabled;
    privacyConfig.Save();

    // Apply live rather than requiring a restart.
    textToSpeech_ = audio::CreateTextToSpeech(hwnd_, kTtsEventMessage);
    if (contextEngine_) {
        contextEngine_->SetProactiveSpeechEnabled(values.proactiveSpeechEnabled);
        contextEngine_->SetGameDetectionEnabled(values.gameDetectionEnabled);
        contextEngine_->SetMemoryEnabled(values.memoryEnabled);
    }
    if (memoryEngine_) {
        memoryEngine_->SetEnabled(values.memoryEnabled);
    }
    core::Logger::Info("Settings saved and applied");
}

void MainWindow::ResetCharacterPosition() {
    RECT rect{};
    GetWindowRect(hwnd_, &rect);
    const POINT position = DefaultPosition(rect.right - rect.left, rect.bottom - rect.top);

    SetWindowPos(hwnd_, nullptr, position.x, position.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    SaveCurrentPosition();
    if (chatBubble_ && chatBubble_->IsVisible()) {
        chatBubble_->Reposition(ComputeBubbleAnchor());
    }
    core::Logger::Info("Character position reset via settings");
}

void MainWindow::HandleMouthAnimationTick() {
    if (!sprite_) {
        return;
    }
    mouthFrameToggle_ = !mouthFrameToggle_;
    const std::vector<uint8_t> pixels = rendering::WithTalkingIndicator(*sprite_, mouthFrameToggle_);
    ApplyPixelsToWindow(pixels, sprite_->Width(), sprite_->Height());
}

int MainWindow::RunMessageLoop() {
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) {
        return self->HandleMessage(message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_LBUTTONDOWN:
            // Borderless drag: let DefWindowProc's caption-move loop handle
            // it as if the user grabbed a title bar (there is none).
            ReleaseCapture();
            SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        case WM_MOUSEMOVE:
            HandleMouseMove(lParam);
            return 0;
        case WM_MOUSELEAVE:
            HandleMouseLeave();
            return 0;
        case WM_MOVE:
            // Fires continuously during the caption-move drag loop too, so
            // the bubble tracks the character in real time instead of
            // being left behind.
            if (chatBubble_ && chatBubble_->IsVisible()) {
                chatBubble_->Reposition(ComputeBubbleAnchor());
            }
            return 0;
        case WM_LBUTTONDBLCLK:
            StartChat();
            return 0;
        case kAiResponseMessage: {
            std::unique_ptr<AiResponsePayload> payload(reinterpret_cast<AiResponsePayload*>(lParam));
            OnAiResponse(*payload);
            return 0;
        }
        case kTtsEventMessage:
            HandleTtsEvent();
            return 0;
        case kContextSnapshotMessage:
            if (contextEngine_) {
                contextEngine_->OnSnapshotMessage(lParam);
            }
            return 0;
        case kTrayIconMessage:
            if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
                ShowTrayMenu();
            }
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case kMenuIdTogglePause:
                    TogglePause();
                    return 0;
                case kMenuIdSettings:
                    OpenSettings();
                    return 0;
                case kMenuIdExit:
                    DestroyWindow(hwnd_);
                    return 0;
                default:
                    return 0;
            }
        case WM_ENTERSIZEMOVE:
            // Fired by the caption-move loop the WM_LBUTTONDOWN trick enters.
            characterState_.OnDragStart(std::chrono::steady_clock::now());
            SyncSpriteToEmotion();
            return 0;
        case WM_EXITSIZEMOVE:
            characterState_.OnDragEnd(std::chrono::steady_clock::now(), isHovering_);
            SyncSpriteToEmotion();
            SaveCurrentPosition();
            return 0;
        case WM_TIMER:
            if (wParam == kCharacterTickTimerId) {
                HandleTick();
            } else if (wParam == kMouthAnimationTimerId) {
                HandleMouthAnimationTick();
            }
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd_, kCharacterTickTimerId);
            KillTimer(hwnd_, kMouthAnimationTimerId);
            SaveCurrentPosition();
            if (memoryEngine_) {
                memoryEngine_->Save();
            }
            core::Logger::Info("Main window destroyed");
            PostQuitMessage(0);
            return 0;
        default:
            // TaskbarCreated's message ID is assigned at runtime
            // (RegisterWindowMessageW), so it can't be a case label.
            if (trayIcon_ && message == TrayIcon::TaskbarCreatedMessage()) {
                trayIcon_->Readd();
                return 0;
            }
            return DefWindowProcW(hwnd_, message, wParam, lParam);
    }
}

} // namespace sveta::window
