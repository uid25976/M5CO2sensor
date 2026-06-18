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
#include "I2C_Class.h"

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
 * To be called after power up
 
 *  @return True if sensor found on I2C, False if something went wrong!
 */
boolean CO2sensGen::begin()
{  
  // setup I2C and tryCnt to read serial number;  
  bool state = co2sensor.begin(&Wire, SCD4X_I2C_ADDR, GPIO_SDA_EXT, GPIO_SCL_EXT, 400000U, false, false, true, false);
  
  return (state);
}
  

bool CO2sensGen::prepareMeasurements()
{    
    // wakeup sensor
    bool success = co2sensor.sendCommand(SCD4x_COMMAND_WAKEUP);
    if (success)
    {
      ESP_LOGI(TAG, "Wakeup sent");
      delay(30);

      success = co2sensor.stopPeriodicMeasurement();
      if (success)
      {
          ESP_LOGI(TAG, "measurements stopped");

          success = co2sensor.reInit();
          if (success)
          {
              ESP_LOGI(TAG, "reinit sent: request serial");

              // get serial to be sure of sensor existance
              char serialNumber[13];  // Serial number is 12 digits plus trailing NULL
              success = co2sensor.getSerialNumber(serialNumber); 
              if (success)
              {
                  ESP_LOGI(TAG, "serial = %s", serialNumber);
                  // get type                  
                  scd4x_sensor_type_e type;
                  success = co2sensor.getFeatureSetVersion(&type);
                  

                  if (success)
                  {                      
                      sensorType = static_cast<int>(type);                      
                      ESP_LOGI(TAG, "sensor_type = %s", sensor_type_dict.at(sensorType) );
                  }
              }
          }
      }
    }
    return (success);
}


const char* CO2sensGen::getSensorName()
{
  if (sensorType <= 2)
      return (sensor_type_dict.at(sensorType));
  else
      return("");
}


// get measurements: requests up to 60 times every 100ms
bool CO2sensGen::getMeasurements()
{  
  firstValidData = false;
  bool meas_achieved = false;
  bool success = false;
  //success = co2sensor.sendCommand(SCD4x_COMMAND_WAKEUP);
  //if (success)
  {
      //ESP_LOGI(TAG, "Wakeup sent");

      // request measurement now
      success = co2sensor.measureSingleShot();
      if (success)
      {
        ESP_LOGI(TAG, "mesurement requested");
        // it can take up to 5s to get the response...
               
        bool data_rdy = false;
        uint8_t tryCnt = 0;
        for (tryCnt = 0; (tryCnt < 60) && (!data_rdy) ; tryCnt++)
        {
            data_rdy = co2sensor.getDataReadyStatus();
            if (!data_rdy) ESP_LOGI(TAG, "data NOT ready");
            delay(100);
        }

        if (data_rdy)
        {
            ESP_LOGI(TAG, "data ready after %d attempts", tryCnt); 
            success = co2sensor.readMeasurement();
            if (success)
            {
                meas_achieved =  true;               
                firstValidData = true;
                lastSuccessTimestamp = millis();

                CO2ppm = co2sensor.getCO2();
                humidityPerCent = co2sensor.getHumidity();
                tempDegC = co2sensor.getTemperature();

                CO2calcUpdates();

                ESP_LOGI(TAG, "CO2:%d ppm - Temp:%f degC - RH:%f)", CO2ppm, tempDegC, humidityPerCent);
            } else
            {
                ESP_LOGE(TAG, "1st Measurements after power up failed");
            } // end measurements read
        } else
        {
            ESP_LOGE(TAG, "Data ready time out on 1st request after power up");                
        } // end data ready        
         
      } // end command
      else
      {
          ESP_LOGE(TAG, "Request measure failed");
      }

  }
  
  return (meas_achieved);
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

