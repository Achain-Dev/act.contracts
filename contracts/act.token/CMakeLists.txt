add_contract(act.token act.token ${CMAKE_CURRENT_SOURCE_DIR}/src/act.token.cpp)

target_include_directories(act.token
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(act.token
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/act.token.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/act.token.contracts.md @ONLY )

target_compile_options( act.token PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
