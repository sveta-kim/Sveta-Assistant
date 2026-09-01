#include "ai/Persona.h"

#include <format>

namespace sveta::ai {

namespace {

// primaryLanguage is stored using the audio::ToString(Language) spelling
// (e.g. "Korean") -- translated here since the rest of the prompt is
// written in Korean and an English word sitting alone in that sentence
// would read oddly.
std::string LanguageNameForPrompt(const std::string& primaryLanguage) {
    if (primaryLanguage == "English") return "영어";
    if (primaryLanguage == "Japanese") return "일본어";
    if (primaryLanguage == "Chinese") return "중국어";
    if (primaryLanguage == "Spanish") return "스페인어";
    if (primaryLanguage == "German") return "독일어";
    if (primaryLanguage == "Russian") return "러시아어";
    return "한국어";
}

} // namespace

std::string BuildSystemPrompt(
    const character::Personality& personality, const std::string& userName, const std::string& relationshipNote,
    const std::string& primaryLanguage) {
    std::string prompt = std::format(
        "너는 'Sveta'라는 이름의 데스크톱 캐릭터야. 사용자의 컴퓨터 화면 한쪽에서 "
        "실제로 살아있는 것처럼 함께 지내는 캐릭터고, 지금은 사용자와 텍스트로 "
        "대화하고 있어. 성격 수치(0~1): 애정 {:.2f}, 장난기 {:.2f}, 호기심 {:.2f}, "
        "인내심 {:.2f}, 표현력 {:.2f}, 수줍음 {:.2f}. 이 성격이 말투에 자연스럽게 "
        "묻어나게 짧고 친근하게 대답해. 한두 문장이면 충분해.",
        personality.affection, personality.playfulness, personality.curiosity,
        personality.patience, personality.expressiveness, personality.shyness);

    if (!userName.empty()) {
        prompt += std::format(" 사용자를 '{}'라고 불러줘.", userName);
    }
    if (!relationshipNote.empty()) {
        prompt += std::format(" 사용자가 원하는 대하는 방식: {}", relationshipNote);
    }
    prompt += std::format(" 특별한 이유가 없으면 주로 {}로 대답해줘.", LanguageNameForPrompt(primaryLanguage));
    return prompt;
}

} // namespace sveta::ai
