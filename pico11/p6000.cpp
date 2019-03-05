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
			checkStatus(status);
			appBuffers[i * 2] = (int16_t*)calloc(sampleCount, sizeof(int16_t));
			appBuffers[i * 2 + 1] = (int16_t*)calloc(sampleCount, sizeof(int16_t));
		}
	}

	// Set information in structure
	bufferInfo.unit = unit;
	bufferInfo.driverBuffers = buffers;
	bufferInfo.appBuffers = appBuffers;

	status = ps6000RunStreaming(unit->handle, &sampleInterval, PS6000_US, preTrigger, postTrigger - preTrigger, autoStop, downsampleRatio, PS6000_RATIO_MODE_NONE, sampleCount);
	checkStatus(status);
	totalSamples = 0;

	while (IsRunningGetValues)
	{
		/* Poll until data is received. Until then, GetStreamingLatestValues wont call the callback */
		Sleep(1);
		g_ready = FALSE;

		status = ps6000GetStreamingLatestValues(unit->handle, CallBackStreaming, &bufferInfo);
		checkStatus(status);
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

void p6000::BlockDataHandler(UNIT * unit, int32_t offset, int16_t etsModeSet)
{
	sampleCount = BUFFER_SIZE;
	uint32_t maxSamples;
	uint32_t segmentIndex = 0;
	float timeInterval = 0.00f;

	chanATT.clear();
	chanBTT.clear();
	chanCTT.clear();
	chanDTT.clear();

	int64_t * etsTimes; // Buffer for ETS time data

	for (i = 0; i < unit->channelCount; i++)
	{
		if (unit->channelSettings[i].enabled)
		{
			buffers[i * 2] = (int16_t*)calloc(sampleCount, sizeof(int16_t));
			buffers[i * 2 + 1] = (int16_t*)calloc(sampleCount, sizeof(int16_t));

			status = ps6000SetDataBuffers(unit->handle, (PS6000_CHANNEL)i, buffers[i * 2], buffers[i * 2 + 1], sampleCount, PS6000_RATIO_MODE_NONE);
			checkStatus(status);
		}
	}

	// Set up ETS time buffers if ETS Block mode data is being captured
	if (etsModeSet)
	{
		etsTimes = (int64_t *)calloc(sampleCount, sizeof(int64_t));
		status = ps6000SetEtsTimeBuffer(unit->handle, etsTimes, sampleCount);
		checkStatus(status);
	}

	/*  Find the maximum number of samples, the time interval (in timeUnits),
	*		 the most suitable time units, and the maximum oversample at the current timebase*/
	while ((status = ps6000GetTimebase2(unit->handle, timebase, sampleCount, &timeInterval, oversample, &maxSamples, segmentIndex)) != PICO_OK)
	{
		timebase++;
	}

	/* Start the device collecting, then wait for completion. */
	g_ready2 = NULL;

	status = ps6000RunBlock(unit->handle, 0, sampleCount, timebase, oversample, &timeIndisposed, segmentIndex, NULL, NULL);
	checkStatus(status);
	if (status != PICO_OK)
	{
		status = ps6000Stop(unit->handle);
		checkStatus(status);
		return;
	}
	while (g_ready2 == NULL)
	{
		status = ps6000IsReady(unit->handle, &g_ready2);
	}

	if (g_ready2)
	{
		status = ps6000GetValues(unit->handle, 0, (uint32_t*)&sampleCount, 1, PS6000_RATIO_MODE_NONE, 0, NULL);
		checkStatus(status);
		for (i = offset; i < offset + 10; i++)
		{
			for (j = 0; j < unit->channelCount; j++)
			{
				if (unit->channelSettings[j].enabled)
				{
					switch (j)
					{
					case 0:
						(chanATT).push_back(buffers[j * 2][i]);
						break;
					case 1:
						(chanBTT).push_back(buffers[j * 2][i]);
						break;
					case 2:
						(chanCTT).push_back(buffers[j * 2][i]);
						break;
					case 3:
						(chanDTT).push_back(buffers[j * 2][i]);
						break;
					}															// else print ADC Count
				}
			}
		}

		sampleCount = min(sampleCount, BUFFER_SIZE);
		for (i = 0; (uint32_t)i < sampleCount; i++)
		{
			for (j = 0; j < unit->channelCount; j++)
			{
				if (unit->channelSettings[j].enabled)
				{
					if (etsModeSet)
					{
						switch (j)
						{
						case 0:
							(chanATT).push_back(buffers[j * 2][i]);
							break;
						case 1:
							(chanBTT).push_back(buffers[j * 2][i]);
							break;
						case 2:
							(chanCTT).push_back(buffers[j * 2][i]);
							break;
						case 3:
							(chanDTT).push_back(buffers[j * 2][i]);
							break;
						}
					}
					else
					{

						switch (j)
						{
						case 0:
							(chanATT).push_back(buffers[j * 2][i]);
							break;
						case 1:
							(chanBTT).push_back(buffers[j * 2][i]);
							break;
						case 2:
							(chanCTT).push_back(buffers[j * 2][i]);
							break;
						case 3:
							(chanDTT).push_back(buffers[j * 2][i]);
							break;
						}
					}
				}
			}
		}
		for (qweA = 0; qweA < 7000; qweA = qweA + 1)
		{
			chanAT.push_back(chanATT[qweA]);
		}
		chanATT.clear();
		for (qweB = 0; qweB < 7000; qweB = qweB + 1)
		{
			chanBT.push_back(chanBTT[qweB]);
		}
		chanBTT.clear();
		for (qweC = 0; qweC < 7000; qweC = qweC + 1)
		{
			chanCT.push_back(chanCTT[qweC]);
		}
		chanCTT.clear();
		for (qweD = 0; qweD < 7000; qweD = qweD + 1)
		{
			chanDT.push_back(chanDTT[qweD]);
		}
		chanDTT.clear();
		BusyCopyDataA = FALSE;
		BusyCopyDataB = FALSE;
		BusyCopyDataC = FALSE;
		BusyCopyDataD = FALSE;

		
	}
	else
	{
		printf("Data collection aborted\n");
	}
	status = ps6000Stop(unit->handle);
	checkStatus(status);

	for (i = 0; i < unit->channelCount; i++)
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
		checkStatus(status);
	}
	else
	{
		checkStatus(status);
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
	checkStatus(status);


	for (i = 0; i < unit->channelCount; i++) // reset channels to most recent settings
	{
		status = ps6000SetChannel(unit->handle, (PS6000_CHANNEL)(PS6000_CHANNEL_A + i),
			unit->channelSettings[PS6000_CHANNEL_A + i].enabled,
			(PS6000_COUPLING)unit->channelSettings[PS6000_CHANNEL_A + i].DCcoupled,
			(PS6000_RANGE)unit->channelSettings[PS6000_CHANNEL_A + i].range, 0, PS6000_BW_FULL);
		checkStatus(status);
	}

}

PICO_STATUS p6000::HandleDevice(UNIT * unit)
{
	int16_t value = 0;
	checkStatus(unit->openStatus);
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
	SetTrigger(unit->handle, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0);

	return unit->openStatus;
}

void p6000::TriggerSetup(int trg)
{
	UNIT * unit;
	unit = &(allUnits[ChoosenDev]);
	int16_t triggerThreshold = 0;

	switch (trg)
	{
	case 0:
		SetTrigger(unit->handle, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0);
		break;
	case 1:
		SetTrigger(unit->handle, 1, &conditions, 1, &directions, &pulseWidth, TrigDelay, 0, 0);
		break;
	}
	

}

PICO_STATUS p6000::SetTrigger(int16_t handle,
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

	if (EnablChan[0] == 1)
	{
		if ((status = ps6000SetTriggerChannelProperties(handle,
			&sourceDetailsA,
			nChannelProperties,
			auxOutputEnabled,
			autoTriggerMs)) != PICO_OK)
		{
			checkStatus(status);
			return status;
		}
	}
	if (EnablChan[1] == 1)
	{
		if ((status = ps6000SetTriggerChannelProperties(handle,
			&sourceDetailsB,
			nChannelProperties,
			auxOutputEnabled,
			autoTriggerMs)) != PICO_OK)
		{
			checkStatus(status);
			return status;
		}
	}
	if (EnablChan[2] == 1)
	{
		if ((status = ps6000SetTriggerChannelProperties(handle,
			&sourceDetailsC,
			nChannelProperties,
			auxOutputEnabled,
			autoTriggerMs)) != PICO_OK)
		{
			checkStatus(status);
			return status;
		}
	}
	if (EnablChan[3] == 1)
	{
		if ((status = ps6000SetTriggerChannelProperties(handle,
			&sourceDetailsD,
			nChannelProperties,
			auxOutputEnabled,
			autoTriggerMs)) != PICO_OK)
		{
			checkStatus(status);
			return status;
		}
	}

	if ((status = ps6000SetTriggerChannelConditions(handle, triggerConditions, nTriggerConditions)) != PICO_OK)
	{
		checkStatus(status);
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
		checkStatus(status);
		return status;
	}


	if ((status = ps6000SetTriggerDelay(handle, delay)) != PICO_OK)
	{
		checkStatus(status);
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
		checkStatus(status);
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
	TrigDelay = 0;

	conditions.channelA = PS6000_CONDITION_DONT_CARE;
	conditions.channelB = PS6000_CONDITION_DONT_CARE;
	conditions.channelC = PS6000_CONDITION_DONT_CARE;
	conditions.channelD = PS6000_CONDITION_DONT_CARE;
	conditions.external = PS6000_CONDITION_DONT_CARE;
	conditions.aux = PS6000_CONDITION_DONT_CARE;
	conditions.pulseWidthQualifier = PS6000_CONDITION_DONT_CARE;

	directions.channelA = PS6000_NONE;
	directions.channelB = PS6000_NONE;
	directions.channelC = PS6000_NONE;
	directions.channelD = PS6000_NONE;
	directions.ext = PS6000_NONE;
	directions.aux = PS6000_NONE;

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
			checkStatus(status6000);
			ChoosenDev = DevNum;
}

void p6000::off()
{
	CloseDevice(&allUnits[ChoosenDev]);
}

void p6000::on()
{
	status6000 = OpenDevice(&(allUnits[ChoosenDev]), NULL);
	checkStatus(status6000);

}

void p6000::CollectBlockStr()
{
	std::thread thr(&p6000::StreamDataHandler, this, &(allUnits[ChoosenDev]));
	thr.detach();
	IsRunningGetValues = TRUE;
}

void p6000::CollectBlock()
{

	BusyCopyDataA = TRUE;
	BusyCopyDataB = TRUE;
	BusyCopyDataC = TRUE;
	BusyCopyDataD = TRUE;
	std::thread thr(&p6000::BlockDataHandler, this, &(allUnits[ChoosenDev]), 0, FALSE);
	thr.detach();
}

void p6000::CollectBlockEts()
{
	int32_t ets_sampletime;
	status = ps6000SetEts(allUnits[ChoosenDev].handle, PS6000_ETS_FAST, 20, 4, &ets_sampletime);
	checkStatus(status);
	if (status == PICO_OK)
	{
		etsModeSet = TRUE;
	}
	BusyCopyDataA = TRUE;
	BusyCopyDataB = TRUE;
	BusyCopyDataC = TRUE;
	BusyCopyDataD = TRUE;
	std::thread thr(&p6000::BlockDataHandler, this, &(allUnits[ChoosenDev]), 0, FALSE);
	thr.detach();
	etsModeSet = FALSE;
}

void p6000::StopCollectStr()
{
	IsRunningGetValues = FALSE;
}




void p6000::checkStatus(PICO_STATUS status)
{
	std::stringstream ss;
	switch (status)
	{
	case PICO_NOT_FOUND:
		ss << "PICO_NOT_FOUND"; 
		break;
	case PICO_INVALID_HANDLE:
		ss << "PICO_INVALID_HANDLE"; 
		break;
	case PICO_DRIVER_FUNCTION:
		ss << "PICO_DRIVER_FUNCTION"; 
		break;
	case PICO_INVALID_VOLTAGE_RANGE:
		ss << "PICO_INVALID_VOLTAGE_RANGE"; 
		break;
	case PICO_NULL_PARAMETER:
		ss << "PICO_NULL_PARAMETER"; 
		break;
	case PICO_MAX_UNITS_OPENED:
		ss << "PICO_MAX_UNITS_OPENED"; 
		break;
	case PICO_MEMORY_FAIL:
		ss << "PICO_MEMORY_FAIL"; 
		break;
	case PICO_FW_FAIL:
		ss << "PICO_FW_FAIL"; 
		break;
	case PICO_OPEN_OPERATION_IN_PROGRESS:
		ss << "PICO_OPEN_OPERATION_IN_PROGRESS"; 
		break;
	case PICO_OPERATION_FAILED:
		ss << "PICO_OPERATION_FAILED"; 
		break;
	case PICO_NOT_RESPONDING:
		ss << "PICO_NOT_RESPONDING"; 
		break;
	case PICO_CONFIG_FAIL:
		ss << "PICO_CONFIG_FAIL"; 
		break;
	case PICO_KERNEL_DRIVER_TOO_OLD:
		ss << "PICO_KERNEL_DRIVER_TOO_OLD"; 
		break;
	case PICO_EEPROM_CORRUPT:
		ss << "PICO_EEPROM_CORRUPT"; 
		break;
	case PICO_OS_NOT_SUPPORTED:
		ss << "PICO_OS_NOT_SUPPORTED"; 
		break;
	case PICO_INVALID_PARAMETER:
		ss << "PICO_INVALID_PARAMETER"; 
		break;
	case PICO_INVALID_TIMEBASE:
		ss << "PICO_INVALID_TIMEBASE"; 
		break;
	case PICO_INVALID_CHANNEL:
		ss << "PICO_INVALID_CHANNEL"; 
		break;
	case PICO_INVALID_TRIGGER_CHANNEL:
		ss << "PICO_INVALID_TRIGGER_CHANNEL"; 
		break;
	case PICO_INVALID_CONDITION_CHANNEL:
		ss << "PICO_INVALID_CONDITION_CHANNEL"; 
		break;
	case PICO_NO_SIGNAL_GENERATOR:
		ss << "PICO_NO_SIGNAL_GENERATOR"; 
		break;
	case PICO_STREAMING_FAILED:
		ss << "PICO_STREAMING_FAILED"; 
		break;
	case PICO_BLOCK_MODE_FAILED:
		ss << "PICO_BLOCK_MODE_FAILED"; 
		break;
	case PICO_ETS_MODE_SET:
		ss << "PICO_ETS_MODE_SET"; 
		break;
	case PICO_DATA_NOT_AVAILABLE:
		ss << "PICO_DATA_NOT_AVAILABLE"; 
		break;
	case PICO_STRING_BUFFER_TO_SMALL:
		ss << "PICO_STRING_BUFFER_TO_SMALL"; 
		break;
	case PICO_ETS_NOT_SUPPORTED:
		ss << "PICO_ETS_NOT_SUPPORTED"; 
		break;
	case PICO_AUTO_TRIGGER_TIME_TO_SHORT:
		ss << "PICO_AUTO_TRIGGER_TIME_TO_SHORT"; 
		break;
	case PICO_BUFFER_STALL:
		ss << "PICO_BUFFER_STALL"; 
		break;
	case PICO_TOO_MANY_SAMPLES:
		ss << "PICO_TOO_MANY_SAMPLES"; 
		break;
	case PICO_TOO_MANY_SEGMENTS:
		ss << "PICO_TOO_MANY_SEGMENTS"; 
		break;
	case PICO_PULSE_WIDTH_QUALIFIER:
		ss << "PICO_PULSE_WIDTH_QUALIFIER"; 
		break;
	case PICO_DELAY:
		ss << "PICO_DELAY"; 
		break;
	case PICO_SOURCE_DETAILS:
		ss << "PICO_SOURCE_DETAILS"; 
		break;
	case PICO_CONDITIONS:
		ss << "PICO_CONDITIONS"; 
		break;
	case PICO_USER_CALLBACK:
		ss << "PICO_USER_CALLBACK"; 
		break;
	case PICO_DEVICE_SAMPLING:
		ss << "PICO_DEVICE_SAMPLING"; 
		break;
	case PICO_NO_SAMPLES_AVAILABLE:
		ss << "PICO_NO_SAMPLES_AVAILABLE"; 
		break;
	case PICO_SEGMENT_OUT_OF_RANGE:
		ss << "PICO_SEGMENT_OUT_OF_RANGE"; 
		break;
	case PICO_BUSY:
		ss << "PICO_BUSY"; 
		break;
	case PICO_STARTINDEX_INVALID:
		ss << "PICO_STARTINDEX_INVALID"; 
		break;
	case PICO_INVALID_INFO:
		ss << "PICO_INVALID_INFO"; 
		break;
	case PICO_INFO_UNAVAILABLE:
		ss << "PICO_INFO_UNAVAILABLE"; 
		break;
	case PICO_INVALID_SAMPLE_INTERVAL:
		ss << "PICO_INVALID_SAMPLE_INTERVAL"; 
		break;
	case PICO_TRIGGER_ERROR:
		ss << "PICO_TRIGGER_ERROR"; 
		break;
	case PICO_MEMORY:
		ss << "PICO_MEMORY"; 
		break;
	case PICO_SIG_GEN_PARAM:
		ss << "PICO_SIG_GEN_PARAM"; 
		break;
	case PICO_SHOTS_SWEEPS_WARNING:
		ss << "PICO_SHOTS_SWEEPS_WARNING"; 
		break;
	case PICO_SIGGEN_TRIGGER_SOURCE:
		ss << "PICO_SIGGEN_TRIGGER_SOURCE"; 
		break;
	case PICO_AUX_OUTPUT_CONFLICT:
		ss << "PICO_AUX_OUTPUT_CONFLICT"; 
		break;
	case PICO_AUX_OUTPUT_ETS_CONFLICT:
		ss << "PICO_AUX_OUTPUT_ETS_CONFLICT"; 
		break;
	case PICO_WARNING_EXT_THRESHOLD_CONFLICT:
		ss << "PICO_WARNING_EXT_THRESHOLD_CONFLICT"; 
		break;
	case PICO_WARNING_AUX_OUTPUT_CONFLICT:
		ss << "PICO_WARNING_AUX_OUTPUT_CONFLICT"; 
		break;
	case PICO_SIGGEN_OUTPUT_OVER_VOLTAGE:
		ss << "PICO_SIGGEN_OUTPUT_OVER_VOLTAGE";
		break;
	case PICO_DELAY_NULL:
		ss << "PICO_DELAY_NULL"; 
		break;
	case PICO_INVALID_BUFFER:
		ss << "PICO_INVALID_BUFFER"; 
		break;
	case PICO_SIGGEN_OFFSET_VOLTAGE:
		ss << "PICO_SIGGEN_OFFSET_VOLTAGE"; 
		break;
	case PICO_SIGGEN_PK_TO_PK:
		ss << "PICO_SIGGEN_PK_TO_PK"; 
		break;
	case PICO_CANCELLED:
		ss << "PICO_CANCELLED"; 
		break;
	case PICO_SEGMENT_NOT_USED:
		ss << "PICO_SEGMENT_NOT_USED"; 
		break;
	case PICO_INVALID_CALL:
		ss << "PICO_INVALID_CALL"; 
		break;
	case PICO_GET_VALUES_INTERRUPTED:
		ss << "PICO_GET_VALUES_INTERRUPTED"; 
		break;
	case PICO_NOT_USED:
		ss << "PICO_NOT_USED"; 
		break;
	case PICO_INVALID_SAMPLERATIO:
		ss << "PICO_INVALID_SAMPLERATIO"; 
		break;
	case PICO_INVALID_STATE:
		ss << "PICO_INVALID_STATE"; 
		break;
	case PICO_NOT_ENOUGH_SEGMENTS:
		ss << "PICO_NOT_ENOUGH_SEGMENTS"; 
		break;
	case PICO_RESERVED:
		ss << "PICO_RESERVED"; 
		break;
	case PICO_INVALID_COUPLING:
		ss << "PICO_INVALID_COUPLING"; 
		break;
	case PICO_BUFFERS_NOT_SET:
		ss << "PICO_BUFFERS_NOT_SET"; 
		break;
	case PICO_RATIO_MODE_NOT_SUPPORTED:
		ss << "PICO_RATIO_MODE_NOT_SUPPORTED"; 
		break;
	case PICO_RAPID_NOT_SUPPORT_AGGREGATION:
		ss << "PICO_RAPID_NOT_SUPPORT_AGGREGATION"; 
		break;
	case PICO_INVALID_TRIGGER_PROPERTY:
		ss << "PICO_INVALID_TRIGGER_PROPERTY"; 
		break;
	case PICO_INTERFACE_NOT_CONNECTED:
		ss << "PICO_INTERFACE_NOT_CONNECTED"; 
		break;
	case PICO_RESISTANCE_AND_PROBE_NOT_ALLOWED:
		ss << "PICO_RESISTANCE_AND_PROBE_NOT_ALLOWED"; 
		break;
	case PICO_POWER_FAILED:
		ss << "PICO_POWER_FAILED"; 
		break;
	case PICO_SIGGEN_WAVEFORM_SETUP_FAILED:
		ss << "PICO_SIGGEN_WAVEFORM_SETUP_FAILED"; 
		break;
	case PICO_FPGA_FAIL:
		ss << "PICO_FPGA_FAIL"; 
		break;
	case PICO_POWER_MANAGER:
		ss << "PICO_POWER_MANAGER"; 
		break;
	case PICO_INVALID_ANALOGUE_OFFSET:
		ss << "PICO_INVALID_ANALOGUE_OFFSET"; 
		break;
	case PICO_PLL_LOCK_FAILED:
		ss << "PICO_PLL_LOCK_FAILED"; 
		break;
	case PICO_ANALOG_BOARD:
		ss << "PICO_ANALOG_BOARD"; 
		break;
	case PICO_CONFIG_FAIL_AWG:
		ss << "PICO_CONFIG_FAIL_AWG"; 
		break;
	case PICO_INITIALISE_FPGA:
		ss << "PICO_INITIALISE_FPGA"; 
		break;
	case PICO_EXTERNAL_FREQUENCY_INVALID:
		ss << "PICO_EXTERNAL_FREQUENCY_INVALID"; 
		break;
	case PICO_CLOCK_CHANGE_ERROR:
		ss << "PICO_CLOCK_CHANGE_ERROR"; 
		break;
	case PICO_TRIGGER_AND_EXTERNAL_CLOCK_CLASH:
		ss << "PICO_TRIGGER_AND_EXTERNAL_CLOCK_CLASH"; 
		break;
	case PICO_PWQ_AND_EXTERNAL_CLOCK_CLASH:
		ss << "PICO_PWQ_AND_EXTERNAL_CLOCK_CLASH"; 
		break;
	case PICO_UNABLE_TO_OPEN_SCALING_FILE:
		ss << "PICO_UNABLE_TO_OPEN_SCALING_FILE"; 
		break;
	case PICO_MEMORY_CLOCK_FREQUENCY:
		ss << "PICO_MEMORY_CLOCK_FREQUENCY"; 
		break;
	case PICO_I2C_NOT_RESPONDING:
		ss << "PICO_I2C_NOT_RESPONDING"; 
		break;
	case PICO_NO_CAPTURES_AVAILABLE:
		ss << "PICO_NO_CAPTURES_AVAILABLE"; 
		break;
	case PICO_TOO_MANY_TRIGGER_CHANNELS_IN_USE:
		ss << "PICO_TOO_MANY_TRIGGER_CHANNELS_IN_USE"; 
		break;
	case PICO_INVALID_TRIGGER_DIRECTION:
		ss << "PICO_INVALID_TRIGGER_DIRECTION"; 
		break;
	case PICO_INVALID_TRIGGER_STATES:
		ss << "PICO_INVALID_TRIGGER_STATES";
		break;
	case PICO_NOT_USED_IN_THIS_CAPTURE_MODE:
		ss << "PICO_NOT_USED_IN_THIS_CAPTURE_MODE"; 
		break;
	case PICO_GET_DATA_ACTIVE:
		ss << "PICO_GET_DATA_ACTIVE"; 
		break;
	case PICO_IP_NETWORKED:
		ss << "PICO_IP_NETWORKED"; 
		break;
	case PICO_INVALID_IP_ADDRESS:
		ss << "PICO_INVALID_IP_ADDRESS"; 
		break;
	case PICO_IPSOCKET_FAILED:
		ss << "PICO_IPSOCKET_FAILED"; 
		break;
	case PICO_IPSOCKET_TIMEDOUT:
		ss << "PICO_IPSOCKET_TIMEDOUT"; 
		break;
	case PICO_SETTINGS_FAILED:
		ss << "PICO_SETTINGS_FAILED";
		break;
	case PICO_NETWORK_FAILED:
		ss << "PICO_NETWORK_FAILED"; 
		break;
	case PICO_WS2_32_DLL_NOT_LOADED:
		ss << "PICO_WS2_32_DLL_NOT_LOADED";
		break;
	case PICO_INVALID_IP_PORT:
		ss << "PICO_INVALID_IP_PORT";
		break;
	case PICO_COUPLING_NOT_SUPPORTED:
		ss << "PICO_COUPLING_NOT_SUPPORTED"; 
		break;
	case PICO_BANDWIDTH_NOT_SUPPORTED:
		ss << "PICO_BANDWIDTH_NOT_SUPPORTED"; 
		break;
	case PICO_INVALID_BANDWIDTH:
		ss << "PICO_INVALID_BANDWIDTH"; 
		break;
	case PICO_AWG_NOT_SUPPORTED:
		ss << "PICO_AWG_NOT_SUPPORTED"; 
		break;
	case PICO_ETS_NOT_RUNNING:
		ss << "PICO_ETS_NOT_RUNNING"; 
		break;
	case PICO_SIG_GEN_WHITENOISE_NOT_SUPPORTED:
		ss << "PICO_SIG_GEN_WHITENOISE_NOT_SUPPORTED"; 
		break;
	case PICO_SIG_GEN_WAVETYPE_NOT_SUPPORTED:
		ss << "PICO_SIG_GEN_WAVETYPE_NOT_SUPPORTED"; 
		break;
	case PICO_INVALID_DIGITAL_PORT:
		ss << "PICO_INVALID_DIGITAL_PORT"; 
		break;
	case PICO_INVALID_DIGITAL_CHANNEL:
		ss << "PICO_INVALID_DIGITAL_CHANNEL"; 
		break;
	case PICO_INVALID_DIGITAL_TRIGGER_DIRECTION:
		ss << "PICO_INVALID_DIGITAL_TRIGGER_DIRECTION"; 
		break;
	case PICO_SIG_GEN_PRBS_NOT_SUPPORTED:
		ss << "PICO_SIG_GEN_PRBS_NOT_SUPPORTED"; 
		break;
	case PICO_ETS_NOT_AVAILABLE_WITH_LOGIC_CHANNELS:
		ss << "PICO_ETS_NOT_AVAILABLE_WITH_LOGIC_CHANNELS"; 
		break;
	case PICO_WARNING_REPEAT_VALUE:
		ss << "PICO_WARNING_REPEAT_VALUE"; 
		break;
	case PICO_POWER_SUPPLY_CONNECTED:
		ss << "PICO_POWER_SUPPLY_CONNECTED"; 
		break;
	case PICO_POWER_SUPPLY_NOT_CONNECTED:
		ss << "PICO_POWER_SUPPLY_NOT_CONNECTED"; 
		break;
	case PICO_POWER_SUPPLY_REQUEST_INVALID:
		ss << "PICO_POWER_SUPPLY_REQUEST_INVALID"; 
		break;
	case PICO_POWER_SUPPLY_UNDERVOLTAGE:
		ss << "PICO_POWER_SUPPLY_UNDERVOLTAGE"; 
		break;
	case PICO_CAPTURING_DATA:
		ss << "PICO_CAPTURING_DATA";
		break;
	case PICO_USB3_0_DEVICE_NON_USB3_0_PORT:
		ss << "PICO_USB3_0_DEVICE_NON_USB3_0_PORT"; 
		break;
	case PICO_NOT_SUPPORTED_BY_THIS_DEVICE:
		ss << "PICO_NOT_SUPPORTED_BY_THIS_DEVICE";
		break;
	case PICO_INVALID_DEVICE_RESOLUTION:
		ss << "PICO_INVALID_DEVICE_RESOLUTION"; 
		break;
	case PICO_INVALID_NUMBER_CHANNELS_FOR_RESOLUTION:
		ss << "PICO_INVALID_NUMBER_CHANNELS_FOR_RESOLUTION"; 
		break;
	case PICO_CHANNEL_DISABLED_DUE_TO_USB_POWERED:
		ss << "PICO_CHANNEL_DISABLED_DUE_TO_USB_POWERED"; 
		break;
	case PICO_SIGGEN_DC_VOLTAGE_NOT_CONFIGURABLE:
		ss << "PICO_SIGGEN_DC_VOLTAGE_NOT_CONFIGURABLE"; 
		break;
	case PICO_NO_TRIGGER_ENABLED_FOR_TRIGGER_IN_PRE_TRIG:
		ss << "PICO_NO_TRIGGER_ENABLED_FOR_TRIGGER_IN_PRE_TRIG"; 
		break;
	case PICO_TRIGGER_WITHIN_PRE_TRIG_NOT_ARMED:
		ss << "PICO_TRIGGER_WITHIN_PRE_TRIG_NOT_ARMED"; 
		break;
	case PICO_TRIGGER_WITHIN_PRE_NOT_ALLOWED_WITH_DELAY:
		ss << "PICO_TRIGGER_WITHIN_PRE_NOT_ALLOWED_WITH_DELAY"; 
		break;
	case PICO_TRIGGER_INDEX_UNAVAILABLE:
		ss << "PICO_TRIGGER_INDEX_UNAVAILABLE"; 
		break;
	case PICO_AWG_CLOCK_FREQUENCY:
		ss << "PICO_AWG_CLOCK_FREQUENCY"; 
		break;
	case PICO_TOO_MANY_CHANNELS_IN_USE:
		ss << "PICO_TOO_MANY_CHANNELS_IN_USE"; 
		break;
	case PICO_NULL_CONDITIONS:
		ss << "PICO_NULL_CONDITIONS"; 
		break;
	case PICO_DUPLICATE_CONDITION_SOURCE:
		ss << "PICO_DUPLICATE_CONDITION_SOURCE"; 
		break;
	case PICO_INVALID_CONDITION_INFO:
		ss << "PICO_INVALID_CONDITION_INFO"; 
		break;
	case PICO_SETTINGS_READ_FAILED:
		ss << "PICO_SETTINGS_READ_FAILED"; 
		break;
	case PICO_SETTINGS_WRITE_FAILED:
		ss << "PICO_SETTINGS_WRITE_FAILED"; 
		break;
	case PICO_ARGUMENT_OUT_OF_RANGE:
		ss << "PICO_ARGUMENT_OUT_OF_RANGE"; 
		break;
	case PICO_HARDWARE_VERSION_NOT_SUPPORTED:
		ss << "PICO_HARDWARE_VERSION_NOT_SUPPORTED"; 
		break;
	case PICO_DIGITAL_HARDWARE_VERSION_NOT_SUPPORTED:
		ss << "PICO_DIGITAL_HARDWARE_VERSION_NOT_SUPPORTED"; 
		break;
	case PICO_ANALOGUE_HARDWARE_VERSION_NOT_SUPPORTED:
		ss << "PICO_ANALOGUE_HARDWARE_VERSION_NOT_SUPPORTED";
		break;
	case PICO_UNABLE_TO_CONVERT_TO_RESISTANCE:
		ss << "PICO_UNABLE_TO_CONVERT_TO_RESISTANCE"; 
		break;
	case PICO_DUPLICATED_CHANNEL:
		ss << "PICO_DUPLICATED_CHANNEL"; 
		break;
	case PICO_INVALID_RESISTANCE_CONVERSION:
		ss << "PICO_INVALID_RESISTANCE_CONVERSION"; 
		break;
	case PICO_INVALID_VALUE_IN_MAX_BUFFER:
		ss << "PICO_INVALID_VALUE_IN_MAX_BUFFER"; 
		break;
	case PICO_INVALID_VALUE_IN_MIN_BUFFER:
		ss << "PICO_INVALID_VALUE_IN_MIN_BUFFER";
		break;
	case PICO_SIGGEN_FREQUENCY_OUT_OF_RANGE:
		ss << "PICO_SIGGEN_FREQUENCY_OUT_OF_RANGE"; 
		break;
	case PICO_EEPROM2_CORRUPT:
		ss << "PICO_EEPROM2_CORRUPT"; 
		break;
	case PICO_EEPROM2_FAIL:
		ss << "PICO_EEPROM2_FAIL"; 
		break;
	case PICO_SERIAL_BUFFER_TOO_SMALL:
		ss << "PICO_SERIAL_BUFFER_TOO_SMALL"; 
		break;
	case PICO_SIGGEN_TRIGGER_AND_EXTERNAL_CLOCK_CLASH:
		ss << "PICO_SIGGEN_TRIGGER_AND_EXTERNAL_CLOCK_CLASH"; 
		break;
	case PICO_WARNING_SIGGEN_AUXIO_TRIGGER_DISABLED:
		ss << "PICO_WARNING_SIGGEN_AUXIO_TRIGGER_DISABLED"; 
		break;
	case PICO_SIGGEN_GATING_AUXIO_NOT_AVAILABLE:
		ss << "PICO_SIGGEN_GATING_AUXIO_NOT_AVAILABLE"; 
		break;
	case PICO_SIGGEN_GATING_AUXIO_ENABLED:
		ss << "PICO_SIGGEN_GATING_AUXIO_ENABLED";
		break;
	case PICO_RESOURCE_ERROR:
		ss << "PICO_RESOURCE_ERROR";
		break;
	case PICO_TEMPERATURE_TYPE_INVALID:
		ss << "PICO_TEMPERATURE_TYPE_INVALID"; 
		break;
	case PICO_TEMPERATURE_TYPE_NOT_SUPPORTED:
		ss << "PICO_TEMPERATURE_TYPE_NOT_SUPPORTED";
		break;
	case PICO_TIMEOUT:
		ss << "PICO_TIMEOUT"; 
		break;
	case PICO_DEVICE_NOT_FUNCTIONING:
		ss << "PICO_DEVICE_NOT_FUNCTIONING"; 
		break;
	case PICO_INTERNAL_ERROR:
		ss << "PICO_INTERNAL_ERROR"; 
		break;
	case PICO_MULTIPLE_DEVICES_FOUND:
		ss << "PICO_MULTIPLE_DEVICES_FOUND"; 
		break;
	case PICO_WARNING_NUMBER_OF_SEGMENTS_REDUCED:
		ss << "PICO_WARNING_NUMBER_OF_SEGMENTS_REDUCED"; 
		break;
	case PICO_CAL_PINS_STATES:
		ss << "PICO_CAL_PINS_STATES"; 
		break;
	case PICO_CAL_PINS_FREQUENCY:
		ss << "PICO_CAL_PINS_FREQUENCY"; 
		break;
	case PICO_CAL_PINS_AMPLITUDE:
		ss << "PICO_CAL_PINS_AMPLITUDE"; 
		break;
	case PICO_CAL_PINS_WAVETYPE:
		ss << "PICO_CAL_PINS_WAVETYPE"; 
		break;
	case PICO_CAL_PINS_OFFSET:
		ss << "PICO_CAL_PINS_OFFSET"; 
		break;
	case PICO_PROBE_FAULT:
		ss << "PICO_PROBE_FAULT";
		break;
	case PICO_PROBE_IDENTITY_UNKNOWN:
		ss << "PICO_PROBE_IDENTITY_UNKNOWN"; 
		break;
	case PICO_PROBE_POWER_DC_POWER_SUPPLY_REQUIRED:
		ss << "PICO_PROBE_POWER_DC_POWER_SUPPLY_REQUIRED"; 
		break;
	case PICO_PROBE_NOT_POWERED_WITH_DC_POWER_SUPPLY:
		ss << "PICO_PROBE_NOT_POWERED_WITH_DC_POWER_SUPPLY";
		break;
	case PICO_PROBE_CONFIG_FAILURE:
		ss << "PICO_PROBE_CONFIG_FAILURE"; 
		break;
	case PICO_PROBE_INTERACTION_CALLBACK:
		ss << "PICO_PROBE_INTERACTION_CALLBACK"; 
		break;
	case PICO_UNKNOWN_INTELLIGENT_PROBE:
		ss << "PICO_UNKNOWN_INTELLIGENT_PROBE"; 
		break;
	case PICO_INTELLIGENT_PROBE_CORRUPT:
		ss << "PICO_INTELLIGENT_PROBE_CORRUPT";
		break;
	case PICO_PROBE_COLLECTION_NOT_STARTED:
		ss << "PICO_PROBE_COLLECTION_NOT_STARTED"; 
		break;
	case PICO_PROBE_POWER_CONSUMPTION_EXCEEDED:
		ss << "PICO_PROBE_POWER_CONSUMPTION_EXCEEDED";
		break;
	case PICO_WARNING_PROBE_CHANNEL_OUT_OF_SYNC:
		ss << "PICO_WARNING_PROBE_CHANNEL_OUT_OF_SYNC";
		break;
	case PICO_DEVICE_TIME_STAMP_RESET:
		ss << "PICO_DEVICE_TIME_STAMP_RESET";
		break;
	case PICO_WATCHDOGTIMER:
		ss << "PICO_WATCHDOGTIMER"; 
		break;
	case PICO_IPP_NOT_FOUND:
		ss << "PICO_IPP_NOT_FOUND"; 
		break;
	case PICO_IPP_NO_FUNCTION:
		ss << "PICO_IPP_NO_FUNCTION"; 
		break;
	case PICO_IPP_ERROR:
		ss << "PICO_IPP_ERROR";
		break;
	case PICO_SHADOW_CAL_NOT_AVAILABLE:
		ss << "PICO_SHADOW_CAL_NOT_AVAILABLE"; 
		break;
	case PICO_SHADOW_CAL_DISABLED:
		ss << "PICO_SHADOW_CAL_DISABLED"; 
		break;
	case PICO_SHADOW_CAL_ERROR:
		ss << "PICO_SHADOW_CAL_ERROR "; 
		break;
	case PICO_SHADOW_CAL_CORRUPT:
		ss << "PICO_SHADOW_CAL_CORRUPT"; 
		break;
	case PICO_DEVICE_MEMORY_OVERFLOW:
		ss << "PICO_DEVICE_MEMORY_OVERFLOW"; 
		break;
	case PICO_RESERVED_1:
		ss << "PICO_RESERVED_1"; 
		break;
	default:
		ss << "UNKNOWN EXCEPTION";
		break;
	}
	throw std::runtime_error(ss.str());
}


void p6000::CloseDevice(UNIT *unit)
{
	ps6000CloseUnit(unit->handle);
}

int16_t p6000::mv_to_adc(int16_t mv, int16_t ch)
{
	return (mv * PS6000_MAX_VALUE) / inputRanges[ch];
}

int32_t p6000::adc_to_mv(int32_t raw, int32_t ch)
{
	return (raw * inputRanges[ch]) / PS6000_MAX_VALUE;
}

enPS6000ThresholdDirection p6000::TriggerDirections(int Dir)
{
	switch (Dir)
	{
	case 0:
		return PS6000_ABOVE;
		break;
	case 1:
		return PS6000_ABOVE_LOWER;
		break;
	case 2:
		return PS6000_BELOW;
		break;
	case 3:
		return PS6000_BELOW_LOWER;
		break;
	case 4:
		return PS6000_RISING;
		break;
	case 5:
		return PS6000_RISING_LOWER;
		break;
	case 6:
		return PS6000_FALLING;
		break;
	case 7:
		return PS6000_FALLING_LOWER;
		break;
	case 8:
		return PS6000_RISING_OR_FALLING;
		break;
	case 9:
		return PS6000_INSIDE;
		break;
	case 10:
		return PS6000_OUTSIDE;
		break;
	case 11:
		return PS6000_ENTER;
		break;
	case 12:
		return PS6000_EXIT;
		break;
	case 13:
		return PS6000_ENTER_OR_EXIT;
		break;
	case 14:
		return PS6000_POSITIVE_RUNT;
		break;
	case 15:
		return PS6000_NEGATIVE_RUNT;
		break;
	case 16:
		return PS6000_NONE;
		break;
	}
}

