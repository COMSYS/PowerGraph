#include "bcm2835_interface.h"

#include <mach/platform.h>



// =======================================================================================================
// ==================================  hardware map  =====================================================
// =======================================================================================================
#define VIRT_FROM_PHY(x) (IO_ADDRESS(x))


uint32_t noinline read32(volatile void* ptr)
{
	return *((uint32_t*)ptr);
}

#define LOWER_N_BITS(n) ((1 << (n))-1)

void uint32_set_mask(volatile uint32_t *reg, uint32_t mask, bool set_reset)
{
	if(set_reset)
		*reg |= mask;
	else
		*reg &= ~mask;
}

void uint32_set_bits(volatile uint32_t *reg, int bit_count, int offset, int value)
{
	uint32_t mask = LOWER_N_BITS(bit_count);
	uint32_t old_reg = *reg;
	uint32_t new_reg = (old_reg & ~(mask << offset)) | ((value & mask) << offset);
	if(new_reg != old_reg)
		*reg = new_reg;
}


// =======================================================================================================
// ==============================  GPIO  =================================================================
// =======================================================================================================

// ------------------------------ hardware ---------------------------------------------------------------

volatile struct io_gpio_t* const io_gpio = (struct io_gpio_t*)VIRT_FROM_PHY(GPIO_BASE);


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
struct gpio_port gpio_make_port(short size, short offset);
void   gpio_port_set_output(struct gpio_port, int value);
int    gpio_port_get_input(struct gpio_port);


// ------------------------------ code -------------------------------------------------------------------

void gpio_reset(void)
{
	io_gpio->GPFSEL[2] = io_gpio->GPFSEL[1] = io_gpio->GPFSEL[0] = 0;
}


void gpio_pin_select_function(int pin, int mode)
{
	// each GPFSEL register controls 10 pins
	// each 3 bits define 1 pin
	uint32_set_bits(&io_gpio->GPFSEL[pin/10], 3, 3*(pin%10), mode);
}

void __inline gpio_pin_set_output(int pin, int on_or_off)
{
	//((on_or_off)?(io_gpio->GPSET):(io_gpio->GPCLR))[pin / 32] = 1 << (pin % 32);  // this line is not needed, less than 32 pins on Raspberry B+
	*((on_or_off)?(io_gpio->GPSET):(io_gpio->GPCLR)) = 1 << pin;
}

bool __inline gpio_pin_get_input(int pin)
{
	//return io_gpio->GPLEV[pin / 32] & (1 << (pin % 32)); // this line is not needed, less than 32 pins on Raspberry B+
	return *io_gpio->GPLEV & (1 << pin);
}

struct gpio_port gpio_make_port(short size, short offset)
{
	struct gpio_port r;
	r.size = size;
	r.offset = offset;
	return r;
}

void gpio_port_set_output(struct gpio_port port, int value)
{
	uint32_t mask = LOWER_N_BITS(port.size);
	*io_gpio->GPSET = ( value & mask) << port.offset;
	*io_gpio->GPCLR = (~value & mask) << port.offset;
}

int gpio_port_get_input(struct gpio_port port)
{
	return (*io_gpio->GPLEV >> port.offset) & LOWER_N_BITS(port.size);
}




// =======================================================================================================
// ==============================  system timer  =========================================================
// =======================================================================================================

// ------------------------------ hardware ---------------------------------------------------------------

volatile struct io_systemtimer_t* const io_systemtimer = (struct io_systemtimer_t*)VIRT_FROM_PHY(ST_BASE);

// ------------------------------ data -------------------------------------------------------------------

typedef void (*systemtimer_callback_t)(void);
systemtimer_callback_t systemtimer_callbacks[4] = {0, 0, 0, 0};
int systemtimer_delays[4] = {0, 0, 0, 0};

void systemtimer_reset_timeout	(int timern);
int  systemtimer_advance		(int timern, int new_delay); // usually called from within irq, to make timer fire again later
int  systemtimer_continue		(int timern);  // same as above
bool systemtimer_interrupt_bind	(int timern, systemtimer_callback_t callback);
void systemtimer_interrupt_free	(int timern);
void systemtimer_start			(int timern, uint32_t delay);
uint64_t systemtimer_read		(void);
bool systemtimer_is_timeout		(int timern);


// ------------------------------ code -------------------------------------------------------------------


void systemtimer_reset_timeout(int timern)
{
	io_systemtimer->status = (1 << timern);
}

irqreturn_t systemtimer_irq_handler(int irqn, void *unused)
{
	systemtimer_callback_t callback = systemtimer_callbacks[irqn - INTERRUPT_TIMER0];
	systemtimer_reset_timeout(1);
	if(callback)callback();
	return IRQ_HANDLED;
}

// usually called from within irq, to make timer fire again later
int systemtimer_advance(int timern, int new_delay)
{
	io_systemtimer->compare[timern] += new_delay;
	return 0; // no overrun calc yet
}
int systemtimer_continue(int timern)
{
	systemtimer_advance(timern, systemtimer_delays[timern]);
	return 0; // no overrun calc yet
}

bool systemtimer_interrupt_bind(int timern, systemtimer_callback_t callback)
{
	int irqn = INTERRUPT_TIMER0 + timern;
	if (request_irq(irqn, &systemtimer_irq_handler, IRQF_DISABLED, "PGBoard", (void*)0)) {
		printk(KERN_ERR "could not register system timer %d IRQ\n", timern);
		return false;
	}
	systemtimer_callbacks[timern] = callback;
	return true;
}

void systemtimer_interrupt_free(int timern)
{
	int irqn = INTERRUPT_TIMER0 + timern;
	systemtimer_callbacks[timern] = 0;
	free_irq(irqn, (void*)0);
}

void systemtimer_start(int timern, uint32_t delay)
{
	io_systemtimer->compare[timern] = io_systemtimer->value_low + delay;
	systemtimer_delays[timern] = delay;
}

uint64_t systemtimer_read(void)
{
	return io_systemtimer->value_low | ((uint64_t)io_systemtimer->value_high << 32);
}

bool systemtimer_is_timeout(int timern)
{
	return !!(io_systemtimer->status & (1 << timern));
}


/*
	// setup timer
	if(!systemtimer_interrupt_bind(1, timer_callback))
		return -EIO;
	systemtimer_start(1, 150);
	systemtimer_continue(1);
	systemtimer_interrupt_free(1);
*/









// =======================================================================================================
// ==============================  aux  ==================================================================
// =======================================================================================================

// ------------------------------ hardware ---------------------------------------------------------------

volatile uint32_t* const io_aux_enable = (uint32_t*)VIRT_FROM_PHY(0x20215004);
#define IO_AUX_ENABLE_MINIUART 1
#define IO_AUX_ENABLE_SPI1 2
#define IO_AUX_ENABLE_SPI2 2



// =======================================================================================================
// ==============================  SPI 1 =========  SPI 2  ===============================================
// =======================================================================================================

// ------------------------------ hardware ---------------------------------------------------------------

volatile struct io_spix_t* const io_spi1 = (struct io_spix_t*)VIRT_FROM_PHY(0x20215080);
volatile struct io_spix_t* const io_spi2 = (struct io_spix_t*)VIRT_FROM_PHY(0x202150C0);

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




// *((uint32_t *) VIRT_FROM_PHY(0x7E00B20C)) = // FIQ control register

/*
static int fiq_op(void *ref, int relinquish)
{
	return 0;
}

static struct fiq_handler my_fiq = {
	.name   = "PGBoard",
	.fiq_op = fiq_op,
};
*/



// ------------------------------ data -------------------------------------------------------------------

typedef void (*spi1_callback_t)(void);
spi1_callback_t spi1_callback = 0;


// ------------------------------ code -------------------------------------------------------------------

void spi1_setup_gpio_pins(bool miso, bool mosi, bool cs0, bool cs1)
{
	// switches the corresponding GPIO pins to output the special function like "mosi"

	if(miso)gpio_pin_select_function(19, GPIO_PIN_MODE_ALT4);
	if(mosi)gpio_pin_select_function(20, GPIO_PIN_MODE_ALT4);
	if(cs0) gpio_pin_select_function(18, GPIO_PIN_MODE_ALT4);
	if(cs1) gpio_pin_select_function(17, GPIO_PIN_MODE_ALT4);
	        gpio_pin_select_function(21, GPIO_PIN_MODE_ALT4); // clock
}
void spi1_reset_gpio_pins(void)
{
	// switches all spi1 related gpio pins to the default mode, as inputs

	int i;
	for(i = 17; i <= 21; i++)
		gpio_pin_select_function( i, GPIO_PIN_MODE_INPUT);
}

irqreturn_t spi1_irq_handler(int irqn, void *unused)
{
	spi1_callback_t callback = spi1_callback;
	if(callback)callback();
	return IRQ_HANDLED;
}

bool spi1_interrupt_bind(systemtimer_callback_t callback)
{
	int irqn = INTERRUPT_AUX;
	if (request_irq(irqn, &spi1_irq_handler, IRQF_DISABLED, "PGBoard", (void*)0)) {
		printk(KERN_ERR "could not register AUX IRQ\n");
		return false;
	}
	spi1_callback = callback;
	return true;
}

void spi1_interrupt_free(void)
{
	int irqn = INTERRUPT_AUX;
	free_irq(irqn, (void*)0);
	spi1_callback = 0;
}




// =======================================================================================================
// ==============================  SPI SLAVE  ============================================================
// =======================================================================================================

// ------------------------------ hardware ---------------------------------------------------------------

volatile struct io_slave_t* const io_slave = (struct io_slave_t*)VIRT_FROM_PHY(0x20214000);

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

volatile struct io_spi0_t* io_spi0 = 0;

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

uint32_t spi0_configuration = 0;

// ------------------------------ code -------------------------------------------------------------------

void spi0_setup_gpio_pins(bool miso, bool mosi, bool cs0, bool cs1)
{
	if(miso)gpio_pin_select_function( 9, GPIO_PIN_MODE_ALT0);
	if(mosi)gpio_pin_select_function(10, GPIO_PIN_MODE_ALT0);
	if(cs0) gpio_pin_select_function( 8, GPIO_PIN_MODE_ALT0);
	if(cs1) gpio_pin_select_function( 7, GPIO_PIN_MODE_ALT0);
	        gpio_pin_select_function(11, GPIO_PIN_MODE_ALT0); // clock

	// do not support LoSSi mode (Low speed serial mode) so just reset the register
	io_spi0->LTOH = 0;
}
void spi0_reset_gpio_pins(void)
{
	int i;
	for(i = 7; i <= 11; i++)
		gpio_pin_select_function( i, GPIO_PIN_MODE_INPUT);
}

bool spi0_is_read_available(void)
{
	return io_spi0->CS & IO_SPI0_CS_RXR;
}

bool spi0_is_write_possible(void)
{
	return io_spi0->CS & IO_SPI0_CS_TXD;
}

bool spi0_is_transfer_in_progress(void)
{
	return io_spi0->CS & IO_SPI0_CS_DONE;
}

void spi0_clear_buffers(bool read, bool write)
{
	io_spi0->CS |= ((read)?(0x20):(0)) | ((write)?(0x10):(0));
}

void spi0_write(uint32_t b)
{
	io_spi0->FIFO = b;
}

uint32_t spi0_read(void)
{
	return io_spi0->FIFO;
}

void spi0_set_divider(uint32_t multiple_of_2)
{
	io_spi0->CLK = multiple_of_2;
}

void spi0_set_remaining_transfer_byte_count(uint16_t byte_count)
{
	io_spi0->DLEN = byte_count;
}

uint32_t spi0_get_remaining_transfer_byte_count(void)
{
	return io_spi0->DLEN;
}


// When in DMA mode, compare the fill-level of input and output buffers and send
//   a request signal to DMA when these threasholds are breached
void spi0_set_dma_signal_threshholds(
	uint8_t read_request_level,   // send a normal request to DMA when input  buffer size is higher
	uint8_t read_panic_level,     // send a high-priority  to DMA when input  buffer size is higher
	uint8_t write_request_level,  // send a normal request to DMA when output buffer size is lower
	uint8_t write_panic_level)    // send a high-priority  to DMA when output buffer size is lower
{
	//                             R PANIC                       R DREQ                     T PANIC                 T DREQ
	io_spi0->DC = (read_panic_level << 24) | (read_request_level << 16) | (write_panic_level << 8) | (write_request_level);
}

void spi0_set_clock_mode(
	bool clock_polarity_rest_state_high_or_low,
	bool clock_first_transition_at_beginning_or_middle)
{
	uint32_set_mask(&spi0_configuration, IO_SPI0_CS_CPOL, clock_polarity_rest_state_high_or_low);
	uint32_set_mask(&spi0_configuration, IO_SPI0_CS_CPHA, clock_first_transition_at_beginning_or_middle);
}

void spi0_set_dma_mode(
	bool dma_enabled,
	bool dma_turn_off_cs_pin_afterwards)
{
	uint32_set_mask(&spi0_configuration, IO_SPI0_CS_DMAEN, dma_enabled);
	uint32_set_mask(&spi0_configuration, IO_SPI0_CS_ADCS , dma_turn_off_cs_pin_afterwards);
}

void spi0_start(void)
{
	io_spi0->CS = IO_SPI0_CS_TA | spi0_configuration;
}

void spi0_stop(void)
{
	io_spi0->CS = spi0_configuration;
}

void spi0_reset(void)
{
	io_spi0->CS = 0;
	spi0_clear_buffers(true, true);
}


// =======================================================================================================
// ==============================  DMA  ==================================================================
// =======================================================================================================

// ------------------------------ hardware ---------------------------------------------------------------

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

volatile struct io_dma_array_t* const io_dma = (struct io_dma_array_t*)VIRT_FROM_PHY(0x20007000);





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
dma_callback_t dma_callbacks[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

struct dma_mempage_t dma_mempage_for_control_blocks;
struct io_dma_control_block_t *dma_control_blocks;
int dma_allocated_mempage_count = 0;
#define DMA_CONTROL_BLOCK_BUS_ADDRESS(a) (((uint32_t)dma_mempage_for_control_blocks.bus_ptr) + (a*32))

// ------------------------------ private code -----------------------------------------------------------

bool _dma_alloc_page_without_sideeffects(struct dma_mempage_t *mempage, int numpages)
{
	mempage->virtual_ptr = dma_alloc_coherent(NULL, SZ_4K*numpages, &mempage->bus_ptr, GFP_KERNEL|GFP_DMA);
	printk(KERN_INFO "dma_alloc_coherent() bus: %x  virtual: %x\n", (uint32_t)mempage->bus_ptr, (uint32_t)mempage->virtual_ptr);
	if(!mempage->virtual_ptr)
		return false;
	mempage->size = SZ_4K*numpages;
	return true;
}

void _dma_free_page_without_sideeffects(struct dma_mempage_t *mempage)
{
	dma_free_coherent(NULL, SZ_4K, mempage->virtual_ptr, mempage->bus_ptr);
}

// ------------------------------ code -------------------------------------------------------------------

bool dma_alloc_page(struct dma_mempage_t *mempage, int numpages)
{
	bool success = _dma_alloc_page_without_sideeffects(mempage, numpages);
	if(!success)
		return false;

	// allocate an additional mempage dedicated for control blocks when the first mempage was requested
	if(dma_allocated_mempage_count == 0)
	{
		_dma_alloc_page_without_sideeffects(&dma_mempage_for_control_blocks, 1);
		dma_control_blocks = (struct io_dma_control_block_t *)dma_mempage_for_control_blocks.virtual_ptr;
	}
	dma_allocated_mempage_count++;

	return true;
}

void dma_free_page(struct dma_mempage_t *mempage)
{
	_dma_free_page_without_sideeffects(mempage);

	// free the control block mempage when all the other mempages were freed
	dma_allocated_mempage_count--;
	if(dma_allocated_mempage_count == 0)
	{
		_dma_free_page_without_sideeffects(&dma_mempage_for_control_blocks);
	}
}

irqreturn_t dma_irq_handler(int irqn, void *unused)
{
	int dma_channel = irqn - INTERRUPT_DMA0;
	dma_callback_t callback = dma_callbacks[dma_channel];
	if(callback)callback();
	return IRQ_HANDLED;
}

bool dma_interrupt_bind(dma_callback_t callback, int dma_channel)
{
	int irqn = INTERRUPT_DMA0 + dma_channel;
	if (request_irq(irqn, &dma_irq_handler, IRQF_DISABLED, "PGBoard", (void*)0)) {
		printk(KERN_ERR "could not register DMA IRQ\n");
		return false;
	}
	dma_callbacks[dma_channel] = callback;
	return true;
}

void dma_interrupt_free(int dma_channel)
{
	int irqn = INTERRUPT_DMA0 + dma_channel;
	free_irq(irqn, (void*)0);
	dma_callbacks[dma_channel] = 0;
}


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
	int      next_cb_index)                          // pass  DMA_INDEX_NO_MORE  if this was the last one
{
	uint32_t next_cb_bus_address;
	if(next_cb_index == DMA_INDEX_NO_MORE)
		next_cb_bus_address = 0;       // DMA chip recognises all-zero address and will stops
	else
		next_cb_bus_address = DMA_CONTROL_BLOCK_BUS_ADDRESS(next_cb_index);

	uint32_t trans_information = 0;
	if(source_increment)              trans_information |= DMA_TI_INC_SRC;
	if(destination_increment)         trans_information |= DMA_TI_INC_DEST;
	if(dreq_source)                   trans_information |= DMA_TI_PERMAP(dreq_source);
	if(dreq_gates_reads)              trans_information |= DMA_TI_DREQ_READS;
	if(dreq_gates_writes)             trans_information |= DMA_TI_DREQ_WRITES;
	if(wait_for_write_confirmation)   trans_information |= DMA_TI_WAIT_RESP;
	if(add_wait_cycles_per_readwrite) trans_information |= DMA_TI_WAIT_CYCLES(add_wait_cycles_per_readwrite);
	if(generate_interrupt)            trans_information |= DMA_TI_GEN_INTERRUPT;

	struct io_dma_control_block_t *cb = &dma_control_blocks[cb_index];

	cb->TRANS_INFORMATION = trans_information;
	cb->SOURCE_AD = source_bus_address;
	cb->DEST_AD = destination_bus_address;
	cb->TRANS_LEN = transfer_byte_count;
	cb->STRIDE = 0;
	cb->NEXT_CONTROL_BLOCK_AD = next_cb_bus_address;
	cb->_PADDING[0] =
	cb->_PADDING[1] = 0;
}

int dma_get_channel_current_control_block_index(struct dma_t* dma_channel)
{
	// DMA chip sets this value to zero when it's off
	if(dma_channel->io_base->CONTROL_BLOCK_AD == 0)
	{
		return DMA_INDEX_NO_MORE;
	}

	// calculate byte offset from the first control block. both adresses are in bus address space
	uint32_t offset = dma_channel->io_base->CONTROL_BLOCK_AD - dma_mempage_for_control_blocks.bus_ptr;

	// offset must be inside the mempage, and aligned
	if(offset < 0 || offset % sizeof(struct io_dma_control_block_t) || offset > 4096)
	{
		// invalid address
		return DMA_INDEX_ERROR;
	}

	// calculate the index of the control block, it's guaranteed to be correct now
	return offset / sizeof(struct io_dma_control_block_t);
}

void dma_reserve_channel(struct dma_t* fill_structure)
{
	// since this version of linux doesn't support dma reservations, hardcode the unused DMA channels
	static int dma_reservation_count = 0;
	dma_reservation_count++;

	if(dma_reservation_count == 1)
	{
		fill_structure->channel = 10;
		fill_structure->irq = INTERRUPT_DMA10;
		fill_structure->io_base = &io_dma->DMA[10].IO_BLOCK;
	}
	else if(dma_reservation_count == 2)
	{
		fill_structure->channel = 11;
		fill_structure->irq = INTERRUPT_DMA11;
		fill_structure->io_base = &io_dma->DMA[11].IO_BLOCK;
	}
	else
	{
		fill_structure->channel = 12;
		fill_structure->irq = INTERRUPT_DMA12;
		fill_structure->io_base = &io_dma->DMA[12].IO_BLOCK;
	}
}

void dma_free_channel(struct dma_t* fill_structure)
{
	// no changes were made in reserve that need to be undone
}

// 0 is the lowest priority, 15 is the highest
void dma_start(struct dma_t *dma_channel, int first_control_block_index, int normal_priority, int panic_priority)
{
	dma_channel->io_base->CONTROL_BLOCK_AD = DMA_CONTROL_BLOCK_BUS_ADDRESS(first_control_block_index);
	dma_channel->io_base->CONTROL_STATUS = 1 | // activate DMA
		(normal_priority << 16) |              // no panic bus priority of this DMA channel
		(panic_priority << 20);                // no panic bus priority of this DMA channel
}

void dma_stop_and_reset(struct dma_t *dma_channel)
{
	dma_channel->io_base->CONTROL_STATUS = 0x80000000;
}

// returns whether this DMA channel has requested an IRQ, since last time you called dma_clear_channel_flags()
bool dma_has_requested_interrupt(struct dma_t *dma_channel)
{
	// read the current control status
	return !!(dma_channel->io_base->CONTROL_STATUS & DMA_CS_INT);
}

// returns whether this DMA channel has finished a CB(control block), since last time you called dma_clear_channel_flags()
bool dma_has_finished_control_block(struct dma_t *dma_channel)
{
	// read the current control status
	return !!(dma_channel->io_base->CONTROL_STATUS & DMA_CS_END);
}

// this function should be called each interrupt callback
void dma_clear_channel_flags(struct dma_t *dma_channel)
{
	// writes the 1 bits back into the register to clear them
	uint32_t control_status = dma_channel->io_base->CONTROL_STATUS;
	dma_channel->io_base->CONTROL_STATUS = control_status;
}


// =======================================================================================================
// ==============================  INIT  =================================================================
// =======================================================================================================

bool bcm2835_init(void)
{
	// the SPI kernel memory is wrongly mapped. use our own mapping instead
	if(! (io_spi0 = ioremap(SPI0_BASE, 4*1024)) )
	{
		printk(KERN_ERR "could not map spi physical io range to virtual memory\n");
		return false;
	}
	return true;
}

void bcm2835_denit(void)
{
	iounmap(io_spi0);
}
