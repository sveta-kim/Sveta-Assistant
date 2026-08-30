#pragma once

#include <windows.h>

#include <memory>

#include "audio/ITextToSpeech.h"

namespace sveta::audio {

// Picks TextToSpeech (local SAPI voices) or GoogleTextToSpeech (Chirp 3:
// HD) based on config/tts_config.json's "provider" field ("sapi" or
// "google"). Falls back to SAPI if "google" is requested but no API key
// is configured, or if neither backend could be created at all (e.g. no
// SAPI voices installed) -- in which case this returns nullptr and
// callers should treat TTS as simply unavailable, same as today.
std::unique_ptr<ITextToSpeech> CreateTextToSpeech(HWND notifyWindow, UINT notifyMessage);

} // namespace sveta::audio
