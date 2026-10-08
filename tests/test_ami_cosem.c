#include "ami_cosem_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t obis_voltage_a[6] = {1u, 0u, 32u, 7u, 0u, 255u};
static const uint8_t obis_current_b[6] = {1u, 0u, 51u, 7u, 0u, 255u};
static const uint8_t obis_active_import[6] = {1u, 0u, 1u, 8u, 0u, 255u};
static const uint8_t obis_clock[6] = {0u, 0u, 1u, 0u, 0u, 255u};

static void test_voltage_and_scaler(void)
{
    AmiCosemRegisterSnapshot snapshot = {0};
    snapshot.phase_voltage_millivolts[0] = 230000u;
    snapshot.valid_mask = AMI_COSEM_VALID_VOLTAGE_A;
    AmiCosemData out;
    assert(ami_cosem_register_read(&snapshot, 3u, obis_voltage_a, 2u, &out) == AMI_COSEM_OK);
    assert(out.type == AMI_COSEM_TYPE_DOUBLE_LONG_UNSIGNED);
    assert(out.value.double_long_unsigned == 230000u);
    assert(ami_cosem_register_read(&snapshot, 3u, obis_voltage_a, 3u, &out) == AMI_COSEM_OK);
    assert(out.type == AMI_COSEM_TYPE_STRUCTURE);
    assert(out.value.scaler_unit.scaler == -3 && out.value.scaler_unit.unit == 35u);
}

static void test_current_energy_and_logical_name(void)
{
    AmiCosemRegisterSnapshot snapshot = {0};
    snapshot.phase_current_milliamps[1] = 5120u;
    snapshot.active_import_wh = UINT64_C(12345678901234);
    snapshot.valid_mask = AMI_COSEM_VALID_CURRENT_B | AMI_COSEM_VALID_ACTIVE_IMPORT;
    AmiCosemData out;
    assert(ami_cosem_register_read(&snapshot, 3u, obis_current_b, 2u, &out) == AMI_COSEM_OK);
    assert(out.value.double_long_unsigned == 5120u);
    assert(ami_cosem_register_read(&snapshot, 3u, obis_active_import, 2u, &out) == AMI_COSEM_OK);
    assert(out.type == AMI_COSEM_TYPE_LONG64_UNSIGNED);
    assert(out.value.long64_unsigned == UINT64_C(12345678901234));
    assert(ami_cosem_register_read(&snapshot, 3u, obis_voltage_a, 1u, &out) == AMI_COSEM_OK);
    assert(out.type == AMI_COSEM_TYPE_OCTET_STRING && out.length == 6u);
    assert(memcmp(out.value.logical_name, obis_voltage_a, 6u) == 0);
}

static void test_validity_and_invalid_reads(void)
{
    AmiCosemRegisterSnapshot snapshot = {0};
    AmiCosemData out;
    const uint8_t unknown[6] = {9u, 9u, 9u, 9u, 9u, 9u};
    assert(ami_cosem_register_read(&snapshot, 3u, obis_voltage_a, 2u, &out) == AMI_COSEM_VALUE_UNAVAILABLE);
    assert(ami_cosem_register_read(&snapshot, 3u, unknown, 2u, &out) == AMI_COSEM_OBJECT_NOT_FOUND);
    assert(ami_cosem_register_read(&snapshot, 1u, obis_voltage_a, 2u, &out) == AMI_COSEM_OBJECT_NOT_FOUND);
    assert(ami_cosem_register_read(&snapshot, 3u, obis_voltage_a, 4u, &out) == AMI_COSEM_ATTRIBUTE_NOT_FOUND);
    assert(ami_cosem_register_read(NULL, 3u, obis_voltage_a, 2u, &out) == AMI_COSEM_INVALID_ARGUMENT);
}

static void test_clock_attributes(void)
{
    const AmiCosemRegisterSnapshot snapshot = {
        .clock_date_time = {
            .year = {0x07u, 0xEAu}, .month = 6u, .day = 15u,
            .day_of_week = 1u, .hour = 12u, .minute = 34u,
            .second = 56u, .hundredths = 0u, .deviation = {0u, 0u}, .status = 0u
        },
        .clock_time_zone_minutes = 0,
        .clock_status = 0u,
        .valid_mask = AMI_COSEM_VALID_CLOCK
    };
    AmiCosemData out;
    assert(sizeof(AmiCosemDateTime) == 12u);
    assert(ami_cosem_register_read(&snapshot, AMI_COSEM_CLOCK_CLASS_ID, obis_clock, 2u, &out) == AMI_COSEM_OK);
    assert(out.type == AMI_COSEM_TYPE_OCTET_STRING && out.length == 12u);
    assert(out.value.date_time.year[0] == 0x07u && out.value.date_time.year[1] == 0xEAu);
    assert(ami_cosem_register_read(&snapshot, AMI_COSEM_CLOCK_CLASS_ID, obis_clock, 3u, &out) == AMI_COSEM_OK);
    assert(out.type == AMI_COSEM_TYPE_DOUBLE_LONG && out.value.double_long == 0);
    assert(ami_cosem_register_read(&snapshot, AMI_COSEM_CLOCK_CLASS_ID, obis_clock, 4u, &out) == AMI_COSEM_OK);
    assert(out.type == AMI_COSEM_TYPE_UNSIGNED && out.value.unsigned_integer == 0u);
    assert(ami_cosem_object_count() == ami_cosem_register_count() + 1u);
}

int main(void)
{
    test_voltage_and_scaler();
    test_current_energy_and_logical_name();
    test_validity_and_invalid_reads();
    test_clock_attributes();
    puts("COSEM model tests passed.");
    return 0;
}
