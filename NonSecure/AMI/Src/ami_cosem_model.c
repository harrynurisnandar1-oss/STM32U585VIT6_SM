#include "ami_cosem_model.h"

#include <string.h>

_Static_assert(sizeof(AmiCosemDateTime) == 12u,
               "COSEM Clock date-time representation must be 12 bytes");

enum {
    COSEM_UNIT_ACTIVE_ENERGY_WH = 30,
    COSEM_UNIT_REACTIVE_ENERGY_VARH = 32,
    COSEM_UNIT_CURRENT_AMPERE = 33,
    COSEM_UNIT_VOLT = 35
};

typedef enum {
    VALUE_VOLTAGE,
    VALUE_CURRENT,
    VALUE_ACTIVE_IMPORT,
    VALUE_ACTIVE_EXPORT,
    VALUE_REACTIVE_IMPORT,
    VALUE_REACTIVE_EXPORT
} ValueSource;

typedef struct {
    uint8_t logical_name[AMI_COSEM_LOGICAL_NAME_LENGTH];
    ValueSource source;
    uint8_t phase;
    AmiCosemDataType value_type;
    int8_t scaler;
    uint8_t unit;
    uint32_t valid_mask;
} RegisterDefinition;

#define OBIS(a, b, c, d, e, f) {a, b, c, d, e, f}

static const RegisterDefinition registers[] = {
    {OBIS(1, 0, 32, 7, 0, 255), VALUE_VOLTAGE, 0, AMI_COSEM_TYPE_DOUBLE_LONG_UNSIGNED, -3, COSEM_UNIT_VOLT, AMI_COSEM_VALID_VOLTAGE_A},
    {OBIS(1, 0, 52, 7, 0, 255), VALUE_VOLTAGE, 1, AMI_COSEM_TYPE_DOUBLE_LONG_UNSIGNED, -3, COSEM_UNIT_VOLT, AMI_COSEM_VALID_VOLTAGE_B},
    {OBIS(1, 0, 72, 7, 0, 255), VALUE_VOLTAGE, 2, AMI_COSEM_TYPE_DOUBLE_LONG_UNSIGNED, -3, COSEM_UNIT_VOLT, AMI_COSEM_VALID_VOLTAGE_C},
    {OBIS(1, 0, 31, 7, 0, 255), VALUE_CURRENT, 0, AMI_COSEM_TYPE_DOUBLE_LONG_UNSIGNED, -3, COSEM_UNIT_CURRENT_AMPERE, AMI_COSEM_VALID_CURRENT_A},
    {OBIS(1, 0, 51, 7, 0, 255), VALUE_CURRENT, 1, AMI_COSEM_TYPE_DOUBLE_LONG_UNSIGNED, -3, COSEM_UNIT_CURRENT_AMPERE, AMI_COSEM_VALID_CURRENT_B},
    {OBIS(1, 0, 71, 7, 0, 255), VALUE_CURRENT, 2, AMI_COSEM_TYPE_DOUBLE_LONG_UNSIGNED, -3, COSEM_UNIT_CURRENT_AMPERE, AMI_COSEM_VALID_CURRENT_C},
    {OBIS(1, 0, 1, 8, 0, 255), VALUE_ACTIVE_IMPORT, 0, AMI_COSEM_TYPE_LONG64_UNSIGNED, 0, COSEM_UNIT_ACTIVE_ENERGY_WH, AMI_COSEM_VALID_ACTIVE_IMPORT},
    {OBIS(1, 0, 2, 8, 0, 255), VALUE_ACTIVE_EXPORT, 0, AMI_COSEM_TYPE_LONG64_UNSIGNED, 0, COSEM_UNIT_ACTIVE_ENERGY_WH, AMI_COSEM_VALID_ACTIVE_EXPORT},
    {OBIS(1, 0, 3, 8, 0, 255), VALUE_REACTIVE_IMPORT, 0, AMI_COSEM_TYPE_LONG64_UNSIGNED, 0, COSEM_UNIT_REACTIVE_ENERGY_VARH, AMI_COSEM_VALID_REACTIVE_IMPORT},
    {OBIS(1, 0, 4, 8, 0, 255), VALUE_REACTIVE_EXPORT, 0, AMI_COSEM_TYPE_LONG64_UNSIGNED, 0, COSEM_UNIT_REACTIVE_ENERGY_VARH, AMI_COSEM_VALID_REACTIVE_EXPORT}
};

#undef OBIS

static const uint8_t clock_logical_name[AMI_COSEM_LOGICAL_NAME_LENGTH] =
    {0, 0, 1, 0, 0, 255};

size_t ami_cosem_register_count(void)
{
    return sizeof(registers) / sizeof(registers[0]);
}

size_t ami_cosem_object_count(void)
{
    return ami_cosem_register_count() + 1u;
}

static const RegisterDefinition *find_register(
    const uint8_t logical_name[AMI_COSEM_LOGICAL_NAME_LENGTH])
{
    for (size_t i = 0; i < ami_cosem_register_count(); ++i) {
        if (memcmp(registers[i].logical_name, logical_name,
                   AMI_COSEM_LOGICAL_NAME_LENGTH) == 0) {
            return &registers[i];
        }
    }
    return NULL;
}

static uint64_t read_source(const AmiCosemRegisterSnapshot *snapshot,
                            const RegisterDefinition *definition)
{
    switch (definition->source) {
    case VALUE_VOLTAGE:
        return snapshot->phase_voltage_millivolts[definition->phase];
    case VALUE_CURRENT:
        return snapshot->phase_current_milliamps[definition->phase];
    case VALUE_ACTIVE_IMPORT:
        return snapshot->active_import_wh;
    case VALUE_ACTIVE_EXPORT:
        return snapshot->active_export_wh;
    case VALUE_REACTIVE_IMPORT:
        return snapshot->reactive_import_varh;
    case VALUE_REACTIVE_EXPORT:
        return snapshot->reactive_export_varh;
    }
    return 0;
}

AmiCosemStatus ami_cosem_register_read(
    const AmiCosemRegisterSnapshot *snapshot,
    uint16_t class_id,
    const uint8_t logical_name[AMI_COSEM_LOGICAL_NAME_LENGTH],
    uint8_t attribute_id,
    AmiCosemData *out)
{
    if (snapshot == NULL || logical_name == NULL || out == NULL) {
        return AMI_COSEM_INVALID_ARGUMENT;
    }
    if (class_id != AMI_COSEM_REGISTER_CLASS_ID &&
        class_id != AMI_COSEM_CLOCK_CLASS_ID) {
        return AMI_COSEM_OBJECT_NOT_FOUND;
    }

    if (class_id == AMI_COSEM_CLOCK_CLASS_ID) {
        if (memcmp(logical_name, clock_logical_name,
                   AMI_COSEM_LOGICAL_NAME_LENGTH) != 0) {
            return AMI_COSEM_OBJECT_NOT_FOUND;
        }

        memset(out, 0, sizeof(*out));
        if (attribute_id == 1u) {
            out->type = AMI_COSEM_TYPE_OCTET_STRING;
            out->length = AMI_COSEM_LOGICAL_NAME_LENGTH;
            memcpy(out->value.logical_name, clock_logical_name,
                   AMI_COSEM_LOGICAL_NAME_LENGTH);
            return AMI_COSEM_OK;
        }
        if ((snapshot->valid_mask & AMI_COSEM_VALID_CLOCK) == 0u) {
            return AMI_COSEM_VALUE_UNAVAILABLE;
        }
        switch (attribute_id) {
        case 2:
            out->type = AMI_COSEM_TYPE_OCTET_STRING;
            out->length = sizeof(AmiCosemDateTime);
            out->value.date_time = snapshot->clock_date_time;
            return AMI_COSEM_OK;
        case 3:
            out->type = AMI_COSEM_TYPE_DOUBLE_LONG;
            out->length = sizeof(int32_t);
            out->value.double_long = snapshot->clock_time_zone_minutes;
            return AMI_COSEM_OK;
        case 4:
            out->type = AMI_COSEM_TYPE_UNSIGNED;
            out->length = sizeof(snapshot->clock_status);
            out->value.unsigned_integer = snapshot->clock_status;
            return AMI_COSEM_OK;
        default:
            return AMI_COSEM_ATTRIBUTE_NOT_FOUND;
        }
    }

    const RegisterDefinition *definition = find_register(logical_name);
    if (definition == NULL) {
        return AMI_COSEM_OBJECT_NOT_FOUND;
    }

    memset(out, 0, sizeof(*out));
    switch (attribute_id) {
    case 1:
        out->type = AMI_COSEM_TYPE_OCTET_STRING;
        out->length = AMI_COSEM_LOGICAL_NAME_LENGTH;
        memcpy(out->value.logical_name, definition->logical_name,
               AMI_COSEM_LOGICAL_NAME_LENGTH);
        return AMI_COSEM_OK;
    case 2: {
        if ((snapshot->valid_mask & definition->valid_mask) == 0u) {
            return AMI_COSEM_VALUE_UNAVAILABLE;
        }
        uint64_t value = read_source(snapshot, definition);
        out->type = definition->value_type;
        out->length = definition->value_type == AMI_COSEM_TYPE_DOUBLE_LONG_UNSIGNED
            ? sizeof(uint32_t) : sizeof(uint64_t);
        if (definition->value_type == AMI_COSEM_TYPE_DOUBLE_LONG_UNSIGNED) {
            if (value > UINT32_MAX) return AMI_COSEM_INVALID_ARGUMENT;
            out->value.double_long_unsigned = (uint32_t)value;
        } else {
            out->value.long64_unsigned = value;
        }
        return AMI_COSEM_OK;
    }
    case 3:
        out->type = AMI_COSEM_TYPE_STRUCTURE;
        out->length = 2u;
        out->value.scaler_unit.scaler = definition->scaler;
        out->value.scaler_unit.unit = definition->unit;
        return AMI_COSEM_OK;
    default:
        return AMI_COSEM_ATTRIBUTE_NOT_FOUND;
    }
}
