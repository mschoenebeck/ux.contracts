add_contract(eosio.info eosio.info ${CMAKE_CURRENT_SOURCE_DIR}/src/eosio.info.cpp)

target_include_directories(eosio.info
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(eosio.info
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/eosio.info.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/eosio.info.contracts.md @ONLY )

target_compile_options( eosio.info PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
