/**
 * CO2 sensor using a M5stick plus and SGP30 sensor
 * Board library : ESP32 3.3.10
 * Libraries: requires M5StickCPlus2 1.0.1 
 *            - M5Unified 0.2.7 - M5Utility 0.0.9
 *
 * NOTE: main button allows to reset the max recorded CO2 value
 * There is a 100ms task to monitor the main button.
 *  
 * History:
 * 2025-12-27: v 0.1 first version based on SCP30: discared because sensor does not work in caves.
 * 2026-06-22: V 0.9 first working version based on SCD41
 * @TODO add buzzer
 * @TODO move UI to a separate class
 * @TODO add detection of stable MAX
 * @TODO: add MAX detection + reset using button
 * @TODO: add low power
 *
 */
#include <M5Unified.h>
#include <M5StickCPlus2.h>


// ESP32 debug origin 
static const  char* TAG = "MAIN";

#include "CO2sensGen.h"

// proto ----------------------------------------------------
void displayMainValue(float fval);
void displayMAXvalue(float fval);
void canvasMain_drawVirtualLED(bool on_off);
void buttonPollingTask(void *pvParameters);


// customize ------------------------------------------------
#define ACQ_TIMEOUT_S (uint32_t)300u

// constant --------------------------------------------------

// CO2 thresholds to notify level of danger 
static const float thres1 = 0.2;
static const float thres2 = 0.7;
static const float thres3 = 3; // warning must be < 4 with the SCD41 sensor as it is its max value !

// display settings
const int header_height = 35;
const int footer_height = 45;
// LED like display
static const uint8_t LED_Size = 10;


// variables ------------------------------------------------
// first measurement
static bool isFirstMeasurement = true;
// sensor object
CO2sensGen CO2sensor;
static uint32_t last_millis = 0;
static int16_t lcd_width = 0;
static int16_t lcd_height = 0;
static float co2percent = 0;
static float co2percentMAX = 0;

static int main_height;

M5Canvas canvasHeader(&M5.Lcd);
M5Canvas canvasMain(&M5.Lcd);
M5Canvas canvasFooter(&M5.Lcd);

// Task handle for the button polling task
TaskHandle_t buttonTaskHandle = NULL;


// ======================== INIT =============================
// ===========================================================
void setup() 
{
    bool success = false;
	auto cfg = M5.config();  // Assign a structure for initializing M5Stack
    
    M5.begin(cfg);  // initialize M5 device    

    // buzzer volume
    M5.Speaker.setVolume(250);

    // Create the button polling task
    xTaskCreate(
        buttonPollingTask,  // Task function
        "ButtonPollingTask", // Task name
        2048,               // Stack size (bytes)
        NULL,               // Parameters
        1,                  // Priority
        &buttonTaskHandle   // Task handle
    );

    // Init display --------------------------------
    M5.Lcd.clear(TFT_BLACK);
    lcd_width = M5.Lcd.width();
    lcd_height = M5.Lcd.height();

    main_height = lcd_height-(header_height + footer_height);
    
    // create header ------------------------------
    canvasHeader.createSprite(lcd_width, header_height);
    canvasHeader.fillSprite(TFT_NAVY);    
    canvasHeader.setTextSize(3);
    canvasHeader.setTextColor(TFT_YELLOW);  
    canvasHeader.drawString("INIT",lcd_width/2-int(lcd_width/5), 5);
    canvasHeader.pushSprite(&M5.Lcd, 0, 0);//"&M5
	

    // create footer ------------------------------
    canvasFooter.createSprite(lcd_width, footer_height);
    canvasFooter.fillSprite(TFT_DARKGREY);    
    canvasFooter.setTextSize(4);
    canvasFooter.setTextColor(TFT_BLACK);  
    canvasFooter.drawString("MAX: ",1, 5);
    canvasFooter.pushSprite(&M5.Lcd, 0, lcd_height-footer_height);

    // create main ------------------------------
    canvasMain.createSprite(lcd_width, main_height);
    canvasMain.fillSprite(TFT_BLACK);    
    canvasMain.setTextSize(4);
    canvasMain.setTextColor(TFT_WHITE);  
    canvasMain.drawString("WAIT",lcd_width/3, int(main_height/5));
    canvasMain.pushSprite(&M5.Lcd, 0, header_height);
        
	ESP_LOGI(TAG, "Init sensor started");
    
    // Sensor init -------------------	    
	success = CO2sensor.begin();
	if (!success) 
    {
        ESP_LOGE(TAG, "Sensor not found");

        M5.Speaker.tone(740, 500);             

        canvasMain.clear(TFT_BLACK);
        canvasMain.setTextColor(TFT_RED);
        canvasMain.drawString("NO",lcd_width/3, int(main_height/5));
        canvasMain.drawString("SENSOR",0, 2*int(main_height/5));
        canvasMain.pushSprite(&M5.Lcd, 0, header_height);
        // stop forever: we cannot do anything
        while (1) {delay(1000);};
    }
    
    // sensor initialization
    success = CO2sensor.prepareMeasurements();
    if (!success)
    {
        ESP_LOGE(TAG, "Sensor prepare error");

        M5.Speaker.tone(740, 500);        

        canvasMain.clear(TFT_BLACK);
        canvasMain.setTextColor(TFT_RED);
        canvasMain.drawString("ERROR",0, int(main_height/5));
        canvasMain.pushSprite(&M5.Lcd, 0, header_height);
        // stop forever: we cannot do anything
        while (1) {delay(1000);};
    } else
    {
        canvasHeader.clear(TFT_NAVY);            
        canvasHeader.drawString("READY",lcd_width/2-int(lcd_width/3), 5);
        canvasHeader.pushSprite(&M5.Lcd, 0, 0);//"&M5

        canvasMain.clear(TFT_BLACK);
        canvasMain.setTextColor(TFT_RED);
        canvasMain.drawString(CO2sensor.getSensorName(),0, int(main_height/5));
        canvasMain.pushSprite(&M5.Lcd, 0, header_height);
        ESP_LOGI(TAG, "Sensor started");
    } 
}




// ======================== LOOP =============================
// ===========================================================
void loop() 
{
    uint32_t nb_failed = 0;
    
    

    // request measurements 
    ESP_LOGI(TAG, "request measurement -----------");
    bool success = CO2sensor.getMeasurements();
    if (success)
    {
        ESP_LOGI(TAG, "New measurement available");
        
        if (isFirstMeasurement)
        {
            isFirstMeasurement = false;
            // change header
            canvasHeader.clear(TFT_NAVY);            
            canvasHeader.drawString("%CO2",lcd_width/2-int(lcd_width/5), 5);                        
            canvasHeader.pushSprite(&M5.Lcd, 0, 0);//"&M5
        }

        displayMainValue(CO2sensor.getterCO2percent());
        
        displayMAXvalue(CO2sensor.getterCO2max());
        
        nb_failed = 0;
    } else
    {
        nb_failed++;
    }

    uint32_t time_elapsed = CO2sensor.get_timeSinceLastSuccessfulMeasure_s();
    if (time_elapsed != 0XFFFFFFFF)
    { 
        // there has been at least one valid acquisition in the past
        if(time_elapsed > ACQ_TIMEOUT_S)
        {
            ESP_LOGE(TAG, "Last valid acquisition was %d s earlier", time_elapsed);
            M5.Speaker.tone(200, 500);        
            canvasMain.clear(TFT_BLACK);
            canvasMain.setTextColor(TFT_RED);
            canvasMain.drawString("ERROR",0, int(main_height/5));
            canvasMain.pushSprite(&M5.Lcd, 0, header_height);
            // wait and reboot after delay
            delay(3000);
            esp_restart(); // Hardware reset
        }
    } else
    {
        // no measurement at all up to now
        if (nb_failed > 3)
        {
            ESP_LOGE(TAG, "No Measurement up to now after 3 attempts");
            M5.Speaker.tone(200, 500);        
            canvasMain.clear(TFT_BLACK);
            canvasMain.setTextColor(TFT_RED);
            canvasMain.drawString("ERROR",0, int(main_height/5));
            canvasMain.pushSprite(&M5.Lcd, 0, header_height);
            // wait and reboot after delay
            delay(3000);
            esp_restart(); // Hardware reset
        }
    }
    // draw a circle to notify that data has been updated
    delay(500);    
    if (success)
    {
        // erase the virtual LED
        canvasMain_drawVirtualLED(false);
        canvasMain.pushSprite(&M5.Lcd, 0, header_height);
    }

} // end main loop




// utilities ---------------------------------------------
void canvasMain_drawVirtualLED(bool on_off)
{
    if (on_off)
    {
        canvasMain.fillCircle(lcd_width/2, 4*int(main_height/5), LED_Size, TFT_GREEN);   
    } else
    {
        canvasMain.fillCircle(lcd_width/2, 4*int(main_height/5), LED_Size, TFT_BLACK);
    }
}


void displayMainValue(float fval)
{
    int nb_decimals = 1;
    // update main panel ---------------------
    canvasMain.clear(TFT_BLACK);

    // customize color according to the risk 
    if (fval < thres1)
    {
        nb_decimals = 2;
        canvasMain.setTextColor(TFT_GREEN);
    } else if (fval < thres2)
    {
       canvasMain.setTextColor(TFT_ORANGE);
    } else if (fval < thres3)
    {
        canvasMain.setTextColor(TFT_PURPLE);
    } else 
    {
        canvasMain.setTextColor(TFT_RED);
        M5.Speaker.tone(740, 700);
    }
    // value, nb decimals, X offset, Y offset
    canvasMain.drawFloat(fval, nb_decimals, lcd_width/4, int(main_height/5));
    // add temporary virtual LED to notify the update
    canvasMain_drawVirtualLED(true);
    canvasMain.pushSprite(&M5.Lcd, 0, header_height);
}


void displayMAXvalue(float fval)
{    
    // update footer panel ---------------------      
    canvasFooter.setTextSize(4);
    canvasFooter.setTextColor(TFT_BLACK);  
    canvasFooter.clear(TFT_DARKGREY);

    //canvasFooter.drawString("MAX: ",1, 5);
    // value, nb decimals, X offset, Y offset
    canvasFooter.drawFloat(fval, 1, int(lcd_width/3), 5);
    canvasFooter.pushSprite(&M5.Lcd, 0, (lcd_height-footer_height) );
}



// Function to check button A and call startMeasInfo() if pressed
void buttonPollingTask(void *pvParameters) {
  (void)pvParameters; // Unused parameter

  TickType_t lastWakeTime = xTaskGetTickCount();

  while (1) 
  {
    // Update M5 button states
    M5.update();
    
    // Check if Button A was pressed
    if (M5.BtnA.wasPressed()) 
    {
        ESP_LOGI(TAG, "CO2 max reset");
        // reset CO2max on main button press
        CO2sensor.resetCO2max();
        // update display     
        canvasFooter.clear(TFT_DARKGREY);

        // value, nb decimals, X offset, Y offset
        canvasFooter.drawFloat(0, 1, int(lcd_width/3), 5);
        canvasFooter.pushSprite(&M5.Lcd, 0, (lcd_height-footer_height) );
    }

    // Delay for 200ms (periodic)
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(200));
  }
}