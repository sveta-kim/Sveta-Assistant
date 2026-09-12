add_executable(SvetaAssistant WIN32
    ai/AiConfig.cpp
    ai/ChatClient.cpp
    ai/Persona.cpp
    ai/UserProfile.cpp
    app/main.cpp
    audio/GoogleOAuthTokenProvider.cpp
    audio/GoogleServiceAccount.cpp
    audio/GoogleTextToSpeech.cpp
    audio/GoogleTtsConfig.cpp
    audio/LanguageDetection.cpp
    audio/SpeakableText.cpp
    audio/TextToSpeech.cpp
    audio/TextToSpeechFactory.cpp
    behavior/IdleBehavior.cpp
    character/Action.cpp
    character/CharacterState.cpp
    character/Emotion.cpp
    character/Personality.cpp
    context/ActiveWindowTracker.cpp
    context/ContextEngine.cpp
    context/EpicLibraryScanner.cpp
    context/GameDetector.cpp
    context/LeagueLiveClient.cpp
    context/PrivacyConfig.cpp
    context/SteamLibraryScanner.cpp
    context/UiAutomationReader.cpp
    core/Logger.cpp
    core/Paths.cpp
    core/StringConvert.cpp
    interaction/HeadHitbox.cpp
    interaction/PettingDetector.cpp
    memory/DailyMemory.cpp
    memory/LongTermMemory.cpp
    memory/MemoryEngine.cpp
    proactive/ErrorRepeatDetector.cpp
    proactive/InterruptionScore.cpp
    rendering/Sprite.cpp
    rendering/SpriteOverlay.cpp
    window/ChatBubble.cpp
    window/MainWindow.cpp
    window/SettingsWindow.cpp
    window/TrayIcon.cpp
    window/WindowPosition.cpp
)

target_include_directories(SvetaAssistant PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(SvetaAssistant PRIVATE
    user32
    gdi32
    dwmapi
    windowscodecs
    ole32
    winhttp
    comctl32
    gdiplus
    advapi32
    crypt32
    winmm
    bcrypt
    shell32
    uxtheme
)

if(MSVC)
    target_compile_options(SvetaAssistant PRIVATE /utf-8)
    # Without this, comctl32 controls (buttons, checkboxes, the Settings
    # window's trackbar) render in the unthemed Windows 2000 style instead
    # of the current visual style -- this merges a Common-Controls-v6
    # dependency into the linker's auto-generated manifest, no .rc file needed.
    target_link_options(SvetaAssistant PRIVATE
        "/MANIFESTDEPENDENCY:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'")
endif()

target_compile_definitions(SvetaAssistant PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
    NTDDI_VERSION=0x0A000008
    SVETA_CONTENT_DIR="${CMAKE_SOURCE_DIR}/content"
    SVETA_CONFIG_DIR="${CMAKE_SOURCE_DIR}/config"
)
