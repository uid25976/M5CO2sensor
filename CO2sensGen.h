/*!
 * @file CO2sensGen.h
 * 2026-06-17: aprocha46 : creation of generic API for I2C CO2 sensor
 *
 * @remark The SCD41 or SCD40 are accurate on low CO2 values (< 0.4% for the SCD41), and 
 * could look like useless for caving where we want to send a warning when CO2 is above 1% or more.
 * But their range goes up to 4% with low accuracy but far enough for us.
 * @warning This code is intended for the SCD41 only because it uses:
 * - measure_single_shot
 * - wakeup and powerdown
 * The SCD40 does not support low power, neither measurement on demand.
 * @remark For improved precision you can set the defaut altitude
 */
#ifndef CO2SENSGEN_H
#define CO2SENSGEN_H

#include "Arduino.h"
#include <map>


// missing command in the driver for SCD41
#define SCD4x_COMMAND_WAKEUP       0x36F6  // execution time: 30ms
#define SCD4x_COMMAND_POWEROFF		 0x36E0  //
// sensor names
static const std::map<int, const char*> sensor_type_dict = { {0,"SCD40"},{1,"SCD41"},{2,"???"} };

// behaviour customization ------------
#define DEFAULT_ALTITUDE (uint16_t)300u
// CO2 filter constant
static const float alpha = 0.32f; // Adjust for smoothness (0.1-0.5 typical)

/*!
 *  @brief  Class that stores state and functions for interacting with
 *          SCD41 Gas Sensor
 */
class CO2sensGen
{
public:
	// constructor
	CO2sensGen();

	// init sensor and I2C
	bool begin();
	
	// preliminary checks
	bool prepareMeasurements();

	// init measurements (even if one is ongoing)
	bool getMeasurements();

	
  float getterCO2percent();
	float getterCO2filtered();
	float getterCO2max();
	void resetCO2max();
	uint32_t get_timeSinceLastSuccessfulMeasure_s();
	const char* getSensorName();

protected:
	void CO2calcUpdates();

	// tells that data content is valid
	bool firstValidData;
	// last measurement timestamp
	uint32_t lastSuccessTimestamp;	

	volatile uint16_t CO2ppm;
	float CO2percent;
	float CO2filtered;
	volatile float CO2max;

	volatile float tempDegC;
	volatile float humidityPerCent;

	int sensorType;
};

#endif 
