
#include <linux/module.h>
#include <linux/miscdevice.h>		// for misc-driver calls.
#include <linux/fs.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/kfifo.h>
#include <errno.h>

#define MY_DEVICE_FILE  "morse-code"
#define SPACE_DOT_TIME 4 // will be added onto the 3 gap dot times after a character
#define GAP_DOT_TIME 3

#define DOT_TIME_default 200
#define MASK 0x8000
#define KFIFO_LENGTH 128

static DECLARE_KFIFO(morse_fifo, char, KFIFO_LENGTH);

/******************************************************
 * Parameter
 ******************************************************/
static int dottime = DOT_TIME_default;
module_param(dottime, int, S_IRUGO);
MODULE_PARM_DESC(dottime, "My dot time!");


/******************************************************
 * LED
 ******************************************************/
#include <linux/leds.h>

DEFINE_LED_TRIGGER(ledtrig_demo);


/******************************************************
 * Initialization of Data
 ******************************************************/
static unsigned short morsecode_codes[] = {
		0xB800,	// A 1011 1
		0xEA80,	// B 1110 1010 1
		0xEBA0,	// C 1110 1011 101
		0xEA00,	// D 1110 101
		0x8000,	// E 1
		0xAE80,	// F 1010 1110 1
		0xEE80,	// G 1110 1110 1
		0xAA00,	// H 1010 101
		0xA000,	// I 101
		0xBBB8,	// J 1011 1011 1011 1
		0xEB80,	// K 1110 1011 1
		0xBA80,	// L 1011 1010 1
		0xEE00,	// M 1110 111
		0xE800,	// N 1110 1
		0xEEE0,	// O 1110 1110 111
		0xBBA0,	// P 1011 1011 101
		0xEEB8,	// Q 1110 1110 1011 1
		0xBA00,	// R 1011 101
		0xA800,	// S 1010 1
		0xE000,	// T 111
		0xAE00,	// U 1010 111
		0xAB80,	// V 1010 1011 1
		0xBB80,	// W 1011 1011 1
		0xEAE0,	// X 1110 1010 111
		0xEBB8,	// Y 1110 1011 1011 1
		0xEEA0	// Z 1110 1110 101
};


/******************************************************
 * Callbacks
 ******************************************************/
static ssize_t my_read(struct file *file, char *buff, size_t count, loff_t *ppos)
{
	int bytes_read = 0;

	if( kfifo_to_user(&morse_fifo, buff, count, *bytes_read) ) {
		return -EFAULT;
	}

	return bytes_read;  // # bytes actually read.
}

static ssize_t my_write(struct file *file, const char *buff, size_t count, loff_t *ppos)
{
	int buff_idx = 0;
	char min_char = 0;

	printk(KERN_INFO "demo_miscdrv: In my_write()\n");

	// Find min character
	for (buff_idx = 0; buff_idx < count; buff_idx++) {
		char ch;
    unsigned short character;
    int counter = 0;

		// Get the character
		if (copy_from_user(&ch, &buff[buff_idx], sizeof(ch))) {
			return -EFAULT;
		}

    // Process the character
		if ('A' <= ch && ch <= 'Z') {
      character = morsecode_codes[(int)ch - 'A'];
    }
    else if ('a' <= ch && ch <= 'z') {
      character = morsecode_codes[(int)ch - 'a'];
    }
    else if (ch == ' ') {
      led_trigger_event(ledtrig_demo, LED_OFF);
      msleep(SPACE_DOT_TIME * dottime);
      kfifo_put(&morse_fifo, ' ');
      kfifo_put(&morse_fifo, ' ');
      kfifo_put(&morse_fifo, ' ');
      continue;
    }
    else {
      continue;
    }

    while( character != 0) {
      if ( (character & MASK) == MASK ) {
        led_trigger_event(ledtrig_demo, LED_FULL);
      	msleep(dottime);
        counter++;
      }
      else {
        led_trigger_event(ledtrig_demo, LED_OFF);
      	msleep(dottime);
        if (counter == 1) {
          kfifo_put(&morse_fifo, '.');
        }
        else if (counter == 3) {
          kfifo_put(&morse_fifo, '-');
        }
        counter = 0;
      }
      character = character << 1;
    }

    if (counter == 1) {
      kfifo_put(&morse_fifo, '.');
    }
    else if (counter == 3) {
      kfifo_put(&morse_fifo, '-');
    }

    kfifo_put(&morse_fifo, ' ');

    led_trigger_event(ledtrig_demo, LED_OFF);
    msleep(GAP_DOT_TIME * dottime);
	}

  kfifo_put(&morse_fifo, '\n');

	return count;
}


/******************************************************
 * Misc support
 ******************************************************/
// Callbacks:  (structure defined in /linux/fs.h)
struct file_operations my_fops = {
	.owner    =  THIS_MODULE,
	.read     =  my_read,
	.write    =  my_write,
};

// Character Device info for the Kernel:
static struct miscdevice my_miscdevice = {
		.minor    = MISC_DYNAMIC_MINOR,         // Let the system assign one.
		.name     = MY_DEVICE_FILE,             // /dev/.... file.
		.fops     = &my_fops                    // Callback functions.
};


/******************************************************
 * Driver initialization and exit:
 ******************************************************/
static int __init testdriver_init(void)
{
  int ret;
  printk(KERN_INFO "----> My morse code driver init()\n");

  ret = misc_register(&my_miscdevice);
  led_trigger_register_simple("morse-code", &ledtrig_demo);

  INIT_KFIFO(morse_fifo);

  return ret;
}

static void __exit testdriver_exit(void)
{
  printk(KERN_INFO "<---- My morse code driver exit().\n");
  misc_deregister(&my_miscdevice);
  led_trigger_unregister_simple(ledtrig_demo);
}
// Link our init/exit functions into the kernel's code.
module_init(testdriver_init);
module_exit(testdriver_exit);
// Information about this module:
MODULE_AUTHOR("Brandon Nguyen");
MODULE_DESCRIPTION("A morse code driver");
MODULE_LICENSE("GPL"); // Important to leave as GPL.
