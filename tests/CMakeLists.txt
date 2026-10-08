cmake_minimum_required(VERSION 3.20)
project(ami_tests C)

enable_testing()

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
    "${AMI_SRC}/ami_dlms_association.c")
target_include_directories(test_ami_dlms_association PRIVATE "${AMI_INC}")
add_test(NAME AmiDlmsAssociationTest COMMAND test_ami_dlms_association)

add_executable(test_ami_dlms_apdu
    test_ami_dlms_apdu.c
    "${AMI_SRC}/ami_dlms_apdu.c"
    "${AMI_SRC}/ami_dlms_association.c"
    "${AMI_SRC}/ami_cosem_model.c")
target_include_directories(test_ami_dlms_apdu PRIVATE "${AMI_INC}")
add_test(NAME AmiDlmsApduTest COMMAND test_ami_dlms_apdu)
