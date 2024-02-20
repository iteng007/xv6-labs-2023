#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *tx_mbufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;
// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
// struct spinlock e1000_tx_lock;
// struct spinlock e1000_rx_lock;
int messages;
void
e1000_init(uint32 *xregs)
{
  int i;

  initlock(&e1000_lock, "e1000");
  // initlock(&e1000_tx_lock, "e1000_tx_lock");
  // initlock(&e1000_rx_lock, "e1000_rx_lock");
  regs = xregs;

  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_mbufs[i] = 0;
  }
  regs[E1000_TDBAL] = (uint64) tx_ring;
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;
  
  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head;
  }
  regs[E1000_RDBAL] = (uint64) rx_ring;
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  regs[E1000_RDLEN] = sizeof(rx_ring);

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back
}

int
e1000_transmit(struct mbuf *m)
{
  //
  // Your code here.
  
  acquire(&e1000_lock);
  uint32 next_index = regs[E1000_TDT];
  struct tx_desc * tx_desc = &tx_ring[next_index];
  uint32 prev_index = next_index==0?TX_RING_SIZE-1:next_index-1;
  if (next_index>=TX_RING_SIZE||(!(tx_desc->status&E1000_TXD_STAT_DD)&&prev_index!=regs[E1000_TDH])) {
    release(&e1000_lock);
    return -1;
//Check if the the ring is overflowing or
//E1000 hasn't finished the corresponding previous transmission request, so return an error.
  }
  //Use mbuffree() to free the last mbuf that was transmitted from that descriptor (if there was one)
  if (tx_mbufs[next_index]) {
      mbuffree(tx_mbufs[next_index]);
  }
  
  //Fill in the descriptor
  tx_desc->addr = (uint64)m->head;
  tx_desc->length = (uint16)m->len;
  tx_desc->cso = 0;
  tx_desc->cmd|=1;
  tx_desc->cmd|=(1<<3);
  tx_desc->special = 0;
  tx_mbufs[next_index] =  m;
  
  //Finally, update the ring position by adding one to E1000_TDT modulo TX_RING_SIZE.
  regs[E1000_TDT] =(regs[E1000_TDT]+1)%TX_RING_SIZE;
  release(&e1000_lock);
  return 0;
}

static void
e1000_recv(void)
{
  uint32 next_index = (regs[E1000_RDT]+1)%RX_RING_SIZE;
  struct rx_desc * rx_desc = &rx_ring[next_index];
  //If a new packet is available by checking for the E1000_RXD_STAT_DD bit in the status portion of the descriptor. 
  //If not, stop.
  while (rx_desc->status&E1000_TXD_STAT_DD) {
  struct mbuf * mbuf = rx_mbufs[next_index];
  mbuf->len = rx_desc->length;
  // mbuf->head = (char*)rx_desc->addr;
  // messages++;
  // printf("Now send up the stack,current received %d messages\n",messages);
  net_rx(mbuf);
  struct mbuf *new_mbuf = mbufalloc(0);
  rx_mbufs[next_index] = new_mbuf; 
  // new_mbuf->head = (char*)rx_desc->addr;
  rx_desc->addr = (uint64)new_mbuf->head;
  rx_desc->status=0;
  // regs[E1000_RDT] = next_index;
  next_index = (next_index+1)%RX_RING_SIZE;
  rx_desc = &rx_ring[next_index];
  }
  regs[E1000_RDT] = (next_index-1)%RX_RING_SIZE;
  //update the mbuf's m->len to the length reported in the descriptor. Deliver the mbuf to the network stack using net_rx().
  
  // release(&e1000_rx_lock);
}

void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}