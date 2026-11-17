cmake_minimum_required(VERSION 3.20)
project(ami_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN)
    add_compile_options(/W3)
endif()

option(ENABLE_COVERAGE "Enable code coverage instrumentation (gcov/lcov)" OFF)
if(ENABLE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Enabling code coverage instrumentation (--coverage)")
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    endif()
endif()

set(AMI_INC "${CMAKE_CURRENT_LIST_DIR}/../NonSecure/AMI/Inc")
set(AMI_SRC "${CMAKE_CURRENT_LIST_DIR}/../NonSecure/AMI/Src")

add_executable(test_ami_cosem
    test_ami_cosem.c
    "${AMI_SRC}/ami_cosem_model.c")
target_include_directories(test_ami_cosem PRIVATE "${AMI_INC}")
add_test(NAME AmiCosemTest COMMAND test_ami_cosem)

add_executable(test_ami_security
    test_ami_security.c
    "${AMI_SRC}/ami_security.c")
target_include_directories(test_ami_security PRIVATE "${AMI_INC}")
add_test(NAME AmiSecurityTest COMMAND test_ami_security)

add_executable(test_ami_dlms_association
    test_ami_dlms_association.c
    "${AMI_SRC}/ami_dlms_association.c"
    "${AMI_SRC}/ami_security.c")
target_include_directories(test_ami_dlms_association PRIVATE "${AMI_INC}")
add_test(NAME AmiDlmsAssociationTest COMMAND test_ami_dlms_association)

add_executable(test_ami_firmware_auth_policy
    test_ami_firmware_auth_policy.c
    "${AMI_SRC}/ami_dlms_association.c")
target_include_directories(test_ami_firmware_auth_policy PRIVATE "${AMI_INC}")
target_compile_definitions(test_ami_firmware_auth_policy PRIVATE STM32U585xx)
add_test(NAME AmiFirmwareAuthPolicyTest COMMAND test_ami_firmware_auth_policy)

add_executable(test_ami_dlms_apdu
    test_ami_dlms_apdu.c
    "${AMI_SRC}/ami_dlms_apdu.c"
    "${AMI_SRC}/ami_dlms_association.c"
    "${AMI_SRC}/ami_disconnect_control.c"
    "${AMI_SRC}/ami_cosem_model.c")
target_include_directories(test_ami_dlms_apdu PRIVATE "${AMI_INC}")
add_test(NAME AmiDlmsApduTest COMMAND test_ami_dlms_apdu)

add_executable(test_ami_dlms_hdlc
    test_ami_dlms_hdlc.c
    "${AMI_SRC}/ami_dlms_hdlc.c")
target_include_directories(test_ami_dlms_hdlc PRIVATE "${AMI_INC}")
add_test(NAME AmiDlmsHdlcTest COMMAND test_ami_dlms_hdlc)

add_executable(gurux_simulator
    gurux_simulator.c
    "${AMI_SRC}/ami_dlms_port.c"
    "${AMI_SRC}/ami_dlms_cipher.c"
    "${AMI_SRC}/ami_security.c"
    "${AMI_SRC}/ami_anti_bruteforce.c"
    "${AMI_SRC}/ami_nvm_counter.c"
    "${AMI_SRC}/ami_dlms_hdlc.c"
    "${AMI_SRC}/ami_dlms_association.c"
    "${AMI_SRC}/ami_dlms_apdu.c"
    "${AMI_SRC}/ami_dlms_block_transfer.c"
    "${AMI_SRC}/ami_disconnect_control.c"
    "${AMI_SRC}/ami_cosem_model.c")
target_include_directories(gurux_simulator PRIVATE "${AMI_INC}")
set_target_properties(gurux_simulator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/host-tools")
if(WIN32)
    target_link_libraries(gurux_simulator PRIVATE ws2_32)
endif()

add_executable(test_ami_port
    test_ami_port.c
    "${AMI_SRC}/ami_dlms_port.c"
    "${AMI_SRC}/ami_dlms_block_transfer.c"
    "${AMI_SRC}/ami_anti_bruteforce.c"
    "${AMI_SRC}/ami_disconnect_control.c"
    "${AMI_SRC}/ami_dlms_hdlc.c"
    "${AMI_SRC}/ami_dlms_association.c"
    "${AMI_SRC}/ami_dlms_apdu.c"
    "${AMI_SRC}/ami_cosem_model.c")
target_include_directories(test_ami_port PRIVATE "${AMI_INC}")
add_test(NAME AmiDlmsPortTest COMMAND test_ami_port)

add_executable(test_ami_ade9000
    test_ami_ade9000.c
    "${AMI_SRC}/ami_ade9000.c"
    "${AMI_SRC}/ami_cosem_model.c")
target_include_directories(test_ami_ade9000 PRIVATE "${AMI_INC}")
add_test(NAME AmiAde9000Test COMMAND test_ami_ade9000)

add_executable(test_ami_dlms_cipher
    test_ami_dlms_cipher.c
    "${AMI_SRC}/ami_dlms_cipher.c"
    "${AMI_SRC}/ami_nvm_counter.c"
    "${AMI_SRC}/ami_dlms_port.c"
    "${AMI_SRC}/ami_dlms_block_transfer.c"
    "${AMI_SRC}/ami_anti_bruteforce.c"
    "${AMI_SRC}/ami_disconnect_control.c"
    "${AMI_SRC}/ami_dlms_hdlc.c"
    "${AMI_SRC}/ami_dlms_association.c"
    "${AMI_SRC}/ami_dlms_apdu.c"
    "${AMI_SRC}/ami_cosem_model.c"
    "${AMI_SRC}/ami_security.c")
target_include_directories(test_ami_dlms_cipher PRIVATE "${AMI_INC}")
add_test(NAME AmiDlmsCipherTest COMMAND test_ami_dlms_cipher)

add_executable(test_ami_nvm_counter
    test_ami_nvm_counter.c
    "${AMI_SRC}/ami_nvm_counter.c"
    "${AMI_SRC}/ami_dlms_cipher.c"
    "${AMI_SRC}/ami_dlms_port.c"
    "${AMI_SRC}/ami_dlms_block_transfer.c"
    "${AMI_SRC}/ami_anti_bruteforce.c"
    "${AMI_SRC}/ami_disconnect_control.c"
    "${AMI_SRC}/ami_dlms_hdlc.c"
    "${AMI_SRC}/ami_dlms_association.c"
    "${AMI_SRC}/ami_dlms_apdu.c"
    "${AMI_SRC}/ami_cosem_model.c"
    "${AMI_SRC}/ami_security.c")
target_include_directories(test_ami_nvm_counter PRIVATE "${AMI_INC}")
add_test(NAME AmiNvmCounterTest COMMAND test_ami_nvm_counter)

add_executable(test_ami_key_transfer
    test_ami_key_transfer.c
    "${AMI_SRC}/ami_dlms_cipher.c"
    "${AMI_SRC}/ami_nvm_counter.c"
    "${AMI_SRC}/ami_dlms_port.c"
    "${AMI_SRC}/ami_dlms_block_transfer.c"
    "${AMI_SRC}/ami_anti_bruteforce.c"
    "${AMI_SRC}/ami_disconnect_control.c"
    "${AMI_SRC}/ami_dlms_hdlc.c"
    "${AMI_SRC}/ami_dlms_association.c"
    "${AMI_SRC}/ami_dlms_apdu.c"
    "${AMI_SRC}/ami_cosem_model.c"
    "${AMI_SRC}/ami_security.c")
target_include_directories(test_ami_key_transfer PRIVATE "${AMI_INC}")
add_test(NAME AmiKeyTransferTest COMMAND test_ami_key_transfer)

add_executable(test_ami_anti_bruteforce
    test_ami_anti_bruteforce.c
    "${AMI_SRC}/ami_anti_bruteforce.c"
    "${AMI_SRC}/ami_dlms_port.c"
    "${AMI_SRC}/ami_dlms_block_transfer.c"
    "${AMI_SRC}/ami_disconnect_control.c"
    "${AMI_SRC}/ami_dlms_hdlc.c"
    "${AMI_SRC}/ami_dlms_association.c"
    "${AMI_SRC}/ami_dlms_apdu.c"
    "${AMI_SRC}/ami_cosem_model.c"
    "${AMI_SRC}/ami_security.c")
target_include_directories(test_ami_anti_bruteforce PRIVATE "${AMI_INC}")
add_test(NAME AmiAntiBruteforceTest COMMAND test_ami_anti_bruteforce)

add_executable(test_ami_block_transfer
    test_ami_block_transfer.c
    "${AMI_SRC}/ami_dlms_block_transfer.c"
    "${AMI_SRC}/ami_dlms_port.c"
    "${AMI_SRC}/ami_anti_bruteforce.c"
    "${AMI_SRC}/ami_disconnect_control.c"
    "${AMI_SRC}/ami_dlms_hdlc.c"
    "${AMI_SRC}/ami_dlms_association.c"
    "${AMI_SRC}/ami_dlms_apdu.c"
    "${AMI_SRC}/ami_cosem_model.c"
    "${AMI_SRC}/ami_security.c")
target_include_directories(test_ami_block_transfer PRIVATE "${AMI_INC}")
add_test(NAME AmiBlockTransferTest COMMAND test_ami_block_transfer)

add_executable(test_ami_disconnect_control
    test_ami_disconnect_control.c
    "${AMI_SRC}/ami_disconnect_control.c"
    "${AMI_SRC}/ami_dlms_cipher.c"
    "${AMI_SRC}/ami_nvm_counter.c"
    "${AMI_SRC}/ami_dlms_port.c"
    "${AMI_SRC}/ami_dlms_block_transfer.c"
    "${AMI_SRC}/ami_anti_bruteforce.c"
    "${AMI_SRC}/ami_dlms_hdlc.c"
    "${AMI_SRC}/ami_dlms_association.c"
    "${AMI_SRC}/ami_dlms_apdu.c"
    "${AMI_SRC}/ami_cosem_model.c"
    "${AMI_SRC}/ami_security.c")
target_include_directories(test_ami_disconnect_control PRIVATE "${AMI_INC}")
add_test(NAME AmiDisconnectControlTest COMMAND test_ami_disconnect_control)

add_executable(test_ami_trustzone_boundary
    test_ami_trustzone_boundary.c)
target_include_directories(test_ami_trustzone_boundary PRIVATE "${AMI_INC}")
add_test(NAME AmiTrustZoneBoundaryTest COMMAND test_ami_trustzone_boundary)

add_executable(test_ami_boot_crypto
    test_ami_boot_crypto.c
    "${CMAKE_CURRENT_LIST_DIR}/../Secure/Core/Src/ami_boot_crypto.c")
target_include_directories(test_ami_boot_crypto PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../Secure/Core/Inc")
target_compile_definitions(test_ami_boot_crypto PRIVATE AMI_BOOT_CRYPTO_ENABLE_SIGN)
add_test(NAME AmiBootCryptoTest COMMAND test_ami_boot_crypto)

add_executable(ami_sign_tool
    ami_sign_tool.c
    "${CMAKE_CURRENT_LIST_DIR}/../Secure/Core/Src/ami_boot_crypto.c")
target_include_directories(ami_sign_tool PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../Secure/Core/Inc")
target_compile_definitions(ami_sign_tool PRIVATE AMI_BOOT_CRYPTO_ENABLE_SIGN)
set_target_properties(ami_sign_tool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/host-tools")

