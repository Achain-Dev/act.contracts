project(dice)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( dice dice dice.cpp )
target_include_directories( dice PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( dice ${CMAKE_SOURCE_DIR}/../ricardian )