add_contract(act.wrap act.wrap ${CMAKE_CURRENT_SOURCE_DIR}/src/act.wrap.cpp)

target_include_directories(act.wrap
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(act.wrap
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/act.wrap.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/act.wrap.contracts.md @ONLY )

target_compile_options( act.wrap PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
