/**
 * CO2 sensor using a M5stick plus and SGP30 sensor
 * Board library : ESP32 3.3.10
 * Libraries: requires M5StickCPlus2 1.0.1 
 *            - M5Unified 0.2.7 - M5Utility 0.0.9
 * Original SGP30 library from Adafruit caused conflicts with the ESP32 SPI driver: we override
 * it to change the SPI driver that it used: this required to change from "private" to "protected" some members of original Adafruit class
 * History:
 * 2025-12-27: v 0.9 first version
 *
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


// customize ------------------------------------------------
#define ACQ_TIMEOUT_S (uint32_t)300u

// constant --------------------------------------------------

// CO2 thresholds to notify level of danger 
const float thres1 = 0.1;
const float thres2 = 0.5;
const float thres3 = 4;
// display settings
const int header_height = 35;
const int footer_height = 45;

// variables ------------------------------------------------
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


// ======================== INIT =============================
// ===========================================================
void setup() 
{
    bool report = false;
	auto cfg = M5.config();  // Assign a structure for initializing M5Stack
    
    M5.begin(cfg);  // initialize M5 device    

    // buzzer volume
    M5.Speaker.setVolume(250);

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
	report = CO2sensor.begin();
	if (!report) 
    {
        ESP_LOGE(TAG, "Sensor not found");

        M5.Speaker.tone(740, 500);             

        canvasMain.clear(TFT_BLACK);
        canvasMain.setTextColor(TFT_RED);
        canvasMain.drawString("NO",lcd_width/3, int(main_height/5));
        canvasMain.drawString("SENSOR",0, 2*int(main_height/5));
        canvasMain.pushSprite(&M5.Lcd, 0, header_height);
        // stop forever: we cannot do anything
        while (1);
    }
    

    // start sensor meaurements
    report = CO2sensor.startMeasurements();
    if (!report) 
    {
        ESP_LOGE(TAG, "Sensor start error");

        M5.Speaker.tone(740, 500);        

        canvasMain.clear(TFT_BLACK);
        canvasMain.setTextColor(TFT_RED);
        canvasMain.drawString("ERROR",4, int(main_height/5));
        canvasMain.pushSprite(&M5.Lcd, 0, header_height);
        // stop forever: we cannot do anything
        while (1);
    }
    
	ESP_LOGI(TAG, "Sensor Init OK");
    canvasHeader.fillSprite(TFT_NAVY);    
    canvasHeader.drawString("%CO2",lcd_width/2-int(lcd_width/5), 5);
    canvasHeader.pushSprite(&M5.Lcd, 0, 0);
}




// ======================== LOOP =============================
// ===========================================================
void loop() 
{

    if ( CO2sensor.areDataReady() )
    {
        ESP_LOGI(TAG, "New measurement available");
        
        displayMainValue(co2percent);
        displayMAXvalue(co2percentMAX);

    } else
    {
        // check if time since last measurement matches expected periodicity
        uint32_t time_elapsed = CO2sensor.get_timeSinceLastSuccessfulMeasure_s();
        if (time_elapsed > ACQ_TIMEOUT_S)
        {
            ESP_LOGE(TAG, "Timeout on valid acquisitions");
            M5.Speaker.tone(200, 500);        
            canvasMain.clear(TFT_BLACK);
            canvasMain.setTextColor(TFT_RED);
            canvasMain.drawString("ERROR",lcd_width/3, int(main_height/5));
            canvasMain.pushSprite(&M5.Lcd, 0, header_height);
        }
    }


   // one sec delay required between measurements
   delay(1000);
}




void displayMainValue(float fval)
{
    // update main panel ---------------------
    canvasMain.clear(TFT_BLACK);

    // customize color according to the risk 
    if (fval < thres1)
    {
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
    canvasMain.drawFloat(fval, 1, lcd_width/4, int(main_height/5));
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

