#include "keyboard.h"
#include "ports.h"
#include "terminal.h"
#include "shell.h"

static bool shift_pressed = false;
static bool alt_pressed = false;

char scancode_to_char_normal[MAX_SCANCODE] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',  
    '\t',  
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',  
    0,    
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 
    0,    
    '*',
    0,    
    ' ',  
    0,    
};

const char scancode_to_char_shifted[MAX_SCANCODE] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',  
    '\t', 
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 
	'\n',  
    0, 
	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, 
	'|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 
    0, 
	'*', 0, ' ',
	0
};


// Process scancode input
void handle_scancode(uint8_t scancode) {
    if (scancode == SC_LSHIFT || scancode == SC_RSHIFT) { 
        shift_pressed = true;
    } else if (scancode == SC_LSHIFT_RELEASE || scancode == SC_RSHIFT_RELEASE) { 
        shift_pressed = false;
    } else if (scancode == SC_ALT) { 
        alt_pressed = true;
    } else if (scancode == SC_ALT_RELEASE) { 
        alt_pressed = false;
    } else if (alt_pressed && scancode >= SC_F1 && scancode <= SC_F9) { 
        switch_screen((uint8_t)(scancode - SC_F1));
        shell_on_screen_switch();
    } else if (scancode < MAX_SCANCODE) {
        char c = shift_pressed ? scancode_to_char_shifted[scancode] : scancode_to_char_normal[scancode];
        if (c) {
            shell_input(c);
        }
    }
}

// Poll keyboard input
void poll_keyboard() {
    if (inb(KEYBOARD_STATUS_PORT) & 1) { 
        uint8_t scancode = inb(KEYBOARD_DATA_PORT);
        handle_scancode(scancode);
    }
}
