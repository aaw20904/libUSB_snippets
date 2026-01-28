#include <stdio.h>
#include <stdlib.h>
#include "libusb.h"
#include "libusb_dyn.h"

#define VID 0xCafe
#define PID 0x4000

#define EP_OUT 0x01   // endpoint 1 OUT
#define EP_IN  0x81   // endpoint 1 IN


/** Generated using Dr LUT - Free Lookup Table Generator
  * https://github.com/ppelikan/drlut
  **/
// Formula: sin(2*pi*t/T)
uint8_t testAudio[192] = {
0x64,0x6A,0x70,0x77,0x7C,0x82,0x88,0x8D,0x91,0x95,
0x99,0x9C,0x9F,0xA1,0xA3,0xA4,0xA4,0xA4,0xA3,0xA1,
0x9F,0x9C,0x99,0x95,0x91,0x8D,0x88,0x82,0x7C,0x77,
0x70,0x6A,0x64,0x5E,0x58,0x51,0x4C,0x46,0x40,0x3B,
0x37,0x33,0x2F,0x2C,0x29,0x27,0x25,0x24,0x24,0x24,
0x25,0x27,0x29,0x2C,0x2F,0x33,0x37,0x3B,0x40,0x46,
0x4C,0x51,0x58,0x5E,
0x64,0x6A,0x70,0x77,0x7C,0x82,0x88,0x8D,0x91,0x95,
0x99,0x9C,0x9F,0xA1,0xA3,0xA4,0xA4,0xA4,0xA3,0xA1,
0x9F,0x9C,0x99,0x95,0x91,0x8D,0x88,0x82,0x7C,0x77,
0x70,0x6A,0x64,0x5E,0x58,0x51,0x4C,0x46,0x40,0x3B,
0x37,0x33,0x2F,0x2C,0x29,0x27,0x25,0x24,0x24,0x24,
0x25,0x27,0x29,0x2C,0x2F,0x33,0x37,0x3B,0x40,0x46,
0x4C,0x51,0x58,0x5E,
0x64,0x6A,0x70,0x77,0x7C,0x82,0x88,0x8D,0x91,0x95,
0x99,0x9C,0x9F,0xA1,0xA3,0xA4,0xA4,0xA4,0xA3,0xA1,
0x9F,0x9C,0x99,0x95,0x91,0x8D,0x88,0x82,0x7C,0x77,
0x70,0x6A,0x64,0x5E,0x58,0x51,0x4C,0x46,0x40,0x3B,
0x37,0x33,0x2F,0x2C,0x29,0x27,0x25,0x24,0x24,0x24,
0x25,0x27,0x29,0x2C,0x2F,0x33,0x37,0x3B,0x40,0x46,
0x4C,0x51,0x58,0x5E,
};

uint16_t circAudioIndex;

libusb_context *ctx = NULL;
libusb_device_handle *dev = NULL;

typedef struct {
	uint32_t totalLenght;   //in bytes
	uint32_t parcelSize;  //in bytes
	uint32_t amountOfParcels;
	uint32_t sampleRate;  //in Hz
}audioParams;

typedef struct {
	uint32_t status;
	uint32_t requestType;
	uint32_t xyz1; //not used
	uint32_t xyz2; //not used
}commandResp;

audioParams trackParams;
commandResp devResp;

// Buffer and length — used by callback (USB thread) and read owner (main thread)
volatile int read_len = 0;                 // volatile to avoid compiler reordering
unsigned char read_buf[2048];     // fixed-size global buffer
unsigned char write_buf[2048];
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

DWORD WINAPI usb_event_thread (LPVOID param) {
    //This creates a POSIX-style timeout struct:
    //This is fast enough for high-performance USB transfers and slow enough to avoid CPU overload.
    struct timeval tv = {0, 1000}; // 1ms timeout
    //
    while (usb_thread_running)
    {
        //The libusb say to Kernel OS: “Wake me when a USB transfer completes, or after the timeout expires.”
        int r = libusb_handle_events_timeout_d(ctx, &tv);
        //The thread is not running until USB completion events
        //When USB event occured -  libusb wakes up
        if (r != LIBUSB_SUCCESS && r != LIBUSB_ERROR_TIMEOUT)
        {
            printf("libusb_handle_events_timeout error: %s\n",
                   libusb_error_name_d(r));
        }
    }
    return 0;
}


// ----------------- READ CALLBACK -----------------

void LIBUSB_CALL read_callback(struct libusb_transfer *t)
{

      if (t->status == LIBUSB_TRANSFER_COMPLETED)
    {
        memcpy(read_buf + read_len, t->buffer, t->actual_length);
        read_len += t->actual_length;
    }
    else
    {
        printf("[READ CALLBACK] ERROR status=%d\n", t->status);
        read_len = -1;
    }

    // notify main thread (unblocks WaitForSingleObject)
    ReleaseSemaphore(sem_read_done, 1, NULL);

    // free allocated transfer & its buffer (we already copied data)
    free(t->buffer);
    libusb_free_transfer_d(t);
}



// ----------------- WRITE CALLBACK -----------------

void LIBUSB_CALL write_callback(struct libusb_transfer *t)
{
    if (t->status == LIBUSB_TRANSFER_COMPLETED)
    {
        /*printf("[WRITE CALLBACK] Write OK (%d bytes)\n",
               t->actual_length);*/
        write_result = 0;
    }
    else
    {
        printf("[WRITE CALLBACK] ERROR status=%d\n", t->status);
        write_result = -1;
    }

    ReleaseSemaphore(sem_write_done, 1, NULL);

    free(t->buffer);
    libusb_free_transfer_d(t);
}



 // ----------------- ASYNC READ FUNCTION -----------------

int usb_read_async(unsigned char ep, int size)
{    // 1) Allocate a libusb transfer structure
    struct libusb_transfer *t = libusb_alloc_transfer_d(0);
    if (!t) {
    // when allocation failed
        return -1;
    }
     // 2) Allocate memory for incoming USB data
    unsigned char *buf = malloc(size);
    if (!buf) return -2;
    // 3) Prepare the transfer object (endpoint, buffer, callback, timeout)
    libusb_fill_bulk_transfer(
        t, dev, ep,
        buf, size,
        read_callback,
        NULL,
        2000
    );
     // 4) Submit the transfer to libusb/kernel
    int r = libusb_submit_transfer_d(t);
    if (r != LIBUSB_SUCCESS)
    {    // 5) when Submission failed — free allocated resources
        printf("submit read error: %s\n", libusb_error_name_d(r));
        free(buf);
        libusb_free_transfer_d(t);
        return -3;
    }

    return 0;
}

// ----------------- ASYNC WRITE FUNCTION ----------------------

int usb_write_async(unsigned char ep, unsigned char *data, int size) {
    // 1) Allocate a libusb transfer structure
    struct libusb_transfer *t = libusb_alloc_transfer_d(0);
    if (!t){
        //when allocation failed
        return -1;
    }
     // 2) Allocate memory for outgoing USB data
    unsigned char *buf = malloc(size);
    if (!buf) return -2;
    //copy data into Tx buffer
    memcpy(buf, data, size);
     // 3) Prepare the transfer object (endpoint, buffer, callback, timeout)
    libusb_fill_bulk_transfer(
        t, dev, ep,
        buf, size,
        write_callback,
        NULL,
        2000
    );
    // 4) Submit the transfer to libusb/kernel
    int r = libusb_submit_transfer_d(t);
    if (r != LIBUSB_SUCCESS)
    {
        //when an error has happened - free memory and release transfer
        printf("submit write error: %s\n", libusb_error_name_d(r));
        free(buf);
        libusb_free_transfer_d(t);
        return -3;
    }

    return 0;
}
// ----------------- WAIT FUNCTIONS -----------------

void wait_for_read()
{
    //When a semaphore is not released, The kernel does NOT execute your thread anymore.
    //CPU core immediately gets reassigned to other runnable threads in the system.
    //When a semaphore releases - the scheduler wakes up this thread, and
    //the program continue execution fron the next string (after the  WaitForSingleObject() )
    WaitForSingleObject(sem_read_done, INFINITE);
}

void wait_for_write() {
    //When a semaphore is not released, The kernel does NOT execute your thread anymore.
    //CPU core immediately gets reassigned to other runnable threads in the system.
    //When a semaphore releases - the scheduler wakes up this thread, and
    //the program continue execution fron the next string (after the  WaitForSingleObject() )
    WaitForSingleObject(sem_write_done, INFINITE);
}
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

     // Create semaphores with initial count = 0
    sem_read_done = CreateSemaphore(NULL, 0, 1, NULL);
    sem_write_done = CreateSemaphore(NULL, 0, 1, NULL);
     // Start USB background thread

     usb_thread_running = 1;
     usb_thread_handle = CreateThread(NULL, 0,
                                     usb_event_thread,
                                     NULL, 0, NULL);

    while (1){
      usb_write_async(0x01, testAudio, 192);
       wait_for_write();
       if (write_result != 0)
            printf("Write FAILED!\n");
       }




    // -------------------------------
    // Example: ASYNC WRITE
    // -------------------------------

    unsigned char msg[] = { 0x80, 0x84, 0x88, 0x8c, 0x90, 0x94, 0x98, 0x9c,
                    0xa0, 0xa4, 0xa8, 0xac, 0xaf, 0xb3, 0xb7, 0xba,
                    0xbe, 0xc2, 0xc5, 0xc8, 0xcc, 0xcf, 0xd2, 0xd5,
                    0xd8, 0xdb, 0xde, 0xe1, 0xe3, 0xe6, 0xe8, 0xea,
                    0xed, 0xef, 0xf1, 0xf2, 0xf4, 0xf6, 0xf7, 0xf9,
                    0xfa, 0xfb, 0xfc, 0xfd, 0xfd, 0xfe, 0xfe, 0xff,
                    0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0xfd, 0xfd,
                    0xfc, 0xfb, 0xfa, 0xf9, 0xf7, 0xf6, 0xf4, 0xf2,
                    0xf1, 0xef, 0xed, 0xea, 0xe8, 0xe6, 0xe3, 0xe1,
                    0xde, 0xdb, 0xd8, 0xd5, 0xd2, 0xcf, 0xcc, 0xc8,
                    0xc5, 0xc2, 0xbe, 0xba, 0xb7, 0xb3, 0xaf, 0xac,
                    0xa8, 0xa4, 0xa0, 0x9c, 0x98, 0x94, 0x90, 0x8c,
                    0x88, 0x84, 0x80, 0x7b, 0x77, 0x73, 0x6f, 0x6b,
                    0x67, 0x63, 0x5f, 0x5b, 0x57, 0x53, 0x50, 0x4c,
                    0x48, 0x45, 0x41, 0x3d, 0x3a, 0x37, 0x33, 0x30,
                    0x2d, 0x2a, 0x27, 0x24, 0x21, 0x1e, 0x1c, 0x19,
                    0x17, 0x15, 0x12, 0x10, 0xe, 0xd, 0xb, 0x9,
                    0x8, 0x6, 0x5, 0x4, 0x3, 0x2, 0x2, 0x1,
                    0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x1,
                    0x2, 0x2, 0x3, 0x4, 0x5, 0x6, 0x8, 0x9,
                    0xb, 0xd, 0xe, 0x10, 0x12, 0x15, 0x17, 0x19,
                    0x1c, 0x1e, 0x21, 0x24, 0x27, 0x2a, 0x2d, 0x30,
                    0x33, 0x37, 0x3a, 0x3d, 0x41, 0x45, 0x48, 0x4c,
                    0x50, 0x53, 0x57, 0x5b, 0x5f, 0x63, 0x67, 0x6b,
                    0x6f, 0x73, 0x77, 0x7b, 0x7f,   };

    printf("Submitting async write...\n");
    usb_write_async(0x01, msg, sizeof(msg));

    printf("Waiting for write to finish...\n");
    //When a semaphore not released, the OS kernel stop execution
    //and reassign CPU to another tasks.When a semaphore released - the Kernel wakes up

    wait_for_write();
     // the main thread and execution continue from the next code line
    if (write_result == 0)
        printf("Write OK!\n");
    else
        printf("Write FAILED!\n");

    // -------------------------------
    // Example: ASYNC READ
    // -------------------------------

    printf("Submitting async read...\n");
    usb_read_async(0x81, sizeof(msg)); //128

    printf("Waiting for read...\n");
     //When a semaphore not released, the OS kernel stop execution
    //and reassign CPU to another tasks.When a semaphore released - the Kernel wakes up
    wait_for_read();
    // the main thread and execution continue from the next code line
    if (read_len > 0)
    {
        printf("Read %d bytes: ", read_len);
        for (int i = 0; i < read_len; i++)
            printf("%02X ", read_buf[i]);
            printf("\n");
    }
    else
    {
        printf("Read failed!\n");
    }

    //Release
    // -------------------------------
    // CLEANUP
    // -------------------------------

    stop_usb_thread();
    //remove semaphores
    CloseHandle(sem_read_done);
    CloseHandle(sem_write_done);
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
