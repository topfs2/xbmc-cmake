set(SOURCES DummyVideoPlayer.cpp
            FFmpeg.cpp)

core_add_library(cores)

add_dependencies(cores ffmpeg)
