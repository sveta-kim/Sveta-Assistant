#include "context/ContextEngine.h"

#include <format>
#include <thread>

#include "context/UiAutomationReader.h"
#include "core/Logger.h"
#include "core/StringConvert.h"
#include "proactive/InterruptionScore.h"

namespace sveta::context {

std::unique_ptr<ContextEngine> ContextEngine::Create(HWND notifyWindow, UINT notifyMessage) {
    PrivacyConfig privacy = PrivacyConfig::Load();

    // The tracker's callback needs a live ContextEngine to call into, but
    // that doesn't exist until after the tracker itself is constructed.
    // A heap cell the lambda captures by (shared_ptr) value, updated once
    // construction finishes, resolves the chicken-and-egg problem without
    // a dangling reference to a stack variable.
    auto selfCell = std::make_shared<ContextEngine*>(nullptr);
    auto tracker = ActiveWindowTracker::Create([selfCell](const ActiveWindowTracker::WindowInfo& info) {
        if (*selfCell) {
            (*selfCell)->OnActiveWindowChanged(info);
        }
    });
    if (!tracker) {
        core::Logger::Warn("ContextEngine: failed to install foreground-window hook; desktop awareness disabled");
        return nullptr;
    }

    auto engine = std::unique_ptr<ContextEngine>(
        new ContextEngine(std::move(tracker), notifyWindow, notifyMessage, std::move(privacy)));
    *selfCell = engine.get();

    if (!engine->privacy_.screenAwarenessEnabled) {
        core::Logger::Info("ContextEngine: screen awareness disabled via config/privacy_config.json");
    }
    // Seed an initial snapshot immediately rather than waiting for the
    // first EVENT_SYSTEM_FOREGROUND change, which may not fire for a
    // while (or ever, if the user opens chat without switching windows
    // first). Unconditional even when screen awareness is off: the
    // gaming-context check below doesn't read window title/content.
    engine->OnActiveWindowChanged(ActiveWindowTracker::GetCurrentWindowInfo());

    return engine;
}

ContextEngine::ContextEngine(
    std::unique_ptr<ActiveWindowTracker> tracker, HWND notifyWindow, UINT notifyMessage, PrivacyConfig privacy)
    : tracker_(std::move(tracker)), notifyWindow_(notifyWindow), notifyMessage_(notifyMessage), privacy_(std::move(privacy)) {}

ContextEngine::~ContextEngine() = default;

bool ContextEngine::IsExcluded(const std::wstring& processName) const {
    for (const auto& excluded : privacy_.excludedProcesses) {
        if (_wcsicmp(excluded.c_str(), processName.c_str()) == 0) {
            return true;
        }
    }
    return false;
}

void ContextEngine::OnActiveWindowChanged(const ActiveWindowTracker::WindowInfo& info) {
    // Independent of the privacy toggle below: this only looks at the
    // process name and window geometry, never title/UI text.
    const bool wasGaming = isGaming_;
    isGaming_ = gameDetector_.IsLikelyGame(info.processName, info.hwnd);
    if (isGaming_ != wasGaming) {
        core::Logger::Info(std::format("ContextEngine: gaming context {}", isGaming_ ? "started" : "ended"));
    }

    if (!privacy_.screenAwarenessEnabled) {
        return;
    }

    ++currentGeneration_;

    if (IsExcluded(info.processName)) {
        current_ = ContextSnapshot{};
        hasSnapshot_ = false;
        return;
    }

    // The cheap parts (title/process) apply immediately; the UI
    // Automation text — potentially slow, since it calls into whatever
    // app now has focus — arrives later via notifyMessage.
    current_.processName = info.processName;
    current_.windowTitle = info.title;
    current_.uiText.clear();
    hasSnapshot_ = true;

    core::Logger::Info(std::format(
        "ContextEngine: active window changed to '{}' ({})",
        core::WideToUtf8(info.title), core::WideToUtf8(info.processName)));

    const HWND targetHwnd = info.hwnd;
    const HWND notifyWindow = notifyWindow_;
    const UINT notifyMessage = notifyMessage_;
    const int generation = currentGeneration_;

    std::thread([targetHwnd, notifyWindow, notifyMessage, generation]() {
        auto snapshot = std::make_unique<ContextSnapshot>();
        snapshot->generation = generation;
        snapshot->uiText = UiAutomationReader::ExtractShallowText(targetHwnd).value_or(L"");
        PostMessageW(notifyWindow, notifyMessage, 0, reinterpret_cast<LPARAM>(snapshot.release()));
    }).detach();
}

void ContextEngine::OnSnapshotMessage(LPARAM lParam) {
    const std::unique_ptr<ContextSnapshot> snapshot(reinterpret_cast<ContextSnapshot*>(lParam));
    if (!hasSnapshot_ || snapshot->generation != currentGeneration_) {
        return; // the active window changed again before this finished
    }
    current_.uiText = snapshot->uiText;
    core::Logger::Info(std::format(
        "ContextEngine: UI Automation text captured ({} chars)", current_.uiText.size()));

    if (errorRepeatDetector_.Observe(current_.windowTitle, current_.uiText)) {
        pendingSameErrorRepeatedEvent_ = true;
        core::Logger::Info(std::format(
            "ContextEngine: same error repeated (score={:.2f} >= threshold={:.2f}) -> proactive candidate",
            proactive::InterruptionScore(proactive::ProactiveEvent::SameErrorRepeated), proactive::kInterruptionThreshold));
    }
}

std::optional<std::wstring> ContextEngine::ConsumeSameErrorRepeatedEvent() {
    if (!pendingSameErrorRepeatedEvent_) {
        return std::nullopt;
    }
    pendingSameErrorRepeatedEvent_ = false;

    std::wstring description = L"(사용자가 방금 전과 같은 오류를 다시 마주친 것 같다: \"" + current_.windowTitle + L"\"";
    if (!current_.processName.empty()) {
        description += L" (" + current_.processName + L")";
    }
    description += L". 화면에 보이는 내용: " + current_.uiText + L". ";
    description +=
        L"사용자가 직접 물어본 건 아니지만, 네가 먼저 짧게 한마디 건네보자 — 너무 참견하는 "
        L"느낌은 피하고 네 성격을 살려서 자연스럽게 말해줘.)";
    return description;
}

std::wstring ContextEngine::BuildContextLine() const {
    if (!privacy_.screenAwarenessEnabled || !hasSnapshot_) {
        return L"";
    }

    std::wstring line = L"(사용자는 지금 \"" + current_.windowTitle + L"\"";
    if (!current_.processName.empty()) {
        line += L" (" + current_.processName + L")";
    }
    line += L" 창을 보고 있다.";
    if (!current_.uiText.empty()) {
        line += L" 화면에 보이는 내용: " + current_.uiText;
    }
    line += L")";
    return line;
}

} // namespace sveta::context
