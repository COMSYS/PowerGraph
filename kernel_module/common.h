#ifndef __COMMON__
#define __COMMON__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <asm/fiq.h>
#include <asm/io.h>
#include <linux/time.h>


// dma
#include <linux/page-flags.h>
#include <asm/dma.h>
#include <linux/dma-mapping.h>
#include <asm/scatterlist.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/log2.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <mach/dma.h>

#include <linux/jiffies.h>
#define current_system_msec_time() (jiffies_to_msecs(jiffies))


#define true 1
#define false 0

//typedef uint8_t bool;

#define SUCCESS 0
#define FAILURE -1

/* 
 * Get rid of taint message by declaring code as GPL. 
 */
MODULE_LICENSE("GPL");


#endif