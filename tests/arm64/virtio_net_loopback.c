#include <stdint.h>

#define UART_DR_ADDR 0x09000000u
#define VNET_BASE 0x09050000u

#define REG_MAGIC_VALUE 0x000u
#define REG_VERSION 0x004u
#define REG_DEVICE_ID 0x008u
#define REG_DEVICE_FEATURES 0x010u
#define REG_DEVICE_FEATURES_SEL 0x014u
#define REG_DRIVER_FEATURES 0x020u
#define REG_DRIVER_FEATURES_SEL 0x024u
#define REG_QUEUE_SEL 0x030u
#define REG_QUEUE_NUM_MAX 0x034u
#define REG_QUEUE_NUM 0x038u
#define REG_QUEUE_READY 0x044u
#define REG_QUEUE_NOTIFY 0x050u
#define REG_INTERRUPT_STATUS 0x060u
#define REG_INTERRUPT_ACK 0x064u
#define REG_STATUS 0x070u
#define REG_QUEUE_DESC_LOW 0x080u
#define REG_QUEUE_DESC_HIGH 0x084u
#define REG_QUEUE_AVAIL_LOW 0x090u
#define REG_QUEUE_AVAIL_HIGH 0x094u
#define REG_QUEUE_USED_LOW 0x0a0u
#define REG_QUEUE_USED_HIGH 0x0a4u
#define REG_CONFIG 0x100u

#define STATUS_ACKNOWLEDGE 1u
#define STATUS_DRIVER 2u
#define STATUS_DRIVER_OK 4u
#define STATUS_FEATURES_OK 8u

#define FEAT_MAC (1ull << 5)
#define FEAT_STATUS (1ull << 16)
#define FEAT_VERSION_1 (1ull << 32)

#define DESC_F_NEXT 1u
#define DESC_F_WRITE 2u

#define QUEUE_SIZE 8u
#define TX_FRAME_SIZE 60u
#define NET_HDR_SIZE 10u

struct virtq_desc {
  uint64_t addr;
  uint32_t len;
  uint16_t flags;
  uint16_t next;
};

struct virtq_avail {
  uint16_t flags;
  uint16_t idx;
  uint16_t ring[QUEUE_SIZE];
};

struct virtq_used_elem {
  uint32_t id;
  uint32_t len;
};

struct virtq_used {
  uint16_t flags;
  uint16_t idx;
  struct virtq_used_elem ring[QUEUE_SIZE];
};

struct virtio_net_hdr {
  uint8_t flags;
  uint8_t gso_type;
  uint16_t hdr_len;
  uint16_t gso_size;
  uint16_t csum_start;
  uint16_t csum_offset;
} __attribute__((packed));

static struct virtq_desc rx_desc[QUEUE_SIZE] __attribute__((aligned(16)));
static struct virtq_avail rx_avail __attribute__((aligned(2)));
static volatile struct virtq_used rx_used __attribute__((aligned(4)));
static struct virtq_desc tx_desc[QUEUE_SIZE] __attribute__((aligned(16)));
static struct virtq_avail tx_avail __attribute__((aligned(2)));
static volatile struct virtq_used tx_used __attribute__((aligned(4)));

static volatile struct virtio_net_hdr rx_hdr;
static struct virtio_net_hdr tx_hdr;
static volatile uint8_t rx_frame[128];
static uint8_t tx_frame[TX_FRAME_SIZE];

static volatile uint32_t* const UART_DR = (volatile uint32_t*)UART_DR_ADDR;

static void putc(char ch) {
  UART_DR[0] = (uint32_t)(unsigned char)ch;
}

static void fail(char code) {
  putc('F');
  putc(code);
  putc('\n');
  __asm__ volatile("brk #1");
  for (;;) {
  }
}

static uint32_t mmio_read32(uintptr_t addr) {
  return *(volatile uint32_t*)addr;
}

static uint16_t mmio_read16(uintptr_t addr) {
  return *(volatile uint16_t*)addr;
}

static uint8_t mmio_read8(uintptr_t addr) {
  return *(volatile uint8_t*)addr;
}

static void mmio_write32(uintptr_t addr, uint32_t value) {
  *(volatile uint32_t*)addr = value;
}

static uint64_t read_device_features(void) {
  uint64_t features = 0;
  mmio_write32(VNET_BASE + REG_DEVICE_FEATURES_SEL, 0u);
  features |= (uint64_t)mmio_read32(VNET_BASE + REG_DEVICE_FEATURES);
  mmio_write32(VNET_BASE + REG_DEVICE_FEATURES_SEL, 1u);
  features |= (uint64_t)mmio_read32(VNET_BASE + REG_DEVICE_FEATURES) << 32;
  return features;
}

static void write_driver_features(uint64_t features) {
  mmio_write32(VNET_BASE + REG_DRIVER_FEATURES_SEL, 0u);
  mmio_write32(VNET_BASE + REG_DRIVER_FEATURES, (uint32_t)features);
  mmio_write32(VNET_BASE + REG_DRIVER_FEATURES_SEL, 1u);
  mmio_write32(VNET_BASE + REG_DRIVER_FEATURES, (uint32_t)(features >> 32));
}

static void set_queue_addr(uint32_t low_reg, uint32_t high_reg, uintptr_t addr) {
  mmio_write32(VNET_BASE + low_reg, (uint32_t)addr);
  mmio_write32(VNET_BASE + high_reg, (uint32_t)((uint64_t)addr >> 32));
}

static void setup_queue(uint32_t queue_sel,
                        struct virtq_desc* desc,
                        struct virtq_avail* avail,
                        volatile struct virtq_used* used) {
  mmio_write32(VNET_BASE + REG_QUEUE_SEL, queue_sel);
  if (mmio_read32(VNET_BASE + REG_QUEUE_NUM_MAX) < QUEUE_SIZE) {
    fail('q');
  }
  mmio_write32(VNET_BASE + REG_QUEUE_NUM, QUEUE_SIZE);
  set_queue_addr(REG_QUEUE_DESC_LOW, REG_QUEUE_DESC_HIGH, (uintptr_t)desc);
  set_queue_addr(REG_QUEUE_AVAIL_LOW, REG_QUEUE_AVAIL_HIGH, (uintptr_t)avail);
  set_queue_addr(REG_QUEUE_USED_LOW, REG_QUEUE_USED_HIGH, (uintptr_t)used);
  mmio_write32(VNET_BASE + REG_QUEUE_READY, 1u);
}

static void check_headers_and_config(uint8_t mac[6]) {
  if (mmio_read32(VNET_BASE + REG_MAGIC_VALUE) != 0x74726976u) {
    fail('m');
  }
  if (mmio_read32(VNET_BASE + REG_VERSION) != 2u) {
    fail('v');
  }
  if (mmio_read32(VNET_BASE + REG_DEVICE_ID) != 1u) {
    fail('d');
  }

  for (uint32_t i = 0; i < 6u; ++i) {
    mac[i] = mmio_read8(VNET_BASE + REG_CONFIG + i);
  }
  if (mmio_read16(VNET_BASE + REG_CONFIG + 6u) != 1u) {
    fail('s');
  }
}

static void build_tx_frame(const uint8_t mac[6]) {
  static const uint8_t src_mac[6] = {0x02u, 0x12u, 0x34u, 0x56u, 0x78u, 0x9au};
  for (uint32_t i = 0; i < 6u; ++i) {
    tx_frame[i] = mac[i];
    tx_frame[6u + i] = src_mac[i];
  }
  tx_frame[12] = 0x88u;
  tx_frame[13] = 0xb5u;
  for (uint32_t i = 14u; i < TX_FRAME_SIZE; ++i) {
    tx_frame[i] = (uint8_t)i;
  }
}

static void prepare_rx_descriptors(void) {
  rx_desc[0].addr = (uintptr_t)&rx_hdr;
  rx_desc[0].len = sizeof(rx_hdr);
  rx_desc[0].flags = DESC_F_NEXT | DESC_F_WRITE;
  rx_desc[0].next = 1u;
  rx_desc[1].addr = (uintptr_t)&rx_frame[0];
  rx_desc[1].len = sizeof(rx_frame);
  rx_desc[1].flags = DESC_F_WRITE;
  rx_desc[1].next = 0u;
  rx_avail.ring[0] = 0u;
  rx_avail.idx = 1u;
}

static void prepare_tx_descriptors(void) {
  tx_desc[0].addr = (uintptr_t)&tx_hdr;
  tx_desc[0].len = sizeof(tx_hdr);
  tx_desc[0].flags = DESC_F_NEXT;
  tx_desc[0].next = 1u;
  tx_desc[1].addr = (uintptr_t)&tx_frame[0];
  tx_desc[1].len = sizeof(tx_frame);
  tx_desc[1].flags = 0u;
  tx_desc[1].next = 0u;
  tx_avail.ring[0] = 0u;
  tx_avail.idx = 1u;
}

static void wait_for_completion(void) {
  for (uint32_t spins = 0; spins < 1000000u; ++spins) {
    if (tx_used.idx == 1u && rx_used.idx == 1u) {
      return;
    }
  }
  fail('t');
}

static void verify_results(void) {
  if (tx_used.ring[0].id != 0u || tx_used.ring[0].len != 0u) {
    fail('u');
  }
  if (rx_used.ring[0].id != 0u || rx_used.ring[0].len != (NET_HDR_SIZE + TX_FRAME_SIZE)) {
    fail('r');
  }
  if ((mmio_read32(VNET_BASE + REG_INTERRUPT_STATUS) & 1u) == 0u) {
    fail('i');
  }
  mmio_write32(VNET_BASE + REG_INTERRUPT_ACK, 1u);
  if (mmio_read32(VNET_BASE + REG_INTERRUPT_STATUS) != 0u) {
    fail('a');
  }
  for (uint32_t i = 0; i < sizeof(rx_hdr); ++i) {
    if (((volatile const uint8_t*)&rx_hdr)[i] != 0u) {
      fail('h');
    }
  }
  for (uint32_t i = 0; i < TX_FRAME_SIZE; ++i) {
    if (rx_frame[i] != tx_frame[i]) {
      fail('p');
    }
  }
}

int main(void) {
  uint8_t mac[6];
  const uint64_t required_features = FEAT_MAC | FEAT_STATUS | FEAT_VERSION_1;
  uint32_t status = STATUS_ACKNOWLEDGE | STATUS_DRIVER;

  check_headers_and_config(mac);

  mmio_write32(VNET_BASE + REG_STATUS, 0u);
  mmio_write32(VNET_BASE + REG_STATUS, status);
  if ((read_device_features() & required_features) != required_features) {
    fail('f');
  }
  write_driver_features(required_features);
  status |= STATUS_FEATURES_OK;
  mmio_write32(VNET_BASE + REG_STATUS, status);
  if ((mmio_read32(VNET_BASE + REG_STATUS) & STATUS_FEATURES_OK) == 0u) {
    fail('o');
  }

  setup_queue(0u, rx_desc, &rx_avail, &rx_used);
  setup_queue(1u, tx_desc, &tx_avail, &tx_used);

  build_tx_frame(mac);
  prepare_rx_descriptors();
  prepare_tx_descriptors();

  status |= STATUS_DRIVER_OK;
  mmio_write32(VNET_BASE + REG_STATUS, status);

  mmio_write32(VNET_BASE + REG_QUEUE_NOTIFY, 0u);
  mmio_write32(VNET_BASE + REG_QUEUE_NOTIFY, 1u);

  wait_for_completion();
  verify_results();

  putc('N');
  putc('\n');
  for (;;) {
  }
}
