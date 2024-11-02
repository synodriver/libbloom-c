cmake_minimum_required(VERSION 3.29)
project(libbloom C)

set(CMAKE_C_STANDARD 11)
set(ENVNAME nogil)
set(PYLIBNAME python313t)

include_directories(D:\\conda\\envs\\${ENVNAME}\\include D:\\conda\\envs\\${ENVNAME}\\Library\\include dep dep/murmur2)
link_directories(D:\\conda\\envs\\${ENVNAME}\\libs "D:\\conda\\envs\\${ENVNAME}\\Library\\lib" D:\\conda\\envs\\${ENVNAME}\\Library\\bin)


add_library(libbloom SHARED src/bloom_wrap.c dep/bloom.c dep/murmur2/murmurhash2.c)
target_link_libraries(libbloom ${PYLIBNAME})
set_target_properties(libbloom PROPERTIES PREFIX "" OUTPUT_NAME "_bloom")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_compile_definitions(Py_GIL_DISABLED=1)
    set_target_properties(libbloom PROPERTIES SUFFIX ".pyd")
endif ()

add_compile_definitions(BLOOM_VERSION_MAJOR=2 BLOOM_VERSION_MINOR=0 PYDBG=1)