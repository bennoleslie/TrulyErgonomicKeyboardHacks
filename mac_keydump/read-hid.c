/* A very simple program that dumps keycodes from a TrulyErgonomic
   keyboard.  It could easily be adapted to work with other keyboards
   if required. */

#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <CoreFoundation/CoreFoundation.h>
#include <wchar.h>
#include <locale.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdbool.h>

#include "mapping.h"

#define TE_VID 0xe6a
#define TE_PID 0x30c

static void show_key(uint8_t key)
{
    const char *str;
    if (key < sizeof keyboard_usage / sizeof keyboard_usage[0]) {
        str = keyboard_usage[key];
    } else {
        str = "Unknown";
    }
    printf("Key 0x%02x %s\n", key, str);
}

static void keyb_report(void *inContext, IOReturn inResult, void *inSender, IOHIDReportType inType,
                        uint32_t inReportID, uint8_t *inReport, CFIndex inReportLength)
{
    int i;

#if defined(EXTRA_DEBUG)
    printf( "%s( context: %p, result: %d, sender: %p,"          \
            "type: %ld, id: %u, report: %p, length: %d ).\n", 
            __PRETTY_FUNCTION__, 
            inContext,
            inResult, 
            inSender,
            ( long ) inType, 
            inReportID, inReport, (int) inReportLength );
    printf("0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
           inReport[0], inReport[1], inReport[2], inReport[3],
           inReport[4], inReport[5], inReport[6], inReport[7]);
#endif
    
    if (inReportLength != 8) {
        printf("Bad report length (%d)\n", (int) inReportLength);
    } else {
        uint8_t modifiers = inReport[0];
        
        if (modifiers & 0x1) { show_key(0xE0); }
        if (modifiers & 0x2) { show_key(0xE1); }
        if (modifiers & 0x4) { show_key(0xE2); }
        if (modifiers & 0x8) { show_key(0xE3); }
        if (modifiers & 0x10) { show_key(0xE4); }
        if (modifiers & 0x20) { show_key(0xE5); }
        if (modifiers & 0x40) { show_key(0xE6); }
        if (modifiers & 0x80) { show_key(0xE7); }

        for (i = 2; i < 8; i++) {
            if (inReport[i] != 0x0) {
                show_key(inReport[i]);
            }
        }
    }
}

uint32_t get_int_property(IOHIDDeviceRef d, CFStringRef id)
{
    CFTypeRef ref;
    uint32_t value = 0;

    ref = IOHIDDeviceGetProperty(d, id);
    if (ref && (CFGetTypeID(ref) == CFNumberGetTypeID())) {
            CFNumberGetValue((CFNumberRef)ref, kCFNumberSInt32Type, &value);
    }
    return value;
}

// this will be called when the HID Manager matches a new ( hot plugged ) HID device
static void device_attach_cb(void *inContext, IOReturn inResult, void *inSender, IOHIDDeviceRef  inIOHIDDeviceRef)
{
    uint32_t pid, vid, page, usage;

    pid = get_int_property(inIOHIDDeviceRef, CFSTR(kIOHIDProductIDKey));
    vid = get_int_property(inIOHIDDeviceRef, CFSTR(kIOHIDVendorIDKey));
    page = get_int_property(inIOHIDDeviceRef, CFSTR(kIOHIDPrimaryUsagePageKey));
    usage = get_int_property(inIOHIDDeviceRef, CFSTR(kIOHIDPrimaryUsageKey));
    printf("VID.PID : Usage: %04x.%04x : %05x.%04x\n", vid, pid, page, usage);
    
    if (vid == TE_VID && pid == TE_PID && page == kHIDPage_GenericDesktop && usage == kHIDUsage_GD_Keyboard) {
        /* could seize here I think */
        IOReturn ret = IOHIDDeviceOpen(inIOHIDDeviceRef, kIOHIDOptionsTypeSeizeDevice);

        if (ret != kIOReturnSuccess) {
            fprintf(stderr, "Error opening HID device\n");
        } else {
            IOHIDDeviceScheduleWithRunLoop(inIOHIDDeviceRef, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
            
            CFIndex reportSize = 8;
            uint8_t *report = malloc(reportSize);
            IOHIDDeviceRegisterInputReportCallback(inIOHIDDeviceRef, report, reportSize, keyb_report, NULL);
        }
    }
}

int
main(void)
{
    IOReturn res;
    IOHIDManagerRef hidmgr;

    /* Create the IOHID Manager */
    hidmgr = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeSeizeDevice);

    /* We don't use any matching criteria. Just match everything, we do
     the selection in the callback */
    IOHIDManagerSetDeviceMatching(hidmgr, NULL);

    /* Register an attach callback */
    IOHIDManagerRegisterDeviceMatchingCallback(hidmgr, device_attach_cb, NULL);

    /* Associate the manager with the client run look (what is that?) */
    IOHIDManagerScheduleWithRunLoop(hidmgr, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

    /* Likely we need to use 'sieze' here but we possibly don't want to do that? */
    res = IOHIDManagerOpen(hidmgr, kIOHIDOptionsTypeNone);
    
    if (res != kIOReturnSuccess) {
        fprintf(stderr, "Error opening HID manager.");
        return 1;
    } else {
        CFRunLoopRun();
        return 0;
    }
}
