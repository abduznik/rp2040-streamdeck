#include <string.h>
#include <stdio.h>
#include "tusb.h"
#include "usb_descriptors.h"
#include "pico/unique_id.h"

//--------------------------------------------------------------------
// Device Descriptor
//--------------------------------------------------------------------

#define USB_VID   0xCafe   // placeholder VID — replace with your own if you plan to distribute
#define USB_PID   0x4001

tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    // Composite device: use IAD (Interface Association Descriptor), so class = misc
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------
// HID Report Descriptor
//--------------------------------------------------------------------

uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(REPORT_ID_CONSUMER))
};

uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance) {
    (void) instance;
    return desc_hid_report;
}

//--------------------------------------------------------------------
// Configuration Descriptor
//--------------------------------------------------------------------

enum {
    CONFIG_TOTAL_LEN = TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_CDC_DESC_LEN
};

#define EPNUM_HID          0x81
#define EPNUM_CDC_NOTIF    0x82
#define EPNUM_CDC_OUT      0x03
#define EPNUM_CDC_IN       0x83

uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // HID Interface: itf num, string index, protocol, report descriptor len, EP In, size, polling interval
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 5),

    // CDC Interface: itf num, string index, EP notif, size, EP out, EP in, size
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_COM, 0, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    return desc_configuration;
}

//--------------------------------------------------------------------
// String Descriptors
//--------------------------------------------------------------------

char const *string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 }, // 0: English (0x0409)
    "TeamThrow",                    // 1: Manufacturer
    "StreamDeck RP2040",            // 2: Product
    "SD2040-001",                   // 3: Serial (overridden below w/ unique board ID)
    "StreamDeck Config",             // 4: CDC interface name
};

static uint16_t _desc_str[32];

uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    size_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        // Use board's unique flash ID for the serial number so multiple
        // decks plugged into one machine enumerate distinctly.
        static char serial_buf[17];
        if (index == 3) {
            pico_unique_board_id_t id;
            pico_get_unique_board_id(&id);
            for (int i = 0; i < 8; i++) {
                sprintf(serial_buf + i * 2, "%02X", id.id[i]);
            }
        }

        const char *str = (index == 3) ? serial_buf
                         : (index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))
                           ? string_desc_arr[index] : NULL;
        if (!str) return NULL;

        chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;

        for (size_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}
