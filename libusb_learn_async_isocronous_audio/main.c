#include <stdio.h>
#include <stdlib.h>
#include "libusb.h"
#include "libusb_dyn.h"
#include <windows.h>
#include <conio.h>
#define VID 0xCafe
#define PID 0x4000

#define ISO_EP_OUT     0x01
#define ISO_PKT_SIZE  32
#define ISO_PKTS      1    // FS: 1 packet per frame

#define READ_BUF_SIZE 64
#define WRITE_BUF_SIZE 64
#define NUM_BUFFERS 4
/*NOTE:
Use the "stm32_usb_interrrupt_dev_to_host" folder with device software to run the current application.
*/
libusb_context *ctx = NULL;
libusb_device_handle *dev = NULL;
unsigned int anyData;

// Async WRITE semaphore
HANDLE sem_write_done;

int keyExit;
///It is an example for interrupt device->host USB transition

// Buffer and length — used by callback (USB thread) and read owner (main thread)
volatile int read_len = 0;                 // volatile to avoid compiler reordering
unsigned char read_buf[READ_BUF_SIZE];     // fixed-size global buffer
unsigned char write_buf[WRITE_BUF_SIZE];     // fixed-size global buffer

volatile int write_result = 0;

// Async READ semaphore
HANDLE sem_read_done;

// Async WRITE semaphore
HANDLE sem_write_done;

// Background USB events thread
HANDLE usb_thread_handle;
//a thread running flag
volatile int usb_thread_running = 1;

HANDLE usb_thread_h;  // handle to the thread
volatile int usb_thread_run = 1; // flag to stop thread

// ----------------- EVENT THREAD -----------------

DWORD WINAPI usb_event_thread(LPVOID param)
{
    struct timeval tv = {0, 1000}; // 1ms
    while (usb_thread_running)
        libusb_handle_events_timeout_d(ctx, &tv);
}



// ----------------- READ CALLBACK -----------------

//read CB was removed
// ----------------- WRITE CALLBACK -----------------

void LIBUSB_CALL write_callback(struct libusb_transfer *t)
{
    if (t->status == LIBUSB_TRANSFER_COMPLETED)
    {
       // printf("[WRITE CALLBACK] Write OK (%d bytes)\n",t->actual_length);
        write_result = 0;
    }
    else
    {
        printf("[WRITE CALLBACK] ERROR status=%d\n", t->status);
        write_result = -1;
    }
    //“USB finished one buffer — you may send another.”
    ReleaseSemaphore(sem_write_done, 1, NULL);

    free(t->buffer);
    libusb_free_transfer_d(t);
}

 // ----------------- ASYNC READ FUNCTION -----------------

//was removed

// ----------------- ASYNC WRITE FUNCTION ----------------------

int usb_iso_write_async(unsigned char *data, int size, int packetLen, int packetsInTransfer) {
    if (size > packetLen * packetsInTransfer) {
        printf("usb_write_async: size too large for ISO packets\n");
        return -4;
    }

    // 1) Allocate a libusb transfer structure
    struct libusb_transfer *t = libusb_alloc_transfer_d(packetsInTransfer);
    if (!t) {
        return -1;
    }

    // 2) Allocate memory for outgoing USB data
    unsigned char *buf = malloc(size);
    if (!buf) {
        libusb_free_transfer_d(t);
        return -2;
    }
    memcpy(buf, data, size);

    // 3) Prepare the isochronous transfer
    libusb_fill_iso_transfer(
        t, dev, ISO_EP_OUT,
        buf,
        size, // total transfer size
        packetsInTransfer, // number of packets
        write_callback, // callback function
        NULL,    // user data
        0 // timeout in ms
    );

    // 4) Set each ISO packet length
    libusb_set_iso_packet_lengths(t, packetLen);

    // 5) Submit the transfer
    int r = libusb_submit_transfer_d(t);
    if (r != LIBUSB_SUCCESS) {
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
    WaitForSingleObject(usb_thread_handle, INFINITE);
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

    libusb_set_interface_alt_setting_d(dev, 0, 1);


         // Create semaphores with initial count = 0
     sem_write_done = CreateSemaphore(NULL,
                                NUM_BUFFERS, // initial count
                                NUM_BUFFERS,
                                NULL);

     // Start USB background thread

     usb_thread_running = 1;
     usb_thread_handle = CreateThread(NULL, 0,
                                     usb_event_thread,
                                     NULL, 0, NULL);


        keyExit=0;
        anyData=0;

        while (1) {

                if (_kbhit()) {
                    keyExit = _getch();
                    if (keyExit == 'q'){
                        break;
                    }
                }

                // wait BEFORE submitting
                WaitForSingleObject(sem_write_done, INFINITE);

                // now it is SAFE to submit ONE transfer
               if( usb_iso_write_async("Alice was beginning to get very tired of sitting by her sister o",64,8,8) !=0) {
                    // submission failed → return credit
                    ReleaseSemaphore(sem_write_done, 1, NULL);
               }



        }


   // Stop streaming
    libusb_set_interface_alt_setting_d(dev, 0, 0);

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
