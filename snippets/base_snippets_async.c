
 /******Find USB device by PID, VID, BCD***/

int findUsbDeviceX1 (void) {
       libusb_device* myDevice = NULL;
       libusb_device**  listOfConnected;
       ssize_t     howManyWasFound;
       struct libusb_device_descriptor  myDevDescriptor;

       //1) get the list of connected devices
       howManyWasFound = libusb_get_device_list_d(myLibUsbContext, &listOfConnected);

       if (howManyWasFound == 0) {
            //no connected devices was found
            libusb_free_device_list_d(listOfConnected,0);
            //clear handle
            myUsbDeviceHead.myDevicehandle = NULL;
             return 0;
       }
       //2)Iterate devices
       for (int devI=0; devI<howManyWasFound; devI++) {
             //a)get descriptor
             libusb_get_device_descriptor_d( listOfConnected[devI],&myDevDescriptor);
             //b)Compare PID, VID, BCD
             if ((myDevDescriptor.idProduct == myUsbDeviceHead.pid) && (myDevDescriptor.idVendor == myUsbDeviceHead.vid) && (myDevDescriptor.idVendor == myUsbDeviceHead.bcd) ){
                //when matched - assign a device
                myDevice = listOfConnected[devI];
             }
       }
       ////if the device with exactly PID, VID, VID was found:
       if (myDevice != NULL) {
            //3)Connection with libusb_open
             if (libusb_open_d(myDevice, &myUsbDeviceHead.myDevicehandle) != 0){
                   //when open was fail
                libusb_free_device_list_d(listOfConnected,0);
                  //clear handle
                myUsbDeviceHead.myDevicehandle = NULL;
                return -1;
             }
            //4)Claim the interface with number 0
             if (libusb_claim_interface_d(myUsbDeviceHead.myDevicehandle,0) !=0 ){
               return -1;
             }
       }

       //4)Clean the found list
       libusb_free_device_list_d(listOfConnected,0);



   }

