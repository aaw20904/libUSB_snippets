#ifndef LIBUSB_DYN_H
#define LIBUSB_DYN_H

#include <windows.h>
#include "libusb.h"

// ----------------------------
// Function pointer typedefs
// ----------------------------

// Init / Exit
typedef int  (*libusb_init_t)(libusb_context **ctx);
typedef void (*libusb_exit_t)(libusb_context *ctx);

// Device list / enumeration
typedef ssize_t (*libusb_get_device_list_t)
    (libusb_context *ctx, libusb_device ***list);

typedef void (*libusb_free_device_list_t)
    (libusb_device **list, int unref_devices);

typedef int (*libusb_get_device_descriptor_t)
    (libusb_device *dev, struct libusb_device_descriptor *desc);

typedef int (*libusb_get_config_descriptor_t)
    (libusb_device *dev, uint8_t config_index,
     struct libusb_config_descriptor **config);

typedef void (*libusb_free_config_descriptor_t)
    (struct libusb_config_descriptor *config);

// Device open / close
typedef libusb_device_handle * (*libusb_open_device_with_vid_pid_t)
    (libusb_context *ctx, uint16_t vendor, uint16_t product);

typedef int (*libusb_open_t)
    (libusb_device *dev, libusb_device_handle **handle);

typedef void (*libusb_close_t)
    (libusb_device_handle *dev);

// Interfaces
typedef int (*libusb_claim_interface_t)
    (libusb_device_handle *dev, int interface_number);

typedef int (*libusb_release_interface_t)
    (libusb_device_handle *dev, int interface_number);

typedef int (*libusb_set_configuration_t)
    (libusb_device_handle *dev, int configuration);

// Transfers
typedef int (*libusb_bulk_transfer_t)
    (libusb_device_handle *dev, unsigned char ep,
     unsigned char *data, int len, int *transferred,
     unsigned int timeout);

typedef int (*libusb_interrupt_transfer_t)
    (libusb_device_handle *dev, unsigned char ep,
     unsigned char *data, int len, int *transferred,
     unsigned int timeout);

typedef int (*libusb_control_transfer_t)
    (libusb_device_handle *dev, uint8_t bmRequestType,
     uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
     unsigned char *data, uint16_t wLength,
     unsigned int timeout);

// Async transfer API
typedef struct libusb_transfer * (*libusb_alloc_transfer_t)(int iso_packets);
typedef void (*libusb_free_transfer_t)(struct libusb_transfer *);
typedef int  (*libusb_submit_transfer_t)(struct libusb_transfer *);
typedef int  (*libusb_cancel_transfer_t)(struct libusb_transfer *);
typedef void (*libusb_set_iso_packet_lengths_t)
    (struct libusb_transfer *, unsigned int length);

typedef int (*libusb_handle_events_t)
    (libusb_context *ctx);

// Kernel driver
typedef int (*libusb_kernel_driver_active_t)
    (libusb_device_handle *dev, int interface_number);

typedef int (*libusb_detach_kernel_driver_t)
    (libusb_device_handle *dev, int interface_number);

typedef int (*libusb_attach_kernel_driver_t)
    (libusb_device_handle *dev, int interface_number);

// Errors
typedef const char * (*libusb_error_name_t)(int code);
typedef const char * (*libusb_strerror_t)(enum libusb_error code);

// Reset
typedef int (*libusb_reset_device_t)
    (libusb_device_handle *dev);

// ----------------------------------------
// Global function pointers with "_d" suffix
// ----------------------------------------

extern libusb_init_t                     libusb_init_d;
extern libusb_exit_t                     libusb_exit_d;

extern libusb_get_device_list_t          libusb_get_device_list_d;
extern libusb_free_device_list_t         libusb_free_device_list_d;

extern libusb_get_device_descriptor_t    libusb_get_device_descriptor_d;
extern libusb_get_config_descriptor_t    libusb_get_config_descriptor_d;
extern libusb_free_config_descriptor_t   libusb_free_config_descriptor_d;

extern libusb_open_device_with_vid_pid_t libusb_open_device_with_vid_pid_d;
extern libusb_open_t                     libusb_open_d;
extern libusb_close_t                    libusb_close_d;

extern libusb_claim_interface_t          libusb_claim_interface_d;
extern libusb_release_interface_t        libusb_release_interface_d;

extern libusb_bulk_transfer_t            libusb_bulk_transfer_d;
extern libusb_interrupt_transfer_t       libusb_interrupt_transfer_d;
extern libusb_control_transfer_t         libusb_control_transfer_d;

extern libusb_alloc_transfer_t           libusb_alloc_transfer_d;
extern libusb_free_transfer_t            libusb_free_transfer_d;
extern libusb_submit_transfer_t          libusb_submit_transfer_d;
extern libusb_cancel_transfer_t          libusb_cancel_transfer_d;
extern libusb_set_iso_packet_lengths_t   libusb_set_iso_packet_lengths_d;

extern libusb_handle_events_t            libusb_handle_events_d;

extern libusb_kernel_driver_active_t     libusb_kernel_driver_active_d;
extern libusb_detach_kernel_driver_t     libusb_detach_kernel_driver_d;
extern libusb_attach_kernel_driver_t     libusb_attach_kernel_driver_d;

extern libusb_error_name_t               libusb_error_name_d;
extern libusb_strerror_t                 libusb_strerror_d;

extern libusb_reset_device_t             libusb_reset_device_d;

// Loader
int  load_libusb(void);
void unload_libusb(void);

#endif
