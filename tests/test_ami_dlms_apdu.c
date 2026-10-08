#include "ami_dlms_apdu.h"

#include <assert.h>
#include <stdio.h>

static void test_voltage_get_request_response(void)
{
    AmiDlmsAssociation association = {AMI_DLMS_ASSOC_ASSOCIATED};
    const AmiCosemRegisterSnapshot snapshot = {
        .phase_voltage_millivolts = {230000u, 229000u, 231000u},
        .valid_mask = AMI_COSEM_VALID_VOLTAGE_A
    };
    const uint8_t request[] = {
        0xC0u, 0x01u, 0xC1u, 0x00u, 0x03u,
        0x01u, 0x00u, 0x20u, 0x07u, 0x00u, 0xFFu,
        0x02u, 0x00u
    };
    const uint8_t expected[] = {
        0xC4u, 0x01u, 0xC1u, 0x00u, 0x06u,
        0x00u, 0x03u, 0x82u, 0x70u
    };
    uint8_t response[32];
    size_t response_length = 0u;
    assert(ami_dlms_process_get_request(&association, &snapshot, request,
             sizeof(request), response, sizeof(response), &response_length) ==
           AMI_DLMS_APDU_OK);
    assert(response_length == sizeof(expected));
    for (size_t i = 0u; i < sizeof(expected); ++i) {
        assert(response[i] == expected[i]);
    }
}

static void test_unknown_obis_returns_access_error(void)
{
    AmiDlmsAssociation association = {AMI_DLMS_ASSOC_ASSOCIATED};
    const AmiCosemRegisterSnapshot snapshot = {0};
    const uint8_t request[] = {
        0xC0u, 0x01u, 0xC2u, 0x00u, 0x03u,
        0x01u, 0x00u, 0xFAu, 0x07u, 0x00u, 0xFFu,
        0x02u, 0x00u
    };
    uint8_t response[8];
    size_t response_length = 0u;
    assert(ami_dlms_process_get_request(&association, &snapshot, request,
             sizeof(request), response, sizeof(response), &response_length) ==
           AMI_DLMS_APDU_ACCESS_ERROR);
    assert(response_length == 5u);
    assert(response[0] == 0xC4u && response[1] == 0x01u &&
           response[2] == 0xC2u && response[3] == 1u && response[4] == 4u);
}

int main(void)
{
    test_voltage_get_request_response();
    test_unknown_obis_returns_access_error();
    puts("DLMS APDU tests passed.");
    return 0;
}
