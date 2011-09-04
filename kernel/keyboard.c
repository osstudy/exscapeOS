#include <types.h>
#include <gdtidt.h> /* isr_t, registers_t */
#include <kernutil.h> /* panic */
#include <monitor.h>

void register_interrupt_handler(uint8 n, isr_t handler);

typedef struct keyboard_mod_state {
	uint8 shift;
	uint8 ctrl;
	uint8 alt;
} keyboard_mod_state;

static keyboard_mod_state mod_keys = {0};

/* A US keymap, courtesy of Bran's tutorial */
unsigned char kbdus[128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
  '9', '0', '-', '=', '\b',	/* Backspace */
  '\t',			/* Tab */
  'q', 'w', 'e', 'r',	/* 19 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
    0,			/* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
 '\'', '`',   0,		/* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
  'm', ',', '.', '/',   0,				/* Right shift */
  '*',
    0,	/* Alt */
  ' ',	/* Space bar */
    0,	/* Caps lock */
    0,	/* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,	/* < ... F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    0,	/* Home key */
    0,	/* Up Arrow */
    0,	/* Page Up */
  '-',
    0,	/* Left Arrow */
    0,
    0,	/* Right Arrow */
  '+',
    0,	/* 79 - End key*/
    0,	/* Down Arrow */
    0,	/* Page Down */
    0,	/* Insert Key */
    0,	/* Delete Key */
    0,   0,   0,
    0,	/* F11 Key */
    0,	/* F12 Key */
    0,	/* All other keys are undefined */
};		


// Excerpt from the US no-modifier key-table
  //'q', 'w', 'e', 'r',	/* 19 */
  //'t', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
    //0,			/* 29   - Control */
  //'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
 //'\'', '`',   0,		/* Left shift */
 //'<', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
  //'m', ',', '.', '-',   0,				/* Right shift */

unsigned char kbdse_shift[128] =
{
    0,  27, '!', '\"', '#', 0 /* shift+4 */, '%', '&', '/', '(',	/* 9 */
  ')', '=', '?', '`', '\b',	/* Backspace */
  '\t',			/* Tab */

 'Q', 'W', 'E', 'R',   /* 19 */
  'T', 'Y', 'U', 'I', 'O', 'P', 'A', 'A', '\n', /* Enter key */
    0,          /* 29   - Control */
  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 'O', /* 39 */
 '\'', '`',   0,        /* Left shift */
 '>', 'Z', 'X', 'C', 'V', 'B', 'N',            /* 49 */
  'M', ';', ':', '_',   0,              /* Right shift */

  '*',
    0,	/* Alt */
  ' ',	/* Space bar */
    0,	/* Caps lock */
    0,	/* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,	/* < ... F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    0,	/* Home key */
    0,	/* Up Arrow */
    0,	/* Page Up */
  '-',
    0,	/* Left Arrow */
    0,
    0,	/* Right Arrow */
  '+',
    0,	/* 79 - End key*/
    0,	/* Down Arrow */
    0,	/* Page Down */
    0,	/* Insert Key */
    0,	/* Delete Key */
    0,   0,   0,
    0,	/* F11 Key */
    0,	/* F12 Key */
    0,	/* All other keys are undefined */
};		



void keyboard_callback(registers_t regs) {
	regs = regs; // avoid a warning
	unsigned char scancode = inb(0x60);
	unsigned char c;
	if (scancode == 0xe0)
		return; // For now

//	printk("in: %02x\n", scancode);

	/* 
	 * Note: This code ignores escaped scancodes (0xe0 0x*) for now.
	 * After looking through a table of possibilities, none of them
	 * matter much! For instance, they are used to differ between
	 * left and right ctrl/alt, Keypad enter and return...
	 * Since there's no current support for arrow keys/the keypad,
	 * ignoring the 0xe0 byte means nothing bad.
	 */
	switch (scancode) {
		case 0x2a: /* shift down */
		case 0x36: /* right shift down */
			mod_keys.shift = 1;
			return;
			break;
		case 0xaa: /* shift up */
		case 0xb6: /* right shift up */
			mod_keys.shift = 0;
			return;
			break;

		case 0x1d: /* ctrl down */
			mod_keys.ctrl = 1;
			return;
			break;
		case 0x9d: /* ctrl up */
			mod_keys.ctrl = 0;
			return;
			break;

		case 0x38: /* alt down */
			mod_keys.alt = 1;
			return;
			break;
		case 0xb8: /* alt up */
			mod_keys.alt = 0;
			return;
			break;

		default:
			break;
	}

	/* We're still here, so the scancode wasn't a modifier key changing state */

	if (!mod_keys.shift && !mod_keys.alt && !mod_keys.ctrl && !(scancode & 0x80)) {
		// No modifiers
		c = kbdus[scancode];
	}
	else if (mod_keys.shift && !mod_keys.alt && !mod_keys.ctrl && !(scancode & 0x80)) {
		// Shift + key
		c = kbdse_shift[scancode];
	}
	else if ( !(scancode & 0x80) ) { // scancode isn't simply a supported key being released
		printk("Not implemented (scancode = %02x)\n", scancode);
		return;
	}
	else if (scancode & 0x80) {
		// Key was released
		return;
	}

//	printk("%02x ", scancode);

	if (! (scancode & 0x80) && c != 0) {
/*	   if (c == 0x08) {
		   cursor.x--; putchar(' '); cursor.x--;
	   } 
	   else { */
		printk("%c", c);
	   }
}
