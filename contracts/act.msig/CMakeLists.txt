add_contract(act.msig act.msig ${CMAKE_CURRENT_SOURCE_DIR}/src/act.msig.cpp)

target_include_directories(act.msig
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(act.msig
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/act.msig.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/act.msig.contracts.md @ONLY )

target_compile_options( act.msig PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
