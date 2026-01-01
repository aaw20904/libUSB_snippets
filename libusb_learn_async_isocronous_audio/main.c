#include <stdio.h>
#include <stdlib.h>
#include "libusb.h"
#include "libusb_dyn.h"
#include <windows.h>
#include <conio.h>
#define VID 0x0483
#define PID 0x5750

#define ISO_EP_OUT     0x01
#define ISO_PKT_SIZE  64   // Full-speed ISO max packet size
#define ISO_PKTS      8    // 8 packets per transfer
#define TRANSFER_SIZE (ISO_PKT_SIZE * ISO_PKTS) // 64*8 = 512 bytes


// USB globals
libusb_context *ctx = NULL;
libusb_device_handle *dev = NULL;
unsigned int anyData;

unsigned char init_buf[64]={0};
unsigned char lookupTable[]__attribute__((aligned(64)))={
0x80, 0x8c, 0x99, 0xa5, 0xb1, 0xbc, 0xc7, 0xd1,
0xdb, 0xe3, 0xeb, 0xf1, 0xf6, 0xfa, 0xfd, 0xff,
0xff, 0xfe, 0xfc, 0xf8, 0xf4, 0xee, 0xe7, 0xdf,
0xd6, 0xcc, 0xc2, 0xb7, 0xab, 0x9f, 0x93, 0x86,
0x79, 0x6c, 0x60, 0x54, 0x48, 0x3d, 0x33, 0x29,
0x20, 0x18, 0x11, 0xb,  0x7,  0x3,  0x1,  0x0,
0x0,  0x2,  0x5,  0x9,  0xe,  0x14, 0x1c, 0x24,
0x2e, 0x38, 0x43, 0x4e, 0x5a, 0x66, 0x73, 0x7f,
};
//a pointer for "circular" buffer
unsigned char* sinePtr __attribute__((aligned(64)));
unsigned int ptrRemainder;

int keyExit;
///It is an example for interrupt device->host USB transition

// Buffer and length — used by callback (USB thread) and read owner (main thread)


// Background USB events thread
HANDLE usb_thread_handle;
//a thread running flag
volatile int usb_thread_running = 1;

HANDLE usb_thread_h;  // handle to the thread
volatile int usb_thread_run = 1; // flag to stop thread

// -----------------USB EVENT THREAD -----------------

DWORD WINAPI usb_event_thread(LPVOID param)
{
    struct timeval tv = {0, 1000}; // 1ms
    while (usb_thread_running)
        libusb_handle_events_timeout_d(ctx, &tv);
}



// ----------------- READ CALLBACK -----------------

//read CB was removed
// ----------------- WRITE CALLBACK -----------------

/// Host finished sending all  packets (full transfer)
void LIBUSB_CALL write_callback(struct libusb_transfer *t)
{
    if(t->status == LIBUSB_TRANSFER_COMPLETED)
    {
        // fill each ISO packet with 64-byte slices from lookupTable
        for (int pkt = 0; pkt < t->num_iso_packets; pkt++)
        {   //get address for the write procedure
            unsigned char *buf = libusb_get_iso_packet_buffer_simple(t, pkt);
            memcpy(buf, lookupTable + ptrRemainder, ISO_PKT_SIZE);
            ptrRemainder += ISO_PKT_SIZE;
            if(ptrRemainder >= sizeof(lookupTable))
                ptrRemainder = 0;  // wrap around
        }

        // resubmit for continuous streaming
        libusb_submit_transfer_d(t);
        return;
    }

    // on error stop
    free(t->buffer);
    libusb_free_transfer_d(t);
}



 // ----------------- ASYNC READ FUNCTION -----------------

//was removed

// ----------------- ASYNC WRITE FUNCTION ----------------------

int usb_iso_write_async(void)
{
    // allocate transfer with ISO_PKTS packets
    struct libusb_transfer *t = libusb_alloc_transfer_d(ISO_PKTS);
    if(!t) return -1;

    unsigned char *buf = malloc(TRANSFER_SIZE);
    if(!buf) { libusb_free_transfer_d(t); return -2; }
    memset(buf, 0, TRANSFER_SIZE);  // initially empty

    // fill transfer
    libusb_fill_iso_transfer(
        t, dev, ISO_EP_OUT,
        buf,
        TRANSFER_SIZE,
        ISO_PKTS,
        write_callback,
        NULL,
        0
    );

    libusb_set_iso_packet_lengths(t, ISO_PKT_SIZE);

    int r = libusb_submit_transfer_d(t);
    if(r != LIBUSB_SUCCESS)
    {
        printf("submit write error: %s\n", libusb_error_name_d(r));
        free(buf);
        libusb_free_transfer_d(t);
        return -3;
    }

    return 0;
}

// ----------------- WAIT FUNCTIONS -----------------


// ----------------- THREAD START/STOP -----------------

void start_usb_thread()
{
    usb_thread_running = 1;

   usb_thread_handle = CreateThread(
        NULL,               // security attributes
        0,                  // stack size (0 = default size 1 MB reserved for Windows)
        usb_event_thread,   // thread function
        NULL,               // parameter to thread function
        0,                  // creation flags
        NULL                // receives thread ID
    );

}

void stop_usb_thread()
{
    usb_thread_running = 0;

    CloseHandle(usb_thread_handle);
}


/*


                                            iiii
                                           i::::i
                                            iiii

   mmmmmmm    mmmmmmm     aaaaaaaaaaaaa   iiiiiiinnnn  nnnnnnnn
 mm:::::::m  m:::::::mm   a::::::::::::a  i:::::in:::nn::::::::nn
m::::::::::mm::::::::::m  aaaaaaaaa:::::a  i::::in::::::::::::::nn
m::::::::::::::::::::::m           a::::a  i::::inn:::::::::::::::n
m:::::mmm::::::mmm:::::m    aaaaaaa:::::a  i::::i  n:::::nnnn:::::n
m::::m   m::::m   m::::m  aa::::::::::::a  i::::i  n::::n    n::::n
m::::m   m::::m   m::::m a::::aaaa::::::a  i::::i  n::::n    n::::n
m::::m   m::::m   m::::ma::::a    a:::::a  i::::i  n::::n    n::::n
m::::m   m::::m   m::::ma::::a    a:::::a i::::::i n::::n    n::::n
m::::m   m::::m   m::::ma:::::aaaa::::::a i::::::i n::::n    n::::n
m::::m   m::::m   m::::m a::::::::::aa:::ai::::::i n::::n    n::::n
mmmmmm   mmmmmm   mmmmmm  aaaaaaaaaa  aaaaiiiiiiii nnnnnn    nnnnnn


*/

    int main() {
        sinePtr = lookupTable;
        ptrRemainder = 0;
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

          libusb_set_interface_alt_setting_d(dev, 0, 0);

          libusb_set_auto_detach_kernel_driver_d(dev, 1);

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





         // Start USB background thread

         usb_thread_running = 1;
         usb_thread_handle = CreateThread(NULL, 0,
                                         usb_event_thread,
                                         NULL, 0, NULL);


            keyExit=0;
            anyData=0;


        // 2) Give libusb a moment to register the device
        struct timeval tv = {0, 1000};
        libusb_handle_events_timeout_d(ctx, &tv);  // <-- one call before submit

                // START ISO STREAMING ONCE - the first transfered data are from init_buf,
                //after this - data must be consumed inside the fired callback
            usb_iso_write_async();


            while (1) {

                    if (_kbhit()) {
                        keyExit = _getch();
                        if (keyExit == 'q'){
                            break;
                        }
                    }






            }


       // Stop streaming
        //libusb_set_interface_alt_setting_d(dev, 0, 0);

        //Release
        // -------------------------------
        // CLEANUP
        // -------------------------------

        stop_usb_thread();

        //relese USB interface 0
        libusb_release_interface_d(dev, 0);
        //close the library
        libusb_close_d(dev);
        libusb_exit_d(ctx);
        //close DLL and free memory that was busy for DLL functions
        printf("DONE.\n");
          unload_libusb();

        return 0;
    }
