cmake_minimum_required(VERSION 3.20)
project(ami_docker_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

set(AMI_INC "${CMAKE_CURRENT_LIST_DIR}/../NonSecure/AMI/Inc")
set(AMI_SRC "${CMAKE_CURRENT_LIST_DIR}/../NonSecure/AMI/Src")
set(TESTS_SRC "${CMAKE_CURRENT_LIST_DIR}/../tests")

# 1. Porting & Hardware Abstraction Layer Unit Test
add_executable(test_ami_port
    test_ami_port.c
    "${AMI_SRC}/ami_dlms_port.c"
    "${AMI_SRC}/ami_dlms_hdlc.c"
    "${AMI_SRC}/ami_dlms_association.c"
    "${AMI_SRC}/ami_dlms_apdu.c"
    "${AMI_SRC}/ami_cosem_model.c"
)
target_include_directories(test_ami_port PRIVATE "${AMI_INC}")
add_test(NAME AmiDlmsPortTest COMMAND test_ami_port)

# 2. Re-use existing unit test suites
add_executable(test_ami_cosem
    "${TESTS_SRC}/test_ami_cosem.c"
    "${AMI_SRC}/ami_cosem_model.c"
)
target_include_directories(test_ami_cosem PRIVATE "${AMI_INC}")
add_test(NAME AmiCosemTest COMMAND test_ami_cosem)

add_executable(test_ami_security
    "${TESTS_SRC}/test_ami_security.c"
    "${AMI_SRC}/ami_security.c"
)
target_include_directories(test_ami_security PRIVATE "${AMI_INC}")
add_test(NAME AmiSecurityTest COMMAND test_ami_security)

add_executable(test_ami_dlms_association
    "${TESTS_SRC}/test_ami_dlms_association.c"
    "${AMI_SRC}/ami_dlms_association.c"
)
target_include_directories(test_ami_dlms_association PRIVATE "${AMI_INC}")
add_test(NAME AmiDlmsAssociationTest COMMAND test_ami_dlms_association)

add_executable(test_ami_dlms_apdu
    "${TESTS_SRC}/test_ami_dlms_apdu.c"
    "${AMI_SRC}/ami_dlms_apdu.c"
    "${AMI_SRC}/ami_dlms_association.c"
    "${AMI_SRC}/ami_cosem_model.c"
)
target_include_directories(test_ami_dlms_apdu PRIVATE "${AMI_INC}")
add_test(NAME AmiDlmsApduTest COMMAND test_ami_dlms_apdu)

add_executable(test_ami_dlms_hdlc
    "${TESTS_SRC}/test_ami_dlms_hdlc.c"
    "${AMI_SRC}/ami_dlms_hdlc.c"
)
target_include_directories(test_ami_dlms_hdlc PRIVATE "${AMI_INC}")
add_test(NAME AmiDlmsHdlcTest COMMAND test_ami_dlms_hdlc)

# 3. Simulator for E2E TCP Client Validation
add_executable(gurux_simulator
    "${TESTS_SRC}/gurux_simulator.c"
    "${AMI_SRC}/ami_dlms_hdlc.c"
    "${AMI_SRC}/ami_dlms_association.c"
    "${AMI_SRC}/ami_dlms_apdu.c"
    "${AMI_SRC}/ami_cosem_model.c"
)
target_include_directories(gurux_simulator PRIVATE "${AMI_INC}")
if(WIN32)
    target_link_libraries(gurux_simulator PRIVATE ws2_32)
endif()

