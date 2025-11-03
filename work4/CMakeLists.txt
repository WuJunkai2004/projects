cmake_minimum_required(VERSION 3.27)
project(lwork C)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

if(MSVC)
    add_compile_options(/MP)
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
endif()

find_package(Readline REQUIRED)

#read logo file to configure the logo.h
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/src/logo.txt LOGO_TXT_CONTENT)
# escape special characters for C string literal
string(REPLACE "\\" "\\\\" LOGO_TXT_CONTENT "${LOGO_TXT_CONTENT}")
string(REPLACE "\"" "\\\"" LOGO_TXT_CONTENT "${LOGO_TXT_CONTENT}")
string(REPLACE "\n" "\\n\"\\\n\"" LOGO_TXT_CONTENT "${LOGO_TXT_CONTENT}")
set(LOGO_CONTENT "${LOGO_TXT_CONTENT}")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/logo.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/logo.h
)

# add macro END_LINE='\n' to avoid issues with different OS line endings
add_compile_definitions(END_LINE=10)

#set source directory
file(GLOB SOURCES "src/*.c")

#add executable with sources and main.c file
add_executable(app ${SOURCES} main.c)

# add include directories
target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

# link readline library
target_link_libraries(app PRIVATE readline)