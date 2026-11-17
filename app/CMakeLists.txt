add_executable(AtmosDX12
    main.cpp
    Application.h
    Application.cpp
)

target_link_libraries(AtmosDX12 PRIVATE AtmosEngine)

if(MSVC)
    target_compile_options(AtmosDX12 PRIVATE /W4 /permissive- /utf-8)
endif()

atmos_add_runtime_files(AtmosDX12)
