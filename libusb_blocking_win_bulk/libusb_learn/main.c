#include <stdio.h>
#include <stdlib.h>
#include "libusb.h"
#include "libusb_dyn.h"

#define VID 0xCafe
#define PID 0x4000

#define EP_OUT 0x01   // endpoint 1 OUT
#define EP_IN  0x81   // endpoint 1 IN

libusb_context *ctx = NULL;
libusb_device_handle *dev = NULL;

int main()
{
   //load a library dynamically
    if (load_libusb()!=0)
        return 1;
  //initialize the library
    if (libusb_init_d(&ctx) < 0) {
        printf("libusb_init failed\n");
        return 1;
    }
  //open a device with given PID, VID
    dev = libusb_open_device_with_vid_pid_d (ctx, VID, PID);
    if (!dev) {
        printf ("Device not found\n");
        return 1;
    }
   /*
   this function checking
   - is a device driver was assigned to a device by OS
   - Choose the concrete interface (parameter 2) by sending a SETUP packet to a device.
   NOTE: The interface number can be assigned by a device developer.
       The endpoint 0 not need to be claimed.A device must have mandatory at teast one interface
       , for example easy USB device has one interface of only one IN OUT pair of bulk endpoints.
   */
    if ( libusb_claim_interface_d(dev, 0) < 0) {
        printf("Cannot claim interface 0\n");
        return 1;
    }

    // --- Send data ---
    unsigned char tx[4] = {1, 2, 3, 4};
    int written = 0;
    int r =  libusb_bulk_transfer_d(dev, EP_OUT, tx, sizeof(tx), &written, 0);

    printf("Write result = %d, bytes = %d\n", r, written);

    // --- Receive data ---
    unsigned char rx[64];
    int read = 0;
    r =  libusb_bulk_transfer_d(dev, EP_IN, rx, sizeof(rx), &read, 0);

    printf("Read result = %d, bytes = %d\n", r, read);

    // Print received bytes
    for (int i=0; i<read; i++)
        printf("%02X ", rx[i]);
    printf("\n");

    //Release
    libusb_release_interface_d(dev, 0);
    libusb_close_d(dev);
    libusb_exit_d(ctx);
      unload_libusb();

    return 0;
}
