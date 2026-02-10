///The library-wrapper arund the LibUSB
#include <stdio.h>
#include <stdlib.h>
#include "libusb.h"
#include "libusb_dyn.h"
#include "music.h"
#include <windows.h>


#define EP_OUT 0x01   // endpoint 1 OUT
#define EP_IN  0x81   // endpoint 1 IN

#define MYUSB_BUFF_L 65535

libusb_context *ctx = NULL;
libusb_device_handle *dev = NULL;


// Buffer and length — used by callback (USB thread) and read owner (main thread)
volatile LONG read_len = 0;                 // volatile to avoid compiler reordering
unsigned char read_buf[MYUSB_BUFF_L];     // fixed-size global buffer
unsigned char write_buf[MYUSB_BUFF_L];
volatile int write_result = 0;
static volatile LONG read_in_flight = 0;

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
        LONG offset = InterlockedExchangeAdd(&read_len, t->actual_length);
        memcpy(read_buf + offset, t->buffer, t->actual_length);
       // memcpy(read_buf + read_len, t->buffer, t->actual_length);


    }
    else
    {
        InterlockedExchange(&read_len, -1);

    }

    // Mark read as finished
    InterlockedExchange(&read_in_flight, 0);
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

 int getReadDataLengh(void){
    return (int) InterlockedCompareExchange(&read_len, 0, 0);
 }

int usbReadAsyncW(unsigned char ep, int size)
{    //clear previous results
    InterlockedExchange(&read_len, 0);


        // Prevent multiple concurrent reads
    if (InterlockedCompareExchange(&read_in_flight, 1, 0) != 0)
        return -EBUSY;

    InterlockedExchange(&read_len, 0);
    // 1) Allocate a libusb transfer structure
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

int usbWriteAsyncW(unsigned char ep, unsigned char *data, int size) {
    write_result = 0;
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

void waitForReadW(void)
{
    //When a semaphore is not released, The kernel does NOT execute your thread anymore.
    //CPU core immediately gets reassigned to other runnable threads in the system.
    //When a semaphore releases - the scheduler wakes up this thread, and
    //the program continue execution fron the next string (after the  WaitForSingleObject() )
    WaitForSingleObject(sem_read_done, INFINITE);
}

void waitForWriteW(void) {
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
    Sleep(10); // allow callbacks to finish
    CloseHandle(usb_thread_handle);
}



int initLibraryW(void){
   //load a library dynamically
    if (load_libusb()!=0)
        return -1;
  //initialize the library
    if (libusb_init_d(&ctx) < 0) {
        printf("libusb_init failed\n");
        return -1;
    }
    return 0;
}

int getWriteResultW(void) {
  return write_result;
}

int deInitLibraryW(void) {

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

int attachDeviceW (uint16_t vid, uint16_t pid) {
        //open a device with given PID, VID
    dev = libusb_open_device_with_vid_pid_d (ctx, vid, pid);
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

   //for Linux
   if (libusb_kernel_driver_active_d(dev, 0) == 1) {
      libusb_detach_kernel_driver_d(dev, 0);
    }

    if ( libusb_claim_interface_d(dev, 0) < 0) {
        printf("Cannot claim interface 0\n");
        return 1;
    }

     // Create semaphores with initial count = 0
    sem_read_done = CreateSemaphore(NULL, 0, 0x7FFFFFFF, NULL);

    sem_write_done = CreateSemaphore(NULL, 0, 0x7FFFFFFF, NULL);

     // Start USB background thread

     usb_thread_running = 1;
     usb_thread_handle = CreateThread(NULL, 0,
                                     usb_event_thread,
                                     NULL, 0, NULL);
    return 0;
}

uint8_t* getReadBuffer(void){
  return read_buf;
}



