/* Shared Use License: This file is owned by Derivative Inc. (Derivative) and
 * can only be used, and/or modified for use, in conjunction with 
 * Derivative's TouchDesigner software, and only if you are a licensee who has
 * accepted Derivative's TouchDesigner license or assignment agreement (which
 * also govern the use of this file).  You may share a modified version of this
 * file with another authorized licensee of Derivative's TouchDesigner software.
 * Otherwise, no redistribution or sharing of this file, with or without
 * modification, is permitted.
 */

#include "CHOP_CPlusPlusBase.h"
#include <libusb.h>

#include <thread>
#include <atomic>

/*

This example file implements a class that does 2 different things depending on
if a CHOP is connected to the CPlusPlus CHOPs input or not.
The example is timesliced, which is the more complex way of working.

If an input is connected the node will output the same number of channels as the
input and divide the first 'N' samples in the input channel by 2. 'N' being the current
timeslice size. This is noteworthy because if the input isn't changing then the output
will look wierd since depending on the timeslice size some number of the first samples
of the input will get used.

If no input is connected then the node will output a smooth sine wave at 120hz.
*/


// To get more help about these functions, look at CHOP_CPlusPlusBase.h
class CPlusPlusCHOPExample : public CHOP_CPlusPlusBase
{
public:
	void OnTransferFinished(libusb_transfer * transfer);
	CPlusPlusCHOPExample(const OP_NodeInfo* info);
	virtual ~CPlusPlusCHOPExample();

	virtual void		getGeneralInfo(CHOP_GeneralInfo* ) override;
	virtual bool		getOutputInfo(CHOP_OutputInfo*) override;
	virtual const char*	getChannelName(int32_t index, void* reserved) override;

	virtual void		execute(const CHOP_Output*,
								OP_Inputs*,
								void* reserved) override;


	virtual int32_t		getNumInfoCHOPChans() override;
	virtual void		getInfoCHOPChan(int index,
										OP_InfoCHOPChan* chan) override;

	virtual bool		getInfoDATSize(OP_InfoDATSize* infoSize) override;
	virtual void		getInfoDATEntries(int32_t index,
											int32_t nEntries,
											OP_InfoDATEntries* entries) override;

	virtual void		setupParameters(OP_ParameterManager* manager) override;
	virtual void		pulsePressed(const char* name) override;

	void PollUsbForEvents();

private:

	// We don't need to store this pointer, but we do for the example.
	// The OP_NodeInfo class store information about the node that's using
	// this instance of the class (like its name).
	const OP_NodeInfo*	myNodeInfo;

	// In this example this value will be incremented each time the execute()
	// function is called, then passes back to the CHOP 
	int32_t				myExecuteCount;


	double				myOffset;
	int					open;
	bool				transferring;

	int submitCnt;

	bool threadOpened;

	// attempting to make a memory allocated char array for buffer
	char *datas;

	//libusb_device_handle* handle;
	unsigned char *dataPkt;

	std::thread pollThread_;
	std::atomic<bool> terminate_;
	std::atomic<bool> open_;
	std::atomic<bool> threadOpened_;
};
