#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Mocking TrustZone memory space layout for testing */
#define MOCK_SECURE_FLASH_START     0x0C000000UL
#define MOCK_SECURE_FLASH_END       0x0C0FFFFFUL
#define MOCK_SECURE_RAM_START       0x30000000UL
#define MOCK_SECURE_RAM_END         0x3004FFFFUL

#define MOCK_NONSECURE_FLASH_START  0x08100000UL
#define MOCK_NONSECURE_FLASH_END    0x081FFFFFUL
#define MOCK_NONSECURE_RAM_START    0x20000000UL
#define MOCK_NONSECURE_RAM_END      0x200BFFFFUL

/* CMSE simulation flags */
#define CMSE_MPU_READ       0x01
#define CMSE_MPU_READWRITE  0x02
#define CMSE_NONSECURE      0x04

static void *mock_cmse_check_address_range(void *ptr, size_t size, int flags)
{
    uintptr_t start = (uintptr_t)ptr;
    if (size == 0) return ptr;
    if (start + size < start) return NULL; /* Overflow */
    uintptr_t end = start + size - 1u;

    /* Check if target range is inside NonSecure RAM */
    int is_ns_ram = (start >= MOCK_NONSECURE_RAM_START && end <= MOCK_NONSECURE_RAM_END);
    /* Check if target range is inside NonSecure Flash */
    int is_ns_flash = (start >= MOCK_NONSECURE_FLASH_START && end <= MOCK_NONSECURE_FLASH_END);

    /* If write access requested, Flash is read-only so it must fail */
    if ((flags & CMSE_MPU_READWRITE) && is_ns_flash) {
        return NULL;
    }

    if (is_ns_ram || is_ns_flash) {
        return ptr;
    }

    /* Target falls into Secure memory or invalid area */
    return NULL;
}

static int mock_validate_ns_buffer(const void *ptr, size_t size, int is_write)
{
    if (ptr == NULL && size == 0) {
        return 0;
    }
    if (ptr == NULL || size == 0) {
        return -1;
    }
    uintptr_t uptr = (uintptr_t)ptr;
    if (uptr + size < uptr) {
        return -1;
    }
    int flags = CMSE_NONSECURE | (is_write ? CMSE_MPU_READWRITE : CMSE_MPU_READ);
    if (mock_cmse_check_address_range((void *)ptr, size, flags) == NULL) {
        return -1;
    }
    return 0;
}

static void test_null_buffer(void)
{
    assert(mock_validate_ns_buffer(NULL, 0, 0) == 0);
    assert(mock_validate_ns_buffer(NULL, 16, 0) == -1);
    assert(mock_validate_ns_buffer(NULL, 16, 1) == -1);
    uint8_t dummy = 0;
    assert(mock_validate_ns_buffer(&dummy, 0, 0) == -1);
    printf("  [PASS] test_null_buffer\n");
}

static void test_integer_overflow(void)
{
    void *ptr = (void *)((uintptr_t)-8);
    assert(mock_validate_ns_buffer(ptr, 16, 0) == -1);
    assert(mock_validate_ns_buffer(ptr, 16, 1) == -1);
    assert(mock_validate_ns_buffer((void *)0xFFFFFFFFUL, 10, 0) == -1);
    printf("  [PASS] test_integer_overflow\n");
}

static void test_secure_world_boundary_violation(void)
{
    /* Pointer directly inside Secure RAM */
    void *sec_ram = (void *)MOCK_SECURE_RAM_START;
    assert(mock_validate_ns_buffer(sec_ram, 32, 0) == -1);
    assert(mock_validate_ns_buffer(sec_ram, 32, 1) == -1);

    /* Pointer directly inside Secure Flash */
    void *sec_flash = (void *)MOCK_SECURE_FLASH_START;
    assert(mock_validate_ns_buffer(sec_flash, 64, 0) == -1);
    assert(mock_validate_ns_buffer(sec_flash, 64, 1) == -1);

    /* Buffer crossing from NonSecure RAM into invalid / Secure address */
    void *crossing = (void *)(MOCK_NONSECURE_RAM_END - 10u);
    assert(mock_validate_ns_buffer(crossing, 20, 0) == -1);
    assert(mock_validate_ns_buffer(crossing, 20, 1) == -1);
    printf("  [PASS] test_secure_world_boundary_violation\n");
}

static void test_nonsecure_valid_access(void)
{
    /* Read-write to valid NonSecure RAM */
    void *ns_ram = (void *)(MOCK_NONSECURE_RAM_START + 0x1000UL);
    assert(mock_validate_ns_buffer(ns_ram, 128, 0) == 0);
    assert(mock_validate_ns_buffer(ns_ram, 128, 1) == 0);

    /* Read from valid NonSecure Flash */
    void *ns_flash = (void *)(MOCK_NONSECURE_FLASH_START + 0x2000UL);
    assert(mock_validate_ns_buffer(ns_flash, 256, 0) == 0);
    /* Write to NonSecure Flash must fail */
    assert(mock_validate_ns_buffer(ns_flash, 256, 1) == -1);
    printf("  [PASS] test_nonsecure_valid_access\n");
}

static void test_volatile_zeroization_verification(void)
{
    volatile uint8_t secret_buf[64];
    for (size_t i = 0; i < sizeof(secret_buf); ++i) {
        secret_buf[i] = (uint8_t)(i ^ 0xA5);
    }

    /* Zeroize */
    volatile uint8_t *p = secret_buf;
    size_t len = sizeof(secret_buf);
    while (len--) {
        *p++ = 0u;
    }
#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("" : : "r"((void *)secret_buf) : "memory");
#endif

    for (size_t i = 0; i < sizeof(secret_buf); ++i) {
        assert(secret_buf[i] == 0u);
    }
    printf("  [PASS] test_volatile_zeroization_verification\n");
}

int main(void)
{
    printf("=== Starting AmiTrustZoneBoundaryTest ===\n");
    test_null_buffer();
    test_integer_overflow();
    test_secure_world_boundary_violation();
    test_nonsecure_valid_access();
    test_volatile_zeroization_verification();
    printf(">>> ALL TRUSTZONE BOUNDARY & CMSE TESTS PASSED SUCCESSFULLY! <<<\n");
    return 0;
}
