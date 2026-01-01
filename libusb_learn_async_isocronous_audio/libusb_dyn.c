#include "libusb_dyn.h"
#include <stdio.h>
//Loads the lbrary. Create pointers to functions. Initialize them.
//Each function has "_d " suffix in addition to a library`s function name.
HMODULE hLib = NULL;

// Global pointers
libusb_init_t                     libusb_init_d;
libusb_exit_t                     libusb_exit_d;

libusb_get_device_list_t          libusb_get_device_list_d;
libusb_free_device_list_t         libusb_free_device_list_d;

libusb_get_device_descriptor_t    libusb_get_device_descriptor_d;
libusb_get_config_descriptor_t    libusb_get_config_descriptor_d;
libusb_free_config_descriptor_t   libusb_free_config_descriptor_d;

libusb_open_device_with_vid_pid_t libusb_open_device_with_vid_pid_d;
libusb_open_t                     libusb_open_d;
libusb_close_t                    libusb_close_d;

libusb_claim_interface_t          libusb_claim_interface_d;
libusb_release_interface_t        libusb_release_interface_d;

libusb_bulk_transfer_t            libusb_bulk_transfer_d;
libusb_interrupt_transfer_t       libusb_interrupt_transfer_d;
libusb_control_transfer_t         libusb_control_transfer_d;

libusb_alloc_transfer_t           libusb_alloc_transfer_d;
libusb_free_transfer_t            libusb_free_transfer_d;
libusb_submit_transfer_t          libusb_submit_transfer_d;
libusb_cancel_transfer_t          libusb_cancel_transfer_d;
libusb_set_iso_packet_lengths_t   libusb_set_iso_packet_lengths_d;

libusb_handle_events_t            libusb_handle_events_d;

libusb_kernel_driver_active_t     libusb_kernel_driver_active_d;
libusb_detach_kernel_driver_t     libusb_detach_kernel_driver_d;
libusb_attach_kernel_driver_t     libusb_attach_kernel_driver_d;

libusb_error_name_t               libusb_error_name_d;
libusb_strerror_t                 libusb_strerror_d;

libusb_reset_device_t             libusb_reset_device_d;
 libusb_handle_events_timeout_t   libusb_handle_events_timeout_d;
 libusb_set_interface_alt_setting_t libusb_set_interface_alt_setting_d;
 libusb_set_auto_detach_kernel_driver_t  libusb_set_auto_detach_kernel_driver_d;



// Helper for loading
static int load_one(void **func, const char *name)
{
    *func = (void *)GetProcAddress(hLib, name);
    if (!*func) {
        printf("Missing libusb function: %s\n", name);
        return 0;
    }
    return 1;
}

// ----------------------------------------
// Load DLL and assign all function pointers
// ----------------------------------------

int load_libusb(void)
{
    hLib = LoadLibraryA("msys-usb-1.0.dll");
    if (!hLib) {
        printf("Failed to load msys-usb-1.0.dll\n");
        return 0;
    }

    if (!load_one((void**)&libusb_init_d, "libusb_init")) return -1;
    if (!load_one((void**)&libusb_exit_d, "libusb_exit")) return -1;

    if (!load_one((void**)&libusb_get_device_list_d,  "libusb_get_device_list")) return -1;
    if (!load_one((void**)&libusb_free_device_list_d, "libusb_free_device_list")) return -1;

    if (!load_one((void**)&libusb_get_device_descriptor_d, "libusb_get_device_descriptor")) return -1;
    if (!load_one((void**)&libusb_get_config_descriptor_d, "libusb_get_config_descriptor")) return -1;
    if (!load_one((void**)&libusb_free_config_descriptor_d, "libusb_free_config_descriptor")) return -1;

    if (!load_one((void**)&libusb_open_device_with_vid_pid_d, "libusb_open_device_with_vid_pid")) return -1;
    if (!load_one((void**)&libusb_open_d, "libusb_open")) return -1;
    if (!load_one((void**)&libusb_close_d, "libusb_close")) return -1;

    if (!load_one((void**)&libusb_claim_interface_d, "libusb_claim_interface")) return -1;
    if (!load_one((void**)&libusb_release_interface_d, "libusb_release_interface")) return -1;

    if (!load_one((void**)&libusb_bulk_transfer_d, "libusb_bulk_transfer")) return -1;
    if (!load_one((void**)&libusb_interrupt_transfer_d, "libusb_interrupt_transfer")) return -1;
    if (!load_one((void**)&libusb_control_transfer_d, "libusb_control_transfer")) return -1;

    if (!load_one((void**)&libusb_alloc_transfer_d, "libusb_alloc_transfer")) return -1;
    if (!load_one((void**)&libusb_free_transfer_d, "libusb_free_transfer")) return -1;
    if (!load_one((void**)&libusb_submit_transfer_d, "libusb_submit_transfer")) return -1;
    if (!load_one((void**)&libusb_cancel_transfer_d, "libusb_cancel_transfer")) return -1;


    if (!load_one((void**)&libusb_handle_events_d, "libusb_handle_events")) return -1;

    if (!load_one((void**)&libusb_kernel_driver_active_d, "libusb_kernel_driver_active")) return -1;
    if (!load_one((void**)&libusb_detach_kernel_driver_d, "libusb_detach_kernel_driver")) return -1;
    if (!load_one((void**)&libusb_attach_kernel_driver_d, "libusb_attach_kernel_driver")) return -1;

    if (!load_one((void**)&libusb_error_name_d, "libusb_error_name")) return -1;
    if (!load_one((void**)&libusb_strerror_d,    "libusb_strerror")) return -1;

    if (!load_one((void**)&libusb_reset_device_d, "libusb_reset_device")) return -1;
    if (!load_one((void**)&libusb_handle_events_timeout_d, "libusb_handle_events_timeout")) return -1;
    if (!load_one((void**)&libusb_set_interface_alt_setting_d , "libusb_set_interface_alt_setting")) return -1;
    if (!load_one((void**)&libusb_set_auto_detach_kernel_driver_d , "libusb_set_interface_alt_setting")) return -1;



    return 0;
}

// ----------------------------------------

void unload_libusb(void)
{
    if (hLib) {
        FreeLibrary(hLib);
        hLib = NULL;
    }
}
