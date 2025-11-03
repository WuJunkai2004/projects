cmake_minimum_required(VERSION 3.27)
project(lwork C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

include(FetchContent)
FetchContent_Declare(
    c_types_deps
    GIT_REPOSITORY  https://github.com/wujunkai2004/C_types.git
    GIT_TAG         main
)
FetchContent_MakeAvailable(c_types_deps)

if(MSVC)
    add_compile_options(/MP)
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()


#set include directory
include_directories(include)

#set source directory
file(GLOB SOURCES "src/*.c")

# add macro END_LINE='\n' to avoid issues with different OS line endings
add_compile_definitions(END_LINE=10)

#add executable with sources and main.c file
add_executable(app ${SOURCES} main.c)
target_link_libraries(app PRIVATE c_types)