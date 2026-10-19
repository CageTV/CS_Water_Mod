# WaterProbe.dll on alandtse's CommonLibSSE-NG (Skyrim 1.6.1170 and 1.7.x); included by E:/WorkSpace/ng-build/CMakeLists.txt (see ng_plugin there).
ng_plugin(TARGET WaterProbe NAME WaterProbe VERSION 0.1.0 AUTHOR "CageTV" ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES src/main.cpp
    INCLUDES "${CMAKE_CURRENT_LIST_DIR}/src"
    PCH "${CMAKE_CURRENT_LIST_DIR}/src/PCH.h")
