/*!
 * @file CO2sensGen.cpp
 * @brief Generic API for CO2 sensor
 * @author aprocha46
 * @copyright Copyright (c) 2026 aprocha46
 * @license MIT
 *
 * @TODO : set SCD41 + set delat temp + set altitude
 */
#include "Arduino.h"
#include <M5Unified.h>
// M5 sensors
#include "M5UnitENV.h"

#include "CO2sensGen.h"

// ESP32 debug options 
static const  char* TAG = "CO2_GEN";

// I2C pins of the M5 grove connector on M5Stick
#define GPIO_SDA_EXT 32
#define GPIO_SCL_EXT 33

// sensor object: here for M5 CO2L
SCD4X co2sensor;


/*!
 *  @brief  default constructor
 * to prevent mis interpretation of data the valid flag is set to false
 * and will be set to true after first successful measurement.
 */
CO2sensGen::CO2sensGen() {firstValidData = false;}



/**
 *  @brief  Sensor initialization.
 *  Setups the I2C for communication over grove link with appropriate I2C address.
 *  Checks hardware and detects a valid sensor
 
 *  @return True if sensor found on I2C, False if something went wrong!
 */
boolean CO2sensGen::begin()
{  
  bool state = co2sensor.begin(&Wire, SCD4X_I2C_ADDR, GPIO_SDA_EXT, GPIO_SCL_EXT, 400000U);
  
  return (state);
}
  

bool CO2sensGen::startMeasurements()
{
  uint16_t error;
  bool success = true;
  firstValidData = false;

  stop potentially previously started measurement
  error = co2sensor.stopPeriodicMeasurement();
  if (error) 
  {
      success =false;
      ESP_LOGE(TAG, "Error trying to execute stopPeriodicMeasurement()");
  } else
  {
      // Start Measurement
      error = co2sensor.startPeriodicMeasurement();
      if (error) 
      {
          success =false;
          ESP_LOGE(TAG, "Error trying to start startPeriodicMeasurement()");
      }
  }

  return success;
}



bool CO2sensGen::areDataReady()
{
  bool success = co2sensor.update();

  if (success)
  {
      firstValidData = true;
      uint32_t lastSuccessTimestamp = millis();

      // update measurements ------------------
      CO2ppm = co2sensor.getCO2();
      tempDegC = co2sensor.getTemperature();
      humidityPerCent = co2sensor.getHumidity();

      ESP_LOGI(TAG, "CO2:%d ppm - Temp:%f degC - RH:%f",CO2ppm, tempDegC, humidityPerCent);

     // update calculations
     CO2calcUpdates();
  }
}


void CO2sensGen::CO2calcUpdates()
{
    CO2percent = static_cast<uint32_t>(CO2ppm/10000);
    
    // update CO2 MAX 
    if (CO2percent > CO2max)
    {
        CO2max = CO2percent;
    }

    // filter CO2 to smooth
    CO2filtered = alpha * CO2percent + (1.0f - alpha) * CO2filtered;
}


uint32_t CO2sensGen::get_timeSinceLastSuccessfulMeasure_s()
{
  uint32_t time_s = 0XFFFFFFFF;
  

  if(firstValidData)
  {
    uint32_t deltaT_ms = millis() - lastSuccessTimestamp;
    time_s = static_cast<uint32_t>( static_cast<float>(deltaT_ms) / 1000 );
  }
  
  return time_s;
}



// getters ------------------------------------
float CO2sensGen::getterCO2percent()
{
    return CO2percent;
}


float CO2sensGen::getterCO2filtered()
{
  return CO2filtered;
}

float CO2sensGen::getterCO2max()
{
  return CO2max;
}

