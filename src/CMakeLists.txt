add_executable(SvetaAssistant WIN32
    ai/AiConfig.cpp
    ai/ChatClient.cpp
    ai/Persona.cpp
    app/main.cpp
    behavior/IdleBehavior.cpp
    character/Action.cpp
    character/CharacterState.cpp
    character/Emotion.cpp
    character/Personality.cpp
    core/Logger.cpp
    core/Paths.cpp
    core/StringConvert.cpp
    interaction/HeadHitbox.cpp
    interaction/PettingDetector.cpp
    rendering/Sprite.cpp
    window/ChatBubble.cpp
    window/MainWindow.cpp
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
)

if(MSVC)
    target_compile_options(SvetaAssistant PRIVATE /utf-8)
endif()

target_compile_definitions(SvetaAssistant PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    SVETA_CONTENT_DIR="${CMAKE_SOURCE_DIR}/content"
    SVETA_CONFIG_DIR="${CMAKE_SOURCE_DIR}/config"
)
