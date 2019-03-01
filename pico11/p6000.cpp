#include "p6000.h"
#include <iostream>
#include <sstream>
#include <exception>
#include <string>
#include <thread>

void PREF4 p6000::CallBackStreaming(int16_t handle,
	uint32_t noOfSamples,
	uint32_t startIndex,
	int16_t overflow,
	uint32_t triggerAt,
	int16_t triggered,
	int16_t autoStop,
	void	*pParameter)
{
	int32_t channel;
	p6000::BUFFER_INFO * bufferInfo = NULL;

	if (pParameter != NULL)
	{
		bufferInfo = (p6000::BUFFER_INFO *)pParameter;
	}

	// used for streaming
	g_sampleCount = noOfSamples;
	g_startIndex = startIndex;
	g_autoStopped = autoStop;
	g_overflow = overflow;
	// flag to say done reading data
	g_ready = TRUE;

	// flags to show if & where a trigger has occurred
	g_trig = triggered;
	g_trigAt = triggerAt;

	if (bufferInfo != NULL && noOfSamples)
	{
		for (channel = 0; channel < bufferInfo->unit->channelCount; channel++)
		{
			if (bufferInfo->unit->channelSettings[channel].enabled)
			{
				if (bufferInfo->appBuffers && bufferInfo->driverBuffers)
				{
					// Copy data...

					// Max buffers
					if (bufferInfo->appBuffers[channel * 2] && bufferInfo->driverBuffers[channel * 2])
					{
						memcpy_s(&bufferInfo->appBuffers[channel * 2][startIndex], noOfSamples * sizeof(int16_t),
							&bufferInfo->driverBuffers[channel * 2][startIndex], noOfSamples * sizeof(int16_t));
					}

					// Min buffers
					if (bufferInfo->appBuffers[channel * 2 + 1] && bufferInfo->driverBuffers[channel * 2 + 1])
					{
						memcpy_s(&bufferInfo->appBuffers[channel * 2 + 1][startIndex], noOfSamples * sizeof(int16_t),
							&bufferInfo->driverBuffers[channel * 2 + 1][startIndex], noOfSamples * sizeof(int16_t));
					}
				}
			}
		}
	}

}

void p6000::StreamDataHandler(UNIT * unit)
{
	sampleCount = BUFFER_SIZE;
	sampleInterval = 1;
	index = 0;
	totalSamples;
	autoStop = FALSE;
	postTrigger = 200;
	downsampleRatio = 1;
	triggeredAt = 0;
	previousTotal = 0;
	uint32_t preTrigger = 0;

	for (i = PS6000_CHANNEL_A; (int32_t)i < unit->channelCount; i++) // create data buffers
	{
		if (unit->channelSettings[i].enabled)
		{
			buffers[i * 2] = (int16_t*)calloc(sampleCount, sizeof(int16_t));
			buffers[i * 2 + 1] = (int16_t*)calloc(sampleCount, sizeof(int16_t));

			status = ps6000SetDataBuffers(unit->handle, (PS6000_CHANNEL)i, buffers[i * 2], buffers[i * 2 + 1],
				sampleCount, PS6000_RATIO_MODE_NONE);

			appBuffers[i * 2] = (int16_t*)calloc(sampleCount, sizeof(int16_t));
			appBuffers[i * 2 + 1] = (int16_t*)calloc(sampleCount, sizeof(int16_t));
		}
	}

	// Set information in structure
	bufferInfo.unit = unit;
	bufferInfo.driverBuffers = buffers;
	bufferInfo.appBuffers = appBuffers;

	status = ps6000RunStreaming(unit->handle, &sampleInterval, PS6000_US, preTrigger, postTrigger - preTrigger, autoStop, downsampleRatio, PS6000_RATIO_MODE_NONE, sampleCount);

	totalSamples = 0;

	while (IsRunningGetValues)
	{
		/* Poll until data is received. Until then, GetStreamingLatestValues wont call the callback */
		Sleep(1);
		g_ready = FALSE;

		status = ps6000GetStreamingLatestValues(unit->handle, CallBackStreaming, &bufferInfo);

		if (status != PICO_OK && status != PICO_BUSY)
		{
			printf("Streaming status return 0x%x\n", status);
			break;
		}

		index++;

		if (g_ready && g_sampleCount > 0) /* can be ready and have no data, if autoStop has fired */
		{
			if (g_trig)
			{
				triggeredAt = totalSamples += g_trigAt;		// calculate where the trigger occurred in the total samples collected
			}

			previousTotal = totalSamples;
			totalSamples += g_sampleCount;

			if (g_trig)
			{
				printf("Trig. at index %lu Total at trigger: %lu", triggeredAt, previousTotal + (triggeredAt - g_startIndex + 1));	// show where trigger occurred
			}

			for (i = g_startIndex; i < (g_startIndex + g_sampleCount); i++)
			{
				for (j = PS6000_CHANNEL_A; (int32_t)j < unit->channelCount; j++)
				{
					if (unit->channelSettings[j].enabled)
					{
						switch (j)
						{
						case 0:
							(chanATT).push_back(appBuffers[j * 2][i]);
							break;
						case 1:
							(chanBTT).push_back(appBuffers[j * 2][i]);
							break;
						case 2:
							(chanCTT).push_back(appBuffers[j * 2][i]);
							break;
						case 3:
							(chanDTT).push_back(appBuffers[j * 2][i]);
							break;
						}	
					}
				}
			}
		}
		if (chanATT.size() > 20000)
		{
			if (!BusyReadingDataA)
			{
				BusyCopyDataA = TRUE;
				for (qweA = 0; qweA < chanATT.size(); qweA = qweA + 1000)
				{
					chanAT.push_back(chanATT[qweA]);
				}
				chanATT.clear();
				BusyCopyDataA = FALSE;
			}
		}
		if (chanBTT.size() > 20000)
		{
			if (!BusyReadingDataB)
			{
				BusyCopyDataB = TRUE;
				for (qweB = 0; qweB < chanBTT.size(); qweB = qweB + 1000)
				{
					chanBT.push_back(chanBTT[qweB]);
				}
				chanBTT.clear();
				BusyCopyDataB = FALSE;
			}
		}
		if (chanCTT.size() > 20000)
		{
			if (!BusyReadingDataC)
			{
				BusyCopyDataC = TRUE;
				for (qweC = 0; qweC < chanCTT.size(); qweC = qweC + 1000)
				{
					chanCT.push_back(chanCTT[qweC]);
				}
				chanCTT.clear();
				BusyCopyDataC = FALSE;
			}
		}
		if (chanDTT.size() > 20000)
		{
			if (!BusyReadingDataD)
			{
				BusyCopyDataD = TRUE;
				for (qweD = 0; qweD < chanDTT.size(); qweD = qweD + 1000)
				{
					chanDT.push_back(chanDTT[qweD]);
				}
				chanDTT.clear();
				BusyCopyDataD = FALSE;
			}
		}

	}

	ps6000Stop(unit->handle);

	for (i = PS6000_CHANNEL_A; (int32_t)i < unit->channelCount; i++)
	{
		if (unit->channelSettings[i].enabled)
		{
			free(buffers[i * 2]);
			free(buffers[i * 2 + 1]);
		}
	}
}

PICO_STATUS p6000::OpenDevice(UNIT *unit, int8_t *serial)
{
	inputRanges[0] = 10;
	inputRanges[1] = 20;
	inputRanges[2] = 50;
	inputRanges[3] = 100;
	inputRanges[4] = 200;
	inputRanges[5] = 500;
	inputRanges[6] = 1000;
	inputRanges[7] = 2000;
	inputRanges[8] = 5000;
	inputRanges[9] = 10000;
	inputRanges[10] = 20000;
	inputRanges[11] = 50000;

	PICO_STATUS status;

	if (serial == NULL)
	{
		status = ps6000OpenUnit(&unit->handle, NULL);
	}
	else
	{
		status = ps6000OpenUnit(&unit->handle, serial);
	}

	unit->openStatus = status;
	unit->complete = 1;

	return status;
}

void p6000::SetDefaults(UNIT * unit)
{
	PICO_STATUS status;
	int32_t i;

	status = ps6000SetEts(unit->handle, PS6000_ETS_OFF, 0, 0, NULL); // Turn off ETS

	for (i = 0; i < unit->channelCount; i++) // reset channels to most recent settings
	{
		status = ps6000SetChannel(unit->handle, (PS6000_CHANNEL)(PS6000_CHANNEL_A + i),
			unit->channelSettings[PS6000_CHANNEL_A + i].enabled,
			(PS6000_COUPLING)unit->channelSettings[PS6000_CHANNEL_A + i].DCcoupled,
			(PS6000_RANGE)unit->channelSettings[PS6000_CHANNEL_A + i].range, 0, PS6000_BW_FULL);
	}

}

PICO_STATUS p6000::HandleDevice(UNIT * unit)
{
	int16_t value = 0;
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	printf("Handle: %d\n", unit->handle);
	if (unit->openStatus != PICO_OK)
	{
		printf("Unable to open device\n");
		printf("Error code : 0x%08x\n", (uint32_t)unit->openStatus);
		while (!_kbhit());
		exit(99); // exit program
	}

	printf("Device opened successfully, cycle %d\n\n", ++cycles);
	// setup device info - unless it's set already
	if (unit->model == MODEL_NONE)
	{
		set_info(unit);
	}
	timebase = 1;

	memset(&directions, 0, sizeof(struct tTriggerDirections));
	memset(&pulseWidth, 0, sizeof(struct tPwq));

	SetDefaults(unit);

	/* Trigger disabled	*/
	SetTrigger(unit->handle, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0);

	return unit->openStatus;
}

PICO_STATUS p6000::SetTrigger(int16_t handle,
	struct tPS6000TriggerChannelProperties * channelProperties,
	int16_t nChannelProperties,
	struct tPS6000TriggerConditions * triggerConditions,
	int16_t nTriggerConditions,
	TRIGGER_DIRECTIONS * directions,
	struct tPwq * pwq,
	uint32_t delay,
	int16_t auxOutputEnabled,
	int32_t autoTriggerMs)
{
	PICO_STATUS status;

	if ((status = ps6000SetTriggerChannelProperties(handle,
		channelProperties,
		nChannelProperties,
		auxOutputEnabled,
		autoTriggerMs)) != PICO_OK)
	{
		printf("SetTrigger:ps6000SetTriggerChannelProperties ------ %d \n", status);
		return status;
	}

	if ((status = ps6000SetTriggerChannelConditions(handle, triggerConditions, nTriggerConditions)) != PICO_OK)
	{
		printf("SetTrigger:ps6000SetTriggerChannelConditions ------ %d \n", status);
		return status;
	}

	if ((status = ps6000SetTriggerChannelDirections(handle,
		directions->channelA,
		directions->channelB,
		directions->channelC,
		directions->channelD,
		directions->ext,
		directions->aux)) != PICO_OK)
	{
		printf("SetTrigger:ps6000SetTriggerChannelDirections ------ %d \n", status);
		return status;
	}


	if ((status = ps6000SetTriggerDelay(handle, delay)) != PICO_OK)
	{
		printf("SetTrigger:ps6000SetTriggerDelay ------ %d \n", status);
		return status;
	}

	if ((status = ps6000SetPulseWidthQualifier(handle,
		pwq->conditions,
		pwq->nConditions,
		pwq->direction,
		pwq->lower,
		pwq->upper,
		pwq->type)) != PICO_OK)
	{
		printf("SetTrigger:ps6000SetPulseWidthQualifier ------ %d \n", status);
		return status;
	}

	return status;
}

p6000::p6000(std::string name)
{

	int8_t ch;
	uint16_t devCount = 0, listIter = 0, openIter = 0;
	g_trig = 0;
	g_trigAt = 0;
	std::stringstream ss;
	ReadyToWork = FALSE;
	do
	{
		memset(&(allUnits[devCount]), 0, sizeof(UNIT));
		status6000 = OpenDevice(&(allUnits[devCount]), NULL);
		if (status6000 == PICO_OK || status6000 == PICO_USB3_0_DEVICE_NON_USB3_0_PORT)
		{
			allUnits[devCount++].openStatus = status6000;
		}

	} while (status6000 != PICO_NOT_FOUND);

	if (devCount == 0)
	{
		ss << "Picoscope devices not found\n";
	}
	else
	{
		// More than one unit
		printf("Found %d devices, initializing...\n\n", devCount);
		for (listIter = 0; listIter < devCount; listIter++)
		{
			if ((allUnits[listIter].openStatus == PICO_OK ||
				allUnits[listIter].openStatus == PICO_USB3_0_DEVICE_NON_USB3_0_PORT))
			{
				set_info(&allUnits[listIter]);
				openIter++;
			}
		}
		Devices.clear();
		Devices = enumer(openIter, devCount);
		//Devices.insert(Devices.end(), enumer(openIter, devCount).begin(), enumer(openIter, devCount).end());
		for (int o = 0; o < Devices.size(); o++)
		{
			printf("%i) %s\n", o, Devices[o].c_str());
			
		}
	}
}



p6000::~p6000()
{
	//PICO_STATUS status6000;
	uint16_t devCount = 0;
	do
	{
		if (status6000 == PICO_OK || status6000 == PICO_USB3_0_DEVICE_NON_USB3_0_PORT)
		{
			allUnits[devCount++].openStatus = status6000;
		}

	} while (status6000 != PICO_NOT_FOUND);
	for (uint16_t listIter = 0; listIter < devCount; listIter++)
	{
		CloseDevice(&allUnits[listIter]);
	}
}

std::vector<std::string> p6000::enumer(uint16_t openIter, uint16_t devCount)
{
	//PICO_STATUS status6000;
	uint16_t listIter = 0;
	std::string PDev;
	char PPDev[10];
	std::vector<std::string> PicoDev;

	int8_t devChars[] = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz#";
	// None
	if (openIter == 0)
	{
		printf("Picoscope devices init failed\n");
	}
	printf("Found %d devices, pick one to open from the list:\n", devCount);
	for (listIter = 0; listIter < devCount; listIter++)
	{
		PDev = ((char*)allUnits[listIter].serial);
		PicoDev.push_back(PDev);
	}
	return PicoDev;
}

void p6000::set_info(UNIT * unit)
{
	int16_t i = 0;
	int16_t r = 20;
	char line[20];// = { 0 };
	int32_t variant;

	int8_t description[11][25] = { "Driver Version",
		"USB Version",
		"Hardware Version",
		"Variant Info",
		"Serial",
		"Cal Date",
		"Kernel",
		"Digital H/W",
		"Analogue H/W",
		"Firmware 1",
		"Firmware 2" };

	if (unit->handle)
	{
		for (i = 0; i < 11; i++)
		{
			ps6000GetUnitInfo(unit->handle, (int8_t *)line, sizeof(line), &r, i);

			if (i == 3)
			{
				// info = 3 - PICO_VARIANT_INFO

				variant = atoi(line);
				memcpy(&(unit->modelString), line, sizeof(unit->modelString) == 7 ? 7 : sizeof(unit->modelString));

				//To identify A or B model variants.....
				if (strlen(line) == 4)							// standard, not A, B, C or D, convert model number into hex i.e 6402 -> 0x6402
				{
					variant += 0x4B00;
				}
				else
				{
					if (strlen(line) == 5)						// A, B, C or D variant unit 
					{
						line[4] = toupper(line[4]);

						switch (line[4])
						{
						case 65: // i.e 6402A -> 0xA402
							variant += 0x8B00;
							break;
						case 66: // i.e 6402B -> 0xB402
							variant += 0x9B00;
							break;
						case 67: // i.e 6402C -> 0xC402
							variant += 0xAB00;
							break;
						case 68: // i.e 6402D -> 0xD402
							variant += 0xBB00;
							break;
						default:
							break;
						}
					}
				}
			}

			if (i == 4)
			{
				// info = 4 - PICO_BATCH_AND_SERIAL
				ps6000GetUnitInfo(unit->handle, unit->serial, sizeof(unit->serial), &r, PICO_BATCH_AND_SERIAL);
			}

			printf("%s: %s\n", description[i], line);
		}

		switch (variant)
		{
		case MODEL_PS6402:
			unit->model = MODEL_PS6402;
			unit->firstRange = PS6000_50MV;
			unit->lastRange = PS6000_20V;
			unit->channelCount = 4;
			unit->AWG = TRUE;
			unit->awgBufferSize = MAX_SIG_GEN_BUFFER_SIZE;

			for (i = 0; i < PS6000_MAX_CHANNELS; i++)
			{
				unit->channelSettings[i].range = PS6000_5V;
				unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
				unit->channelSettings[i].enabled = TRUE;
			}
			break;

		case MODEL_PS6402A:
			unit->model = MODEL_PS6402A;
			unit->firstRange = PS6000_50MV;
			unit->lastRange = PS6000_20V;
			unit->channelCount = 4;
			unit->AWG = FALSE;
			unit->awgBufferSize = 0;

			for (i = 0; i < PS6000_MAX_CHANNELS; i++)
			{
				unit->channelSettings[i].range = PS6000_5V;
				unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
				unit->channelSettings[i].enabled = TRUE;
			}
			break;

		case MODEL_PS6402B:
			unit->model = MODEL_PS6402B;
			unit->firstRange = PS6000_50MV;
			unit->lastRange = PS6000_20V;
			unit->channelCount = 4;
			unit->AWG = TRUE;
			unit->awgBufferSize = MAX_SIG_GEN_BUFFER_SIZE;

			for (i = 0; i < PS6000_MAX_CHANNELS; i++)
			{
				unit->channelSettings[i].range = PS6000_5V;
				unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
				unit->channelSettings[i].enabled = TRUE;
			}
			break;

		case MODEL_PS6402C:
			unit->model = MODEL_PS6402C;
			unit->firstRange = PS6000_50MV;
			unit->lastRange = PS6000_20V;
			unit->channelCount = 4;
			unit->AWG = FALSE;
			unit->awgBufferSize = 0;

			for (i = 0; i < PS6000_MAX_CHANNELS; i++)
			{
				unit->channelSettings[i].range = PS6000_5V;
				unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
				unit->channelSettings[i].enabled = TRUE;
			}
			break;

		case MODEL_PS6402D:
			unit->model = MODEL_PS6402D;
			unit->firstRange = PS6000_50MV;
			unit->lastRange = PS6000_20V;
			unit->channelCount = 4;
			unit->AWG = TRUE;
			unit->awgBufferSize = PS640X_C_D_MAX_SIG_GEN_BUFFER_SIZE;

			for (i = 0; i < PS6000_MAX_CHANNELS; i++)
			{
				unit->channelSettings[i].range = PS6000_5V;
				unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
				unit->channelSettings[i].enabled = TRUE;
			}
			break;

		case MODEL_PS6403:
			unit->model = MODEL_PS6403;
			unit->firstRange = PS6000_50MV;
			unit->lastRange = PS6000_20V;
			unit->channelCount = 4;
			unit->AWG = TRUE;
			unit->awgBufferSize = MAX_SIG_GEN_BUFFER_SIZE;

			for (i = 0; i < PS6000_MAX_CHANNELS; i++)
			{
				unit->channelSettings[i].range = PS6000_5V;
				unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
				unit->channelSettings[i].enabled = TRUE;
			}
			break;

		case MODEL_PS6403A:
			unit->model = MODEL_PS6403;
			unit->firstRange = PS6000_50MV;
			unit->lastRange = PS6000_20V;
			unit->channelCount = 4;
			unit->AWG = FALSE;
			unit->awgBufferSize = 0;

			for (i = 0; i < PS6000_MAX_CHANNELS; i++)
			{
				unit->channelSettings[i].range = PS6000_5V;
				unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
				unit->channelSettings[i].enabled = TRUE;
			}
			break;

		case MODEL_PS6403B:
			unit->model = MODEL_PS6403B;
			unit->firstRange = PS6000_50MV;
			unit->lastRange = PS6000_20V;
			unit->channelCount = 4;
			unit->AWG = TRUE;
			unit->AWG = MAX_SIG_GEN_BUFFER_SIZE;

			for (i = 0; i < PS6000_MAX_CHANNELS; i++)
			{
				unit->channelSettings[i].range = PS6000_5V;
				unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
				unit->channelSettings[i].enabled = TRUE;
			}
			break;

		case MODEL_PS6403C:
			unit->model = MODEL_PS6403C;
			unit->firstRange = PS6000_50MV;
			unit->lastRange = PS6000_20V;
			unit->channelCount = 4;
			unit->AWG = FALSE;
			unit->awgBufferSize = 0;

			for (i = 0; i < PS6000_MAX_CHANNELS; i++)
			{
				unit->channelSettings[i].range = PS6000_5V;
				unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
				unit->channelSettings[i].enabled = TRUE;
			}
			break;

		case MODEL_PS6403D:
			unit->model = MODEL_PS6403D;
			unit->firstRange = PS6000_50MV;
			unit->lastRange = PS6000_20V;
			unit->channelCount = 4;
			unit->AWG = TRUE;
			unit->awgBufferSize = PS640X_C_D_MAX_SIG_GEN_BUFFER_SIZE;

			for (i = 0; i < PS6000_MAX_CHANNELS; i++)
			{
				unit->channelSettings[i].range = PS6000_5V;
				unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
				unit->channelSettings[i].enabled = TRUE;
			}
			break;

		case MODEL_PS6404:
			unit->model = MODEL_PS6404;
			unit->firstRange = PS6000_50MV;
			unit->lastRange = PS6000_20V;
			unit->channelCount = 4;
			unit->AWG = TRUE;
			unit->awgBufferSize = MAX_SIG_GEN_BUFFER_SIZE;

			for (i = 0; i < PS6000_MAX_CHANNELS; i++)
			{
				unit->channelSettings[i].range = PS6000_5V;
				unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
				unit->channelSettings[i].enabled = TRUE;
			}
			break;

		case MODEL_PS6404A:
			unit->model = MODEL_PS6404;
			unit->firstRange = PS6000_50MV;
			unit->lastRange = PS6000_20V;
			unit->channelCount = 4;
			unit->AWG = FALSE;
			unit->awgBufferSize = 0;

			for (i = 0; i < PS6000_MAX_CHANNELS; i++)
			{
				unit->channelSettings[i].range = PS6000_5V;
				unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
				unit->channelSettings[i].enabled = TRUE;
			}
			break;

		case MODEL_PS6404B:
			unit->model = MODEL_PS6404B;
			unit->firstRange = PS6000_50MV;
			unit->lastRange = PS6000_20V;
			unit->channelCount = 4;
			unit->AWG = TRUE;
			unit->awgBufferSize = MAX_SIG_GEN_BUFFER_SIZE;

			for (i = 0; i < PS6000_MAX_CHANNELS; i++)
			{
				unit->channelSettings[i].range = PS6000_5V;
				unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
				unit->channelSettings[i].enabled = TRUE;
			}
			break;

		case MODEL_PS6404C:
			unit->model = MODEL_PS6404C;
			unit->firstRange = PS6000_50MV;
			unit->lastRange = PS6000_20V;
			unit->channelCount = 4;
			unit->AWG = TRUE;
			unit->awgBufferSize = 0;

			for (i = 0; i < PS6000_MAX_CHANNELS; i++)
			{
				unit->channelSettings[i].range = PS6000_5V;
				unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
				unit->channelSettings[i].enabled = TRUE;
			}
			break;

		case MODEL_PS6404D:
			unit->model = MODEL_PS6404D;
			unit->firstRange = PS6000_50MV;
			unit->lastRange = PS6000_20V;
			unit->channelCount = 4;
			unit->AWG = TRUE;
			unit->awgBufferSize = PS640X_C_D_MAX_SIG_GEN_BUFFER_SIZE;

			for (i = 0; i < PS6000_MAX_CHANNELS; i++)
			{
				unit->channelSettings[i].range = PS6000_5V;
				unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
				unit->channelSettings[i].enabled = TRUE;
			}
			break;

		case MODEL_PS6407:
			unit->model = MODEL_PS6407;
			unit->firstRange = PS6000_100MV;
			unit->lastRange = PS6000_100MV;
			unit->channelCount = 4;
			unit->AWG = TRUE;

			for (i = 0; i < PS6000_MAX_CHANNELS; i++)
			{
				unit->channelSettings[i].range = PS6000_100MV;
				unit->channelSettings[i].DCcoupled = PS6000_DC_50R;
				unit->channelSettings[i].enabled = TRUE;
			}
			break;

		default:
			break;
		}

	}
}

void p6000::pick_device(int DevNum)
{
	//PICO_STATUS status6000;
	std::string PicDev;
	PicDev = Devices[DevNum];
	std::cout << PicDev << std::endl;

			if ((allUnits[DevNum].openStatus == PICO_OK
				|| allUnits[DevNum].openStatus == PICO_POWER_SUPPLY_NOT_CONNECTED
				|| allUnits[DevNum].openStatus == PICO_USB3_0_DEVICE_NON_USB3_0_PORT))
			{
				status6000 = HandleDevice(&allUnits[DevNum]);
				ReadyToWork = TRUE;
			}

			if (status6000 != PICO_OK)
			{
				printf("Picoscope devices open failed, error code 0x%x\n", (uint32_t)status6000);
			}
			ChoosenDev = DevNum;
}

void p6000::off()
{
	CloseDevice(&allUnits[ChoosenDev]);
}

void p6000::on()
{
	status6000 = OpenDevice(&(allUnits[ChoosenDev]), NULL);
}

void p6000::CollectBlockStr()
{
	std::thread thr(&p6000::StreamDataHandler, this, &(allUnits[ChoosenDev]));
	thr.detach();
	IsRunningGetValues = TRUE;
}

void p6000::StopCollectStr()
{
	IsRunningGetValues = FALSE;
}




void p6000::checkStatus( )
{
	
}


void p6000::CloseDevice(UNIT *unit)
{
	ps6000CloseUnit(unit->handle);
}
