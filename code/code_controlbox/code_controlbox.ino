/* Thank you for building EDUDEMOS!
 *
 * This file is part of the EduDemos Modular Demonstrator which is released under <license>.
 * See file license.txt or go to <url> for full license details.
 *
 * EduDemos is a cofunded by the European Union ***********EU Förderinformationen
 * You can find more information on the website edudemos.eu
 * You can look for help in the FAQ/Troubleshooting section on edudemos.eu/**************
 */

// Import the needed libaries from the libaries folder on your PC.
#include <Wire.h>                                                     // Include the libary for communication with I2C devices like the PCF8591 and the LCD.
#include <LiquidCrystal_I2C.h>                                        // Include the libary for controlling the LCD with I2C.

LiquidCrystal_I2C lcd(0x27,16,2);                                     // Set the LCD address (0x27) and the number of columns (16) and rows (2).

// Define PCF8591     
#define PCF8591_ADRESS 0x48                                           // I2C-Adress of the PCF8591 (Standard: 0x48)
#define PCF8591_V_REF 5.0                                             // Reference voltage for the PCF8591 (5V is the voltage provided by USB)
#define PCF8591_RESOLUTION 255.0                                      // Resolution of the PCF8591 (Standard: 255 - Depends on the specific PCF8591 used)

// Settings for different modules
// These define how to handle specific sensors like wind, water level, or power usage.

// The calculation to transform the digital value given by the modules into the physical value depends on 
// the sensor so we use the formula physicalValue=(digitalValue*specificSlope)+specificOffset.
// In the following lines we define the sensor specific slopes and offsets.

// Voltage divider configuration for the sun and wind modules
#define VOLTAGE_DIVIDER_SLOPE 2
#define VOLTAGE_DIVIDER_OFFSET 0
#define VOLTAGE_DIVIDER_UPDATE_RATE 2                                 // How often the sensor is read (recommended: five times per second) If you change this value, errors might occur!

// Configuration for the water module     
#define WATER_LEVEL_SENSOR_V_REF 3.3                                  // Reference voltage for the Water-Level Sensor (3.3V is the voltage provided by the GPIO pins.)
#define WATER_LEVEL_SENSOR_SLOPE (WATER_LEVEL_SENSOR_V_REF / 4)     
#define WATER_LEVEL_SENSOR_OFFSET 0     
#define WATER_LEVEL_SENSOR_UPDATE_RATE 60                             // [1/10 s] -> once in six seconds (To prevent the waterlevel sensor from rusting we do not read the data as often as with the other sensors.)

// Configuration for the current sensor used to measure the power consumption of the whole demonstrator
#define ACS712_SENSITIVITY 0.185                                      // This value depends on the model used. We recommend the 5 Ampere version which has a sensitivity of 0.185. It is the most accurate ACS712 model.
#define ACS712_V_REF 5.0                                              // Reference voltage for the ACS712 (5V is the voltage provided by USB.)
#define ACS712_SLOPE (1/ACS712_SENSITIVITY)     
#define ACS712_OFFSET (-ACS712_V_REF/(2*ACS712_SENSITIVITY))      
#define ACS712_UPDATE_RATE 2      

// Define visualization     

// LCD      
#define LCD_FIELD_LENGTH 8                                            // [16/2] -> Space for each value on the screen 
#define LCD_UPDATE_RATE 2                                             // [1/10 s] -> Five times per second
// Serial Communication (via USB to your Computer)      
#define SERIAL_UPDATE_RATE 20                                         // [1/10 s] -> Once every two seconds


// Define the struct (a digital container, in this case for sensor data)
// This makes it easy to manage and organize settings for each sensor.
// After defining the struct this code only uses the number of the analog channel. 
// You can see the number on the PCF8591 and written over the 4 Lusterterminals on the controlbox. 
// For programming we count up from zero so instead of 1-2-3-4 our channels are named 0-1-2-3.
// For each channel the data for the connected module/sensor is put in.

// Channel 0
#define ZERO_NEEDS_GPIO_POWER false 
#define ZERO_GPIO_POWER_PIN 15  
#define ZERO_SLOPE VOLTAGE_DIVIDER_SLOPE  
#define ZERO_OFFSET VOLTAGE_DIVIDER_OFFSET  
#define ZERO_UPDATE_RATE VOLTAGE_DIVIDER_UPDATE_RATE  
// Channel 1
#define ONE_UPDATE_RATE VOLTAGE_DIVIDER_UPDATE_RATE 
#define ONE_NEEDS_GPIO_POWER false  
#define ONE_GPIO_POWER_PIN 13 
#define ONE_SLOPE VOLTAGE_DIVIDER_SLOPE 
#define ONE_OFFSET VOLTAGE_DIVIDER_OFFSET 
// Channel 2
#define TWO_NEEDS_GPIO_POWER true                                     // Needs to be turned on to take measurements.
#define TWO_GPIO_POWER_PIN 12
#define TWO_SLOPE WATER_LEVEL_SENSOR_SLOPE
#define TWO_OFFSET WATER_LEVEL_SENSOR_OFFSET
#define TWO_UPDATE_RATE WATER_LEVEL_SENSOR_UPDATE_RATE
// Channel 3
#define THREE_NEEDS_GPIO_POWER false
#define THREE_GPIO_POWER_PIN 14
#define THREE_SLOPE ACS712_SLOPE
#define THREE_OFFSET ACS712_OFFSET
#define THREE_UPDATE_RATE ACS712_UPDATE_RATE

// ---
// This is the end of the settings part. For most changes (like module swaps) you won't need to change anything below because you can just adjust the settings above.
// ---

// Create the struct
typedef struct{
  bool needsPower;                                                    // Does the sensor need to be turned on?
  int powerPin;                                                       // Which pin controls the sensor's power?
  float slope;                                                        // Conversion factor to turn digital sensor values into physical numbers
  float offset;                                                       // Offset parameter for digital-to-physical conversion
  long updateRate;                                                    // How often a measurement is made (in tenths of a second)
  float lastMeasurement;                                              // The most recent reading from this sensor
} ch;     

ch channel[4] = {0};                                                  // Create 4 channels from the struct to connect up to 4 sensors/modules.


void setup() {      
  Serial.begin(9600);                                                 // Start serial communication with the connected computer.
  Wire.begin();                                                       // Start the I²C connection.
  lcd.begin();                                                        // Connect to the LCD.
  Wire.onReceive(receiveEvent);     
// Put the earlier defined sensor-data into the struct        
// Channel 0:     
  channel[0].needsPower = ZERO_NEEDS_GPIO_POWER;                      
  channel[0].powerPin = ZERO_GPIO_POWER_PIN;                          // (D8)
  channel[0].slope = ZERO_SLOPE;                                      
  channel[0].offset = ZERO_OFFSET;                                    
  channel[0].updateRate = ZERO_UPDATE_RATE;                           
// Channel 1:     
  channel[1].needsPower = ONE_NEEDS_GPIO_POWER;                       
  channel[1].powerPin = ONE_GPIO_POWER_PIN;                           // (D7)
  channel[1].slope = ONE_SLOPE;                                       
  channel[1].offset = ONE_OFFSET;                                     
  channel[1].updateRate = ONE_UPDATE_RATE;                            
  // Channel 2:     
  channel[2].needsPower = TWO_NEEDS_GPIO_POWER;                     
  channel[2].powerPin = TWO_GPIO_POWER_PIN;                           // (D6)  
  channel[2].slope = TWO_SLOPE;                                     
  channel[2].offset = TWO_OFFSET;                                   
  channel[2].updateRate = TWO_UPDATE_RATE;                          
  // Channel 3:     
  channel[3].needsPower = THREE_NEEDS_GPIO_POWER;                   
  channel[3].powerPin = THREE_GPIO_POWER_PIN;                         // (D5)  
  channel[3].slope = THREE_SLOPE;                                 
  channel[3].offset = THREE_OFFSET;                                 
  channel[3].updateRate = THREE_UPDATE_RATE;                        

  for(int i = 0; i < 4; i++){     
    pinMode(channel[i].powerPin, OUTPUT);                             // Enable the power connection to power sensors/modules through GPIO pins if needed.
    digitalWrite(channel[i].powerPin, LOW);                           // Turn off the power on the pins.
  }     

  delay(1000);                                                        // Wait for the serial connection to your computer to be established.
  for(int i = 0; i < 4; i++){                                         // Send the settings for each channel to your computer.
    Serial.println(channel[i].slope);
    Serial.println(channel[i].offset);
    Serial.println(channel[i].updateRate);
    Serial.println(channel[i].needsPower);
    Serial.println();
  }  
  Serial.println("Setup okay");
}

void loop() {
  long mil = millis();                                                // Get the duration the program already ran in milliseconds.
  for(int i = 0; i < 4; i++){                                         // Loop through all channels.
    if((mil % (100 * channel[i].updateRate)) == 0){                   // Check the channel if the duration, that the program has run, is divisible by the set update rate.
      channel[i].lastMeasurement = measureAndConvert(i);      
    }     
    if ((mil % (100*LCD_UPDATE_RATE)) == 0){                          // Update the LCD if the duration, that the program has run, is divisible by the set update rate.
      updateLCD(i, output(i));      
    }     
    if ((mil % (100*SERIAL_UPDATE_RATE)) == 0){                       // Send the recent measurements to your computer if the duration, that the program has run, is divisible by the set update rate.
      updateSerial(output(i));      
    }     
  }     
}     

void receiveEvent(int howMany) {      
   while (0 <Wire.available()) {      
    char c = Wire.read();                                             // Receive the byte as a character.
    Serial.print(c);                                                  // Print the character.
  }                                       
 Serial.println();                                                    // Go to a new line.
}

// Function for reading the Channel with the given ID and converting it into the current analog(voltage) input.
float input(int analogChannelID) {
  if(channel[analogChannelID].needsPower){                            // Power the sensors that need powering through GPIO pins.
    digitalWrite(channel[analogChannelID].powerPin, HIGH);      
    delay(10);      
    };                                                                
  Wire.beginTransmission(PCF8591_ADRESS);     
  Wire.write(0x40 | analogChannelID);       
  Wire.endTransmission();     
  Wire.requestFrom(PCF8591_ADRESS, 2);      
  if (Wire.available() >= 2) {      
    Wire.read();                                                      // Discard the first value.
    float reading = (Wire.read()*(PCF8591_V_REF/PCF8591_RESOLUTION)); // Convert the reading into the analog(voltage) input.
    if(channel[analogChannelID].needsPower){                          // Stop powering the sensors that need powering through GPIO pins.
      digitalWrite(channel[analogChannelID].powerPin, LOW);
      };                                                        
    return (reading);                                                 // Return the calculated analog(voltage) input.
  }
  return 0;                                                           // In case of error. This line is only reached if something went wrong.
}


// Funtion for converting the input on the given channel into the actual measurement data of the connected sensor using the formula mentioned in the beginning.
float measureAndConvert(int analogChannelID){
  return ((input(analogChannelID) * channel[analogChannelID].slope) + channel[analogChannelID].offset);
}

// Put the calculated value with up to 5 decial places, with the channel in front, into a string (text variable) that has the length of the LCD field (defined earlier).
String output(int analogChannelID){
  String str = String(analogChannelID) + ":" + String(channel[analogChannelID].lastMeasurement, 5);
  while(str.length() <= LCD_FIELD_LENGTH){
    str += ' ';
  }
  if(str.length() > LCD_FIELD_LENGTH){
    str = str.substring(0, (LCD_FIELD_LENGTH - 1));
  } 
  return str;
}

// Function to update the specific field of the LCD for each channel.
void updateLCD(int analogChannelID, String output){
  int row;
  int colStart = 0;
  switch(analogChannelID){
    case 0:
      row = 0;
      colStart = 0;
      break;
    case 1:
      row = 0;
      colStart += LCD_FIELD_LENGTH;
      break;
    case 2:
      row = 1;
      colStart = 0;
      break;
    case 3:
      row = 1;
      colStart += LCD_FIELD_LENGTH;
      break;
    default:
      break;
  }
  lcd.setCursor(colStart, row);
  lcd.print(output);
}

// Function to send the channel specific output to your computer.
void updateSerial(String output){
  Serial.println(output);
}
