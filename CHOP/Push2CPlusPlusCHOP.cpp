/* Shared Use License: This file is owned by Derivative Inc. (Derivative) and
 * can only be used, and/or modified for use, in conjunction with 
 * Derivative's TouchDesigner software, and only if you are a licensee who has
 * accepted Derivative's TouchDesigner license or assignment agreement (which
 * also govern the use of this file).  You may share a modified version of this
 * file with another authorized licensee of Derivative's TouchDesigner software.
 * Otherwise, no redistribution or sharing of this file, with or without
 * modification, is permitted.
 */

#include "Push2CPlusPlusCHOP.h"

#include <stdio.h>
#include <string.h>
#include <cmath>
#include <assert.h>

#include <libusb.h>
#include <io.h>

#define HDR_SZ  0x10
#define DATA_SZ (20 * 0x4000)

#define WIDTH   960
#define HEIGHT  160
#define LINEBUF (DATA_SZ / HEIGHT)

#define RGB_H(r,g,b) ( ((g & 0x07) << 5) | (r & 0x1F)) 
#define RGB_L(r,g,b) ( ((b & 0x1F) << 3) | (g & 0xE0))

#define RB 5

// asynchronous stuff
#define PUSH2_BULK_EP_OUT 0x01
#define TRANSFER_TIMEOUT  0 // milliseconds

// These functions are basic C function, which the DLL loader can find
// much easier than finding a C++ Class.
// The DLLEXPORT prefix is needed so the compile exports these functions from the .dll
// you are creating
extern "C"
{

DLLEXPORT
int32_t
GetCHOPAPIVersion(void)
{
	// Always return CHOP_CPLUSPLUS_API_VERSION in this function.
	return CHOP_CPLUSPLUS_API_VERSION;
}

DLLEXPORT
CHOP_CPlusPlusBase*
CreateCHOPInstance(const OP_NodeInfo* info)
{
	// Return a new instance of your class every time this is called.
	// It will be called once per CHOP that is using the .dll
	return new CPlusPlusCHOPExample(info);
}

DLLEXPORT
void
DestroyCHOPInstance(CHOP_CPlusPlusBase* instance)
{
	// Delete the instance here, this will be called when
	// Touch is shutting down, when the CHOP using that instance is deleted, or
	// if the CHOP loads a different DLL
	delete (CPlusPlusCHOPExample*)instance;
}

};

// LIBUSB STUFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
// define the handle for the libusb device
libusb_device_handle *handle;

inline static uint16_t SPixelFromRGB(unsigned char r, unsigned char g, unsigned char b)
{
	uint16_t pixel = (b & 0xF8) >> 3;
	pixel <<= 6;
	pixel += (g & 0xFC) >> 2;
	pixel <<= 5;
	pixel += (r & 0xF8) >> 3;
	return pixel;
}

void LIBUSB_CALL transferComplete(libusb_transfer* transfer)
{
	printf("transferComplete function");
	assert(0);
}

// Callback received whenever a transfer has been completed.
// We defer the processing to the communicator class

void LIBUSB_CALL SOnTransferFinished(libusb_transfer* transfer)
{
	static_cast<CPlusPlusCHOPExample*>(transfer->user_data)->OnTransferFinished(transfer);
}

void CPlusPlusCHOPExample::OnTransferFinished(libusb_transfer* transfer)
{
	if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
	{
		assert(0);
		switch (transfer->status)
		{
		case LIBUSB_TRANSFER_ERROR:     printf("transfer failed\n"); break;
		case LIBUSB_TRANSFER_TIMED_OUT: printf("transfer timed out\n"); break;
		case LIBUSB_TRANSFER_CANCELLED: printf("transfer was cancelled\n"); break;
		case LIBUSB_TRANSFER_STALL:     printf("endpoint stalled/control request not supported\n"); break;
		case LIBUSB_TRANSFER_NO_DEVICE: printf("device was disconnected\n"); break;
		case LIBUSB_TRANSFER_OVERFLOW:  printf("device sent more data than requested\n"); break;
		default:
			printf("snd transfer failed with status: %d\n", transfer->status);
			break;
		}
	}
	else if (transfer->length != transfer->actual_length)
	{
		assert(0);
		printf("only transferred %d of %d bytes\n", transfer->actual_length, transfer->length);
	}
	else
	{
		// since the flags to release the transfer are set, releasing it here is unnessecary
		//libusb_free_transfer(transfer);
		//printf("success");
	}	
}

CPlusPlusCHOPExample::CPlusPlusCHOPExample(const OP_NodeInfo* info) : myNodeInfo(info)
{
	myExecuteCount = 0;
	myOffset = 0.0;
	open = 0;
	threadOpened = false;
}

void CPlusPlusCHOPExample::PollUsbForEvents()
{
	static struct timeval timeout_500ms = { 0 , 500000 };
	int terminate_main_loop = 0;
	threadOpened_ = true;

	while (!terminate_main_loop && !terminate_.load() && open_.load())
	{
		if (libusb_handle_events_timeout_completed(NULL, &timeout_500ms, &terminate_main_loop) < 0)
		{
			printf("libusb_handle_events_timeout_completed error!!!");
			// this function should never be reached if operating correctly. I've printed instead of asserting "0"
			// for more gentle error handling
			//assert(0);
		}
	}

	threadOpened_ = false;
}

CPlusPlusCHOPExample::~CPlusPlusCHOPExample()
{

}

void
CPlusPlusCHOPExample::getGeneralInfo(CHOP_GeneralInfo* ginfo)
{
	// This will cause the node to cook every frame
	ginfo->cookEveryFrameIfAsked = true;
	ginfo->timeslice = true;
	ginfo->inputMatchIndex = 0;
}

bool
CPlusPlusCHOPExample::getOutputInfo(CHOP_OutputInfo* info)
{
	// If there is an input connected, we are going to match it's channel names etc
	// otherwise we'll specify our own.
	if (info->opInputs->getNumInputs() > 0)
	{
		return false;
	}
	else
	{
		info->numChannels = 1;

		// Since we are outputting a timeslice, the system will dictate
		// the numSamples and startIndex of the CHOP data
		//info->numSamples = 1;
		//info->startIndex = 0

		// For illustration we are going to output 120hz data
		info->sampleRate = 60;
		return true;
	}
}

const char*
CPlusPlusCHOPExample::getChannelName(int32_t index, void* reserved)
{
	return "chan1";
}

void
CPlusPlusCHOPExample::execute(const CHOP_Output* output,
							  OP_Inputs* inputs,
							  void* reserved)
{
	myExecuteCount++;

	if (inputs->getNumInputs() > 0)
	{
		const OP_CHOPInput	*cinput = inputs->getInputCHOP(0);
		// here we check if the input channel numbers and length are EXACTLY corrent for the number of pixels on the display
		// 960*160 = 153600 pixels, and three channels for R G B
		if (cinput->numChannels == 3 && cinput->numSamples == 153600 && open == 1) {

			static const uint16_t xOrMasks[2] = { 0xf3e7, 0xffe7 }; // this gets XOR'd with the data per the manual

			int ind = 0;

			for (int y = 0; y < 160; y++) {
				ind = y * 960;
				for (int z = 0; z < 2048; z += 2) {
					int x = y*2048 + z;

					// get the input channel values
					float r = float(cinput->getChannelData(0)[ind]);
					float g = float(cinput->getChannelData(1)[ind]);
					float b = float(cinput->getChannelData(2)[ind]);

					ind++;
					// Make sure we don't read past the end of the CHOP input
					ind = ind % cinput->numSamples;

					// convert to the RGB data format the display is expecting, then XOR values with mask
					uint16_t pixXored = SPixelFromRGB(r, g, b) ^ xOrMasks[(x / 2) % 2];

					// populate the buffer with current pixel
					dataPkt[x] = pixXored & 0xff;
					dataPkt[x + 1] = pixXored >> 8;
				}
			}

			// SYNCHRONOUS SENDING METHOD == BAD (BLOCKING) (leaving it here for reference though)
			/*
			int tfrsize = 0;
			libusb_bulk_transfer(handle, 0x01, headerPkt, sizeof(headerPkt), &tfrsize, 1000);
			libusb_bulk_transfer(handle, 0x01, dataPkt, sizeof(dataPkt), &tfrsize, 1000);
			*/

			// ASYNCHRONOUS SENDING METHOD == GOOD FOR HIGH FPS (NON-BLOCKING)

			// DO NOT CHANGE THIS - YOU COULD ACCIDENTALLY BRICK YOUR PUSH 2
			unsigned char frame_header[16] = {
				0xff, 0xcc, 0xaa, 0x88,
				0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00
			};

			libusb_transfer* frame_header_transfer;

			// allocate transfer
			if ((frame_header_transfer = libusb_alloc_transfer(0)) == NULL)
			{
				//printf("error: could not allocate frame header transfer handle\n");
			}
			else
			{
				libusb_fill_bulk_transfer(
					frame_header_transfer,
					handle,
					PUSH2_BULK_EP_OUT,
					frame_header,
					sizeof(frame_header),
					SOnTransferFinished, // callback function
					NULL,
					TRANSFER_TIMEOUT);
			}

			// this automatically frees the allocated transfer when it completes
			frame_header_transfer->flags = LIBUSB_TRANSFER_FREE_TRANSFER;

			// submit header transfer
			int tranErr = libusb_submit_transfer(frame_header_transfer);
			if ( tranErr < 0)
			{
				printf("Header transfer error: %i\n", tranErr);
				printf("%s\n", libusb_error_name(tranErr));
				libusb_free_transfer(frame_header_transfer);
			}

			libusb_transfer* pixel_data_transfer;

			if ((pixel_data_transfer = libusb_alloc_transfer(0)) == NULL)
			{
				printf("error: could not allocate transfer handle\n");

			}
			else
			{
				libusb_fill_bulk_transfer(
					pixel_data_transfer,
					handle,
					PUSH2_BULK_EP_OUT,
					dataPkt,
					DATA_SZ,
					SOnTransferFinished, // callback function 
					NULL,
					TRANSFER_TIMEOUT);
			}

			// this automatically frees the allocated transfer when it completes
			pixel_data_transfer->flags = LIBUSB_TRANSFER_FREE_TRANSFER;

			// submit pixel data transfer
			tranErr = libusb_submit_transfer(pixel_data_transfer);

			if (tranErr < 0)
			{
				printf("Pixel transfer error: %i\n", tranErr);
				printf("%s\n", libusb_error_name(tranErr));
				libusb_free_transfer(pixel_data_transfer);
			}

			// this still needs to happen, but it cannot happen here or else 
			// the buffer is deleting in the middle of the asynchronus send
			//delete[] dataPkt;

			// END ASYNCHRONOUS SENDING
			
			for (int i = 0; i < output->numChannels; i++)
			{
				for (int j = 0; j < output->numSamples; j++)
				{
					output->channels[i][j] = float(0.0);
				}
			}
		}
		else { // JUST OUTPUT NORMALLY
			int ind = 0;
			for (int i = 0; i < output->numChannels; i++)
			{
				for (int j = 0; j < output->numSamples; j++)
				{
					const OP_CHOPInput	*cinput = inputs->getInputCHOP(0);
					output->channels[i][j] = float(cinput->getChannelData(i)[ind]);
					ind++;

					// Make sure we don't read past the end of the CHOP input
					ind = ind % cinput->numSamples;
				}
			}
		}

	}
	else // If not input is connected, lets output a sine wave instead
	{
		for (int i = 0; i < output->numChannels; i++)
		{
			for (int j = 0; j < output->numSamples; j++)
			{
				output->channels[i][j] = float(0.0);
			}
		}
	}
}

int32_t
CPlusPlusCHOPExample::getNumInfoCHOPChans()
{
	// We return the number of channel we want to output to any Info CHOP
	// connected to the CHOP. In this example we are just going to send one channel.
	return 2;
}

void
CPlusPlusCHOPExample::getInfoCHOPChan(int32_t index,
										OP_InfoCHOPChan* chan)
{
	// This function will be called once for each channel we said we'd want to return
	// In this example it'll only be called once.

	if (index == 0)
	{
		chan->name = "executeCount";
		chan->value = (float)myExecuteCount;
	}

	if (index == 1)
	{
		chan->name = "offset";
		chan->value = (float)myOffset;
	}
}

bool		
CPlusPlusCHOPExample::getInfoDATSize(OP_InfoDATSize* infoSize)
{
	infoSize->rows = 2;
	infoSize->cols = 2;
	// Setting this to false means we'll be assigning values to the table
	// one row at a time. True means we'll do it one column at a time.
	infoSize->byColumn = false;
	return true;
}

void
CPlusPlusCHOPExample::getInfoDATEntries(int32_t index,
										int32_t nEntries,
										OP_InfoDATEntries* entries)
{
	// It's safe to use static buffers here because Touch will make it's own
	// copies of the strings immediately after this call returns
	// (so the buffers can be reuse for each column/row)
	static char tempBuffer1[4096];
	static char tempBuffer2[4096];

	if (index == 0)
	{
		// Set the value for the first column
#ifdef WIN32
		strcpy_s(tempBuffer1, "executeCount");
#else // macOS
        strlcpy(tempBuffer1, "executeCount", sizeof(tempBuffer1));
#endif
		entries->values[0] = tempBuffer1;

		// Set the value for the second column
#ifdef WIN32
		sprintf_s(tempBuffer2, "%d", myExecuteCount);
#else // macOS
        snprintf(tempBuffer2, sizeof(tempBuffer2), "%d", myExecuteCount);
#endif
		entries->values[1] = tempBuffer2;
	}

	if (index == 1)
	{
		// Set the value for the first column
#ifdef WIN32
        strcpy_s(tempBuffer1, "offset");
#else // macOS
        strlcpy(tempBuffer1, "offset", sizeof(tempBuffer1));
#endif
		entries->values[0] = tempBuffer1;

		// Set the value for the second column
#ifdef WIN32
        sprintf_s(tempBuffer2, "%g", myOffset);
#else // macOS
        snprintf(tempBuffer2, sizeof(tempBuffer2), "%g", myOffset);
#endif
		entries->values[1] = tempBuffer2;
	}
}

void
CPlusPlusCHOPExample::setupParameters(OP_ParameterManager* manager)
{

	// pulse
	{
		OP_NumericParameter	np;

		np.name = "Open";
		np.label = "Open";
		
		OP_ParAppendResult res = manager->appendPulse(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// pulse
	{
		OP_NumericParameter	np;

		np.name = "Close";
		np.label = "Close";

		OP_ParAppendResult res = manager->appendPulse(np);
		assert(res == OP_ParAppendResult::Success);
	}

}

void 
CPlusPlusCHOPExample::pulsePressed(const char* name)
{
	if (!strcmp(name, "Open"))
	{
		// LIBUSB STUFFFFFFFFFFFFFFFFFFFFFFFFFFF
		// this section probably needs more error checking
		if (open == 0) {
			libusb_init(NULL);
			// open the device and check for errors
			handle = libusb_open_device_with_vid_pid(NULL, 0x2982, 0x1967);

			if (handle == NULL) {
				printf("handle is null - device not opened\n");
				return;
			}

			libusb_claim_interface(handle, 0x00);

			open = 1;
			open_ = true;

			dataPkt = new unsigned char[DATA_SZ + 1];

			if (threadOpened == false) {
				// We initiate a thread so we can recieve events from libusb
				terminate_ = false;
				pollThread_ = std::thread(&CPlusPlusCHOPExample::PollUsbForEvents, this);
				threadOpened = true;
			}
		}
	}

	if (!strcmp(name, "Close"))
	{
		// close the interface
		if (open == 1) {
			if (handle != NULL) {
				// terminate polling thread for USB events
				
				open = 0;
				

				libusb_release_interface(handle, 0x00);
				libusb_close(handle);
				

				// shutdown the polling thread
				terminate_ = true;
				if (pollThread_.joinable())
				{
					pollThread_.join();
				}

				open_ = false;
				terminate_ = true;
				threadOpened = false;

				libusb_exit(NULL);
			}
		}
	}
}

