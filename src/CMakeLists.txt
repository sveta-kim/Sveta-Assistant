add_executable(SvetaAssistant WIN32
    app/main.cpp
    core/Logger.cpp
    core/Paths.cpp
    rendering/Sprite.cpp
    window/MainWindow.cpp
    window/WindowPosition.cpp
)

target_include_directories(SvetaAssistant PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(SvetaAssistant PRIVATE
    user32
    gdi32
    dwmapi
    windowscodecs
    ole32
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
)
