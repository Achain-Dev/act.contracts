add_contract(act.bios act.bios ${CMAKE_CURRENT_SOURCE_DIR}/src/act.bios.cpp)

target_include_directories(act.bios
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(act.bios
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/act.bios.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/act.bios.contracts.md @ONLY )

target_compile_options( act.bios PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
