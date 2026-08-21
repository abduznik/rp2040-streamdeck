#ifndef USB_DESCRIPTORS_H_
#define USB_DESCRIPTORS_H_

#include "tusb.h"

// Two HID report types multiplexed by Report ID:
// 1 = keyboard (6-key rollover + modifiers)
// 2 = consumer control (media keys, volume, etc.)
enum {
    REPORT_ID_KEYBOARD = 1,
    REPORT_ID_CONSUMER = 2,
    REPORT_ID_COUNT
};

#define ITF_NUM_HID       0
#define ITF_NUM_CDC_COM   1
#define ITF_NUM_CDC_DATA  2
#define ITF_NUM_TOTAL     3

#endif
