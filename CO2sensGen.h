/*!
 * @file CO2sensGen.h
 * 2026-06-17: aprocha46 : generic API for I2C CO2 sensor
 */
#ifndef CO2SENSGEN_H
#define CO2SENSGEN_H

#include "Arduino.h"

// filter constant
const float alpha = 0.32f; // Adjust for smoothness (0.1-0.5 typical)

/*!
 *  @brief  Class that stores state and functions for interacting with
 *          SGP30 Gas Sensor
 */
class CO2sensGen
{
public:
	// constructor
	CO2sensGen();

	// init sensor and I2C
	bool begin();
	

	// init measurements (even if one is ongoing)
	bool startMeasurements();

	// is data ready ?
	bool areDataReady();
	
  float getterCO2percent();
	float getterCO2filtered();
	float getterCO2max();
	uint32_t get_timeSinceLastSuccessfulMeasure_s();

protected:
	void CO2calcUpdates();

	// tells that data content is valid
	bool firstValidData;
	// last measurement timestamp
	uint32_t lastSuccessTimestamp;	

	uint16_t CO2ppm;
	float CO2percent;
	float CO2filtered;
	float CO2max;

	float tempDegC;
	float humidityPerCent;
};

#endif 
