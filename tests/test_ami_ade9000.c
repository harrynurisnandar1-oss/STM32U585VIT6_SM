#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ami_ade9000.h"

/* Mock SPI register memory */
typedef struct {
    uint16_t reg;
    uint32_t val;
} MockRegEntry;

static MockRegEntry mock_registers[32];
static size_t mock_reg_count = 0;

static void mock_spi_set(uint16_t reg, uint32_t val) {
    for (size_t i = 0; i < mock_reg_count; i++) {
        if (mock_registers[i].reg == reg) {
            mock_registers[i].val = val;
            return;
        }
    }
    if (mock_reg_count < 32) {
        mock_registers[mock_reg_count].reg = reg;
        mock_registers[mock_reg_count].val = val;
        mock_reg_count++;
    }
}

static int mock_spi_transfer(uint16_t reg_addr, uint8_t is_read, uint32_t *data, void *user_data) {
    (void)user_data;
    if (is_read) {
        for (size_t i = 0; i < mock_reg_count; i++) {
            if (mock_registers[i].reg == reg_addr) {
                *data = mock_registers[i].val;
                return 0;
            }
        }
        *data = 0;
        return 0;
    } else {
        mock_spi_set(reg_addr, *data);
        return 0;
    }
}

static void test_ade9000_init_and_rw(void) {
    AmiAde9000Context ctx;
    assert(ami_ade9000_init(NULL, NULL, NULL, NULL) == -1);
    assert(ami_ade9000_init(&ctx, NULL, mock_spi_transfer, NULL) == 0);
    assert(ctx.is_initialized == 1);

    /* Test write and read */
    uint32_t val = 0;
    assert(ami_ade9000_write_reg(&ctx, ADE9000_REG_STATUS0, 0x12345678) == 0);
    assert(ami_ade9000_read_reg(&ctx, ADE9000_REG_STATUS0, &val) == 0);
    assert(val == 0x12345678);
    printf("test_ade9000_init_and_rw: PASS\n");
}

static void test_ade9000_acquire_and_scale(void) {
    AmiAde9000Context ctx;
    ami_ade9000_init(&ctx, NULL, mock_spi_transfer, NULL);

    /* Populate simulated nominal values for 3-phase 230V / 5A */
    mock_spi_set(ADE9000_REG_AVRMS, 52702092u); /* Nominal 230,000 mV */
    mock_spi_set(ADE9000_REG_BVRMS, 52702092u);
    mock_spi_set(ADE9000_REG_CVRMS, 52702092u);

    mock_spi_set(ADE9000_REG_AIRMS, 26351046u); /* Nominal 5,000 mA */
    mock_spi_set(ADE9000_REG_BIRMS, 26351046u);
    mock_spi_set(ADE9000_REG_CIRMS, 26351046u);

    mock_spi_set(ADE9000_REG_AWATTHR_HI, 100u); /* 100 Wh per phase */
    mock_spi_set(ADE9000_REG_BWATTHR_HI, 150u);
    mock_spi_set(ADE9000_REG_CWATTHR_HI, 250u);

    mock_spi_set(ADE9000_REG_APERIOD, 159999u); /* 8000000000 / 160000 = 50000 mHz = 50 Hz */

    assert(ami_ade9000_acquire_all(&ctx) == 0);

    /* Check scaled voltages */
    assert(ctx.scaled.phase_voltage_mv[0] == 230000);
    assert(ctx.scaled.phase_voltage_mv[1] == 230000);
    assert(ctx.scaled.phase_voltage_mv[2] == 230000);

    /* Check scaled currents */
    assert(ctx.scaled.phase_current_ma[0] == 5000);
    assert(ctx.scaled.phase_current_ma[1] == 5000);
    assert(ctx.scaled.phase_current_ma[2] == 5000);

    /* Check frequency */
    assert(ctx.scaled.frequency_mhz == 50000);

    /* Check energy */
    assert(ctx.scaled.active_import_wh == 500);

    printf("test_ade9000_acquire_and_scale: PASS\n");
}

static void test_ade9000_feed_cosem_snapshot(void) {
    AmiAde9000Context ctx;
    ami_ade9000_init(&ctx, NULL, mock_spi_transfer, NULL);
    ami_ade9000_acquire_all(&ctx);

    AmiCosemRegisterSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));

    assert(ami_ade9000_feed_cosem_snapshot(&ctx, &snapshot) == 0);

    /* Verify snapshot values match metrology output */
    assert(snapshot.phase_voltage_millivolts[0] == 230000);
    assert(snapshot.phase_voltage_millivolts[1] == 230000);
    assert(snapshot.phase_voltage_millivolts[2] == 230000);

    assert(snapshot.phase_current_milliamps[0] == 5000);
    assert(snapshot.phase_current_milliamps[1] == 5000);
    assert(snapshot.phase_current_milliamps[2] == 5000);

    assert(snapshot.active_import_wh == 500);
    assert((snapshot.valid_mask & AMI_COSEM_VALID_VOLTAGE_A) != 0);
    assert((snapshot.valid_mask & AMI_COSEM_VALID_CURRENT_A) != 0);
    assert((snapshot.valid_mask & AMI_COSEM_VALID_ACTIVE_IMPORT) != 0);

    printf("test_ade9000_feed_cosem_snapshot: PASS\n");
}

static int mock_failing_spi_transfer(uint16_t reg_addr, uint8_t is_read, uint32_t *data, void *user_data) {
    (void)reg_addr;
    (void)is_read;
    (void)data;
    (void)user_data;
    return -1; /* Simulated hardware SPI transmission failure or disconnected cable */
}

static void test_ade9000_spi_error_handling(void) {
    AmiAde9000Context ctx;
    assert(ami_ade9000_init(&ctx, NULL, mock_failing_spi_transfer, NULL) == 0);

    /* 1. Acquire must propagate SPI error code and clear is_valid */
    int st = ami_ade9000_acquire_all(&ctx);
    assert(st == AMI_ADE9000_ERR_SPI);
    assert(ctx.scaled.is_valid == 0u);
    assert(ctx.scaled.last_error_code == AMI_ADE9000_ERR_SPI);

    /* 2. Snapshot feeder must reject feeding invalid data */
    AmiCosemRegisterSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    int feed_st = ami_ade9000_feed_cosem_snapshot(&ctx, &snapshot);
    assert(feed_st == AMI_ADE9000_ERR_INVALID_DATA);
    assert(snapshot.valid_mask == 0u); /* Zero valid mask ensures fail-closed behavior */

    printf("test_ade9000_spi_error_handling: PASS\n");
}

int main(void) {
    printf("Starting ADE9000 Metrology Driver Tests...\n");
    test_ade9000_init_and_rw();
    test_ade9000_acquire_and_scale();
    test_ade9000_feed_cosem_snapshot();
    test_ade9000_spi_error_handling();
    printf("All ADE9000 Metrology Driver Tests PASSED!\n");
    return 0;
}

