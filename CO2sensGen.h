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


static const float sensorCO2capability = 4; /**< maximum CO2 percentage that he sensor is able to return */

// missing command in the driver for SCD41 
#define SCD4x_COMMAND_WAKEUP       0x36F6  /**< Command to wake up the sensor (execution time: 30ms) */
#define SCD4x_COMMAND_POWEROFF		 0x36E0  /**< Command to power down the sensor (currently unused)  */
/**
 * @brief Mapping of sensor type integers to human-readable names
 */
static const std::map<int, const char*> sensor_type_dict = { {0,"SCD40"},{1,"SCD41"},{2,"???"} };

// behaviour customization ------------
#define DEFAULT_ALTITUDE (uint16_t)300u /**< Default altitude in meters for sensor calibration */



/*!
 *  @brief  Class that stores state and functions for interacting with the Gas Sensor
 */
class CO2sensGen
{
public:
	/*!
	 * @brief Default constructor
	 * @details Initializes the sensor with firstValidData set to false
	 */
	CO2sensGen();

	/*!
	 * @brief Initializes the sensor and I2C communication
	 * @details Sets up I2C communication with the sensor using the Grove connector pins
	 * @return true if sensor is found and initialized successfully, false otherwise
	 */
	bool begin();
	
	/*!
	 * @brief Prepares the sensor for measurements
	 * @details Wakes up the sensor, stops any ongoing periodic measurements,
	 * reinitializes the sensor, and sets the default altitude for improved accuracy
	 * @return true if preparation is successful, false otherwise
	 */
	bool prepareMeasurements();

	/*!
	 * @brief Performs a single CO2 measurement
	 * @details Requests a single-shot measurement, waits for data to be ready,
	 * reads the measurement, and updates CO2 calculations
	 * @return true if measurement is successful, false otherwise
	 */
	bool getMeasurements();

	/*!
	 * @brief sensor range
	 * @return max CO2 percentage that the sensor can measure
	 */
  float getterSensorRangeCO2percent();


	/*!
	 * @brief Gets the current CO2 concentration in percent
	 * @return CO2 concentration as a percentage (e.g., 0.004 for 0.4%)
	 */
  float getterCO2percent();
	
	
	/*!
	 * @brief Gets the maximum recorded CO2 concentration in percent
	 * @return Maximum CO2 concentration encountered since last reset
	 */
	float getterCO2max();

	/*!
	 * @brief Gets temperature
	 * @return temperature in degC
	 */
	long getterTemp();

	/*!
	 * @brief Gets Relative humidity
	 * @return RH in precent
	 */
	long getterRH();
	
	/*!
	 * @brief Resets the maximum CO2 concentration tracking
	 * @details Sets the maximum CO2 value back to zero
	 */
	void resetCO2max();
	
	/*!
	 * @brief Gets the time since last successful measurement
	 * @return Time in seconds since last successful measurement, or 0xFFFFFFFF if no measurement has been made
	 */
	uint32_t get_timeSinceLastSuccessfulMeasure_s();
	
	/*!
	 * @brief Gets the sensor type name
	 * @return String representation of the sensor type (e.g., "SCD41")
	 */
	const char* getSensorName();

	

protected:
	/*!
	 * @brief Updates CO2 calculations after a new measurement
	 * @details Calculates CO2 percentage and updates maximum CO2 value
	 */
	void CO2calcUpdates();

	/*!
	 * @brief Flag indicating if valid data has been received
	 * @details Set to true after first successful measurement
	 */
	bool firstValidData;
	
	/*!
	 * @brief Timestamp of the last successful measurement
	 * @details Stores the millis() value when last measurement was successful
	 */
	uint32_t lastSuccessTimestamp;	

	/*!
	 * @brief Raw CO2 concentration in parts per million
	 * @details Direct reading from the sensor in ppm
	 */
	volatile uint16_t CO2ppm;
	
	/*!
	 * @brief CO2 concentration in percent
	 * @details Calculated as CO2ppm / 10000
	 */
	float CO2percent;
	

	
	/*!
	 * @brief Maximum CO2 concentration encountered
	 * @details Tracks the highest CO2 percentage measured since last reset or manual reset using the main button
	 */
	volatile float CO2max;

	/*!
	 * @brief Temperature reading in degrees Celsius
	 */
	volatile float tempDegC;
	
	/*!
	 * @brief Relative humidity reading in percent
	 */
	volatile float humidityPerCent;

	/*!
	 * @brief Sensor type identifier
	 * @details Stores the sensor type as an integer (0=SCD40, 1=SCD41, etc.)
	 */
	int sensorType;
};

#endif 
