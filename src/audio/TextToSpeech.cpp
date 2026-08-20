#include "audio/TextToSpeech.h"

#include <initguid.h>
#include <sapi.h>
#include <sphelper.h>

#include <format>

#include "audio/LanguageDetection.h"
#include "core/Logger.h"

namespace sveta::audio {

namespace {

// Always re-selects a voice for the detected language (not just for
// Korean) — otherwise a voice picked for one reply stays "stuck" and
// reads the next reply's different language in the wrong accent. Falls
// back to whatever voice was already selected if none is installed for
// that language (see README for which of the 7 target languages this
// dev machine actually has voices for).
void SelectVoiceForText(ISpVoice* voice, const std::wstring& text) {
    const Language language = DetectLanguage(text);
    Microsoft::WRL::ComPtr<ISpObjectToken> token;
    if (SUCCEEDED(SpFindBestToken(SPCAT_VOICES, VoiceLcidQuery(language).c_str(), nullptr, &token))) {
        voice->SetVoice(token.Get());
    } else {
        core::Logger::Warn(std::format(
            "TextToSpeech: no installed voice for detected language {}; using whichever voice was last selected",
            ToString(language)));
    }
}

} // namespace

std::unique_ptr<TextToSpeech> TextToSpeech::Create(HWND notifyWindow, UINT notifyMessage) {
    Microsoft::WRL::ComPtr<ISpVoice> voice;
    const HRESULT hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&voice));
    if (FAILED(hr)) {
        core::Logger::Warn(std::format("TextToSpeech: CoCreateInstance failed (hr=0x{:08X}); voice disabled", static_cast<unsigned>(hr)));
        return nullptr;
    }

    voice->SetInterest(
        SPFEI(SPEI_START_INPUT_STREAM) | SPFEI(SPEI_END_INPUT_STREAM),
        SPFEI(SPEI_START_INPUT_STREAM) | SPFEI(SPEI_END_INPUT_STREAM));
    voice->SetNotifyWindowMessage(notifyWindow, notifyMessage, 0, 0);

    return std::unique_ptr<TextToSpeech>(new TextToSpeech(std::move(voice)));
}

TextToSpeech::TextToSpeech(Microsoft::WRL::ComPtr<ISpVoice> voice) : voice_(std::move(voice)) {}

TextToSpeech::~TextToSpeech() {
    Stop();
}

void TextToSpeech::Speak(const std::wstring& text) {
    if (!voice_ || text.empty()) {
        return;
    }
    SelectVoiceForText(voice_.Get(), text);
    // SPF_PURGEBEFORESPEAK: a new reply pre-empts one still being read.
    voice_->Speak(text.c_str(), SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
}

void TextToSpeech::Stop() {
    if (voice_) {
        voice_->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
    }
}

TextToSpeech::EventResult TextToSpeech::PumpEvents() {
    EventResult result;
    if (!voice_) {
        return result;
    }
    SPEVENT event{};
    while (voice_->GetEvents(1, &event, nullptr) == S_OK) {
        if (event.eEventId == SPEI_START_INPUT_STREAM) {
            result.started = true;
        } else if (event.eEventId == SPEI_END_INPUT_STREAM) {
            result.ended = true;
        }
    }
    return result;
}

} // namespace sveta::audio
