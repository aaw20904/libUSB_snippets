

///a calback for debug mode

 void  dbgMessageCallback (libusb_context *ctx, enum libusb_log_level level, const char *str) {
     //NOTE When the console is not available in app - use OWN ,another callback realization to avoid app crash
   printf("%s , \n", str);
 }

/**********open , init the library*********/
int initLibraryX1( uint16_t fifoSize, uint16_t  endpointsRamSize, bool debug) {
  myUsbDeviceHead.myDevicehandle = NULL; //util will opened any device
    //1)Upload  executable code from the .dll inside app
 if (load_libusb() != 0) {
    return -1;
 }
   //2)Allocate memory for the FIFO (IN endpoint stream reconstructing)
  userEndpointsArrays.fifosArray = malloc(fifoSize);
  userEndpointsArrays.inEpArray = malloc(endpointsRamSize);
  userEndpointsArrays.outEpArray = malloc(endpointsRamSize);
  if ( (userEndpointsArrays.fifosArray == NULL) || (userEndpointsArrays.inEpArray == NULL) || (userEndpointsArrays.outEpArray== NULL) ) {
   return -1;
  }
  //3)initializing the libusb context
  if (libusb_init_d(&myLibUsbContext) != 0) {
    myLibUsbContext = NULL;
    return -1;
  }

   //4)add debug features
     if (debug) {
            myUsbDeviceHead.isDebug = true;
            //turn debug info , set debug message level
           libusb_set_debug_d(myLibUsbContext, LIBUSB_LOG_LEVEL_WARNING);
           //attach the callback to the given context
           libusb_set_log_cb_d(myLibUsbContext, dbgMessageCallback, LIBUSB_LOG_CB_CONTEXT);

     }

  //end) When OK
  return 0;
}
 
/********************************************************/
int deInitLibraryX1 ( void ) {
    //close a device
    if (myUsbDeviceHead.myDevicehandle != NULL) {
        //release interface
        libusb_release_interface_d (myUsbDeviceHead.myDevicehandle,0);
        //close device
        libusb_close_d (myUsbDeviceHead.myDevicehandle);
        myUsbDeviceHead.myDevicehandle = NULL;
    }
       //1)free memory, if exist
     if (userEndpointsArrays.fifosArray != NULL) {
        free(userEndpointsArrays.fifosArray);
     }

     if (userEndpointsArrays.inEpArray != NULL) {
        free(userEndpointsArrays.inEpArray);
     }

     if (userEndpointsArrays.outEpArray != NULL) {
        free(userEndpointsArrays.outEpArray);
     }
      //close cotext of libusb
      if (myLibUsbContext != NULL) {
        libusb_exit_d(myLibUsbContext);
      }

       //2)release RAM from executable code of DLL, uploaded lated
      unload_libusb();
      return 0;
}
/*******************************************/
//Search params of the usb device .Must be initialized!
void assignDeviceParamsForSearchX1(uint16_t pid, uint16_t vid, uint16_t bcd ){
  myUsbDeviceHead.pid = pid;
  myUsbDeviceHead.vid = vid;
  myUsbDeviceHead.bcd = bcd;
}


/******Find USB device by PID, VID, BCD***/
/*
NOTE - there may be the  next warnings after closing the lirary  , when the  debug mode turns on in the libusb:
libusb: warning [libusb_exit] device 1.0 still referenced , libusb: warning [libusb_exit] device 1.5 still referenced
 People in the Internet talk, that it is a bug of library.For example, in MacOS an Linux it is one cases ,but absent in other.
*/
//This function requires a list of connected devices from a system,
// when a device was disconnected and appeared again - connect, get descriptor, clam and connect it again
// when a device was connected and disappeared again - close the device, claims the interface
   int findUsbDeviceX1 (void) {
       libusb_device* myDevice = NULL;
       libusb_device**  listOfConnected;
       ssize_t     howManyWasFound;
       struct libusb_device_descriptor  myDevDescriptor;

       //1) get the list of connected devices
       howManyWasFound = libusb_get_device_list_d(myLibUsbContext, &listOfConnected);

       if (howManyWasFound == 0) {
            //no connected devices was found
            libusb_free_device_list_d(listOfConnected,1);
            //clear handle
            myUsbDeviceHead.myDevicehandle = NULL;
             return 0;
       }
       //2)Iterate devices
       for (int devI=0; devI<howManyWasFound; devI++) {
             //a)get descriptor
             libusb_get_device_descriptor_d( listOfConnected[devI],&myDevDescriptor);
             //b)Compare PID, VID
             if ((myDevDescriptor.bcdDevice == myUsbDeviceHead.bcd) && (myDevDescriptor.idProduct == myUsbDeviceHead.pid) && (myDevDescriptor.idVendor == myUsbDeviceHead.vid)){
                //when matched - assign a device
                myDevice = listOfConnected[devI];
                break;
             }
       }
       //if the device with exactly PID, VID was found:
       if (myDevice != NULL) {
            //3)Connection with libusb_open
             if (libusb_open_d(myDevice, &myUsbDeviceHead.myDevicehandle) != 0){
                    //when open was fail
                libusb_free_device_list_d(listOfConnected,1);
                //clear handle
               myUsbDeviceHead.myDevicehandle = NULL;
                return -1;
             }
            //4)Claim the interface with number 0
             if (libusb_claim_interface_d(myUsbDeviceHead.myDevicehandle,0) !=0 ){
                //close device
                libusb_close_d (myUsbDeviceHead.myDevicehandle);
                libusb_free_device_list_d(listOfConnected,1);
               return -1;
             }
              //5)Clean the list
            libusb_free_device_list_d(listOfConnected,1);
            return 0;

       } else {
          //clear handle, because device ws not found
            myUsbDeviceHead.myDevicehandle = NULL;
            //5)Clean the  list
            libusb_free_device_list_d(listOfConnected,1);
            return 1;
       }
   }
/*************************************************************************
this progam search a device with exactly PID,VID,bcdDevice. 
When device not found - release the interface 0, close the device, handle=NULL
When devic was found - reconnect. 
NOTE - there may be the  next warnings after closing the lirary  , when the  debug mode turns on in the libusb:
libusb: warning [libusb_exit] device 1.0 still referenced , libusb: warning [libusb_exit] device 1.5 still referenced
 People in the Internet talk, that it is a bug of library.For example, in MacOS an Linux it is one cases ,but absent in other.  
*********************************************************************/

//a strtucture with PID, HID, device handle and physical state .Must be initialized

//a structure for device state (plug/unplug)
typedef struct {
  enum devPhyState deviceState;
  struct libusb_device_handle * myDevicehandle; //NULL when disconnected
  uint16_t pid;
  uint16_t vid;
  uint16_t bcd;
 bool isDebug;
}usbDeviceState;

usbDeviceState myUsbDeviceHead;
usrEpBuffers userEndpointsArrays;
libusb_context *myLibUsbContext;

///iterate a list of connected devices, find interested device
  /*1)When disconnected: case a: handle =  NULL -> do nothing;
                         case b: handle != NULL -> release interface, disconnect, assign NULL to the handle
                         --------------------------------------------------
    2)When connected:    case a: handle = NULL  -> connect, claim interface
                         case b: handle != NULL ->  do nothing
    */
  int attachUsbDevice(void){
        libusb_device* myDevice = NULL;  //interested device
       libusb_device**  listOfConnected;
       ssize_t     howManyWasFound;  //length of the list
       struct libusb_device_descriptor  myDevDescriptor;  //device descriptor

       //1) get the list of connected devices
       howManyWasFound = libusb_get_device_list_d(myLibUsbContext, &listOfConnected);

       //when there was any error when the list was required:
       if (howManyWasFound < 0) {
            libusb_free_device_list_d(listOfConnected,1);
        return -1;
       }
        ///when was found any USB devcie on the bus of the host:
       if (howManyWasFound > 0) {
        //when something device(s) was found - iterate the list

               for (int devI=0; devI < howManyWasFound; devI++) {
                 //a)get the descriptor
                 libusb_get_device_descriptor_d( listOfConnected[devI],&myDevDescriptor);
                 //b)Compare PID, VID
                     if ((myDevDescriptor.bcdDevice == myUsbDeviceHead.bcd) && (myDevDescriptor.idProduct == myUsbDeviceHead.pid) && (myDevDescriptor.idVendor == myUsbDeviceHead.vid)){
                        //when PID, VID, BCD was matched - assign the device to the pointer
                        myDevice = listOfConnected[devI];
                        break;
                     }
              }
       }
       /*now we know - is device on a physical host`s  bus or not  */
       /*<<<< So, the first case - the DEVICE IS IN SYSTEM NOW (present now) >>>>*/
       if (myDevice != NULL) {
            //a) Was the device connected later?
            if (myUsbDeviceHead.myDevicehandle == NULL)  {
                 //1)when was disconnected - re-connect again

                 if (libusb_open_d(myDevice, &myUsbDeviceHead.myDevicehandle) != 0){
                        //when open was fail clean the list
                    libusb_free_device_list_d(listOfConnected,1);
                    //clear handle
                   myUsbDeviceHead.myDevicehandle = NULL;
                    return -1;
                 }

                 //2)Claim the interfce with number 0:
                 if (libusb_claim_interface_d(myUsbDeviceHead.myDevicehandle,0) !=0 ){
                    //close device
                    libusb_close_d (myUsbDeviceHead.myDevicehandle);
                    //free the list
                    libusb_free_device_list_d(listOfConnected,1);
                        //clear handle
                   myUsbDeviceHead.myDevicehandle = NULL;
                   return -1;
                 }

            }
            ///when a device was already connect later - do nothing, only clean the list
           libusb_free_device_list_d(listOfConnected,1);
           return 1;

       } else {

        /*<<<<< So, the second case - the DEVICE IS NOT IN SYSTEM NOW (absent) >>>>>*/
        //a) Was the device connected later?
            if (myUsbDeviceHead.myDevicehandle != NULL)  {
                ///release interface 0
                libusb_release_interface_d (myUsbDeviceHead.myDevicehandle,0);
               //close the device
                libusb_close_d (myUsbDeviceHead.myDevicehandle);
                //assign null to a pointer
                myUsbDeviceHead.myDevicehandle = NULL;

            }
            ///when device was disconnected in past - do nothing
            //clean the list
             libusb_free_device_list_d(listOfConnected,1);
             return 0;
       }

  }



