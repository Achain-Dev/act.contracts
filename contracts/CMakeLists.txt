cmake_minimum_required( VERSION 3.5 )

project(contracts)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(act.cdt)

add_subdirectory(act.bios)
add_subdirectory(act.msig)
add_subdirectory(act.system)
add_subdirectory(act.token)
add_subdirectory(act.wrap)
