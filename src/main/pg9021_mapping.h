#ifndef PG9021_MAPPING_H
#define PG9021_MAPPING_H

typedef void (*gamepad_handler_t)(uint16_t page, uint16_t event, int32_t value);

// Pages
enum {
  PAGE_GAMEPAD_DPAD_THUMB = 0x0001,
  PAGE_GAMEPAD_BUTTONS = 0x0009,
  PAGE_KEYBOARD_BUTTONS = 0x0007,
  PAGE_MISC_ADDITIONAL_BUTTONS = 0x000c
};

// Misc additional buttons
enum {  // Page - 0x000c
  MISC_BUTTON_HOME = 0x0223,
  MISC_BUTTON_MINUS = 0x00ea,
  MISC_BUTTON_PREV = 0x00b6,
  MISC_BUTTON_PLAY = 0x00cd,
  MISC_BUTTON_NEXT = 0x00b5,
  MISC_BUTTON_PLUS = 0x00e9
};

/*
 * KEYBOARD
 */

// D-pad
enum {  // Page - 0x0007
  KB_DPAD_UP = 0x001a,
  KB_DPAD_DOWN = 0x0016,
  KB_DPAD_RIGHT = 0x0007,
  KB_DPAD_LEFT = 0x0004
};

// Buttons are the main gamepad buttons
enum {  // Page - 0x0007
  KB_BUTTON_A = 0x001f,
  KB_BUTTON_B = 0x0020,
  KB_BUTTON_X = 0x001e,
  KB_BUTTON_Y = 0x0021,
  KB_BUTTON_SHOULDER_L = 0x0022,
  KB_BUTTON_SHOULDER_R = 0x0023,
  KB_BUTTON_TRIGGER_L = 0x0024,
  KB_BUTTON_TRIGGER_R = 0x0025,
  KB_BUTTON_THUMB_L = 0x0026,
  KB_BUTTON_THUMB_R = 0x0027
};

// Misc main buttons
enum {  // Page - 0x0007
  KB_MISC_BUTTON_START = 0x0028,
  KB_MISC_BUTTON_SELECT = 0x002a
};

// Thumb
enum {  // Page - 0x0007
  KB_THUMB_L_UP = 0x0052,
  KB_THUMB_L_DOWN = 0x0051,
  KB_THUMB_L_RIGHT = 0x004f,
  KB_THUMB_L_LEFT = 0x0050,
  KB_THUMB_R_UP = 0x000c,
  KB_THUMB_R_DOWN = 0x000e,
  KB_THUMB_R_RIGHT = 0x000f,
  KB_THUMB_R_LEFT = 0x000d
};

/*
 * GAMEPAD
 */

// Buttons are the main gamepad buttons
enum {  // Page - 0x0009
  GP_BUTTON_A = 0x0001,
  GP_BUTTON_B = 0x0002,
  GP_BUTTON_C = 0x0003,
  GP_BUTTON_X = 0x0004,
  GP_BUTTON_Y = 0x0005,
  GP_BUTTON_Z = 0x0006,
  GP_BUTTON_SHOULDER_L = 0x0007,
  GP_BUTTON_SHOULDER_R = 0x0008,
  GP_BUTTON_TRIGGER_L = 0x0009,
  GP_BUTTON_TRIGGER_R = 0x000a,
  GP_BUTTON_UNKNOWN = 0x000d,
  GP_BUTTON_THUMB_L = 0x000e,
  GP_BUTTON_THUMB_R = 0x000f
};

// Misc main buttons
enum {  // Page - 0x0009
  GP_MISC_BUTTON_START = 0x000c,
  GP_MISC_BUTTON_SELECT = 0x000b
};

// D-pad
enum {  // Page - 0x0001, below NOT usage (usage = 0x0039), it is VALUE (default
        // = 0x8, d-pad released)
  GP_DPAD_UP = 0x0,
  GP_DPAD_DOWN = 0x4,
  GP_DPAD_RIGHT = 0x2,
  GP_DPAD_LEFT = 0x6,
  GP_DPAD_RELEASED = 0x8
};

// Thumb
enum {  // Page - 0x0001, value 0-255, (0x7f) 127 - center (thumb released)
  GP_THUMB_L_X = 0x0030,
  GP_THUMB_L_Y = 0x0031,
  GP_THUMB_R_X = 0x0032,
  GP_THUMB_R_Y = 0x0035
};

// Special

enum { GP_USAGE_DPAD = 0x0039, GP_THUMB_RELEASED = 0x7f };

#endif  // PG9021_MAPPING_H