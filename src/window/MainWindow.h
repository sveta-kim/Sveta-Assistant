#pragma once

#include <windows.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ai/AiConfig.h"
#include "ai/ChatClient.h"
#include "character/CharacterState.h"
#include "interaction/PettingDetector.h"
#include "rendering/Sprite.h"
#include "window/ChatBubble.h"

namespace sveta::window {

// Borderless, always-on-top, per-pixel-transparent window that hosts the
// character sprite. Phase 1: PNG rendering, drag, position save.
// Phase 2: cursor tracking, head hitbox, petting detection.
// Phase 3: emotion/action/personality state, idle behavior, sleep.
// Phase 4: text chat with an AI backend (speech bubble, background
// network worker thread).
class MainWindow {
public:
    static std::unique_ptr<MainWindow> Create(HINSTANCE instance);

    ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    int RunMessageLoop();

private:
    // Heap-allocated by the AI worker thread (via std::unique_ptr::release,
    // matching the coding rules' "no owning raw pointers" outside this kind
    // of explicit, documented ownership handoff), posted through the
    // window message queue, and reclaimed into a unique_ptr again by the
    // UI thread in HandleMessage. Never touched concurrently by both
    // threads at once.
    struct AiResponsePayload {
        bool success = false;
        std::wstring text;
    };

    MainWindow(HWND hwnd, std::optional<rendering::Sprite> sprite);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void ApplySpriteToWindow();
    void SaveCurrentPosition();
    void HandleMouseMove(LPARAM lParam);
    void HandleMouseLeave();
    void HandleTick();
    void SyncSpriteToEmotion();
    void ReloadSpriteForEmotion(character::Emotion emotion);

    POINT ComputeBubbleAnchor() const;
    void StartChat();
    void OnMessageSubmitted(const std::wstring& message);
    void OnChatDismissed();
    void OnAiResponse(const AiResponsePayload& payload);

    HWND hwnd_;
    std::optional<rendering::Sprite> sprite_;
    RECT headHitbox_{};
    bool isHovering_ = false;
    interaction::PettingDetector pettingDetector_;
    character::CharacterState characterState_;
    character::Emotion lastAppliedEmotion_ = character::Emotion::Calm;

    std::unique_ptr<ChatBubble> chatBubble_;
    std::optional<ai::AiConfig> aiConfig_;
    std::vector<ai::ChatMessage> conversationHistory_;
    bool conversationInFlight_ = false;
};

} // namespace sveta::window
