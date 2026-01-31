#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include "libusb_wrp.h"

#define VID 0xCafe
#define PID 0x4000

/** Generated using Dr LUT - Free Lookup Table Generator
  * https://github.com/ppelikan/drlut
  **/
// Formula: sin(2*pi*t/T)
uint8_t testAudio[512];
FILE* audioFile;

unsigned long audioFileLength;

typedef struct wav_header {
    // RIFF Header
    char riff_header[4]; // Contains "RIFF"
    int wav_size; // Size of the wav portion of the file, which follows the first 8 bytes. File size - 8
    char wave_header[4]; // Contains "WAVE"

    // Format Header
    char fmt_header[4]; // Contains "fmt " (includes trailing space)
    int fmt_chunk_size; // Should be 16 for PCM
    short audio_format; // Should be 1 for PCM. 3 for IEEE Float
    short num_channels;
    int sample_rate;
    int byte_rate; // Number of bytes per second. sample_rate * num_channels * Bytes Per Sample
    short sample_alignment; // num_channels * Bytes Per Sample
    short bit_depth; // Number of bits per sample

    // Data
    char data_header[4]; // Contains "data"
    int data_bytes; // Number of bytes in data. Number of samples * num_channels * sample byte size
    // uint8_t bytes[]; // Remainder of wave file is bytes
} wav_header;

wav_header audioFileHeader;

uint16_t circAudioIndex;


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
uint32_t audioDataIndex;




int main()
{

 if (initLibraryW() != 0){
   printf("The dll file not found!");
    return -1;
 }
  attachDeviceW (VID,   PID);


///************open audio file***************
 audioFile = fopen("1.wav","rb");
 if (audioFile == NULL){
    printf("Not found!");
    return -1;
 }

 fread(&audioFileHeader, sizeof(wav_header),1,audioFile);
 fseek(audioFile, 0, SEEK_END);
    audioFileLength = ftell(audioFile);
 fseek(audioFile,0,SEEK_SET);

//***************init**audio**params*****************************
  trackParams.totalLenght = audioFileLength;//256*512
  trackParams.parcelSize = 256;
  trackParams.sampleRate = audioFileHeader.sample_rate;
  trackParams.amountOfParcels = audioFileLength /  trackParams.parcelSize;
//******************write**audio**params*************************
      usbWriteAsyncW(0x01,(unsigned char*)&trackParams,16);
       waitForWriteW();
       if (getWriteResultW != 0){
        printf("Write FAILED!\n");
       }

//*****************write***audio***data***and**controlling**of***

   while (trackParams.amountOfParcels > 0) {
     //----------await for data request
        usbReadAsyncW(0x81, 16);
            waitForReadW();
            if (getReadDataLengh() > 0) {
                memcpy(&devResp, getReadBuffer(), 16);
                //read_len=0;
                //-----read data from file
                fread(testAudio,trackParams.parcelSize, 1, audioFile);
                //------------send audio data

                usbWriteAsyncW(0x01, testAudio , trackParams.parcelSize);
                waitForWriteW();
                 audioDataIndex += trackParams.parcelSize;
                audioDataIndex &= 0x0000FFFF;
                trackParams.amountOfParcels--;
               if (getWriteResultW() != 0){
                printf("Write FAILED!\n");
               }
            }
            else
            {
                printf("Read failed!\n");
            }
            //read_len=0;
   }
   goto cleanup;





    // -------------------------------
    // Example: ASYNC WRITE
    // -------------------------------

    unsigned char msg[] = { 0x80, 0x84, 0x88, 0x8c, 0x90, 0x94, 0x98, 0x9c,
                    0xa0, 0xa4, 0xa8, 0xac, 0xaf, 0xb3, 0xb7, 0xba,
                    0xbe, 0xc2, 0xc5, 0xc8, 0xcc, 0xcf, 0xd2, 0xd5,
                    0xd8, 0xdb, 0xde, 0xe1, 0xe3, 0xe6, 0xe8, 0xea,
                    0xed, 0xef, 0xf1, 0xf2, 0xf4, 0xf6, 0xf7, 0xf9,
    };

    printf("Submitting async write...\n");
    usbWriteAsyncW(0x01, msg, sizeof(msg));

    printf("Waiting for write to finish...\n");
    //When a semaphore not released, the OS kernel stop execution
    //and reassign CPU to another tasks.When a semaphore released - the Kernel wakes up

    waitForWriteW();
     // the main thread and execution continue from the next code line
    if (getWriteResultW() == 0)
        printf("Write OK!\n");
    else
        printf("Write FAILED!\n");

    // -------------------------------
    // Example: ASYNC READ
    // -------------------------------

    printf("Submitting async read...\n");
    usbReadAsyncW(0x81, sizeof(msg)); //128

    printf("Waiting for read...\n");
     //When a semaphore not released, the OS kernel stop execution
    //and reassign CPU to another tasks.When a semaphore released - the Kernel wakes up
    waitForReadW();
    // the main thread and execution continue from the next code line
    if (getReadDataLengh() > 0)
    {
        getReadDataLengh();
        getReadBuffer();
        /*

        */
    }
    else
    {
        printf("Read failed!\n");
    }
cleanup:
     deInitLibraryW();

    return 0;
}
