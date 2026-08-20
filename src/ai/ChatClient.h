#pragma once

#include <string>
#include <vector>

#include "ai/AiConfig.h"

namespace sveta::ai {

struct ChatMessage {
    std::string role; // "system" | "user" | "assistant"
    std::string content;
};

struct ChatResult {
    bool success = false;
    std::string text; // assistant reply, or a human-readable error message
};

// Synchronous OpenAI-Chat-Completions-compatible client (POSTs {model,
// messages} as JSON, reads {choices[0].message.content} back). Blocks on
// network I/O — callers must run this off the UI thread so the character
// keeps moving while waiting (project plan section 49's "AI 응답이
// 느리더라도 캐릭터는 계속 움직일 수 있어야 한다").
class ChatClient {
public:
    explicit ChatClient(AiConfig config);

    ChatResult Send(const std::vector<ChatMessage>& history) const;

private:
    AiConfig config_;
};

} // namespace sveta::ai
