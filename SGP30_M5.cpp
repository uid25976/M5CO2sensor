/*!
 * @file Adafruit_SGP30.cpp
 *
 * @mainpage Adafruit SGP30 gas sensor driver
 *
 * @section intro_sec Introduction
 *
 * This is the documentation for Adafruit's SGP30 driver for the
 * Arduino platform.  It is designed specifically to work with the
 * Adafruit SGP30 breakout: http://www.adafruit.com/products/3709
 *
 * These sensors use I2C to communicate, 2 pins (SCL+SDA) are required
 * to interface with the breakout.
 *
 * Adafruit invests time and resources providing this open source code,
 * please support Adafruit and open-source hardware by purchasing
 * products from Adafruit!
 *
 *
 * @section author Author
 * Written by Ladyada for Adafruit Industries.
 *
 * @section license License
 * BSD license, all text here must be included in any redistribution.
 *
 */
#include "Arduino.h"
#include "SGP30_M5.h"



/*!
 *  @brief  Instantiates a new SGP30 class
 */
SGP30_M5::SGP30_M5() {}



/*!
 *  @brief  Setups the hardware and detects a valid SGP30. Initializes I2C
 *          then reads the serialnumber and checks that we are talking to an SGP30
 
 *  @param  initSensor
 *          Optional pointer to prevent IAQinit to be called. Used for Deep
 *          Sleep.
 *  @return True if SGP30 found on I2C, False if something went wrong!
 */
boolean SGP30_M5::begin(boolean initSensor)
 {  
  uint8_t command[2];
  
  // get serial number to check wether the sensor is connected to the bus
  command[0] = 0x36;
  command[1] = 0x82;
  if (!readWordFromCommand(command, 2, 10, serialnumber, 3))
    return false;

  // check the sensor version for code compatibility --------------------
  uint16_t featureset;
  command[0] = 0x20;
  command[1] = 0x2F;
  if (!readWordFromCommand(command, 2, 10, &featureset, 1))
    return false;

  if ((featureset & 0xF0) != SGP30_FEATURESET)
    return false;


  // init sensor --------------------------------------------------------
  if (initSensor) 
  {
    if (!IAQinit())
      return false;
  }

  CO2max = 0;

   return true;
}


bool SGP30_M5::measureCO2()
{
    bool success = IAQmeasure();

    if (true == success)
    {
        CO2percent = (float)this->eCO2/10000;

        if (CO2percent > CO2max)
        {
            CO2max = CO2percent;
        }

    }

    return success;
}


float SGP30_M5::getterCO2percent()
{
  return CO2percent;
}

float SGP30_M5::getterCO2filtered()
{
  return CO2filtered;
}

float SGP30_M5::getterCO2max()
{
  return CO2max;
}


// ======================================================================
// Sending 06h at address 00h: means general reset to all slave devices
bool SGP30_M5::softReset()
{
	uint8_t command = 0x06;
	Wire.beginTransmission(0x00);
	bool success = Wire.write(&command, 1);     
	Wire.endTransmission(true);
  return success;
}



/**
     * @brief Send a command to the slave device (device_addr) and eventually
     * reads one or more words
     * @param command pointer to the array of bytes that contains the command.
     * @param commandLength is the number of bytes in the command
     * @param delay_ms is a pause after the transmission of the command
     * @param readdata is a pointer of an array of words that will receive the slave answer (skip if no answer expected)
     * @param readlen is the number of words expected fro; the slave (skip if no answer expected)
     * @return false if something went wrong 
     * @remark This API is bespoke for the SGP30 sensor where each word that is returned is appended with a CRC8
     * The CRC will be checked but not copied to the readdata buffer
     */
bool SGP30_M5::readWordFromCommand(uint8_t command[], uint8_t commandLength, uint16_t delay_ms, uint16_t *readdata, uint8_t readlen) 
{
  // send start bit + addr ---------------------------------------
  Wire.beginTransmission(this->device_addr);
  
  // send command ------------------------------------------------
  size_t nb_wr = Wire.write(command, commandLength); 
  if (nb_wr != commandLength)
  {
    Serial.printf("\nEBUG-2C: write returns %d", nb_wr);
     return false;
  }

  // end of communication: set wr/rd bit and send a stop
  Wire.endTransmission(true);
  
  // let slave enough time to prepare the answer -----------------
  delay(delay_ms);

  // is there some data to read ----------------------------------
  if (readlen != 0)
  {
      // some data to read 
      // In this component one CRC is sent for each word
      uint8_t replylen = readlen * (WORD_LEN + 1);
      uint8_t replybuffer[replylen];
      
      // read bytes and send a stop
      size_t nb_rx = Wire.requestFrom(device_addr, replylen, true);
      if (nb_rx != replylen) 
      {
        Serial.printf("\nDEBUG-2C: requestFrom returns %d", nb_rx);
        return false;
      }

      // read bytes -----------------------------
      for (int idx = 0; idx < replylen; idx++)    
      { 
        replybuffer[idx] = Wire.read(); // receive a byte as character
      }

      // check CRC and copy to remote buffer ----
      for (uint8_t i = 0; i < readlen; i++) 
      {
          uint8_t crc = this->generateCRC(replybuffer + i * 3, 2);

          if (crc != replybuffer[i * 3 + 2])
          {
            Serial.printf("\nDEBUG-2C: CRC failed on %d bytes\n", replylen);
            for (int i=0; i< replylen;i++)
            {
                Serial.printf("%02X ", replybuffer[i]);
            }
            
            return false;
          }
          
          // else success store it without CRC
          readdata[i] = replybuffer[i * 3];
          readdata[i] <<= 8;
          readdata[i] |= replybuffer[i * 3 + 1];
      } // end loop on each word

  } // end of slave response processing

  return true;
}

