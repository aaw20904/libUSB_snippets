#ifndef LIBUSB_WRP_H_INCLUDED
#define LIBUSB_WRP_H_INCLUDED
#include <stdint.h>

int usbReadAsyncW(unsigned char ep, int size);
int usbWriteAsyncW(unsigned char ep, unsigned char *data, int size);
void waitForReadW(void);
void waitForWriteW(void);
int initLibraryW(void);
int deInitLibraryW(void);
int attachDeviceW (uint16_t vid, uint16_t pid);
int getWriteResultW(void);
int getReadDataLengh(void);
uint8_t* getReadBuffer(void);

#endif // LIBUSB_WRP_H_INCLUDED
