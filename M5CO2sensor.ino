/**
 * @brief CO2 sensor application using M5Stick Plus and SCD41 sensor
 * Board library : ESP32 3.3.10
 * Libraries: requires M5StickCPlus2 1.0.1 
 *
 * NOTE: main button allows to reset the max recorded CO2 value
 * There is a 100ms task to monitor the main button.
 *  
 * History:
 * 2025-12-27: v0.1 - Initial version using SGP30 sensor (discarded - unsuitable for caves)
 * 2026-06-22: v0.9 - First working version using SCD41 sensor
 * 2026-06-25: v1.0 first release
 * @TODO add low power
 */
#include <M5Unified.h>
// ESP32 debug origin 
static const char* TAG = "MAIN";
// sensor insterface
#include "CO2sensGen.h"

// function prototypes ------------------------------------
void displayMainValue(float fval);
void displayMAXvalue(float fval);
void canvasMain_drawVirtualLED(bool on_off);
void buttonPollingTask(void *pvParameters);
void drawCO2Plot();
void display_InitRelativeDimensions(int lcd_width, int lcd_height);
int center_text_fix(int nb_char);

// configuration constants ---------------------------------
static const char* sw_vers_str = "v1.0";   /**< software version */
#define ACQ_TIMEOUT_S (uint32_t)20u     /**< Acquisition timeout in seconds */
#define TEMP_RH_TO_MS (uint32_t)2000    /**< set time for temporary display of temp and RH using button C */

// CO2 safety thresholds ------------------------------------
// @brief CO2 concentration thresholds for visual warnings
// Hysteresis values for stable display transitions
static const float thres1_up = 0.15f;    /**< Safe level threshold for increasing values */
static const float thres1_down = 0.1f; /**< Safe level threshold for decreasing values  */
static const float thres2_up = 1.0f;    /**< Warning level threshold for increasing values  */
static const float thres2_down = 0.95f; /**< Warning level threshold for decreasing values  */
static const float thres3_up = 3.0f;    /**< Danger level threshold for increasing values  */
static const float thres3_down = 2.95f; /**< Danger level threshold for decreasing values  */

// display configuration ----------------------------------


static const uint8_t LED_Size = 10; /**< Virtual LED indicator size in pixels */

// plot configuration -------------------------------------
#define BUFFER_SIZE 50               /**< Number of values to store and display */
#define DOT_SIZE 2                    /**< Size of plot dots in pixels */

// display configuration ----------------------------------
// --------------------------------------------------------
static const int char_width = 17;   /**< width of a character on the display */
// display is split into a header; main and footer each defined as a canvas or sprite
// these 3 fields have the LCD width (single column)
static int lcd_width = 0;
static int lcd_height = 0;

//header ---------------------------------
M5Canvas canvasHeader(&M5.Lcd);
static const int header_height = 35;     /**< Header area height in pixels */
static int headerText_Yoff;

// Main ---------------------------------
// it contains the CO2 value or error message
// the plot of CO2 values for last 4mn
// a flashing dot to notify measurement update
M5Canvas canvasMain(&M5.Lcd);
static int main_height;

// main value to display
static int CO2val_lcd_Y_MainOffset;
static int CO2val_next_lcd_Y_MainOffset;

// Runtime plot dimensions and location
static int Plot_Xoffset = 0;              /**< X coordinate for plot origin */
static int Plot_Yoffset = 0;              /**< Y coordinate for plot origin */
static int Plot_width = 0;                /**< Width of the plot area */
static int Plot_height = 0;               /**< Height of the plot area */

// virtual LED to notify measurement update
static int virtualLED_X_LCDoffset ;
static int  virtualLED_Y_Mainoffset ;

// footer --------------------------------- 
M5Canvas canvasFooter(&M5.Lcd);
static const int footerText_Yoff = 5;
static const int footer_height = 45;     /**< Footer area height in pixels */
static int footer_window_Yoff;


// variables ------------------------------------------------
// first measurement flag
static bool isFirstMeasurement = true;
// sensor object
CO2sensGen CO2sensor;
// variable to track previous CO2 value for hysteresis
static float previousCO2Value = 0.0f;

// circular buffer for CO2 trend plotting
static float co2Buffer[BUFFER_SIZE] = {0}; /**< Circular buffer for CO2 values */
static uint8_t bufferIndex = 0;            /**< Current position in circular buffer : will behave like an oscilloscope: when buffer size reached will restart at 0 */
static bool bufferFull = false;            /**< Flag to detect that the whole buffer has been filled */
static uint32_t nb_failed = 0;             /**< count number of successive failed measurements */

// Task handle for the button polling task
TaskHandle_t buttonTaskHandle = NULL;

static uint32_t TempRHdisplayTime_ms = 0;   /**< timer for temporary display of temp + RH */


/**
 * @brief Initialization function - runs once at startup
 * @details Sets up the M5Stick device, initializes displays, 
 * configures the CO2 sensor, and creates the button monitoring task
 */
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
    // get display size 
    lcd_width = M5.Lcd.width();
    lcd_height = M5.Lcd.height();
    // compute display locations accordingly
    
    display_InitRelativeDimensions(lcd_width, lcd_height);
 
    
    // create header ------------------------------
    canvasHeader.createSprite(lcd_width, header_height);
    canvasHeader.fillSprite(TFT_NAVY);    
    canvasHeader.setTextSize(3);
    canvasHeader.setTextColor(TFT_YELLOW);  
    canvasHeader.drawString("INIT",lcd_width/2 - center_text_fix(4), headerText_Yoff);
   
    canvasHeader.pushSprite(&M5.Lcd, 0, 0);
	

    // create footer ------------------------------
    canvasFooter.createSprite(lcd_width, footer_height);
    canvasFooter.fillSprite(TFT_DARKGREY);    
    canvasFooter.setTextSize(4);
    canvasFooter.setTextColor(TFT_BLACK);  
    canvasFooter.drawString("MAX ",lcd_width/2 - center_text_fix(3), footerText_Yoff);
    canvasFooter.pushSprite(&M5.Lcd, 0, footer_window_Yoff);
    

    // create main ------------------------------
    canvasMain.createSprite(lcd_width, main_height);
    canvasMain.fillSprite(TFT_BLACK);    
    canvasMain.setTextSize(4);
    canvasMain.setTextColor(TFT_WHITE);  
    canvasMain.drawString("WAIT",lcd_width/2 - center_text_fix(4), CO2val_lcd_Y_MainOffset);
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
         canvasMain.setTextSize(3);
        canvasMain.drawString("NO",lcd_width/2 - center_text_fix(2), CO2val_lcd_Y_MainOffset);
        canvasMain.drawString("SENSOR",lcd_width/2 - center_text_fix(6), CO2val_next_lcd_Y_MainOffset);
        
        canvasMain.pushSprite(&M5.Lcd, 0, header_height);
        // wait and reboot after delay
        delay(3000);
        esp_restart(); // Hardware reset
    }
    
    // sensor initialization
    success = CO2sensor.prepareMeasurements();
    if (!success)
    {
        ESP_LOGE(TAG, "Sensor prepare error");

        M5.Speaker.tone(740, 500);        

        canvasMain.clear(TFT_BLACK);
        canvasMain.setTextColor(TFT_RED);
        canvasMain.drawString("ERROR",lcd_width/2- center_text_fix(5), CO2val_lcd_Y_MainOffset);
        canvasMain.pushSprite(&M5.Lcd, 0, header_height);
        // wait and reboot after delay
        delay(3000);
        esp_restart(); // Hardware reset
    } else
    {
        canvasHeader.clear(TFT_NAVY);            
        canvasHeader.drawString("READY",lcd_width/2 - center_text_fix(5), headerText_Yoff);
        canvasHeader.pushSprite(&M5.Lcd, 0, 0);

        canvasMain.clear(TFT_BLACK);
        canvasMain.setTextColor(TFT_GREEN);
        // display sensor name while waiting first measurement
        canvasMain.drawString(CO2sensor.getSensorName(),lcd_width/2 - center_text_fix(5), CO2val_lcd_Y_MainOffset);
        // display SW vers
        canvasMain.drawString(sw_vers_str, lcd_width/2 - center_text_fix(4), CO2val_next_lcd_Y_MainOffset);

        canvasMain.pushSprite(&M5.Lcd, 0, header_height);
        ESP_LOGI(TAG, "Sensor started");
    } 
}




/**
 * @brief Main loop - runs repeatedly after setup
 * @details Performs CO2 measurements, updates the display,
 * monitors for errors, and handles timeout conditions
 */
void loop() 
{
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
            canvasHeader.drawString("%CO2",lcd_width/2 - center_text_fix(4), headerText_Yoff);                        
            canvasHeader.pushSprite(&M5.Lcd, 0, 0);
        }

        displayMainValue(CO2sensor.getterCO2percent());

        displayMAXvalue(CO2sensor.getterCO2max());
        
        nb_failed = 0;
    } else
    {
        nb_failed++;
        ESP_LOGE(TAG, "fail %d", nb_failed);
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
            canvasMain.drawString("ERROR",lcd_width/2 - center_text_fix(5), CO2val_lcd_Y_MainOffset);
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
            canvasMain.drawString("ERROR",lcd_width/2 - center_text_fix(5), CO2val_lcd_Y_MainOffset);
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




/**
 * @brief Draws or erases the virtual LED indicator
 * @param on_off true to draw LED, false to erase it
 * @details Visual indicator that shows when data is being updated
 */
void canvasMain_drawVirtualLED(bool on_off)
{       
    if (on_off)
    {
        canvasMain.fillCircle(virtualLED_X_LCDoffset, virtualLED_Y_Mainoffset, LED_Size, TFT_GREEN);   
    } else
    {
        canvasMain.fillCircle(virtualLED_X_LCDoffset, virtualLED_Y_Mainoffset, LED_Size, TFT_BLACK);
    }
}



/**
 * @brief Updates the main display with current CO2 value
 * @param fval CO2 concentration in percent to display
 * @details Shows CO2 value with color coding based on safety thresholds
 * and triggers audible warning for dangerous levels
 */
void displayMainValue(float fval)
{
    int nb_decimals = 1;
    // update main panel
    canvasMain.clear(TFT_BLACK);

    // color coding according to safety thresholds with hysteresis
    bool isIncreasing = fval > previousCO2Value;
    
    if (isIncreasing) {
        // Use upward thresholds when value is increasing
        if (fval < thres1_up)
        {
            nb_decimals = 2;  // more precision for low values
            canvasMain.setTextColor(TFT_GREEN);
        } else if (fval < thres2_up)
        {
           canvasMain.setTextColor(TFT_ORANGE);
        } else if (fval < thres3_up)
        {
            canvasMain.setTextColor(TFT_YELLOW);
        } else 
        {
            canvasMain.setTextColor(TFT_RED);
            M5.Speaker.tone(740, 700);  // audible warning
        }
    } else {
        // Use downward thresholds when value is decreasing
        if (fval < thres1_down)
        {
            nb_decimals = 2;  // more precision for low values
            canvasMain.setTextColor(TFT_GREEN);
        } else if (fval < thres2_down)
        {
           canvasMain.setTextColor(TFT_ORANGE);
        } else if (fval < thres3_down)
        {
            canvasMain.setTextColor(TFT_YELLOW);
        } else 
        {
            canvasMain.setTextColor(TFT_RED);
            M5.Speaker.tone(740, 700);  // audible warning
        }
    }
    
    // Update previous value for next call
    previousCO2Value = fval;
    
    // update circular buffer to store last data 
    co2Buffer[bufferIndex] = fval;
    // update pointer
    if (bufferIndex == (BUFFER_SIZE - 1))
    {
        bufferFull = true;  // buffer is now full
        bufferIndex = 0;
    } else
    {
        bufferIndex++;
    }         
    
    // display CO2 value
    canvasMain.drawFloat(fval, nb_decimals, lcd_width/2 - center_text_fix(4), CO2val_lcd_Y_MainOffset);
    
    // draw CO2 trend plot
    drawCO2Plot();
    
    canvasMain_drawVirtualLED(true);  // update indicator
    canvasMain.pushSprite(&M5.Lcd, 0, header_height);
}






/**
 * @brief Updates the footer display with maximum CO2 value
 * @param fval Maximum CO2 concentration in percent to display
 * @details Shows the maximum recorded CO2 value in the footer area
 */
void displayMAXvalue(float fval)
{    
    // update footer panel ---------------------      
    canvasFooter.setTextSize(4);
    canvasFooter.setTextColor(TFT_BLACK);  
    canvasFooter.clear(TFT_DARKGREY);

    // value, nb decimals, X offset, Y offset
    canvasFooter.drawFloat(fval, 1, lcd_width/2 - center_text_fix(3), footerText_Yoff);
    canvasFooter.pushSprite(&M5.Lcd, 0, footer_window_Yoff );
}



/**
 * @brief Draws the CO2 trend plot using circular buffer data
 * @details Renders a plot showing the last 50 CO2 measurements with proper scaling
 * and axis lines. Uses circles to represent data points.
 */
void drawCO2Plot()
{   
    // Draw plot axis
    canvasMain.drawLine(Plot_Xoffset, Plot_Yoffset, Plot_Xoffset + Plot_width, Plot_Yoffset, TFT_WHITE); // X axis
    canvasMain.drawLine(Plot_Xoffset, Plot_Yoffset - Plot_height, Plot_Xoffset, Plot_Yoffset, TFT_WHITE); // Y axis
    
    // Calculate scaling factors
    float xScale = Plot_width / (BUFFER_SIZE - 1);
    float yScale = Plot_height / CO2sensor.getterSensorRangeCO2percent(); // max CO2 range (0-4%)
    
    // draw no more than samples accumulated
    // Note that bufferFull is pointing to the future value, so we
    // use 'bufferIndex' rather than 'bufferIndex + 1' to count the number of acquired samples
    int nb_samples = (bufferFull  == true)? BUFFER_SIZE : bufferIndex;

    // Draw data points
    for (uint8_t i = 0; i < nb_samples; i++) 
    {               
        // Calculate position
        float co2Value = co2Buffer[i];
        
        int x = Plot_Xoffset + i * xScale;
        int y = Plot_Yoffset - co2Value * yScale;
        
        // Draw dot: last acquired sample will be in a different color (refer to previous note)
        if( (bufferIndex > 1) && (i == (bufferIndex - 1)) )
        {
            canvasMain.fillCircle(x, y, DOT_SIZE*2, TFT_ORANGE);
        } else
        {
            canvasMain.fillCircle(x, y, DOT_SIZE, TFT_GREEN);
        }        
    }   // end loop on samples for display
}

/**
 * @brief Calculates X offset to center text on display
 * @param nb_char Number of characters in the text to center
 * @return X offset in pixels to center the text
 * @details Computes the horizontal offset needed to center text
 * based on character width and number of characters
 */
int center_text_fix(int nb_char)
{
    float text_size = static_cast<float>(char_width * nb_char) / 2 ;
    return(static_cast<int>(text_size));
}


/**
 * @brief Initializes display dimensions based on LCD size
 * @param lcd_width Width of the LCD display in pixels
 * @param lcd_height Height of the LCD display in pixels
 * @details Calculates and sets all display element positions and sizes
 * relative to the available screen dimensions
 */
void display_InitRelativeDimensions(int lcd_width, int lcd_height)
 {
    float f_lcd_width = static_cast<float>(lcd_width);    
   
    // header ----------   
    headerText_Yoff = 5;

    // main ------------
    main_height = lcd_height-(header_height + footer_height);
    float f_main_height = static_cast<float>(main_height);

    // CO2 value location    
    CO2val_lcd_Y_MainOffset = static_cast<int>(f_main_height/5);    
    CO2val_next_lcd_Y_MainOffset = 2*int(f_main_height/5);

    // display plot       
    // Calculate plot dimensions
    Plot_Yoffset = static_cast<int>(3.5 * f_main_height/5);
    Plot_width = lcd_width;
    Plot_height = static_cast<int>(f_main_height / 5);

    // virtual LED coordinates
    virtualLED_X_LCDoffset = static_cast<int>(f_lcd_width/2);
    virtualLED_Y_Mainoffset = static_cast<int>(4.2 * f_main_height/5);


    // footer -----------
    footer_window_Yoff = lcd_height-footer_height;
 }




/**
 * @brief Background task for monitoring the main button
 * @param pvParameters Task parameters (unused)
 * @details Runs every 200ms to check if Button A was pressed
 * and resets the maximum CO2 value when pressed
 */
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
        canvasFooter.drawFloat(0, 1, lcd_width/2 - center_text_fix(3), footerText_Yoff);
        canvasFooter.pushSprite(&M5.Lcd, 0, footer_window_Yoff );
    } else
    if (M5.BtnB.wasPressed()) 
    {        
        // request temporary display of temp and RH
        TempRHdisplayTime_ms = millis();
        ESP_LOGI(TAG, "Request to display Temp  + RH");
        canvasHeader.clear(TFT_NAVY);            
        canvasHeader.drawNumber(CO2sensor.getterTemp(), 0, headerText_Yoff);
        canvasHeader.drawString("C %", lcd_width/2 - center_text_fix(3), headerText_Yoff);        
        canvasHeader.drawNumber(CO2sensor.getterRH(), lcd_width - center_text_fix(4), headerText_Yoff);        
        canvasHeader.pushSprite(&M5.Lcd, 0, 0);
    } else
    {
        if (TempRHdisplayTime_ms > 0)
        {
            if ( (millis() - TempRHdisplayTime_ms) > TEMP_RH_TO_MS)
            {
                // time to erase temporary display of temparature and RH
                TempRHdisplayTime_ms = 0;
                // reset header to standard display
                canvasHeader.clear(TFT_NAVY);            
                canvasHeader.drawString("%CO2",lcd_width/2 - center_text_fix(4), headerText_Yoff);                        
                canvasHeader.pushSprite(&M5.Lcd, 0, 0);
            }
        }        
    }

    // Delay for 200ms (periodic)
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(200));
  }
}


