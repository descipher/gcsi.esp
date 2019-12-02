// gcio.h

#ifndef _GCIO_h
#define _GCIO_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif
#include "stdint.h"

class GC {
	
	private:

	public:
	
	struct DATA {

		uint8_t speakerState;
    uint8_t loggingState;
    //uint8_t alarmState;
    uint8_t geigerTube;
    uint8_t logInterval;
    //uint8_t wifiState;
    uint8_t wifiMode;
    char ssid[17];
    char password[17];
    uint32_t unitID = 0x20AA0001;
		//uint16_t batteryVoltage;				// voltage on battery in millivolts
		uint16_t inverterVoltage;				// high voltage produced by the inverter in volts
		//uint16_t inverterDuty;					// inverter driver signal duty cycle in percents per millivolt
    uint16_t geigerCPS;
		uint32_t geigerCPM;						// radiation in Counts Per Minute CPM
		double geigerDose;						// radiation dose approximated to equivalent dose based on Geiger tube factor

	};

	DATA gcdata;
	void handleMonitor();
  void handleConfig();

};


#endif
