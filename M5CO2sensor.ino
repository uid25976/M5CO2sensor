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

#include <Wire.h>
#include "SGP30_M5.h"

// proto ----------------------------------------------------
void displayMainValue(float fval);
void displayMAXvalue(float fval);


// customize ------------------------------------------------
#define DUMP_ON_SERIAL

// constant --------------------------------------------------
// I2C pins of the M5 grove connector
#define GPIO_SDA_EXT 32
#define GPIO_SCL_EXT 33

// tune humidity compensation: page 10 of data sheet
// target: 13degC 95%HR (optimized for caves)
// units : mg/m3
const uint32_t HR_MG_M3 = 10753;
// sensor delay before operational
const uint32_t stabilization_time_ms = 15000;
// CO2 thresholds to notify level of danger 
const float thres1 = 0.1;
const float thres2 = 0.5;
const float thres3 = 4;
const int header_height = 35;
const int footer_height = 45;

// variables ------------------------------------------------
// sensor object
SGP30_M5 sgp;
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
	auto cfg = M5.config();  // Assign a structure for initializing M5Stack
    
    M5.begin(cfg);  // initialize M5 device
    M5.Lcd.clear(TFT_BLACK);
    lcd_width = M5.Lcd.width();
    lcd_height = M5.Lcd.height();

    main_height = lcd_height-(header_height + footer_height);
    
    
    
    // I2C init -------------------	
    Wire.begin(GPIO_SDA_EXT, GPIO_SCL_EXT); // Initialize I2C with SDA pin 32 and SCL pin 33 for M5StickC Plus2
    Wire.setClock(400000); // Set I2C clock speed to 400 kHz
	
	// SW reset using general call addr --------    
    sgp.softReset();
	
	// Serial port init for debug --------------
	Serial.begin(115200);


    // Init the sensor -------------------------
	bool report = sgp.begin();	
	if (!report) 
    {
      Serial.println("INIT FAILED");
	  while (1);
    } 

    // create header ------------------------------
    canvasHeader.createSprite(lcd_width, header_height);
    canvasHeader.fillSprite(TFT_NAVY);    
    canvasHeader.setTextSize(3);
    canvasHeader.setTextColor(TFT_YELLOW);  
    canvasHeader.drawString("WAIT",lcd_width/2-int(lcd_width/5), 5);
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
    canvasMain.drawString("...",lcd_width/3, int(main_height/5));
    canvasMain.pushSprite(&M5.Lcd, 0, header_height);


    // set humidity ------------------------------
    report = sgp.setHumidity(HR_MG_M3);
    if (!report) 
    {
      Serial.println("HR FIX FAILED");
	  while (1);
    } 

    // dummy measurements during stabilization time
	uint32_t init_start_ts = millis();;
    while ((millis() - init_start_ts) < stabilization_time_ms)
	{		
		report = sgp.IAQmeasure();
        delay(1000);
	}


	
	if (report)
	{
		canvasHeader.fillSprite(TFT_NAVY);    
        canvasHeader.drawString("%CO2",lcd_width/2-int(lcd_width/5), 5);
        canvasHeader.pushSprite(&M5.Lcd, 0, 0);

        M5.Speaker.setVolume(250);        
        M5.Speaker.tone(1000, 500);        
    } else
	{
		Serial.println("INIT FAILED");
	}	
}




// ======================== LOOP =============================
// ===========================================================
void loop() 
{
   if (!sgp.IAQmeasure()) 
   {  // Commands the sensor to take a single eCO2/VOC                              
      Serial.println("Measurement failed");      
   } else
   {
        co2percent = (float)sgp.eCO2/10000; // 100(convert to %)/1000000(ppm) =  10000
	        
        displayMainValue(co2percent);

#ifdef DUMP_ON_SERIAL
        Serial.printf("\nCO2 %d ppm", sgp.eCO2);
        Serial.printf("\nCO2 %.2f %%", co2percent);
#endif
        if (co2percent > co2percentMAX)
        {
            co2percentMAX = co2percent;
            displayMAXvalue(co2percentMAX);
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

