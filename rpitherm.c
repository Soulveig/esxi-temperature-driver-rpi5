/*
 * rpitherm v0.4.4: periodic Raspberry Pi 5 SoC temperature probe.
 *
 * The module binds only to the UEFI ACPI mailbox device BCM2849 (RPIQ).
 * It does not bind to RP1 Ethernet, SD/MMC, or PWM resources and does not
 * allocate or register an interrupt.  The sole hardware write submits the
 * standard firmware property-mailbox GET_TEMPERATURE request.
 *
 * The low-memory page has no confirmed unload-time release API in the
 * available ESXi-Arm ABI. Install, update, and remove this module only through
 * BootBankInstaller followed by a reboot; live lifecycle operations are not
 * supported.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef int32_t vmk_Status;
typedef void *vmk_Driver;
typedef void *vmk_Device;
typedef void *vmk_ACPIDevice;
typedef void *vmk_HeapID;

extern vmk_Status vmk_DriverRegister(const void *props, vmk_Driver *driver);
extern vmk_Status vmk_DriverUnregister(vmk_Driver driver);
extern vmk_Status vmk_NameInitialize(void *name, const char *text);
extern vmk_Status vmk_HeapCreate(const void *props, vmk_HeapID *heap);
extern void vmk_ModuleSetHeapID(uint32_t module_id, vmk_HeapID heap);
extern void vmk_HeapDestroy(vmk_HeapID heap);
extern vmk_Status vmk_DeviceGetRegistrationData(vmk_Device device,
                                                 void **data);
extern vmk_Status vmk_ACPIQueryInfo(vmk_ACPIDevice device, void *info);
extern vmk_Status vmk_ACPIMapIOResource(uint32_t module_id,
                                        vmk_ACPIDevice device,
                                        uint32_t resource_index,
                                        void *resource);
extern vmk_Status vmk_ACPIUnmapIOResource(uint32_t module_id,
                                          vmk_ACPIDevice device,
                                          uint32_t resource_index);
extern vmk_Status vmk_MappedResourceRead32(const void *resource,
                                            uint64_t offset,
                                            uint32_t *value);
extern vmk_Status vmk_MappedResourceWrite32(void *resource,
                                             uint64_t offset,
                                             uint32_t value);
extern vmk_Status vmk_VA2MA(void *virtual_address,
                            uint64_t length,
                            uint64_t *machine_address);
extern vmk_Status vmk_MemPoolCreate(const void *properties,
                                    void **memory_pool);
extern vmk_Status vmk_MemPoolAllocWithRA(void *memory_pool,
                                         const void *constraints,
                                         void *allocation,
                                         void *return_address);
extern vmk_Status vmk_Map(uint32_t module_id,
                          const void *properties,
                          void **virtual_address);
extern vmk_Status vmk_WorldCreate(const void *properties,
                                  uint32_t *world_id);
extern vmk_Status vmk_WorldDestroy(uint32_t world_id);
extern vmk_Status vmk_WorldSleep(uint64_t microseconds);
extern void vmk_WorldWaitForDeath(uint32_t world_id);
extern vmk_HeapID vmk_ModuleGetHeapID(uint32_t module_id);
extern void vmk_DelayUsecs(uint32_t microseconds);
extern void _vmk_WarningMessage(const char *format, ...);
extern uint32_t vmk_ModuleCurrentID;

#define RPITHERM_MBOX_READ       0x00U
#define RPITHERM_MBOX_STATUS     0x18U
#define RPITHERM_MBOX_WRITE      0x20U
#define RPITHERM_MBOX_FULL       (1U << 31)
#define RPITHERM_MBOX_EMPTY      (1U << 30)
#define RPITHERM_MBOX_CHANNEL    8U
#define RPITHERM_GET_TEMPERATURE 0x00030006U
#define RPITHERM_RESPONSE_OK     0x80000000U
#define RPITHERM_MAX_POLLS       100000U
#define RPITHERM_DMA_PHYS_MASK   0x3fffffffULL
#define RPITHERM_DMA_BUS_ALIAS   0xc0000000U
#define RPITHERM_DMA_BUFFER_SIZE 4096U

struct rpitherm_memory_pool_props {
    uint8_t name[32];
    uint32_t module_id;
    uint32_t reserved0;
    uint64_t parent;
    uint32_t type;
    uint32_t reserved1;
    uint64_t reserved2[2];
};

struct rpitherm_memory_range {
    uint64_t first_mpn;
    uint32_t page_count;
    uint32_t reserved;
};

struct rpitherm_memory_allocation {
    uint32_t page_count;
    uint32_t range_count;
    struct rpitherm_memory_range *ranges;
};

struct rpitherm_memory_constraints {
    uint32_t reserved0;
    uint32_t address_class;
    int32_t color;
    uint32_t reserved1;
};

struct rpitherm_map_props {
    uint32_t page_count;
    uint32_t reserved0;
    uint64_t map_count;
    uint32_t range_count;
    uint32_t reserved1;
    struct rpitherm_memory_range *ranges;
    uint64_t reserved2;
};

struct rpitherm_world_props {
    void *name;
    uint32_t module_id;
    uint32_t reserved0;
    vmk_Status (*start)(void *data);
    void *data;
    uint32_t start_world;
    uint32_t reserved1;
    vmk_HeapID heap;
};

_Static_assert(sizeof(struct rpitherm_memory_pool_props) == 72,
               "unexpected memory-pool properties ABI size");
_Static_assert(sizeof(struct rpitherm_memory_allocation) == 16,
               "unexpected memory allocation ABI size");
_Static_assert(sizeof(struct rpitherm_memory_constraints) == 16,
               "unexpected memory constraints ABI size");
_Static_assert(sizeof(struct rpitherm_map_props) == 40,
               "unexpected map properties ABI size");
_Static_assert(sizeof(struct rpitherm_world_props) == 48,
               "unexpected world properties ABI size");

static void *rpitherm_memory_pool;
static void *rpitherm_lowmem_page;
static uint8_t rpitherm_mailbox_resource[64] __attribute__((aligned(8)));
static vmk_ACPIDevice rpitherm_acpi_device;
static uint64_t rpitherm_machine_address;
static uint32_t rpitherm_bus_address;
static volatile uint32_t rpitherm_world_stop;
static volatile uint32_t rpitherm_world_running;
static volatile uint32_t rpitherm_world_id;
static volatile uint32_t rpitherm_world_reads;

struct rpitherm_request {
    uint32_t buffer_size;
    uint32_t response;
    uint32_t tag_id;
    uint32_t tag_size;
    uint32_t tag_value_size;
    uint32_t sensor_id;
    uint32_t value;
    uint32_t end_tag;
} __attribute__((packed));

struct rpitherm_driver_ops {
    vmk_Status (*attach)(vmk_Device);
    vmk_Status (*scan)(vmk_Device);
    vmk_Status (*detach)(vmk_Device);
    vmk_Status (*quiesce)(vmk_Device);
    vmk_Status (*start)(vmk_Device);
    vmk_Status (*forget)(vmk_Device);
};

struct rpitherm_driver_props {
    uint32_t module_id;
    uint8_t name[32];
    uint32_t reserved0;
    struct rpitherm_driver_ops *ops;
    uint8_t reserved1[8];
};

struct rpitherm_heap_props {
    uint32_t type;
    uint8_t name[32];
    uint32_t module_id;
    uint32_t initial_size;
    uint32_t maximum_size;
    int32_t timeout_ms;
    uint32_t reserved0;
    uint64_t reserved1;
};

_Static_assert(sizeof(struct rpitherm_heap_props) == 64,
               "unexpected heap properties ABI size");
_Static_assert(sizeof(struct rpitherm_request) == 32,
               "unexpected mailbox request size");

static vmk_Driver rpitherm_driver;
static vmk_HeapID rpitherm_heap;

static void
rpitherm_cache_clean(const void *address, uint32_t length)
{
    uintptr_t line = (uintptr_t)address & ~(uintptr_t)63U;
    uintptr_t end = (uintptr_t)address + length;

    for (; line < end; line += 64U)
        __asm__ volatile("dc cvac, %0" : : "r"(line) : "memory");
    __asm__ volatile("dsb ish" ::: "memory");
}

static void
rpitherm_cache_invalidate(const void *address, uint32_t length)
{
    uintptr_t line = (uintptr_t)address & ~(uintptr_t)63U;
    uintptr_t end = (uintptr_t)address + length;

    for (; line < end; line += 64U)
        __asm__ volatile("dc ivac, %0" : : "r"(line) : "memory");
    __asm__ volatile("dsb ish" ::: "memory");
}

static vmk_Status
rpitherm_wait_status(const void *resource, uint32_t mask,
                     uint32_t want_clear, uint32_t *last_status)
{
    uint32_t index;
    vmk_Status status;

    for (index = 0; index < RPITHERM_MAX_POLLS; index++) {
        status = vmk_MappedResourceRead32(resource, RPITHERM_MBOX_STATUS,
                                           last_status);
        if (status != 0)
            return status;
        if (((*last_status & mask) == 0U) == (want_clear != 0U))
            return 0;
        if ((index & 0xffU) == 0xffU)
            vmk_DelayUsecs(1);
    }
    return (vmk_Status)0x0bad0001U;
}

static vmk_Status
rpitherm_transaction(void *resource, struct rpitherm_request *request,
                      uint32_t bus_address, uint32_t *mailbox_result,
                      uint32_t *last_status)
{
    uint32_t stale;
    uint32_t drains = 0;
    vmk_Status status;

    for (;;) {
        status = vmk_MappedResourceRead32(resource, RPITHERM_MBOX_STATUS,
                                           last_status);
        if (status != 0)
            return status;
        if ((*last_status & RPITHERM_MBOX_EMPTY) != 0U)
            break;
        status = vmk_MappedResourceRead32(resource, RPITHERM_MBOX_READ,
                                           &stale);
        if (status != 0)
            return status;
        if (++drains >= 256U)
            return (vmk_Status)0x0bad0002U;
    }

    status = rpitherm_wait_status(resource, RPITHERM_MBOX_FULL, 1U,
                                  last_status);
    if (status != 0)
        return status;

    rpitherm_cache_clean(request, RPITHERM_DMA_BUFFER_SIZE);
    __asm__ volatile("dsb ish" ::: "memory");
    status = vmk_MappedResourceWrite32(
        resource, RPITHERM_MBOX_WRITE,
        (bus_address & ~0xfU) | RPITHERM_MBOX_CHANNEL);
    if (status != 0)
        return status;
    __asm__ volatile("dsb ish" ::: "memory");

    status = rpitherm_wait_status(resource, RPITHERM_MBOX_EMPTY, 1U,
                                  last_status);
    if (status != 0)
        return status;
    rpitherm_cache_invalidate(request, RPITHERM_DMA_BUFFER_SIZE);
    __asm__ volatile("dsb ish" ::: "memory");
    status = vmk_MappedResourceRead32(resource, RPITHERM_MBOX_READ,
                                       mailbox_result);
    __asm__ volatile("dsb ish" ::: "memory");
    return status;
}

static vmk_Status
rpitherm_temperature_from_lowmem(void *resource)
{
    struct rpitherm_memory_pool_props pool_props = {
        { 0 }, 0, 0, 0, 1, 0, { 0, 0 },
    };
    struct rpitherm_memory_range range = { 0, 0, 0 };
    struct rpitherm_memory_allocation allocation = {
        1, 1, &range,
    };
    struct rpitherm_memory_constraints constraints = {
        0, 1, -1, 0,
    };
    struct rpitherm_map_props map_props = {
        1, 0, 1, 1, 0, &range, 0,
    };
    struct rpitherm_request *request;
    uint64_t machine_address = 0;
    uint32_t bus_address;
    uint32_t mailbox_result = 0;
    uint32_t last_status = 0;
    vmk_Status status;

    pool_props.module_id = vmk_ModuleCurrentID;
    status = vmk_NameInitialize(pool_props.name, "rpitherm_low1g");
    if (status == 0)
        status = vmk_MemPoolCreate(&pool_props, &rpitherm_memory_pool);
    if (status == 0)
        status = vmk_MemPoolAllocWithRA(
            rpitherm_memory_pool, &constraints, &allocation,
            __builtin_return_address(0));
    if (status == 0 && allocation.range_count == 1 &&
        range.page_count == 1)
        status = vmk_Map(vmk_ModuleCurrentID, &map_props,
                         &rpitherm_lowmem_page);
    if (status == 0 && rpitherm_lowmem_page != 0)
        status = vmk_VA2MA(rpitherm_lowmem_page, 4096,
                           &machine_address);
    if (status != 0 || allocation.range_count != 1 ||
        range.page_count != 1 || rpitherm_lowmem_page == 0 ||
        machine_address != (range.first_mpn << 12) ||
        machine_address > RPITHERM_DMA_PHYS_MASK ||
        (machine_address & 0xfU) != 0U) {
        _vmk_WarningMessage(
            "rpitherm: low1G validation failed status=%x ranges=%u "
            "firstMPN=%lx pages=%u va=%lx ma=%lx expected=%lx below1G=%u",
            status, allocation.range_count, range.first_mpn,
            range.page_count, rpitherm_lowmem_page, machine_address,
            range.first_mpn << 12,
            machine_address <= RPITHERM_DMA_PHYS_MASK);
        return status != 0 ? status : (vmk_Status)0x0bad0003U;
    }

    request = (struct rpitherm_request *)rpitherm_lowmem_page;
    memset(request, 0, RPITHERM_DMA_BUFFER_SIZE);
    request->buffer_size = sizeof(*request);
    request->tag_id = RPITHERM_GET_TEMPERATURE;
    request->tag_size = 8U;
    request->sensor_id = 0U;
    bus_address = ((uint32_t)machine_address & RPITHERM_DMA_PHYS_MASK) |
                  RPITHERM_DMA_BUS_ALIAS;
    rpitherm_machine_address = machine_address;
    rpitherm_bus_address = bus_address;

    _vmk_WarningMessage(
        "rpitherm: low1G request ma=%08x bus=%08x size=%u tag=%08x",
        (uint32_t)machine_address, bus_address,
        request->buffer_size, request->tag_id);
    status = rpitherm_transaction(resource, request, bus_address,
                                  &mailbox_result, &last_status);
    if (status == 0 &&
        mailbox_result == ((bus_address & ~0xfU) | RPITHERM_MBOX_CHANNEL) &&
        request->buffer_size == sizeof(*request) &&
        request->response == RPITHERM_RESPONSE_OK &&
        request->tag_id == RPITHERM_GET_TEMPERATURE &&
        (request->tag_value_size & RPITHERM_RESPONSE_OK) != 0U &&
        (request->tag_value_size & ~RPITHERM_RESPONSE_OK) >= 4U &&
        request->value >= 10000U && request->value <= 120000U) {
        _vmk_WarningMessage(
            "rpitherm: temperature=%u mC (%u.%03u C) ma=%08x bus=%08x",
            request->value, request->value / 1000U,
            request->value % 1000U, (uint32_t)machine_address,
            bus_address);
        return 0;
    }

    _vmk_WarningMessage(
        "rpitherm: low1G temperature failed status=%x mailbox=%08x "
        "last=%08x response=%08x tagResponse=%08x value=%u",
        status, mailbox_result, last_status, request->response,
        request->tag_value_size, request->value);
    return status != 0 ? status : (vmk_Status)0x0bad0004U;
}

static vmk_Status
rpitherm_repeat_temperature(void)
{
    struct rpitherm_request *request =
        (struct rpitherm_request *)rpitherm_lowmem_page;
    uint32_t mailbox_result = 0;
    uint32_t last_status = 0;
    vmk_Status status;

    if (request == 0 || rpitherm_bus_address == 0 ||
        rpitherm_machine_address > RPITHERM_DMA_PHYS_MASK)
        return (vmk_Status)0x0bad0003U;

    memset(request, 0, RPITHERM_DMA_BUFFER_SIZE);
    request->buffer_size = sizeof(*request);
    request->tag_id = RPITHERM_GET_TEMPERATURE;
    request->tag_size = 8U;
    status = rpitherm_transaction(
        rpitherm_mailbox_resource, request, rpitherm_bus_address,
        &mailbox_result, &last_status);
    if (status == 0 &&
        mailbox_result ==
            ((rpitherm_bus_address & ~0xfU) | RPITHERM_MBOX_CHANNEL) &&
        request->response == RPITHERM_RESPONSE_OK &&
        request->tag_id == RPITHERM_GET_TEMPERATURE &&
        (request->tag_value_size & RPITHERM_RESPONSE_OK) != 0U &&
        (request->tag_value_size & ~RPITHERM_RESPONSE_OK) >= 4U &&
        request->value >= 10000U && request->value <= 120000U) {
        rpitherm_world_reads++;
        _vmk_WarningMessage(
            "rpitherm: periodic temperature=%u mC (%u.%03u C) read=%u",
            request->value, request->value / 1000U,
            request->value % 1000U, rpitherm_world_reads);
        return 0;
    }
    _vmk_WarningMessage(
        "rpitherm: periodic read failed status=%x mailbox=%08x "
        "last=%08x response=%08x tagResponse=%08x",
        status, mailbox_result, last_status, request->response,
        request->tag_value_size);
    return status != 0 ? status : (vmk_Status)0x0bad0004U;
}

static vmk_Status
rpitherm_poll_world(void *data)
{
    vmk_Status sleep_status;

    (void)data;
    _vmk_WarningMessage("rpitherm: periodic world entered intervalUsec=60000000");
    while (rpitherm_world_stop == 0) {
        sleep_status = vmk_WorldSleep(60000000ULL);
        if (rpitherm_world_stop != 0)
            break;
        if (sleep_status != 0) {
            rpitherm_world_running = 0;
            return sleep_status;
        }
        (void)rpitherm_repeat_temperature();
    }
    rpitherm_world_running = 0;
    return 0;
}

static vmk_Status
rpitherm_start_poll_world(void)
{
    uint8_t world_name[32] __attribute__((aligned(8))) = { 0 };
    struct rpitherm_world_props props = {
        world_name, 0, 0, rpitherm_poll_world, 0, 1, 0, 0,
    };
    vmk_Status status;

    rpitherm_world_stop = 0;
    rpitherm_world_running = 1;
    rpitherm_world_id = 0;
    rpitherm_world_reads = 0;
    props.module_id = vmk_ModuleCurrentID;
    props.heap = vmk_ModuleGetHeapID(vmk_ModuleCurrentID);
    status = vmk_NameInitialize(world_name, "rpitherm_poll");
    if (status == 0)
        status = vmk_WorldCreate(&props, (uint32_t *)&rpitherm_world_id);
    if (status != 0)
        rpitherm_world_running = 0;
    _vmk_WarningMessage(
        "rpitherm: periodic world create status=%x id=%u intervalSec=60",
        status, rpitherm_world_id);
    return status;
}

static void
rpitherm_stop_poll_world(void)
{
    uint32_t world_id = rpitherm_world_id;
    vmk_Status destroy_status = 0;

    if (world_id == 0)
        return;
    rpitherm_world_stop = 1;
    if (rpitherm_world_running != 0)
        destroy_status = vmk_WorldDestroy(world_id);
    vmk_WorldWaitForDeath(world_id);
    rpitherm_world_id = 0;
    rpitherm_world_running = 0;
    _vmk_WarningMessage(
        "rpitherm: periodic world stopped status=%x id=%u reads=%u",
        destroy_status, world_id, rpitherm_world_reads);
}

static vmk_Status
rpitherm_attach(vmk_Device device)
{
    vmk_ACPIDevice acpi_device = 0;
    uint8_t *resource = rpitherm_mailbox_resource;
    uint8_t acpi_info[256] __attribute__((aligned(8))) = { 0 };
    vmk_Status status;

    status = vmk_DeviceGetRegistrationData(device, (void **)&acpi_device);
    if (status != 0 || acpi_device == 0)
        return status != 0 ? status : 1;
    status = vmk_ACPIQueryInfo(acpi_device, acpi_info);
    if (status != 0)
        return status;
    status = vmk_ACPIMapIOResource(vmk_ModuleCurrentID, acpi_device, 0,
                                   resource);
    if (status != 0) {
        _vmk_WarningMessage("rpitherm: mailbox map failed status=%x",
                            status);
        return status;
    }
    rpitherm_acpi_device = acpi_device;

    /* One standard GET_TEMPERATURE transaction; no PWM or IRQ access. */
    status = rpitherm_temperature_from_lowmem(resource);
    if (status == 0)
        status = rpitherm_start_poll_world();
    _vmk_WarningMessage(
        "rpitherm: periodic temperature probe ready status=%x", status);
    return 0;
}

static vmk_Status
rpitherm_noop(vmk_Device device)
{
    (void)device;
    return 0;
}

static vmk_Status
rpitherm_stop_device(vmk_Device device)
{
    vmk_Status status = 0;

    (void)device;
    rpitherm_stop_poll_world();
    if (rpitherm_acpi_device != 0) {
        status = vmk_ACPIUnmapIOResource(
            vmk_ModuleCurrentID, rpitherm_acpi_device, 0);
        rpitherm_acpi_device = 0;
    }
    return status;
}

static struct rpitherm_driver_ops rpitherm_ops = {
    rpitherm_attach,
    rpitherm_noop,
    rpitherm_stop_device,
    rpitherm_stop_device,
    rpitherm_noop,
    rpitherm_noop,
};

static struct rpitherm_driver_props rpitherm_props = {
    0, { 0 }, 0, &rpitherm_ops, { 0 },
};

__attribute__((section(".vmkmodinfo"), used, aligned(8)))
static const char __vmk_nsRequiredInfo_str[] =
    "nsRequired=com.vmware.vmkapi#v3_0_0_0";
__attribute__((section(".vmkmodinfo"), used, aligned(8)))
static const char __vmk_versionInfo_str[] =
    "version=0.4.4-1rpitherm.803.0.55.24449057";
__attribute__((section(".vmkmodinfo"), used, aligned(8)))
static const char __vmk_buildTypeInfo_str[] = "buildType=release";
__attribute__((section(".vmkmodinfo"), used, aligned(8)))
static const char __vmk_licenseInfo_str[] = "license=BSD";
__attribute__((section(".vmkrequiredns"), used))
static const char rpitherm_required_namespace[] =
    "com.vmware.vmkapi#v3_0_0_0";

int
init_module(void)
{
    vmk_Status status;
    struct rpitherm_heap_props heap = {
        1, { 0 }, 0, 64U * 1024U, 1024U * 1024U, -1, 0, 0,
    };

    rpitherm_props.module_id = vmk_ModuleCurrentID;
    heap.module_id = vmk_ModuleCurrentID;
    status = vmk_NameInitialize(heap.name, "rpitherm_heap");
    if (status != 0)
        return status;
    status = vmk_HeapCreate(&heap, &rpitherm_heap);
    if (status != 0)
        return status;
    vmk_ModuleSetHeapID(vmk_ModuleCurrentID, rpitherm_heap);
    status = vmk_NameInitialize(rpitherm_props.name, "rpitherm");
    if (status == 0)
        status = vmk_DriverRegister(&rpitherm_props, &rpitherm_driver);
    if (status != 0)
        vmk_HeapDestroy(rpitherm_heap);
    return status;
}

void
cleanup_module(void)
{
    rpitherm_stop_poll_world();
    vmk_DriverUnregister(rpitherm_driver);
    vmk_HeapDestroy(rpitherm_heap);
}
