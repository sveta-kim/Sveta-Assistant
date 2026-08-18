add_executable(SvetaAssistant WIN32
    app/main.cpp
    core/Logger.cpp
    window/MainWindow.cpp
)

target_include_directories(SvetaAssistant PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(SvetaAssistant PRIVATE
    user32
    gdi32
    dwmapi
)

target_compile_definitions(SvetaAssistant PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
