cmake_minimum_required(VERSION 3.20)
project(dalia VERSION 0.1.0 LANGUAGES CXX)

# Downloads timestamping
cmake_policy(SET CMP0135 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(engine)
# Studio tool should go here
add_subdirectory(demo)