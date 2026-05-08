/*!
 * @file SGP30_M5.h
 * 2025-07-14: aprocha46 : adaptation layer to M5stick
 * Overrides the Adafruit_SGP30 communication methods in order to use 
 * an I2C driver compatible with the M5Stick
 */
#ifndef SGP30_M5_H
#define SGP30_M5_H

#include "Arduino.h"
// local version of the driver as we need to access proitected methods
#include "Adafruit_SGP30.h"
#include "M5StickCPlus2.h"

#define WORD_LEN 2

/*!
 *  @brief  Class that stores state and functions for interacting with
 *          SGP30 Gas Sensor
 */
class SGP30_M5 : public Adafruit_SGP30
{
public:
	// constructor
	SGP30_M5();

	// overrides using M5 Wire I2C driver
	bool begin( boolean initSensor = true);
	
	// overrides to fix error on number of bytes
	bool softReset() override;

	// get measure and compute average + max CO2
	bool measureCO2();
	
  float getterCO2percent();
	float getterCO2filtered();
	float getterCO2max();
	

protected:
	float CO2percent;
	float CO2filtered;
	float CO2max;

	// SGP30 I2C address
	const uint8_t device_addr = 0x58;

	// overrides Adafruit version
	bool readWordFromCommand(uint8_t command[], uint8_t commandLength, uint16_t delay_ms, uint16_t *readdata = 0, uint8_t readlen = 0) override; 
};

#endif 
