#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ami_nvm_counter.h"
#include "ami_dlms_cipher.h"
#include "ami_dlms_port.h"

/* Instrument driver to track read/write operations */
typedef struct {
    AmiNvmCounterRecord storage;
    uint32_t read_count;
    uint32_t write_count;
} TrackedStorage;

static int tracked_read(void *user_data, AmiNvmCounterRecord *record)
{
    TrackedStorage *ts = (TrackedStorage *)user_data;
    ts->read_count++;
    *record = ts->storage;
    return 0;
}

static int tracked_write(void *user_data, const AmiNvmCounterRecord *record)
{
    TrackedStorage *ts = (TrackedStorage *)user_data;
    ts->write_count++;
    ts->storage = *record;
    return 0;
}

static void setup_tracked_driver(AmiNvmStorageDriver *drv, TrackedStorage *ts)
{
    memset(ts, 0, sizeof(*ts));
    drv->read_record = tracked_read;
    drv->write_record = tracked_write;
    drv->user_data = ts;
}

static void test_nvm_counter_fresh_init(void)
{
    TrackedStorage ts;
    AmiNvmStorageDriver drv;
    setup_tracked_driver(&drv, &ts);

    /* Storage is completely zeroed (unformatted) */
    AmiNvmCounterContext ctx;
    int rc = ami_nvm_counter_init(&ctx, &drv, 50, 10);
    assert(rc == 0);

    /* Should have read once, recognized blank/invalid CRC, and written initial lease */
    assert(ts.read_count == 1);
    assert(ts.write_count == 1);
    assert(ctx.reboot_count == 1);
    assert(ctx.current_tx_counter == 1);
    assert(ctx.persisted_tx_limit == 51); /* 1 + 50 */
    assert(ctx.last_rx_counter == 0);
    assert(ctx.persisted_rx_counter == 0);

    /* Verify stored record integrity */
    assert(ts.storage.magic == AMI_NVM_RECORD_MAGIC);
    assert(ts.storage.version == AMI_NVM_RECORD_VERSION);
    assert(ts.storage.tx_invocation_counter == 51);
    assert(ts.storage.rx_invocation_counter == 0);
    assert(ts.storage.reboot_count == 1);

    uint32_t expected_crc = ami_nvm_calculate_crc32((const uint8_t *)&ts.storage,
                                                    sizeof(AmiNvmCounterRecord) - sizeof(uint32_t));
    assert(ts.storage.crc32 == expected_crc);

    printf("test_nvm_counter_fresh_init: PASS\n");
}

static void test_nvm_counter_monotonic_and_lease_window(void)
{
    TrackedStorage ts;
    AmiNvmStorageDriver drv;
    setup_tracked_driver(&drv, &ts);

    AmiNvmCounterContext ctx;
    assert(ami_nvm_counter_init(&ctx, &drv, 10, 5) == 0);
    assert(ts.write_count == 1); /* Initial lease commit: limit = 1 + 10 = 11 */
    assert(ctx.persisted_tx_limit == 11);

    /* Generate 9 counters: should be 2, 3, 4, 5, 6, 7, 8, 9, 10 */
    for (uint32_t i = 2; i <= 10; i++) {
        uint32_t ic = 0;
        assert(ami_nvm_counter_next_tx(&ctx, &ic) == 1);
        assert(ic == i);
        /* Flash wear mitigation: No writes should occur while within lease window */
        assert(ts.write_count == 1);
    }

    /* Counter 11: reaches persisted_tx_limit, triggers lease expansion to 11 + 10 = 21 */
    uint32_t ic11 = 0;
    assert(ami_nvm_counter_next_tx(&ctx, &ic11) == 1);
    assert(ic11 == 11);
    assert(ts.write_count == 2); /* Write committed to NVM */
    assert(ctx.persisted_tx_limit == 21);
    assert(ts.storage.tx_invocation_counter == 21);

    /* Counter 12: inside new lease window, no write */
    uint32_t ic12 = 0;
    assert(ami_nvm_counter_next_tx(&ctx, &ic12) == 1);
    assert(ic12 == 12);
    assert(ts.write_count == 2);

    printf("test_nvm_counter_monotonic_and_lease_window: PASS\n");
}

static void test_nvm_counter_reboot_and_nonce_uniqueness(void)
{
    TrackedStorage ts;
    AmiNvmStorageDriver drv;
    setup_tracked_driver(&drv, &ts);

    /* First boot cycle */
    {
        AmiNvmCounterContext ctx1;
        assert(ami_nvm_counter_init(&ctx1, &drv, 100, 10) == 0);
        /* Initial limit is 101 */
        assert(ctx1.persisted_tx_limit == 101);

        /* Use counters up to 15 */
        for (int i = 0; i < 14; i++) {
            uint32_t ic = 0;
            assert(ami_nvm_counter_next_tx(&ctx1, &ic) == 1);
        }
        assert(ctx1.current_tx_counter == 15);
    }

    /* Simulated Sudden Power Loss / Brown-Out:
     * Device shuts down immediately. In NVM, tx_invocation_counter was 101.
     * Second boot cycle:
     */
    {
        AmiNvmCounterContext ctx2;
        assert(ami_nvm_counter_init(&ctx2, &drv, 100, 10) == 0);
        /* Reboot count incremented */
        assert(ctx2.reboot_count == 2);

        /* Crucial Nonce Uniqueness Guarantee:
         * ctx2 MUST jump strictly above previous session's lease limit (101),
         * guaranteeing zero risk of AES-128-GCM nonce collision across power cuts!
         */
        assert(ctx2.current_tx_counter == 101);
        assert(ctx2.persisted_tx_limit == 201);
        assert(ts.storage.tx_invocation_counter == 201);

        uint32_t first_ic = 0;
        assert(ami_nvm_counter_next_tx(&ctx2, &first_ic) == 1);
        assert(first_ic == 102);
        assert(first_ic > 15);
        assert(first_ic > 101);
    }

    printf("test_nvm_counter_reboot_and_nonce_uniqueness: PASS\n");
}

static void test_nvm_counter_anti_replay(void)
{
    TrackedStorage ts;
    AmiNvmStorageDriver drv;
    setup_tracked_driver(&drv, &ts);

    AmiNvmCounterContext ctx;
    assert(ami_nvm_counter_init(&ctx, &drv, 100, 5) == 0);
    assert(ctx.last_rx_counter == 0);

    /* Zero counter must be rejected */
    assert(ami_nvm_counter_accept_rx(&ctx, 0) == 0);

    /* Fresh valid counter */
    assert(ami_nvm_counter_accept_rx(&ctx, 10) == 1);
    assert(ctx.last_rx_counter == 10);

    /* Duplicate counter must be rejected (replay attack) */
    assert(ami_nvm_counter_accept_rx(&ctx, 10) == 0);

    /* Smaller counter must be rejected (replay attack) */
    assert(ami_nvm_counter_accept_rx(&ctx, 9) == 0);
    assert(ami_nvm_counter_accept_rx(&ctx, 1) == 0);

    /* Larger counter accepted */
    assert(ami_nvm_counter_accept_rx(&ctx, 12) == 1);
    assert(ctx.last_rx_counter == 12);

    /* Step commit check: rx_window_step = 5.
     * When last_rx_counter (15) >= persisted_rx_counter (0) + 5, commits to NVM.
     */
    uint32_t writes_before = ts.write_count;
    assert(ami_nvm_counter_accept_rx(&ctx, 15) == 1);
    assert(ts.write_count == writes_before + 1);
    assert(ctx.persisted_rx_counter == 15);
    assert(ts.storage.rx_invocation_counter == 15);

    /* Force flush check */
    assert(ami_nvm_counter_accept_rx(&ctx, 18) == 1);
    assert(ami_nvm_counter_flush(&ctx) == 0);
    assert(ts.storage.rx_invocation_counter == 18);
    assert(ts.storage.tx_invocation_counter == ctx.persisted_tx_limit);

    printf("test_nvm_counter_anti_replay: PASS\n");
}

static void test_nvm_counter_reboot_preserves_anti_replay(void)
{
    TrackedStorage ts;
    AmiNvmStorageDriver drv;
    setup_tracked_driver(&drv, &ts);

    /* First boot: accept up to RX IC = 500 and flush */
    {
        AmiNvmCounterContext ctx1;
        assert(ami_nvm_counter_init(&ctx1, &drv, 100, 10) == 0);
        assert(ami_nvm_counter_accept_rx(&ctx1, 500) == 1);
        assert(ami_nvm_counter_flush(&ctx1) == 0);
    }

    /* Second boot: power restored */
    {
        AmiNvmCounterContext ctx2;
        assert(ami_nvm_counter_init(&ctx2, &drv, 100, 10) == 0);
        assert(ctx2.last_rx_counter == 500);

        /* Replay of an APDU with IC <= 500 must fail */
        assert(ami_nvm_counter_accept_rx(&ctx2, 500) == 0);
        assert(ami_nvm_counter_accept_rx(&ctx2, 499) == 0);
        assert(ami_nvm_counter_accept_rx(&ctx2, 1) == 0);

        /* Only fresh IC > 500 accepted */
        assert(ami_nvm_counter_accept_rx(&ctx2, 501) == 1);
        assert(ctx2.last_rx_counter == 501);
    }

    printf("test_nvm_counter_reboot_preserves_anti_replay: PASS\n");
}

static void test_nvm_suite0_cipher_binding(void)
{
    AmiNvmCounterRecord ram_storage;
    memset(&ram_storage, 0, sizeof(ram_storage));

    AmiNvmStorageDriver drv;
    ami_nvm_create_ram_driver(&drv, &ram_storage);

    AmiNvmCounterContext nvm_ctx;
    assert(ami_nvm_counter_init(&nvm_ctx, &drv, 100, 10) == 0);

    const uint8_t TEST_GUEK[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                   0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    const uint8_t TEST_AK[16]   = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                                   0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
    const uint8_t CLIENT_TITLE[8] = {'P', 'L', 'N', 'C', 'L', 'I', '0', '1'};
    const uint8_t SERVER_TITLE[8] = {'P', 'L', 'N', 'M', 'T', 'R', '0', '1'};

    AmiDlmsSecuritySuite0Context cipher_ctx;
    assert(ami_dlms_cipher_suite0_init(&cipher_ctx, TEST_GUEK, TEST_AK,
                                       CLIENT_TITLE, SERVER_TITLE, 0) == 0);

    /* Bind NVM subsystem to Cipher Suite 0 */
    assert(ami_dlms_cipher_suite0_bind_nvm(&cipher_ctx, &nvm_ctx) == 0);

    AmiDlmsCipherBackend backend;
    assert(ami_dlms_cipher_suite0_get_backend(&cipher_ctx, &backend) == 0);

    /* Test TX allocation through Suite 0 callback */
    uint32_t allocated_tx = 0;
    assert(backend.next_tx_counter(backend.user, &allocated_tx) == 1);
    assert(allocated_tx == 2); /* 1 was initial, next is 2 */
    assert(nvm_ctx.current_tx_counter == 2);

    /* Test RX validation through Suite 0 callback */
    assert(backend.accept_rx_counter(backend.user, 50) == 1);
    assert(nvm_ctx.last_rx_counter == 50);

    /* Replay must fail */
    assert(backend.accept_rx_counter(backend.user, 50) == 0);
    assert(backend.accept_rx_counter(backend.user, 49) == 0);

    printf("test_nvm_suite0_cipher_binding: PASS\n");
}

int main(void)
{
    printf("Starting AMI NVM Invocation Counter & Anti-Replay Tests...\n");
    test_nvm_counter_fresh_init();
    test_nvm_counter_monotonic_and_lease_window();
    test_nvm_counter_reboot_and_nonce_uniqueness();
    test_nvm_counter_anti_replay();
    test_nvm_counter_reboot_preserves_anti_replay();
    test_nvm_suite0_cipher_binding();
    printf("All AMI NVM Invocation Counter & Anti-Replay Tests PASSED!\n");
    return 0;
}
