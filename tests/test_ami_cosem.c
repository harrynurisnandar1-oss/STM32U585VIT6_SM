#include "ami_cosem_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t obis_voltage_a[6] = {1u, 0u, 32u, 7u, 0u, 255u};
static const uint8_t obis_current_b[6] = {1u, 0u, 51u, 7u, 0u, 255u};
static const uint8_t obis_active_import[6] = {1u, 0u, 1u, 8u, 0u, 255u};
static const uint8_t obis_clock[6] = {0u, 0u, 1u, 0u, 0u, 255u};
static const uint8_t obis_event_log[6] = {0u, 0u, 99u, 98u, 0u, 255u};
static const uint8_t obis_billing_profile[6] = {1u, 0u, 98u, 1u, 0u, 255u};
static const uint8_t obis_load_profile[6] = {1u, 0u, 99u, 1u, 0u, 255u};
static const uint8_t obis_security_setup[6] = {0u, 0u, 43u, 0u, 0u, 255u};

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
    assert(ami_cosem_object_count() == ami_cosem_register_count() + 5u);
}

static void test_profile_generic_buffers(void)
{
    AmiCosemRegisterSnapshot snapshot = {0};
    snapshot.event_log_count = 1u;
    snapshot.event_log[0].timestamp.year[0] = 0x07u;
    snapshot.event_log[0].timestamp.year[1] = 0xEAu;
    snapshot.event_log[0].timestamp.month = 10u;
    snapshot.event_log[0].timestamp.day = 12u;
    snapshot.event_log[0].event_code = 7u;

    snapshot.billing_history_count = 1u;
    snapshot.billing_history[0].billing_timestamp.year[0] = 0x07u;
    snapshot.billing_history[0].billing_timestamp.year[1] = 0xEAu;
    snapshot.billing_history[0].billing_timestamp.month = 10u;
    snapshot.billing_history[0].billing_timestamp.day = 1u;
    snapshot.billing_history[0].active_import_wh = UINT64_C(123456);
    snapshot.billing_history[0].active_export_wh = UINT64_C(789);
    snapshot.billing_history[0].reactive_import_varh = UINT64_C(456);
    snapshot.billing_history[0].reactive_export_varh = UINT64_C(123);

    snapshot.load_profile_count = 2u;
    snapshot.load_profile[0].timestamp.year[0] = 0x07u;
    snapshot.load_profile[0].timestamp.year[1] = 0xEAu;
    snapshot.load_profile[0].timestamp.month = 10u;
    snapshot.load_profile[0].timestamp.day = 15u;
    snapshot.load_profile[0].timestamp.hour = 12u;
    snapshot.load_profile[0].timestamp.minute = 0u;
    snapshot.load_profile[0].status = 0x00000000u;
    snapshot.load_profile[0].active_import_wh = UINT64_C(1000);
    snapshot.load_profile[0].active_export_wh = UINT64_C(10);
    snapshot.load_profile[0].reactive_import_varh = UINT64_C(50);
    snapshot.load_profile[0].reactive_export_varh = UINT64_C(5);
    snapshot.load_profile[0].phase_voltage_mv[0] = 230000u;
    snapshot.load_profile[0].phase_voltage_mv[1] = 231000u;
    snapshot.load_profile[0].phase_voltage_mv[2] = 229000u;
    snapshot.load_profile[0].phase_current_ma[0] = 1500u;
    snapshot.load_profile[0].phase_current_ma[1] = 1600u;
    snapshot.load_profile[0].phase_current_ma[2] = 1400u;

    snapshot.load_profile[1].timestamp.year[0] = 0x07u;
    snapshot.load_profile[1].timestamp.year[1] = 0xEAu;
    snapshot.load_profile[1].timestamp.month = 10u;
    snapshot.load_profile[1].timestamp.day = 15u;
    snapshot.load_profile[1].timestamp.hour = 12u;
    snapshot.load_profile[1].timestamp.minute = 15u;
    snapshot.load_profile[1].status = 0x00000001u;
    snapshot.load_profile[1].active_import_wh = UINT64_C(1025);
    snapshot.load_profile[1].active_export_wh = UINT64_C(12);
    snapshot.load_profile[1].reactive_import_varh = UINT64_C(55);
    snapshot.load_profile[1].reactive_export_varh = UINT64_C(6);
    snapshot.load_profile[1].phase_voltage_mv[0] = 230100u;
    snapshot.load_profile[1].phase_voltage_mv[1] = 231100u;
    snapshot.load_profile[1].phase_voltage_mv[2] = 229100u;
    snapshot.load_profile[1].phase_current_ma[0] = 1550u;
    snapshot.load_profile[1].phase_current_ma[1] = 1650u;
    snapshot.load_profile[1].phase_current_ma[2] = 1450u;

    snapshot.valid_mask = AMI_COSEM_VALID_EVENT_LOG |
                          AMI_COSEM_VALID_BILLING_PROFILE |
                          AMI_COSEM_VALID_LOAD_PROFILE;

    AmiCosemData out;

    /* 1. Event log buffer & entries */
    assert(ami_cosem_register_read(&snapshot, AMI_COSEM_PROFILE_GENERIC_CLASS_ID,
        obis_event_log, 2u, &out) == AMI_COSEM_OK);
    assert(out.type == AMI_COSEM_TYPE_ENCODED && out.length > 2u);
    assert(out.value.encoded[0] == 1u && out.value.encoded[1] == 1u);
    assert(out.value.encoded[2] == AMI_COSEM_TYPE_STRUCTURE && out.value.encoded[3] == 2u);
    assert(ami_cosem_register_read(&snapshot, AMI_COSEM_PROFILE_GENERIC_CLASS_ID,
        obis_event_log, 7u, &out) == AMI_COSEM_OK);
    assert(out.value.encoded[0] == AMI_COSEM_TYPE_DOUBLE_LONG_UNSIGNED);
    assert(out.value.encoded[4] == 1u);

    /* 2. Billing profile buffer & capture objects (5 columns) */
    assert(ami_cosem_register_read(&snapshot, AMI_COSEM_PROFILE_GENERIC_CLASS_ID,
        obis_billing_profile, 2u, &out) == AMI_COSEM_OK);
    assert(out.type == AMI_COSEM_TYPE_ENCODED && out.length > 2u);
    assert(out.value.encoded[0] == 1u && out.value.encoded[1] == 1u);
    assert(out.value.encoded[2] == AMI_COSEM_TYPE_STRUCTURE && out.value.encoded[3] == 5u);
    assert(ami_cosem_register_read(&snapshot, AMI_COSEM_PROFILE_GENERIC_CLASS_ID,
        obis_billing_profile, 3u, &out) == AMI_COSEM_OK);
    assert(out.type == AMI_COSEM_TYPE_ENCODED && out.value.encoded[0] == 1u && out.value.encoded[1] == 5u);
    assert(ami_cosem_register_read(&snapshot, AMI_COSEM_PROFILE_GENERIC_CLASS_ID,
        obis_billing_profile, 4u, &out) == AMI_COSEM_OK);
    assert(out.value.encoded[0] == AMI_COSEM_TYPE_DOUBLE_LONG_UNSIGNED && out.value.encoded[4] == 0u);
    assert(ami_cosem_register_read(&snapshot, AMI_COSEM_PROFILE_GENERIC_CLASS_ID,
        obis_billing_profile, 8u, &out) == AMI_COSEM_OK);
    assert(out.value.encoded[0] == AMI_COSEM_TYPE_DOUBLE_LONG_UNSIGNED);
    assert(out.value.encoded[4] == AMI_COSEM_PROFILE_MAX_ENTRIES);

    /* 3. Load profile buffer & capture objects (12 columns, capture period 900s) */
    assert(ami_cosem_register_read(&snapshot, AMI_COSEM_PROFILE_GENERIC_CLASS_ID,
        obis_load_profile, 2u, &out) == AMI_COSEM_OK);
    assert(out.type == AMI_COSEM_TYPE_ENCODED && out.length > 2u);
    assert(out.value.encoded[0] == 1u && out.value.encoded[1] == 2u);
    assert(out.value.encoded[2] == AMI_COSEM_TYPE_STRUCTURE && out.value.encoded[3] == 12u);
    assert(ami_cosem_register_read(&snapshot, AMI_COSEM_PROFILE_GENERIC_CLASS_ID,
        obis_load_profile, 3u, &out) == AMI_COSEM_OK);
    assert(out.type == AMI_COSEM_TYPE_ENCODED && out.value.encoded[0] == 1u && out.value.encoded[1] == 12u);
    assert(ami_cosem_register_read(&snapshot, AMI_COSEM_PROFILE_GENERIC_CLASS_ID,
        obis_load_profile, 4u, &out) == AMI_COSEM_OK);
    /* 900 seconds = 0x00000384 */
    assert(out.value.encoded[0] == AMI_COSEM_TYPE_DOUBLE_LONG_UNSIGNED);
    assert(out.value.encoded[3] == 0x03u && out.value.encoded[4] == 0x84u);

    /* 4. Selective Access - Entry Selector (Selector 2) */
    AmiCosemProfileSelection entry_sel = {
        .selector = AMI_COSEM_PROFILE_SELECTOR_ENTRY,
        .from_entry = 2u,
        .to_entry = 2u
    };
    assert(ami_cosem_register_read_selected(&snapshot, AMI_COSEM_PROFILE_GENERIC_CLASS_ID,
        obis_load_profile, 2u, &entry_sel, &out) == AMI_COSEM_OK);
    assert(out.value.encoded[0] == 1u && out.value.encoded[1] == 1u); /* exactly 1 entry */

    /* Entry Selector bounds violations */
    entry_sel.from_entry = 0u;
    assert(ami_cosem_register_read_selected(&snapshot, AMI_COSEM_PROFILE_GENERIC_CLASS_ID,
        obis_load_profile, 2u, &entry_sel, &out) == AMI_COSEM_SCOPE_VIOLATED);

    entry_sel.from_entry = 3u;
    entry_sel.to_entry = 2u;
    assert(ami_cosem_register_read_selected(&snapshot, AMI_COSEM_PROFILE_GENERIC_CLASS_ID,
        obis_load_profile, 2u, &entry_sel, &out) == AMI_COSEM_SCOPE_VIOLATED);

    entry_sel.from_entry = 1u;
    entry_sel.to_entry = 5u; /* exceeds count of 2 */
    assert(ami_cosem_register_read_selected(&snapshot, AMI_COSEM_PROFILE_GENERIC_CLASS_ID,
        obis_load_profile, 2u, &entry_sel, &out) == AMI_COSEM_SCOPE_VIOLATED);

    /* 5. Selective Access - Range Selector (Selector 1) */
    AmiCosemProfileSelection range_sel = {
        .selector = AMI_COSEM_PROFILE_SELECTOR_RANGE,
        .from_date = {
            .year = {0x07u, 0xEAu}, .month = 10u, .day = 15u,
            .day_of_week = 0xFFu, .hour = 12u, .minute = 10u, .second = 0u,
            .hundredths = 0u, .deviation = {0x80u, 0x00u}, .status = 0xFFu
        },
        .to_date = {
            .year = {0x07u, 0xEAu}, .month = 10u, .day = 15u,
            .day_of_week = 0xFFu, .hour = 12u, .minute = 20u, .second = 0u,
            .hundredths = 0u, .deviation = {0x80u, 0x00u}, .status = 0xFFu
        }
    };
    assert(ami_cosem_register_read_selected(&snapshot, AMI_COSEM_PROFILE_GENERIC_CLASS_ID,
        obis_load_profile, 2u, &range_sel, &out) == AMI_COSEM_OK);
    assert(out.value.encoded[0] == 1u && out.value.encoded[1] == 1u); /* matched second entry */

    /* Range reversed violation */
    range_sel.from_date.hour = 13u;
    assert(ami_cosem_register_read_selected(&snapshot, AMI_COSEM_PROFILE_GENERIC_CLASS_ID,
        obis_load_profile, 2u, &range_sel, &out) == AMI_COSEM_SCOPE_VIOLATED);

    /* Selection on non-buffer attribute should return scope violated */
    assert(ami_cosem_register_read_selected(&snapshot, AMI_COSEM_PROFILE_GENERIC_CLASS_ID,
        obis_load_profile, 1u, &entry_sel, &out) == AMI_COSEM_SCOPE_VIOLATED);
    assert(ami_cosem_register_read_selected(&snapshot, AMI_COSEM_PROFILE_GENERIC_CLASS_ID,
        obis_load_profile, 3u, &entry_sel, &out) == AMI_COSEM_SCOPE_VIOLATED);

    snapshot.event_log_count = AMI_COSEM_PROFILE_MAX_ENTRIES + 1u;
    assert(ami_cosem_register_read(&snapshot, AMI_COSEM_PROFILE_GENERIC_CLASS_ID,
        obis_event_log, 2u, &out) == AMI_COSEM_INVALID_ARGUMENT);
}

static void test_security_setup_class64_metadata(void)
{
    AmiCosemRegisterSnapshot snapshot = {
        .security_policy = 3u,
        .security_suite = 0u,
        .client_system_title = {'C', 'L', 'I', 'E', 'N', 'T', '0', '1'},
        .server_system_title = {'M', 'E', 'T', 'E', 'R', '0', '0', '1'},
        .valid_mask = AMI_COSEM_VALID_SECURITY_CLIENT_TITLE
    };
    AmiCosemData out;
    assert(ami_cosem_register_read(&snapshot, AMI_COSEM_SECURITY_SETUP_CLASS_ID,
        obis_security_setup, 2u, &out) == AMI_COSEM_OK);
    assert(out.type == AMI_COSEM_TYPE_ENUM && out.value.unsigned_integer == 3u);
    assert(ami_cosem_register_read(&snapshot, AMI_COSEM_SECURITY_SETUP_CLASS_ID,
        obis_security_setup, 3u, &out) == AMI_COSEM_OK);
    assert(out.type == AMI_COSEM_TYPE_ENUM && out.value.unsigned_integer == 0u);
    assert(ami_cosem_register_read(&snapshot, AMI_COSEM_SECURITY_SETUP_CLASS_ID,
        obis_security_setup, 4u, &out) == AMI_COSEM_OK);
    assert(out.type == AMI_COSEM_TYPE_OCTET_STRING && out.length == 8u);
    assert(memcmp(out.value.octets, snapshot.client_system_title, 8u) == 0);
    assert(ami_cosem_register_read(&snapshot, AMI_COSEM_SECURITY_SETUP_CLASS_ID,
        obis_security_setup, 6u, &out) == AMI_COSEM_OK);
    assert(out.type == AMI_COSEM_TYPE_ENCODED && out.length == 2u &&
           out.value.encoded[0] == 1u && out.value.encoded[1] == 0u);
}

int main(void)
{
    test_voltage_and_scaler();
    test_current_energy_and_logical_name();
    test_validity_and_invalid_reads();
    test_clock_attributes();
    test_profile_generic_buffers();
    test_security_setup_class64_metadata();
    puts("COSEM model tests passed.");
    return 0;
}
