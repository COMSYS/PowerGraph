#ifndef __BCM2835_INTERFACE__
#define __BCM2835_INTERFACE__

#include "common.h"

#include <mach/platform.h>



// =======================================================================================================
// ==================================  hardware map  =====================================================
// =======================================================================================================
#define VIRT_FROM_PHY(x) (IO_ADDRESS(x))

uint32_t noinline read32(volatile void* ptr);

#define LOWER_N_BITS(n) ((1 << (n))-1)

void uint32_set_mask(volatile uint32_t *reg, uint32_t mask, bool set_reset);
void uint32_set_bits(volatile uint32_t *reg, int bit_count, int offset, int value);


// =======================================================================================================
// ==============================  GPIO  =================================================================
// =======================================================================================================

// ------------------------------ hardware ---------------------------------------------------------------

struct io_gpio_t
{
	uint32_t GPFSEL[6];	// only first 3 are used on B+
	uint32_t Reserved1;
	uint32_t GPSET[2];
	uint32_t Reserved2;
	uint32_t GPCLR[2];
	uint32_t Reserved3;
	uint32_t GPLEV[2];
	uint32_t Reserved4;
	uint32_t GPEDS[2];
	uint32_t Reserved5;
	uint32_t GPREN[2];
	uint32_t Reserved6;
	uint32_t GPFEN[2];
	uint32_t Reserved7;
	uint32_t GPHEN[2];
	uint32_t Reserved8;
	uint32_t GPLEN[2];
	uint32_t Reserved9;
	uint32_t GPAREN[2];
	uint32_t Reserved10;
	uint32_t GPAFEN[2];
	uint32_t Reserved11;
	uint32_t GPPUD;
	uint32_t GPPUDCLK[2];
};
extern volatile struct io_gpio_t* const io_gpio;


// ------------------------------ data -------------------------------------------------------------------

#define GPIO_PIN_MODE_INPUT 0
#define GPIO_PIN_MODE_OUTPUT 1
#define GPIO_PIN_MODE_ALT0 4
#define GPIO_PIN_MODE_ALT1 5
#define GPIO_PIN_MODE_ALT2 6
#define GPIO_PIN_MODE_ALT3 7
#define GPIO_PIN_MODE_ALT4 3
#define GPIO_PIN_MODE_ALT5 2

void gpio_reset					(void);
void gpio_pin_select_function	(int pin, int mode);
void gpio_pin_set_output		(int pin, int on_or_off);
bool gpio_pin_get_input			(int pin);

// define consecutive pins as one block, aka port
struct gpio_port
{
	short size;		// bits in port
	short offset;	// offset from gpio pin0
};

struct gpio_port gpio_make_port(short size, short offset);
void   gpio_port_set_output(struct gpio_port, int value);
int    gpio_port_get_input(struct gpio_port);


// ------------------------------ code -------------------------------------------------------------------

void gpio_reset(void);
void gpio_pin_select_function(int pin, int mode);
void gpio_pin_set_output(int pin, int on_or_off);
bool gpio_pin_get_input(int pin);
struct gpio_port gpio_make_port(short size, short offset);
void gpio_port_set_output(struct gpio_port port, int value);
int gpio_port_get_input(struct gpio_port port);



// =======================================================================================================
// ==============================  system timer  =========================================================
// =======================================================================================================

// ------------------------------ hardware ---------------------------------------------------------------

struct io_systemtimer_t
{
	uint32_t status;
	uint32_t value_low;
	uint32_t value_high;
	uint32_t compare[4];
};
extern volatile struct io_systemtimer_t* const io_systemtimer;

// ------------------------------ data -------------------------------------------------------------------

typedef void (*systemtimer_callback_t)(void);

void systemtimer_reset_timeout	(int timern);
int  systemtimer_advance		(int timern, int new_delay); // usually called from within irq, to make timer fire again later
int  systemtimer_continue		(int timern);  // same as above
bool systemtimer_interrupt_bind	(int timern, systemtimer_callback_t callback);
void systemtimer_interrupt_free	(int timern);
void systemtimer_start			(int timern, uint32_t delay);
uint64_t systemtimer_read		(void);
bool systemtimer_is_timeout		(int timern);


// ------------------------------ code -------------------------------------------------------------------


void systemtimer_reset_timeout(int timern);
irqreturn_t systemtimer_irq_handler(int irqn, void *unused);
// usually called from within irq, to make timer fire again later
int systemtimer_advance(int timern, int new_delay);
int systemtimer_continue(int timern);
bool systemtimer_interrupt_bind(int timern, systemtimer_callback_t callback);
void systemtimer_interrupt_free(int timern);
void systemtimer_start(int timern, uint32_t delay);
uint64_t systemtimer_read(void);
bool systemtimer_is_timeout(int timern);






// =======================================================================================================
// ==============================  aux  ==================================================================
// =======================================================================================================

// ------------------------------ hardware ---------------------------------------------------------------

extern volatile uint32_t* const io_aux_enable;
#define IO_AUX_ENABLE_MINIUART 1
#define IO_AUX_ENABLE_SPI1 2
#define IO_AUX_ENABLE_SPI2 2



// =======================================================================================================
// ==============================  SPI 1 =========  SPI 2  ===============================================
// =======================================================================================================

// ------------------------------ hardware ---------------------------------------------------------------

struct io_spix_t
{
	uint32_t CNTL0;
	uint32_t CNTL1;
	uint32_t STAT;
	uint32_t PEEK;
	uint32_t unused[4];
	uint32_t IO;
	uint32_t IO_[3];
	uint32_t TXHOLD;
};
extern volatile struct io_spix_t* const io_spi1;
extern volatile struct io_spix_t* const io_spi2;

#define IO_SPIX_CNTL0_OUT_MSB_FIRST 0x00000040
#define IO_SPIX_CNTL0_INVERT_CLK    0x00000080
#define IO_SPIX_CNTL0_OUT_RISING    0x00000100
#define IO_SPIX_CNTL0_CLEAR_FIFO    0x00000200
#define IO_SPIX_CNTL0_IN_RISING     0x00000400
#define IO_SPIX_CNTL0_ENABLE        0x00000800
#define IO_SPIX_CNTL0_HOLD_CLK0     0x00000000
#define IO_SPIX_CNTL0_HOLD_CLK1     0x00001000
#define IO_SPIX_CNTL0_HOLD_CLK4     0x00002000
#define IO_SPIX_CNTL0_HOLD_CLK7     0x00003000
#define IO_SPIX_CNTL0_VAR_WIDTH     0x00004000
#define IO_SPIX_CNTL0_VAR_CS        0x00008000
#define IO_SPIX_CNTL0_POST_INPUT    0x00010000
#define IO_SPIX_CNTL0_CS0           0x00020000
#define IO_SPIX_CNTL0_CS1           0x00040000
#define IO_SPIX_CNTL0_CS2           0x00080000
#define IO_SPIX_CNTL0_CLOCK_DIVIDER_OFFSET 20

#define IO_SPIX_CNTL1_KEEP_INPUT    0x00000001
#define IO_SPIX_CNTL1_IN_MSB_FIRST  0x00000002
#define IO_SPIX_CNTL1_IRQ_ON_IDLE   0x00000040
#define IO_SPIX_CNTL1_IRQ_ON_TXEMPTY 0x00000080
#define IO_SPIX_CNTL1_APPEND_CS_HIGHTIME_OFFSET 8

#define IO_SPIX_STAT_BUSY           0x00000040



// ------------------------------ data -------------------------------------------------------------------

typedef void (*spi1_callback_t)(void);


// ------------------------------ code -------------------------------------------------------------------

void spi1_setup_gpio_pins(bool miso, bool mosi, bool cs0, bool cs1);
void spi1_reset_gpio_pins(void);
irqreturn_t spi1_irq_handler(int irqn, void *unused);
bool spi1_interrupt_bind(systemtimer_callback_t callback);
void spi1_interrupt_free(void);



// =======================================================================================================
// ==============================  SPI SLAVE  ============================================================
// =======================================================================================================

// ------------------------------ hardware ---------------------------------------------------------------

struct io_slave_t
{
	uint32_t DR;
	uint32_t RSR;
	uint32_t SLV;	// I2C slave address
	uint32_t CR;
	uint32_t FR;
	uint32_t IFLS;
	uint32_t IMSC;
	uint32_t RIS;
	uint32_t MIS;
	uint32_t ICR;
	uint32_t DMACR;
	uint32_t TDR;
	uint32_t GPUSTAT;
	uint32_t HCTRL;
	uint32_t DEBUG1;
	uint32_t DEBUG2;
};
extern volatile struct io_slave_t* const io_slave;

#define IO_SLAVE_DR_OE         0x00000100
#define IO_SLAVE_DR_UE         0x00000200
#define IO_SLAVE_DR_TXBUSY     0x00010000
#define IO_SLAVE_DR_RXFE       0x00020000
#define IO_SLAVE_DR_TXFF       0x00040000
#define IO_SLAVE_DR_RXFF       0x00080000
#define IO_SLAVE_DR_TXFE       0x00100000
#define IO_SLAVE_DR_RXBUSY     0x00200000
#define IO_SLAVE_DR_TXFLEVEL_OFFSET 22
#define IO_SLAVE_DR_RXFLEVEL_OFFSET 27
#define IO_SLAVE_RSR_OE        0x00000001
#define IO_SLAVE_RSR_UE        0x00000002
#define IO_SLAVE_CR_EN         0x00000001
#define IO_SLAVE_CR_SPI        0x00000002
#define IO_SLAVE_CR_I2C        0x00000004
#define IO_SLAVE_CR_CPHA       0x00000008
#define IO_SLAVE_CR_CPOL       0x00000010
#define IO_SLAVE_CR_ENSTAT     0x00000020
#define IO_SLAVE_CR_ENCTRL     0x00000040
#define IO_SLAVE_CR_BRK        0x00000080
#define IO_SLAVE_CR_TXE        0x00000100
#define IO_SLAVE_CR_RXE        0x00000200
#define IO_SLAVE_CR_INV_RXF    0x00000400
#define IO_SLAVE_CR_TESTINFO   0x00000800
#define IO_SLAVE_CR_HOSTCTRLEN 0x00001000
#define IO_SLAVE_CR_INV_TXF    0x00002000
#define IO_SLAVE_FR_TXBUSY     0x00000001
#define IO_SLAVE_FR_RXFE       0x00000002
#define IO_SLAVE_FR_TXFF       0x00000004
#define IO_SLAVE_FR_RXFF       0x00000008
#define IO_SLAVE_FR_TXFE       0x00000010
#define IO_SLAVE_FR_RXBUSY     0x00000020

// ------------------------------ data -------------------------------------------------------------------

// ------------------------------ code -------------------------------------------------------------------




// =======================================================================================================
// ==============================  SPI 0  ================================================================
// =======================================================================================================

// ------------------------------ hardware ---------------------------------------------------------------

struct io_spi0_t
{
	uint32_t CS;
	uint32_t FIFO;
	uint32_t CLK;
	uint32_t DLEN;  // 0xFFFF max
	uint32_t LTOH;
	uint32_t DC;
};
extern volatile struct io_spi0_t* io_spi0;

#define SPI0_FIFO_BUS_ADDRESS (0x7E204004)

#define IO_SPI0_CS_CPHA      0x00000004
#define IO_SPI0_CS_CPOL      0x00000008
#define IO_SPI0_CS_CLEAR_TX  0x00000010
#define IO_SPI0_CS_CLEAR_RX  0x00000020
#define IO_SPI0_CS_CSPOL     0x00000040
#define IO_SPI0_CS_TA        0x00000080
#define IO_SPI0_CS_DMAEN     0x00000100
#define IO_SPI0_CS_INTD      0x00000200
#define IO_SPI0_CS_INTR      0x00000400
#define IO_SPI0_CS_ADCS      0x00000800
#define IO_SPI0_CS_REN       0x00001000
#define IO_SPI0_CS_LEN       0x00002000
#define IO_SPI0_CS_LMONO     0x00004000
#define IO_SPI0_CS_TE_EN     0x00008000
#define IO_SPI0_CS_DONE      0x00010000
#define IO_SPI0_CS_RXD       0x00020000
#define IO_SPI0_CS_TXD       0x00040000
#define IO_SPI0_CS_RXR       0x00080000
#define IO_SPI0_CS_RXF       0x00100000
#define IO_SPI0_CS_CSPOL0    0x00200000
#define IO_SPI0_CS_CSPOL1    0x00400000
#define IO_SPI0_CS_CSPOL2    0x00800000
#define IO_SPI0_CS_DMA_LEN   0x01000000
#define IO_SPI0_CS_LEN_LONG  0x02000000

// ------------------------------ data -------------------------------------------------------------------

void spi0_setup_gpio_pins(bool miso, bool mosi, bool cs0, bool cs1);
void spi0_reset_gpio_pins(void);

// ------------------------------ code -------------------------------------------------------------------

void spi0_setup_gpio_pins(bool miso, bool mosi, bool cs0, bool cs1);
void spi0_reset_gpio_pins(void);
bool spi0_is_read_available(void);
bool spi0_is_write_possible(void);
bool spi0_is_transfer_in_progress(void);
void spi0_clear_buffers(bool read, bool write);
void spi0_write(uint32_t b);
uint32_t spi0_read(void);
void spi0_set_divider(uint32_t multiple_of_2);
void spi0_set_remaining_transfer_byte_count(uint16_t byte_count);
uint32_t spi0_get_remaining_transfer_byte_count(void);

// When in DMA mode, compare the fill-level of input and output buffers and send
//   a request signal to DMA when these threasholds are breached
void spi0_set_dma_signal_threshholds(
	uint8_t read_request_level,   // send a normal request to DMA when input  buffer size is higher
	uint8_t read_panic_level,     // send a high-priority  to DMA when input  buffer size is higher
	uint8_t write_request_level,  // send a normal request to DMA when output buffer size is lower
	uint8_t write_panic_level);   // send a high-priority  to DMA when output buffer size is lower
void spi0_set_clock_mode(
	bool clock_polarity_rest_state_high_or_low,
	bool clock_first_transition_at_beginning_or_middle);
void spi0_set_dma_mode(
	bool dma_enabled,
	bool dma_turn_off_cs_pin_afterwards);
void spi0_start(void);
void spi0_stop(void);
void spi0_reset(void);

// =======================================================================================================
// ==============================  DMA  ==================================================================
// =======================================================================================================

// ------------------------------ hardware ---------------------------------------------------------------

// io mapping

struct io_dma_t
{
	uint32_t CONTROL_STATUS;
	uint32_t CONTROL_BLOCK_AD;
	uint32_t TRANS_INFORMATION;
	uint32_t SOURCE_AD;
	uint32_t DEST_AD;
	uint32_t TRANS_LEN;
	uint32_t STRIDE;
	uint32_t NEXT_CONTROL_BLOCK_AD;
	uint32_t DEBUG;
};
struct io_dma_control_block_t
{
	uint32_t TRANS_INFORMATION;
	uint32_t SOURCE_AD;
	uint32_t DEST_AD;
	uint32_t TRANS_LEN;
	uint32_t STRIDE;
	uint32_t NEXT_CONTROL_BLOCK_AD;
	uint32_t _PADDING[2];
};

// bitmap for the CONTROL_STATUS member above. abbreviation: CS
#define DMA_CS_RESET (1 << 31)
#define DMA_CS_ABORT (1 << 30)
#define DMA_CS_DISDEBUG (1 << 29)
#define DMA_CS_WAIT_FOR_OUTSTANDING_WRITES (1 << 28)
#define DMA_CS_PRIORITY_PANIC(a) (a << 20)
#define DMA_CS_PRIORITY_NORMAL(a) (a << 16)
#define DMA_CS_PRIORITY_ERROR (1 << 8)
#define DMA_CS_WAITING_FOR_OUTSTANDING_WRITES (1 << 6)
#define DMA_CS_WAITING_FOR_DREQ (1 << 5)
#define DMA_CS_IS_PAUSED (1 << 4)
#define DMA_CS_DREQ_STATE (1 << 3)
#define DMA_CS_INT (1 << 2)
#define DMA_CS_END (1 << 1)
#define DMA_CS_ACTIVE (1)

// bitmap for the TRANS_INFORMATION members above. abbreviation: TI
#define DMA_TI_PERMAP(a) (a << 16)
#define DMA_TI_DREQ_WRITES (1 << 6)
#define DMA_TI_DREQ_READS (1 << 10)
#define DMA_TI_INC_DEST (1 << 4)
#define DMA_TI_INC_SRC (1 << 8)
#define DMA_TI_NO_READ (1 << 11)
#define DMA_TI_WAIT_RESP (1 << 3)
#define DMA_TI_WAIT_CYCLES(a)  (a << 21)
#define DMA_TI_GEN_INTERRUPT  (1)
#define DMA_INDEX_NO_MORE -1
#define DMA_INDEX_ERROR -2

union io_dma_padded_t
{
	struct io_dma_t IO_BLOCK;
	uint8_t PADDING[0x100];
};
struct io_dma_array_t
{
	union io_dma_padded_t DMA[0x0F];
};
extern volatile struct io_dma_array_t* const io_dma;


struct dma_t // channel
{
	int channel;
	int irq;
	volatile struct io_dma_t *io_base;
};



// memory page to use with DMA

struct dma_mempage_t
{
	dma_addr_t bus_ptr;
	uint8_t* virtual_ptr;
	uint32_t size;
};

// DREQ channels
#define DREQ_NO_FEEDBACK  0
#define DREQ_PCM_TX       2
#define DREQ_PCM_RX       3
#define DREQ_SMI          4
#define DREQ_PWM          5
#define DREQ_SPI0_TX      6
#define DREQ_SPI0_RX      7
#define DREQ_SPI_SLAVE_TX 8
#define DREQ_SPI_SLAVE_RX 9
#define DREQ_UART_TX      12
#define DREQ_UART_RX      14

#define DMA_DO_NOT_INCREMENT false
#define DMA_INCREMENT true

// ------------------------------ data -------------------------------------------------------------------

typedef void (*dma_callback_t)(void);

#define DMA_CONTROL_BLOCK_BUS_ADDRESS(a) (((uint32_t)dma_mempage_for_control_blocks.bus_ptr) + (a*32))

// ------------------------------ private code -----------------------------------------------------------

bool _dma_alloc_page_without_sideeffects(struct dma_mempage_t *mempage, int numpages);
void _dma_free_page_without_sideeffects(struct dma_mempage_t *mempage);

// ------------------------------ code -------------------------------------------------------------------

bool dma_alloc_page(struct dma_mempage_t *mempage, int numpages);
void dma_free_page(struct dma_mempage_t *mempage);
irqreturn_t dma_irq_handler(int irqn, void *unused);
bool dma_interrupt_bind(dma_callback_t callback, int dma_channel);
void dma_interrupt_free(int dma_channel);

void dma_setup_control_block(
	int      cb_index,                               // which control block should be configured?
	uint32_t transfer_byte_count,
	uint32_t source_bus_address,
	bool     source_increment,
	uint32_t destination_bus_address,
	bool     destination_increment,
	int      dreq_source,                            // use DREQ_NO_FEEDBACK if no throttle required
	bool     dreq_gates_reads,                       // stop DMA before reading, if DREQ is off
	bool     dreq_gates_writes,                      // stop DMA before writing, if DREQ is off
	bool     wait_for_write_confirmation,            // some streams don't recognise 2 writes if they're too fast. This spaces them out enough so both are recognised
	int      add_wait_cycles_per_readwrite,          // allow the DREQ feedback signal to update properly before continuing. prevents overflow
	bool     generate_interrupt,                     // catch it with dma_interrupt_bind()
	int      next_cb_index);                         // pass  DMA_INDEX_NO_MORE  if this was the last one

int dma_get_channel_current_control_block_index(struct dma_t* dma_channel);
void dma_reserve_channel(struct dma_t* fill_structure);
void dma_free_channel(struct dma_t* fill_structure);

// 0 is the lowest priority, 15 is the highest
void dma_start(struct dma_t *dma_channel, int first_control_block_index, int normal_priority, int panic_priority);

void dma_stop_and_reset(struct dma_t *dma_channel);

// returns whether this DMA channel has requested an IRQ, since last time you called dma_clear_channel_flags()
bool dma_has_requested_interrupt(struct dma_t *dma_channel);

// returns whether this DMA channel has finished a CB(control block), since last time you called dma_clear_channel_flags()
bool dma_has_finished_control_block(struct dma_t *dma_channel);

// this function should be called each interrupt callback
void dma_clear_channel_flags(struct dma_t *dma_channel);

// =======================================================================================================
// ==============================  INIT  =================================================================
// =======================================================================================================

bool bcm2835_init(void);
void bcm2835_denit(void);

#endif