#pragma once
#include <iostream>
#include <conio.h>
#include <vector>


#ifdef _WIN32
#include "windows.h"
#include <conio.h>
#include <ps6000Api.h>
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <cstdint>
#include <string>
#include <string.h>

#include <libps6000-1.4/ps6000Api.h>
#ifndef PICO_STATUS
#include <libps6000-1.4/PicoStatus.h>
#endif

#define Sleep(a) usleep(1000*a)
#define scanf_s scanf
#define fscanf_s fscanf
#define memcpy_s(a,b,c,d) memcpy(a,c,d)

typedef enum enBOOL { FALSE, TRUE } BOOL;

#define max(a,b) ((a) > (b) ? a : b)
#define min(a,b) ((a) < (b) ? a : b)
#endif


#define VERSION		1
#define ISSUE		3


#define BUFFER_SIZE 	10000 // Used for block and streaming mode examples

// AWG Parameters
#define	AWG_DAC_FREQUENCY		200e6
#define	AWG_PHASE_ACCUMULATOR	4294967296.0
#define MAX_PICO_DEVICES 64
#define TIMED_LOOP_STEP 500

static int16_t     g_autoStopped;
static BOOL        g_ready;
static uint32_t    g_sampleCount;
static uint32_t	g_startIndex;
static int16_t     g_trig;
static uint32_t	g_trigAt;
static int16_t		g_overflow;

class p6000
{
public:
	
	std::vector<std::string> Devices;

	BOOL Stream;
	BOOL IsRunning;
	BOOL IsRunningGetValues;

	std::vector<double> chanAT;
	std::vector<double> chanATT;
	std::vector<double> chanBT;
	std::vector<double> chanBTT;
	std::vector<double> chanCT;
	std::vector<double> chanCTT;
	std::vector<double> chanDT;
	std::vector<double> chanDTT;

	int ChoosenDev;
	int qweA, qweB, qweC, qweD;

	BOOL BusyCopyDataA;
	BOOL BusyReadingDataA;
	BOOL BusyCopyDataB;
	BOOL BusyReadingDataB;
	BOOL BusyCopyDataC;
	BOOL BusyReadingDataC;
	BOOL BusyCopyDataD;
	BOOL BusyReadingDataD;

	

	typedef enum MODEL_TYPE
	{
		MODEL_NONE = 0,
		MODEL_PS6402 = 0x6402, //Bandwidth: 350MHz, Memory: 32MS, AWG
		MODEL_PS6402A = 0xA402, //Bandwidth: 250MHz, Memory: 128MS, FG
		MODEL_PS6402B = 0xB402, //Bandwidth: 250MHz, Memory: 256MS, AWG
		MODEL_PS6402C = 0xC402, //Bandwidth: 350MHz, Memory: 256MS, AWG
		MODEL_PS6402D = 0xD402, //Bandwidth: 350MHz, Memory: 512MS, AWG
		MODEL_PS6403 = 0x6403, //Bandwidth: 350MHz, Memory: 1GS, AWG
		MODEL_PS6403A = 0xA403, //Bandwidth: 350MHz, Memory: 256MS, FG
		MODEL_PS6403B = 0xB403, //Bandwidth: 350MHz, Memory: 512MS, AWG
		MODEL_PS6403C = 0xC403, //Bandwidth: 350MHz, Memory: 512MS, AWG
		MODEL_PS6403D = 0xD403, //Bandwidth: 350MHz, Memory: 1GS, AWG
		MODEL_PS6404 = 0x6404, //Bandwidth: 500MHz, Memory: 1GS, AWG
		MODEL_PS6404A = 0xA404, //Bandwidth: 500MHz, Memory: 512MS, FG
		MODEL_PS6404B = 0xB404, //Bandwidth: 500MHz, Memory: 1GS, AWG
		MODEL_PS6404C = 0xC404, //Bandwidth: 350MHz, Memory: 1GS, AWG
		MODEL_PS6404D = 0xD404, //Bandwidth: 350MHz, Memory: 2GS, AWG
		MODEL_PS6407 = 0x6407, //Bandwidth: 1GHz,	 Memory: 2GS, AWG

	};

	typedef struct CHANNEL_SETTINGS
	{
		int16_t DCcoupled;
		int16_t range;
		int16_t enabled;
	};

	typedef struct UNIT
	{
		int16_t handle;
		MODEL_TYPE				model;
		int8_t					modelString[8];
		int8_t					serial[10];
		int16_t					complete;
		int16_t					openStatus;
		int16_t					openProgress;
		PS6000_RANGE			firstRange;
		PS6000_RANGE			lastRange;
		int16_t					channelCount;
		BOOL					AWG;
		CHANNEL_SETTINGS		channelSettings[PS6000_MAX_CHANNELS];
		int32_t					awgBufferSize;
	};

	typedef struct tBufferInfo
	{
		UNIT * unit;
		int16_t **driverBuffers;
		int16_t **appBuffers;

	} BUFFER_INFO;

	typedef struct tTriggerDirections
	{
		enum enPS6000ThresholdDirection channelA;
		enum enPS6000ThresholdDirection channelB;
		enum enPS6000ThresholdDirection channelC;
		enum enPS6000ThresholdDirection channelD;
		enum enPS6000ThresholdDirection ext;
		enum enPS6000ThresholdDirection aux;
	}TRIGGER_DIRECTIONS;

	typedef struct tPwq
	{
		struct tPS6000PwqConditions * conditions;
		int16_t nConditions;
		enum enPS6000ThresholdDirection direction;
		uint32_t lower;
		uint32_t upper;
		PS6000_PULSE_WIDTH_TYPE type;
	}PWQ;

	BUFFER_INFO bufferInfo;
	UNIT allUnits[MAX_PICO_DEVICES];
	
	PICO_STATUS SetTrigger(int16_t handle, tPS6000TriggerChannelProperties * channelProperties, int16_t nChannelProperties, tPS6000TriggerConditions * triggerConditions, int16_t nTriggerConditions, TRIGGER_DIRECTIONS * directions, tPwq * pwq, uint32_t delay, int16_t auxOutputEnabled, int32_t autoTriggerMs);
	p6000(std::string name = "");
	~p6000();
	void pick_device(int DevNum);
	void off();
	void on();
	void CollectBlockStr();
	void StopCollectStr();

private:
	int32_t cycles = 0;
	uint32_t	timebase = 8;
	int16_t		oversample = 1;
	int32_t      scaleVoltages = TRUE;
	int16_t inputRanges[PS6000_MAX_RANGES];
	int64_t		g_times[PS6000_MAX_CHANNELS];
	PICO_STATUS status6000;
	BOOL ReadyToWork;
	uint32_t i, j;
	uint32_t sampleCount;
	int16_t * buffers[PS6000_MAX_CHANNEL_BUFFERS];
	int16_t * appBuffers[PS6000_MAX_CHANNEL_BUFFERS]; // Application buffers to copy data into
	PICO_STATUS status;
	uint32_t sampleInterval;
	int32_t index;
	uint32_t totalSamples;
	int16_t autoStop;
	uint32_t postTrigger;
	uint32_t downsampleRatio;
	uint32_t triggeredAt;
	uint32_t previousTotal;
	int16_t     g_timeUnit;
	uint32_t preTrigger;

	void StreamDataHandler(UNIT * unit);
	PICO_STATUS OpenDevice(UNIT * unit, int8_t * serial);
	std::vector<std::string> enumer(uint16_t openIter, uint16_t devCount);
	void set_info(UNIT * unit);
	void checkStatus();
	void CloseDevice(UNIT * unit);
	PICO_STATUS HandleDevice(UNIT * unit);
	void SetDefaults(UNIT * unit);
	static void PREF4 CallBackStreaming(int16_t handle,
		uint32_t noOfSamples,
		uint32_t startIndex,
		int16_t overflow,
		uint32_t triggerAt,
		int16_t triggered,
		int16_t autoStop,
		void	*pParameter);
};