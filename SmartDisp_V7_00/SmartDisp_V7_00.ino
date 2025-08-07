/** @mainpage
* The SmartDisp.ino program implements an application that
* displays times, date, humidity and temperatures on a Matrix
*
* @author  Lukas Christian
* @version V8.0 2020/04/14
* @since   2019-09
* @date 2019/Sep/09 - 2025/aug/4
*/



/* control leds*/
#include <Adafruit_NeoPixel.h>

/* RTC*/
#include <Wire.h>
#include <ds3231.h>

/* humanity and temperatures sensor*/
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

/* NTP_time_client with wifi*/
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
//#include <string.h>



/* Font*/
#include "italic5x5_font.h"


// Neue Includes hinzufügen:
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <DNSServer.h>



// ==================== NEUE GLOBALE VARIABLEN ====================
bool isAPMode = true;  // NEU: Flag für Access Point Modus
unsigned long wifiConnectStartTime = 0;  // NEU: Timer für WiFi Verbindungsversuche

// Neue globale Variablen hinzufügen:
ESP8266WebServer server(80);
DNSServer dnsServer;

const char *ssid = "Dialog WLAN 9ABC";
const char *password = "05156476454093850903";

const long utcOffsetInSeconds = 3600;

//char wday[7][4] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
char wday[7][12] = { "Sonntag\0", "Montag\0", "Dienstag\0", "Mittwoch\0", "Donnerstag\0", "Freitag\0", "Samstag\0" };


// Define NTP Client to get time
//WiFiUDP ntpUDP;
//NTPClient timeClient(ntpUDP, "0.at.pool.ntp.org", utcOffsetInSeconds);



// Which pin on the Arduino is connected to the NeoPixels?
#define PIN D6  ///< On Trinket or Gemma, suggest changing this to 1

ts t;                            ///<ts is a struct findable in ds3231.h
const unsigned char _atoz = 48;  ///<skipping the letters to get Numbers in "intalic5x5_font.h"

#define NUMPIXELS 128          ///<Popular NeoPixel ring size
#define MAXCOLOR 255           ///<Maximum Color brightness
#define MATRIX_DIMENSION_X 20  ///<Maximum of rows
#define MATRIX_DIMENSION_Y 7   ///<Maximum of lines
#define MAX_RAINBOW_COLOR 255  ///<Maximum LED Color brightness
#define RAINBOW_MULTIPLIER 3   ///<stepwidth of the rainbow color

#define DHTPIN D7      ///< Digital pin connected to the DHT sensor
#define DHTTYPE DHT22  ///< DHT 22 (AM2302)

/*! @brief create an object pixels from class Adafruit_NeoPixel
*
* If any argument is invalid, the function has no effect.
*
* @param NUMPIXELS The maximum number of
* @param PIN The Pinname from µC which is connected with data pin.
* @param NEO_GRB The way how the rgb color is transmitted
* @param NEO_KHZ800 The speed how fast the the data are transmitted
*/
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

#define DELAYVAL 0.1    ///< Time (in milliseconds) to pause between pixels
#define CLOCK_TIME 500  ///< Time (in milliseconds) to set clock frequenz

// EEPROM Adressen
#define EEPROM_SIZE 512
#define WIFI_SSID_ADDR 0
#define WIFI_PASS_ADDR 32
#define SETTINGS_ADDR 64
#define CUSTOM_TEXT_ADDR 96

// Display Settings Struktur
struct DisplaySettings {
  bool showWeekday = true;
  bool showDate = true;
  bool showTime = true;
  bool showYear = true;
  bool showTemp = true;
  bool showHumidity = true;
  bool showCustomText = false;
  bool showName = false;
  bool enableWebInterface;
  unsigned char brightness;      // *** NEU: Helligkeit 0-255 ***
  bool autoBrightness;          // *** NEU: Automatische Helligkeit ein/aus ***
  unsigned long lastNtpUpdate = 0;
};

DisplaySettings displaySettings;
char customText[64] = "";
char savedSSID[32] = "Dialog WLAN 9ABC";
char savedPassword[32] = "05156476454093850903";



DHT_Unified dht(DHTPIN, DHTTYPE);  ///<create object from class DHT_Unified in DHT_U.h
uint32_t delayMS;                  ///<delaytime from DHT22


struct RGB {
  unsigned char r;
  unsigned char g;
  unsigned char b;
  bool operator==(const RGB &other) {
    return (other.r == this->r) & (other.g == this->g) & (other.b == this->b);
  }
};

RGB matrix[MATRIX_DIMENSION_X][MATRIX_DIMENSION_Y];  ///< x is 0 to 19, y is 0 to 6

/** @brief Prints color r, g, b on position n when color is set
*
* If any argument is invalid, the function has no effect.
*
* @param copyBlack The Value if the color is disabled.
* @param n The position of the pixel.
* @param r The intensity from green light is from 0 to 255.
* @param g The intensity from red light is from 0 to 255.
* @param b The intensity from blue light is from 0 to 255.
* @return void
* @addtogroup displayMatrix
*/
void overrideColorPixels(bool copy_black, uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
  if (copy_black) {
    pixels.setPixelColor(n, r, g, b);
  } else {
    if (r > 0 && b > 0 && g > 0) {
      pixels.setPixelColor(n, r, g, b);
    }
  }
}

/** @brief calculates the zeroth row
*
* @param copy_black The unused pixels
* @return void
* @addtogroup displayMatrix
*
*/
void rowZero(bool copy_black) {
  int startpix;
  startpix = 0;
  overrideColorPixels(copy_black, startpix, matrix[0][3].r, matrix[0][3].g, matrix[0][3].b);
}


/** @brief calculates the first row
*
* @param copy_black The unused pixels
* @return void
* @addtogroup displayMatrix
*/
void rowOne(bool copy_black) {
  int startpix;
  startpix = 1;
  for (int i = 0; i <= 4; i++) {
    overrideColorPixels(copy_black, startpix + i, matrix[1][5 - i].r, matrix[1][5 - i].g, matrix[1][5 - i].b);
  }
}


/** @brief calculates the last row
*
* @param copy_black The unused pixels
* @return void
* @addtogroup displayMatrix
*/
void lastRow(bool copy_black) {
  int startpix;
  startpix = 125;
  for (int i = 0; i <= 2; i++) {
    overrideColorPixels(copy_black, startpix + i, matrix[19][4 - i].r, matrix[19][4 - i].g, matrix[19][4 - i].b);
  }
}

/** @brief calculates the non inverted row
*
* @param row_nmb The number of the row
* @param copy_black The unused pixels
* @return void
* @addtogroup displayMatrix
*/
void copyRow(short row_nmb, bool copy_black) {
  int startpix;
  // 2 --> 6
  // 4 --> 20
  // 6 --> 34
  startpix = (row_nmb - 2) * 7 + 6;
  for (int i = 0; i <= 6; i++) {
    overrideColorPixels(copy_black, startpix + i, matrix[row_nmb][i].r, matrix[row_nmb][i].g, matrix[row_nmb][i].b);
  }
}

/** @brief calculates the inverted rows
*
* @param row_nmb The number of the row
* @param copy_black The unused pixels
* @return void
* @addtogroup displayMatrix
*/
void copyInvRow(short row_nmb, bool copy_black) {
  int startpix;
  // 3 --> 13
  // 5 --> 27
  // 7 --> 41
  startpix = (row_nmb - 2) * 7 + 6;

  for (int i = 0; i <= 6; i++) {
    overrideColorPixels(copy_black, startpix + i, matrix[row_nmb][6 - i].r, matrix[row_nmb][6 - i].g, matrix[row_nmb][6 - i].b);
  }
}

/** mapping the matrix to the real layout
*
* @param copy_black The unused pixels
* @return void
* @addtogroup displayMatrix
*/
void displ(bool copy_black) {
  for (int i = 0; i <= 19; i++) {
    if (i % 2 == 0 && i <= 18 && i >= 2) {
      copyRow(i, copy_black);
    } else if (i % 2 != 0 && i <= 17 && i >= 3) {
      copyInvRow(i, copy_black);
    }

    else if (i == 0) {
      rowZero(copy_black);
    }

    else if (i == 1) {
      rowOne(copy_black);
    }

    else if (i == 19) {
      lastRow(copy_black);
    }
  }
}
/** @brief
*
* @param copy_black The unused pixels
* @return void
* @addtogroup displayRainbow
*/
void writeRainbowToMatrix(unsigned char iteration) {
  unsigned char wheelPos;  ///< variable used to calculate the colors
  unsigned char r;         ///< red color value
  unsigned char g;         ///< green color value
  unsigned char b;         ///< blue color value

  // iterate over all x and y values of the matrix
  for (int x = 0; x < MATRIX_DIMENSION_X; x++) {
    for (int y = 0; y < MATRIX_DIMENSION_Y; y++) {

      //in a previous example wheelPos was used to create a rainbow effect
      //this is the first try to adapt wheelPos because here the layout of the LEDs are differently aligned
      //wheelPos = (iteration + y * MATRIX_DIMENSION_Y + x) & 255;
      //alternative approach - let's try which one looks better
      unsigned char cycleCount = iteration;
      wheelPos = (cycleCount + y + x * MATRIX_DIMENSION_Y) & 255;

      if (wheelPos < 85) {
        r = wheelPos * RAINBOW_MULTIPLIER;
        g = MAX_RAINBOW_COLOR - wheelPos * RAINBOW_MULTIPLIER;
        b = 0;

      } else if (wheelPos < 170) {
        wheelPos -= 85;
        r = MAX_RAINBOW_COLOR - wheelPos * RAINBOW_MULTIPLIER;
        g = 0;
        b = wheelPos * RAINBOW_MULTIPLIER;

      } else {
        wheelPos -= 170;
        r = 0;
        g = wheelPos * RAINBOW_MULTIPLIER;
        b = MAX_RAINBOW_COLOR - wheelPos * RAINBOW_MULTIPLIER;
      }
      //write the r, g, b values to the matrix according the current x and y value
      matrix[x][y] = { r, g, b };
    }
  }
}

/** @brief
* Color all pixels in the matrix in one Color
*
* @param r The red light intensity from 0-255
* @param g The green light intensity from 0-255
* @param b The blue light intensity from 0-255
* @return void
* @addtogroup application
*/
void fillMatrix(unsigned char r, unsigned char g, unsigned char b) {
  for (int x = 0; x < 20; x++) {
    for (int y = 0; y < 7; y++) {
      matrix[x][y] = { r, g, b };
    }
  }
  displ(true);
  pixels.show();
}

/** @brief
* printing column after column in a color and
* then print row after row in a differen color
* @return void
*
* @addtogroup application
*/
void testMatrix(unsigned char r1, unsigned char g1, unsigned char b1, unsigned char r2, unsigned char g2, unsigned char b2) {
  for (int x = 0; x < 20; x++) {
    for (int y = 0; y < 7; y++) {
      matrix[x][y] = { r1, g1, b1 };
    }
    displ(true);
    pixels.show();
    delay(500);
  }
  delay(1000);

  for (int y = 0; y < 7; y++) {
    for (int x = 0; x < 20; x++) {
      matrix[x][y] = { r2, g2, b2 };
    }
    displ(true);
    pixels.show();
    delay(500);
  }
}

/** @brief
* @param c The character you choose from "italic5x5_font.h"
* @param startColumn The column where the character starts
* @param startBaseline The row where the character starts
* @param r The red light intensity from 0-255
* @param g The green light intensity from 0-255
* @param b The blue light intensity from 0-255
* @return void
*
* @addtogroup displayCharacter
*/
void writeCharToMatrix(unsigned char c, int startColumn, unsigned char startBaseline, unsigned char r, unsigned char g, unsigned char b) {
  int x;
  int y;
  int linecount;
  unsigned char bitvalue;
  linecount = 0;
  x = startColumn;
  y = startBaseline;
  if (c == ',') {
    y = 0;
  }
  for (; x <= startColumn + 4; x++) {
    if (x < MATRIX_DIMENSION_X && x >= 0) {
      if (startColumn < 0) {
        linecount = 0;
        linecount = (linecount - startColumn) + x;
        bitvalue = font[c][linecount];
        y = startBaseline;
        for (; y <= MATRIX_DIMENSION_Y; y++) {
          if (bitvalue % 2) {
            matrix[x][y] = { r, g, b };
          }
          bitvalue = bitvalue >> 1;
        }

      } else {
        bitvalue = font[c][linecount];
        y = startBaseline;
        for (; y <= MATRIX_DIMENSION_Y; y++) {
          if (bitvalue % 2) {
            matrix[x][y] = { r, g, b };
          }
          bitvalue = bitvalue >> 1;
        }
        linecount++;
      }
    }
  }
}

void writeCharToMatrix(unsigned char c, int startColumn) {
  unsigned char startBaseline = 1;
  unsigned char r= 120, g = 120, b = 120;
  int x;
  int y;
  int linecount;
  unsigned char bitvalue;
  linecount = 0;
  x = startColumn;
  y = startBaseline;
  for (; x <= startColumn + 4; x++) {
    if (x < MATRIX_DIMENSION_X) {
      if (startColumn < 0) {
        linecount -= x;
        bitvalue = font[c][linecount];
        y = startBaseline;
        for (; y <= MATRIX_DIMENSION_Y; y++) {
          if (bitvalue % 2) {
            matrix[x + linecount][y] = { r, g, b };
          }
          bitvalue = bitvalue >> 1;
        }
        linecount++;
      } else {
        bitvalue = font[c][linecount];
        y = startBaseline;
        for (; y <= MATRIX_DIMENSION_Y; y++) {
          if (bitvalue % 2) {
            matrix[x][y] = { r, g, b };
          }
          bitvalue = bitvalue >> 1;
        }
        linecount++;
      }
    }
  }
}



/** @brief
* printig the clock time to the matrix with Rainbow background
* @return void
*
* @addtogroup application
*/
void RTCToMatrix(int *colorIterator, unsigned char runs, char brightness, int time_ms) {
  char CR = brightness;
  char CG = brightness;
  char CB = brightness;

  int iteration = *colorIterator;
  unsigned char hour_tenth;
  unsigned char hour_ones;
  unsigned char min_tenth;
  unsigned char min_ones;

  for (; (*colorIterator) < (256 * runs) + iteration; (*colorIterator)++) {  //for (int colorIterator = 0; colorIterator < 256 * runs; colorIterator++) {
    handleWebServerDuringAnimation();

    DS3231_get(&t);  //get the value and pass to the function the pointer to t, that make an lower memory fingerprint and faster execution than use return
    //DS3231_get() will use the pointer of t to directly change t value (faster, lower memory used)

    //Clock Time
    hour_tenth = t.hour / 10;
    hour_ones = t.hour % 10;
    min_tenth = t.min / 10;
    min_ones = t.min % 10;

    if (*colorIterator % 64 >= 32) {
      writeRainbowToMatrix(*colorIterator);

      writeCharToMatrix(hour_tenth + _atoz, 2, 1, CR, CG, CB);
      writeCharToMatrix(hour_ones + _atoz, 6, 1, CR, CG, CB);
      writeCharToMatrix(min_tenth + _atoz, 12, 1, CR, CG, CB);
      writeCharToMatrix(min_ones + _atoz, 16, 1, CR, CG, CB);
      writeCharToMatrix(58, 10, 1, CR, CG, CB);
    } else {
      writeRainbowToMatrix(*colorIterator);
      writeCharToMatrix(hour_tenth + _atoz, 2, 1, CR, CG, CB);
      writeCharToMatrix(hour_ones + _atoz, 6, 1, CR, CG, CB);
      writeCharToMatrix(min_tenth + _atoz, 12, 1, CR, CG, CB);
      writeCharToMatrix(min_ones + _atoz, 16, 1, CR, CG, CB);
    }
    displ(true);
    pixels.setBrightness(brightness);
    pixels.show();
    delay(time_ms);
  }
  *colorIterator -= (256 * runs);
}





/** @brief
* printig the date to the matrix with Rainbow background
* @return void
*
* @addtogroup application
*/
void datetime(int *colorIterator, unsigned char runs, unsigned char brightness, int time_ms) {
  unsigned char CR = brightness;
  unsigned char CG = brightness;
  unsigned char CB = brightness;

  int iteration = *colorIterator;
  int day_tenth;
  int day_ones;
  int month_tenth;
  int month_ones;

  for (; (*colorIterator) < (256 * runs) + iteration; (*colorIterator)++) {  //for (int colorIterator = 0; colorIterator < 256 * runs; colorIterator++)
    handleWebServerDuringAnimation();
    DS3231_get(&t);  //get the value and pass to the function the pointer to t, that make an lower memory fingerprint and faster execution than use return
    //DS3231_get() will use the pointer of t to directly change t value (faster, lower memory used)
    //Date Time
    day_tenth = t.mday / 10;
    day_ones = t.mday % 10;
    month_tenth = t.mon / 10;
    month_ones = t.mon % 10;


    if (*colorIterator % 64 >= 32) {
      writeRainbowToMatrix(*colorIterator);

      writeCharToMatrix(day_tenth + _atoz, 2, 1, CR, CG, CB);
      writeCharToMatrix(day_ones + _atoz, 6, 1, CR, CG, CB);
      writeCharToMatrix(month_tenth + _atoz, 11, 1, CR, CG, CB);
      writeCharToMatrix(month_ones + _atoz, 15, 1, CR, CG, CB);
      writeCharToMatrix(46, 9, 1, CR, CG, CB);
      writeCharToMatrix(46, 18, 1, CR, CG, CB);
    } else {
      writeRainbowToMatrix(*colorIterator);
      writeCharToMatrix(day_tenth + _atoz, 2, 1, CR, CG, CB);
      writeCharToMatrix(day_ones + _atoz, 6, 1, CR, CG, CB);
      writeCharToMatrix(month_tenth + _atoz, 11, 1, CR, CG, CB);
      writeCharToMatrix(month_ones + _atoz, 15, 1, CR, CG, CB);
    }

    displ(true);
    pixels.setBrightness(brightness);
    pixels.show();
    delay(time_ms);
  }
  *colorIterator -= (256 * runs);
}

void showYear(int *colorIterator, unsigned char runs, unsigned char brightness, int time_ms) {
  unsigned char CR = brightness;
  unsigned char CG = brightness;
  unsigned char CB = brightness;

  int iteration = *colorIterator;
  int year_tenth;
  int year_ones;
  int year_hun;
  int month_thou;

  for (; (*colorIterator) < (256 * runs) + iteration; (*colorIterator)++) {
    handleWebServerDuringAnimation();
    DS3231_get(&t);  //get the value and pass to the function the pointer to t, that make an lower memory fingerprint and faster execution than use return
    //DS3231_get() will use the pointer of t to directly change t value (faster, lower memory used)
    //Date Time
    month_thou = t.year / 1000;
    year_hun = (t.year % 1000) / 100;
    year_ones = t.year % 10;
    year_tenth = (t.year % 100) / 10;

    writeRainbowToMatrix(*colorIterator);

    writeCharToMatrix(month_thou + _atoz, 3, 1, CR, CG, CB);
    writeCharToMatrix(year_hun + _atoz, 7, 1, CR, CG, CB);
    writeCharToMatrix(year_tenth + _atoz, 11, 1, CR, CG, CB);
    writeCharToMatrix(year_ones + _atoz, 15, 1, CR, CG, CB);

    displ(true);
    pixels.setBrightness(brightness);
    pixels.show();
    delay(time_ms);
  }
  *colorIterator -= (256 * runs);
}

/*int countCharLenght(char weekday) {
  int count = 0;
  for (int i = 0; i <= 4; i++) {

    if (font[(int)weekday][i] != 0x00) {
      count++;
    }
  }
  //printf("%c: %d\n",weekday, count);
  if (count == 5) {
    return count + 1;
  } else {
    return count;
  }

  //delay(7000);
}
*/
int lastCharRow(char c, int cursor, unsigned char r, unsigned char g, unsigned char b) {
  RGB value = { r, g, b };

  if (c != ' ') {
    for (int y = 1; y <= 5; y++) {

      switch (y) {
        case 1:
          if (matrix[cursor][y] == value && matrix[cursor - 1][y] == value) {
            char text[50];
            sprintf(text, "r,g,b = %d;%d;%d; %d", r, g, b, cursor + 1);
            Serial.println(text);
            return cursor + 1;
          }
          break;
        case 2:
          if (matrix[cursor][y] == value && (matrix[cursor - 1][y - 1] == value || matrix[cursor - 1][y - 1] == value || matrix[cursor - 1][y + 1] == value)) {
            char text[50];
            sprintf(text, "r,g,b = %d;%d;%d; %d", r, g, b, cursor + 1);
            Serial.println(text);
            return cursor + 1;
          }
          break;
        case 3:
          if (matrix[cursor][y] == value && matrix[cursor - 1][y] == value) {
            char text[50];
            sprintf(text, "r,g,b = %d;%d;%d; %d", r, g, b, cursor + 1);
            Serial.println(text);
            return cursor + 1;
          }
          break;
        case 4:
          if (matrix[cursor][y] == value && (matrix[cursor - 1][y - 1] == value || matrix[cursor - 1][y - 1] == value || matrix[cursor - 1][y + 1] == value)) {
            char text[50];
            sprintf(text, "r,g,b = %d;%d;%d; %d", r, g, b, cursor + 1);
            Serial.println(text);
            return cursor + 1;
          }
          break;
        case 5:
          if (matrix[cursor][y] == value && matrix[cursor - 1][y] == value) {
            char text[50];
            sprintf(text, "r,g,b = %d;%d;%d; %d", r, g, b, cursor + 1);
            Serial.println(text);
            return cursor + 1;
          }
          if (matrix[cursor][y] == matrix[cursor - 1][y]) {
            return cursor + 1;
          }
          break;
        default:
          if (y == 5) {
            return cursor;
          }
          break;
      }
    }
  }
  return cursor + 3;
}


void weekDays(int *colorIterator, unsigned char runs, unsigned char brightness, int time_ms) {
  int week_day;
  int iterator = *colorIterator;
  week_day = t.wday;
  int x = 5;

  for (; *colorIterator < (256 * runs) + iterator; (*colorIterator)++) {
    handleWebServerDuringAnimation();
    writeRainbowToMatrix(*colorIterator);

    writeStringToMatrix(wday[week_day], x, brightness);
    displ(true);
    pixels.show();
    delay(time_ms);
  }
  *colorIterator -= (256 * runs);
}
/** @brief
* printig the humidity time to the matrix with Rainbow background
* @return void
*
* @addtogroup application
*/
void humidity(int *colorIterator, unsigned char runs, unsigned char brightness, int time_ms) {
  unsigned char CR = brightness;
  unsigned char CG = brightness;
  unsigned char CB = brightness;

  int iteration = *colorIterator;
  int hum_tenth;
  int hum_ones;

  for (; (*colorIterator) < (256 * runs) + iteration; (*colorIterator)++) {  //for (int colorIterator = 0; colorIterator < 256 * runs; colorIterator++) {
    // Get temperature event and print its value.
    sensors_event_t event;
    handleWebServerDuringAnimation();
    writeRainbowToMatrix(*colorIterator);

    dht.humidity().getEvent(&event);
    //humidity Digits
    if (isnan(event.relative_humidity)) {
    } else {
      hum_tenth = (int)event.relative_humidity / 10;
      hum_ones = (int)event.relative_humidity % 10;
      //delay(delayMS);
      writeRainbowToMatrix(*colorIterator);
      writeCharToMatrix(hum_tenth + _atoz, 4, 1, CR, CG, CB);
      writeCharToMatrix(hum_ones + _atoz, 8, 1, CR, CG, CB);
      writeCharToMatrix('%', 13, 1, CR, CG, CB);
    }
    displ(true);
    pixels.show();
    pixels.setBrightness(brightness);
    delay(time_ms);
  }
  *colorIterator -= (256 * runs);
}
/** @brief
* printig the temperatures to the matrix with Rainbow background
* @return void
*
* @addtogroup application 
*/
void printTemperature(int *colorIterator, unsigned char runs, unsigned char brightness, int time_ms) {
  unsigned char CR = brightness;
  unsigned char CG = brightness;
  unsigned char CB = brightness;

  int iterator = *colorIterator;
  int temp_tenth;
  int temp_ones;

  for (; *colorIterator < (256 * runs) + iterator; (*colorIterator)++) {
    // Get temperature event and print its value.
    sensors_event_t event;
    handleWebServerDuringAnimation();
    writeRainbowToMatrix(*colorIterator);
    dht.temperature().getEvent(&event);
    if (isnan(event.temperature)) {
    } else {
      temp_tenth = (int)event.temperature / 10;
      temp_ones = (int)event.temperature % 10;
      writeCharToMatrix(temp_tenth + _atoz, 2, 1, CR, CG, CB);
      writeCharToMatrix(temp_ones + _atoz, 6, 1, CR, CG, CB);
      writeCharToMatrix(1, 10, 1, CR, CG, CB);
      writeCharToMatrix('C', 13, 1, CR, CG, CB);
    }
    displ(true);
    pixels.setBrightness(brightness);
    pixels.show();
    delay(time_ms);
  }
  *colorIterator -= (256 * runs);
}

/** @brief
* printig a text which moves from right to left
* @return void
*
* @addtogroup application
*/

int stringLength(char c[]) {
  int count = 0;
  for (int i = 0; c[i] != '\0'; i++) {
    count++;
  }
  Serial.printf("stringLength:");
  Serial.printf("%d\n\n", count);
  return count;
}

int charLength(char c[]) {
  int count = 0;
  for (int i = 0; c[i] != '\0'; i++) {
    count += lengthOfEveryChar(c[i]);
  }
  Serial.printf("charLength:");

  Serial.printf("%d\n\n", count);
  return count;
}

int touch(char c, char last_char, int index_last_char_length) {
  if (c == ' ' || last_char == ' ') {
    return 0;
  }
  if ((font[(int)c][0] & font[(int)last_char][index_last_char_length]) == 0b000000) {
    if ((font[(int)c][0] & font[(int)last_char][index_last_char_length - 1]) == 0b00000) {
      if((font[(int)c][1] & font[(int)last_char][index_last_char_length]) == 0b000000) {
        return -1;
      }
      else
      {
        return 0;
      }      
    } 
    else 
    {
      return 0;
    }
  } else {
    return 1;
  }
}

int lengthOfEveryChar(char c) {
  int count = 0;
  for (int i = 0; i <= 4; i++) {
    if (c == ' ') {
      return 3;
    } else if (font[(int)c][i] != 0x00) {
      count++;
    }
  }
  // Serial.printf("lengthOfEveryChar:");
  // Serial.printf(" %c::", c);
  // Serial.printf("%d\n\n", count);
  return count;
}

int lastRowOfChar(char c, char last_char) {
  if (lengthOfEveryChar(c) == 1) {

    if (font[(int)last_char][4] != 0x00)
      return 1;
  } else if (lengthOfEveryChar(c) == ' ') {
    return 0;
  }
  return 0;
}

void writeStringToMatrix(char CAPSLK_text[], int x, unsigned char brightness) {
  unsigned char CR = brightness;
  unsigned char CG = brightness;
  unsigned char CB = brightness;
  int k = x;
  int charcount = 0;
  int index_last_char_lenght = 0;
  writeCharToMatrix(CAPSLK_text[0], k, 1, CR, CG, CB);
  k = k + lengthOfEveryChar(CAPSLK_text[0]);
  Serial.print(CAPSLK_text[charcount]);
  Serial.print(lengthOfEveryChar(CAPSLK_text[0]));
  Serial.print("\nx = ");
  Serial.print(x);
  charcount++;
  for (int i = 1; i <= (stringLength(CAPSLK_text)); i++) {
    index_last_char_lenght = lengthOfEveryChar(CAPSLK_text[i - 1]) - 1;
    Serial.print(index_last_char_lenght);
    int tch = touch(CAPSLK_text[i], CAPSLK_text[i - 1], index_last_char_lenght);
    Serial.print("\ntouch: ");
    Serial.print(tch);
    Serial.print("\n k = ");
    k = k + tch;
    Serial.print(k);

    Serial.print("\n");
    writeCharToMatrix(CAPSLK_text[i], k, 1, CR, CG, CB);
    k = k + lengthOfEveryChar(CAPSLK_text[i]);  //lastRowOfChar(CAPSLK_text[i], last_char,last_char_lenght, cursor)


    Serial.print(CAPSLK_text[charcount]);
    Serial.print(" : ");
    Serial.print(i);
    Serial.print("\n");
    Serial.print("x = ");
    Serial.print(x);
    Serial.print("\n");
    Serial.print("last_char_length:");
    Serial.print(index_last_char_lenght);
    Serial.print("\n\n");
    charcount++;
  }
}

void shiftTextV3(char CAPSLK_text[], int *colorIterator, unsigned char brightness, int time_ms) {
  int x;
  char lastChar = '0';
  int charCount;
  x = 18;
  int txtRowLen = charLength(CAPSLK_text) * -1;
  for (charCount = 0; lastChar == '\n'; charCount++) {
    lastChar = CAPSLK_text[charCount];
  }
  for (*colorIterator = 0; x > (txtRowLen); (*colorIterator)++) {
   
    handleWebServerDuringAnimation();
    writeRainbowToMatrix(*colorIterator);
    if (*colorIterator % 3 == 0) {
      x--;
    }
    writeStringToMatrix(CAPSLK_text, x, brightness);
    displ(true);
    pixels.setBrightness(brightness);
    pixels.show();
    delay(time_ms);
  }
  (*colorIterator) = (*colorIterator) % 256;
}

void handleWebServerDuringAnimation() {
  server.handleClient();
  if(isAPMode) {
    dnsServer.processNextRequest();
  }
  yield(); // Gibt ESP8266 Zeit für interne Tasks
}


// WiFi Management Funktionen
void saveWiFiCredentials(const char* ssid, const char* password) {
  EEPROM.begin(EEPROM_SIZE);
  
  for(int i = 0; i < 32; i++) {
    EEPROM.write(WIFI_SSID_ADDR + i, i < strlen(ssid) ? ssid[i] : 0);
    EEPROM.write(WIFI_PASS_ADDR + i, i < strlen(password) ? password[i] : 0);
  }
  
  EEPROM.commit();
  EEPROM.end();
}

// ERGÄNZT: loadWiFiCredentials und loadSettings mit EEPROM-Check
void loadWiFiCredentials() {
  EEPROM.begin(EEPROM_SIZE);
  
  // Prüfe ob EEPROM initialisiert ist
  if(EEPROM.read(0) == 255 && EEPROM.read(1) == 255) {
    // EEPROM ist leer - setze Standardwerte
    strcpy(savedSSID, "Dialog WLAN 9ABC");
    strcpy(savedPassword, "05156476454093850903");
    saveWiFiCredentials(savedSSID, savedPassword);
  } else {
    for(int i = 0; i < 32; i++) {
      savedSSID[i] = EEPROM.read(WIFI_SSID_ADDR + i);
      savedPassword[i] = EEPROM.read(WIFI_PASS_ADDR + i);
    }
  }
  
  EEPROM.end();
  Serial.printf("WiFi Daten geladen: SSID=%s\n", savedSSID);
}

void saveSettings() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(SETTINGS_ADDR, displaySettings);
  
  for(int i = 0; i < 64; i++) {
    EEPROM.write(CUSTOM_TEXT_ADDR + i, customText[i]);
  }
  
  EEPROM.commit();
  EEPROM.end();
}

void loadSettings() {
  EEPROM.begin(EEPROM_SIZE);
  
  // Prüfe ob Settings existieren
  if(EEPROM.read(SETTINGS_ADDR) == 255) {
    // Setze Standardwerte
    displaySettings.showWeekday = true;
    displaySettings.showDate = true;
    displaySettings.showTime = true;
    displaySettings.showYear = true;
    displaySettings.showTemp = true;
    displaySettings.showHumidity = true;
    displaySettings.showCustomText = false;
    displaySettings.showName = false;
    displaySettings.enableWebInterface = true;
    displaySettings.brightness = 200;        // *** NEU: Standard Helligkeit ***
    displaySettings.autoBrightness = true;   // *** NEU: Auto-Helligkeit AN ***
    displaySettings.lastNtpUpdate = 0;
    strcpy(customText, "");
    saveSettings();
  } else {
    EEPROM.get(SETTINGS_ADDR, displaySettings);
    
    for(int i = 0; i < 64; i++) {
      customText[i] = EEPROM.read(CUSTOM_TEXT_ADDR + i);
    }
  }
  
  EEPROM.end();
  Serial.println("Display Settings geladen");
}


// Text Validierung und Konvertierung
void validateAndConvertText(char* text) {
  int writePos = 0;
  for(int i = 0; text[i] != '\0' && writePos < 63; i++) {
    char c = text[i];
    
    // Kleinbuchstaben zu Großbuchstaben
    if(c >= 'a' && c <= 'z') {
      c = c - 32;
    }
    
    // Erlaubte Zeichen: A-Z, 0-9, Leerzeichen, . , : ! ?
    if((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || 
       c == ' ' || c == '.' || c == ',' || c == ':' || c == '!' || c == '?') {
      text[writePos++] = c;
    }
  }
  text[writePos] = '\0';
}

// WiFi Connection Management
// ERGÄNZT: Bessere WiFi Verbindungsfunktion
// GEÄNDERT: Erweitert um automatische AP-Umschaltung und WiFi-Trennung
// **KOMPLETT GEÄNDERT** - WiFi-Verbindung mit korrekter Trennung
// **KOMPLETT GEÄNDERT** - WiFi-Verbindung mit besserer Trennung
bool connectToWiFiWithSync() {
  if(strlen(savedSSID) == 0) {
    Serial.println("Keine SSID gespeichert");
    return false;
  }
  
  Serial.println("=== Starte WiFi Synchronisation ===");
  
  // **GEÄNDERT**: AP sauber stoppen
  if(isAPMode) {
    Serial.println("Stoppe AP für WiFi-Verbindung...");
    dnsServer.stop();
    delay(500);
    WiFi.softAPdisconnect(true);
    delay(1000);
    WiFi.mode(WIFI_OFF);
    delay(2000);  // **GEÄNDERT**: Längere Wartezeit
    isAPMode = false;
  }
  
  // **GEÄNDERT**: STA-Modus mit Wartezeit
  WiFi.mode(WIFI_STA);
  delay(1000);
  
  Serial.printf("Verbinde mit WiFi: %s\n", savedSSID);
  WiFi.begin(savedSSID, savedPassword);
  
  // **GEÄNDERT**: Erweiterte Verbindungsüberwachung
  int attempts = 0;
  while(WiFi.status() != WL_CONNECTED && attempts < 40) {  // **GEÄNDERT**: Mehr Versuche
    delay(500);
    Serial.print(".");
    attempts++;
    
    // **NEU**: Status-Check alle 10 Versuche
    if(attempts % 10 == 0) {
      Serial.printf("\nWiFi Status: %d (Versuch %d/40)\n", WiFi.status(), attempts);
    }
  }
  
  bool success = false;
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.printf("WiFi verbunden! IP: %s\n", WiFi.localIP().toString().c_str());
    
    // **GEÄNDERT**: NTP mit Wartezeit
    delay(1000);
    success = syncTimeFromNTP();
    Serial.printf("NTP Sync: %s\n", success ? "erfolgreich" : "fehlgeschlagen");
  } else {
    Serial.println("\nWiFi Verbindung fehlgeschlagen!");
  }
  
  // **GEÄNDERT**: Saubere WiFi-Trennung
  Serial.println("Trenne WiFi...");
  WiFi.disconnect();
  delay(1000);
  WiFi.mode(WIFI_OFF);
  delay(2000);
  
  // **GEÄNDERT**: AP mit Verzögerung neu starten
  delay(1000);
  startAccessPoint();
  
  Serial.println("=== WiFi Synchronisation beendet ===");
  return success;
}

// ERGÄNZT: Bessere Access Point Funktion
// **KOMPLETT GEÄNDERT** - Access Point Funktion
// **KOMPLETT GEÄNDERT** - Access Point Funktion mit stabilerer Konfiguration
// **KOMPLETT GEÄNDERT** - Access Point Funktion mit stabilerer Konfiguration
void startAccessPoint() {
  Serial.println("Starte Access Point...");
  
  // **NEU**: WiFi komplett stoppen und zurücksetzen
  WiFi.softAPdisconnect(true);
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(2000);  // **GEÄNDERT**: Längere Wartezeit für vollständigen Reset
  
  // **GEÄNDERT**: AP-Modus setzen und warten
  WiFi.mode(WIFI_AP);
  delay(1000);  // **GEÄNDERT**: Wartezeit nach Modus-Set
  
  // **GEÄNDERT**: AP mit einfacheren Parametern starten
  Serial.println("Konfiguriere Access Point...");
  bool apStarted = WiFi.softAP("SmartDisplay-Config");  // **VEREINFACHT**: Ohne zusätzliche Parameter
  
  if(!apStarted) {
    Serial.println("AP Start fehlgeschlagen - Retry...");
    delay(2000);
    WiFi.mode(WIFI_OFF);
    delay(1000);
    WiFi.mode(WIFI_AP);
    delay(1000);
    WiFi.softAP("SmartDisplay-Config");
  }
  
  // **GEÄNDERT**: Längere Wartezeit vor IP-Konfiguration
  delay(2000);
  
  // **GEÄNDERT**: Einfachere IP-Konfiguration
  IPAddress local_IP(192, 168, 4, 1);  // **GEÄNDERT**: Standard AP-Subnet
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  
  if(!WiFi.softAPConfig(local_IP, gateway, subnet)) {
    Serial.println("AP IP-Konfiguration fehlgeschlagen - verwende Standard");
  }
  
  // **GEÄNDERT**: Längere Wartezeit vor DNS-Server Start
  delay(1000);
  
  // **GEÄNDERT**: DNS-Server mit Fehlerbehandlung
  dnsServer.stop();
  delay(500);
  if(dnsServer.start(53, "*", local_IP)) {
    Serial.println("DNS Server gestartet");
  } else {
    Serial.println("DNS Server Start fehlgeschlagen");
  }
  
  // **NEU**: Warten bis AP wirklich läuft
  int retries = 10;
  while(WiFi.softAPIP().toString() == "0.0.0.0" && retries > 0) {
    delay(500);
    retries--;
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("=== Access Point Status ===");
  Serial.printf("SSID: SmartDisplay-Config\n");
  Serial.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("Gateway: %s\n", gateway.toString().c_str());
  Serial.println("===========================");
  
  isAPMode = true;
}

// Web Server Handlers
// GEÄNDERT: Entfernt WiFi Status Check, da immer im AP-Modus
// 3. ERWEITERE handleRoot() für den Helligkeits-Slider:
void handleRoot() {
  displaySettings.enableWebInterface = true;
  if(!displaySettings.enableWebInterface) {
    server.send(403, "text/html", 
      "<!DOCTYPE html><html><head><title>Deaktiviert</title></head>"
      "<body><h1>Web Interface ist deaktiviert</h1></body></html>");
    return;
  }
  
  String html = "<!DOCTYPE html><html><head><title>SmartDisplay</title>";
  html += "<meta charset='UTF-8'>";
  html += "<style>";
  html += "body{font-family:Arial;margin:20px;}";
  html += "input,select{width:100%;padding:8px;margin:5px 0;}";
  html += "button{background:#4CAF50;color:white;padding:10px;border:none;cursor:pointer;}";
  html += ".slider-container{margin:15px 0;}";
  html += ".slider{width:100%;height:25px;background:#ddd;outline:none;border-radius:15px;}";
  html += ".slider::-webkit-slider-thumb{appearance:none;width:25px;height:25px;background:#4CAF50;cursor:pointer;border-radius:50%;}";
  html += ".brightness-value{font-size:18px;font-weight:bold;color:#333;margin-left:10px;}";
  html += "</style>";
  
  // *** NEU: JavaScript für Live-Slider Update ***
  html += "<script>";
  html += "function updateBrightness(val) {";
  html += "  document.getElementById('brightnessValue').innerHTML = val;";
  html += "  document.getElementById('brightnessHidden').value = val;";
  html += "}";
  html += "function toggleAutoBrightness() {";
  html += "  var auto = document.getElementById('autobrightness').checked;";
  html += "  var slider = document.getElementById('brightnessSlider');";
  html += "  slider.disabled = auto;";
  html += "  slider.style.opacity = auto ? '0.5' : '1';";
  html += "}";
  html += "</script>";
  
  html += "</head><body>";
  html += "<h1>SmartDisplay Config</h1>";
  
  // WiFi Section bleibt gleich...
  html += "<h2>WiFi Sync</h2>";
  html += "<form action='/wifi' method='POST'>";
  html += "<input type='text' name='ssid' placeholder='SSID' value='" + String(savedSSID) + "'>";
  html += "<input type='password' name='password' placeholder='Password'>";
  html += "<button type='submit'>Sync Time</button></form>";
  
  html += "<h2>Display Settings</h2>";
  html += "<form action='/settings' method='POST'>";
  
  // *** NEU: Helligkeits-Sektion ***
  html += "<h3>🔆 Helligkeit</h3>";
  html += "<label><input type='checkbox' id='autobrightness' name='autobrightness'";
  html += String(displaySettings.autoBrightness ? " checked" : "");
  html += " onchange='toggleAutoBrightness()'> Automatische Helligkeit (Tag/Nacht)</label><br><br>";
  
  html += "<div class='slider-container'>";
  html += "<label>Manuelle Helligkeit: <span id='brightnessValue' class='brightness-value'>" + String(displaySettings.brightness) + "</span></label><br>";
  html += "<input type='range' id='brightnessSlider' class='slider' min='10' max='255' value='" + String(displaySettings.brightness) + "'";
  html += " oninput='updateBrightness(this.value)'";
  html += String(displaySettings.autoBrightness ? " disabled style='opacity:0.5'" : "") + ">";
  html += "<input type='hidden' id='brightnessHidden' name='brightness' value='" + String(displaySettings.brightness) + "'>";
  html += "<small>Minimum: 10, Maximum: 255</small>";
  html += "</div><br>";
  
  // Bestehende Display Settings...
  html += "<h3>📺 Anzeige Inhalte</h3>";
  html += "<label><input type='checkbox' name='weekday'" + String(displaySettings.showWeekday ? " checked" : "") + "> Wochentag</label><br>";
  html += "<label><input type='checkbox' name='date'" + String(displaySettings.showDate ? " checked" : "") + "> Datum</label><br>";
  html += "<label><input type='checkbox' name='time'" + String(displaySettings.showTime ? " checked" : "") + "> Uhrzeit</label><br>";
  html += "<label><input type='checkbox' name='year'" + String(displaySettings.showYear ? " checked" : "") + "> Jahr</label><br>";
  html += "<label><input type='checkbox' name='temp'" + String(displaySettings.showTemp ? " checked" : "") + "> Temperatur</label><br>";
  html += "<label><input type='checkbox' name='humidity'" + String(displaySettings.showHumidity ? " checked" : "") + "> Luftfeuchtigkeit</label><br>";
  html += "<label><input type='checkbox' name='Name'" + String(displaySettings.showName ? " checked" : "") + "> Geräte Name</label><br>";
  html += "<label><input type='checkbox' name='customtext'" + String(displaySettings.showCustomText ? " checked" : "") + "> Eigener Text</label><br>";
  html += "<input type='text' name='text' placeholder='Eigener Text' value='" + String(customText) + "'><br><br>";
  
  // System Settings...
  html += "<h3>⚙️ System</h3>";
  html += "<label><input type='checkbox' name='webinterface'" + String(displaySettings.enableWebInterface ? " checked" : "") + "> Web Interface aktiviert</label><br>";
  html += "<small style='color:red;'>Warnung: Wenn deaktiviert, ist keine Web-Konfiguration mehr möglich!</small><br><br>";
  
  html += "<button type='submit'>Einstellungen Speichern</button></form>";
  
  // *** NEU: Aktueller Status ***
  html += "<hr><h3>📊 Status</h3>";
  html += "<p>Aktuelle Helligkeit: <strong>" + String(getCurrentBrightness()) + "</strong></p>";
  html += "<p>Modus: <strong>" + String(displaySettings.autoBrightness ? "Automatisch" : "Manuell") + "</strong></p>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

// GEÄNDERT: Führt sofort WiFi-Sync durch und kehrt zu AP zurück
// **GEÄNDERT** - Bessere Web-Handler mit Fehlerbehandlung
// **GEÄNDERT** - Web-Handler mit verbesserter Fehlerbehandlung
// **GEÄNDERT** - Web-Handler mit verbesserter Fehlerbehandlung
void handleWiFi() {
  Serial.println("WiFi Handler aufgerufen");
  
  if(!server.hasArg("ssid") || !server.hasArg("password")) {
    Serial.println("Unvollständige WiFi Daten empfangen");
    server.send(400, "text/html", 
      "<!DOCTYPE html><html><head><title>Fehler</title><meta charset='UTF-8'></head>"
      "<body><h1>Fehler: Unvollständige Daten</h1>"
      "<p>SSID und Passwort sind erforderlich.</p>"
      "<a href='/'>Zurück</a></body></html>");
    return;
  }
  
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  
  // **NEU**: Input-Validierung
  if(ssid.length() == 0) {
    server.send(400, "text/html", 
      "<!DOCTYPE html><html><head><title>Fehler</title><meta charset='UTF-8'></head>"
      "<body><h1>Fehler: SSID darf nicht leer sein</h1>"
      "<a href='/'>Zurück</a></body></html>");
    return;
  }
  
  Serial.printf("Empfangene WiFi Daten: SSID='%s'\n", ssid.c_str());
  
  // **GEÄNDERT**: Daten speichern
  ssid.toCharArray(savedSSID, 32);
  password.toCharArray(savedPassword, 32);
  
  saveWiFiCredentials(savedSSID, savedPassword);
  Serial.println("WiFi Daten gespeichert");
  
  // **GEÄNDERT**: Verbesserte Response-Seite
  String html = "<!DOCTYPE html><html><head><title>Synchronisiere...</title>";
  html += "<meta charset='UTF-8'><meta http-equiv='refresh' content='25;url=/'></head>";  // **GEÄNDERT**: Auto-Refresh
  html += "<body style='font-family:Arial;text-align:center;margin-top:50px;'>";
  html += "<h1>🔄 Synchronisiere Zeit...</h1>";
  html += "<p>Verbinde mit WiFi: <strong>" + ssid + "</strong></p>";
  html += "<p>Dies kann bis zu 30 Sekunden dauern.</p>";
  html += "<div id='status'>Bitte warten...</div>";
  html += "<script>";
  html += "var dots = 0;";
  html += "setInterval(function(){";
  html += "  dots = (dots + 1) % 4;";
  html += "  document.getElementById('status').innerHTML = 'Bitte warten' + '.'.repeat(dots);";
  html += "}, 500);";
  html += "</script></body></html>";
  
  server.send(200, "text/html", html);
  
  // **GEÄNDERT**: Sync mit Verzögerung im Hintergrund
  delay(1000);  // **GEÄNDERT**: Zeit für Browser-Response
  
  Serial.println("Starte WiFi Sync...");
  bool success = connectToWiFiWithSync();
  Serial.printf("WiFi Sync abgeschlossen: %s\n", success ? "erfolgreich" : "fehlgeschlagen");
}

// **GEÄNDERT** - Settings Handler mit besserer Validierung
// 4. ERWEITERE handleSettings() für Helligkeits-Settings:
void handleSettings() {
  Serial.println("Settings Handler aufgerufen");
  
  // Bestehende Settings...
  displaySettings.showWeekday = server.hasArg("weekday");
  displaySettings.showDate = server.hasArg("date");
  displaySettings.showTime = server.hasArg("time");
  displaySettings.showYear = server.hasArg("year");
  displaySettings.showTemp = server.hasArg("temp");
  displaySettings.showHumidity = server.hasArg("humidity");
  displaySettings.showCustomText = server.hasArg("customtext");
  displaySettings.enableWebInterface = server.hasArg("webinterface");
  
  // *** NEU: Helligkeits-Settings ***
  displaySettings.autoBrightness = server.hasArg("autobrightness");
  
  if(server.hasArg("brightness")) {
    int newBrightness = server.arg("brightness").toInt();
    if(newBrightness >= 10 && newBrightness <= 255) {
      displaySettings.brightness = (unsigned char)newBrightness;
      Serial.printf("Helligkeit gesetzt auf: %d\n", displaySettings.brightness);
    } else {
      Serial.println("Ungültige Helligkeit - verwende Standardwert");
      displaySettings.brightness = 200;
    }
  }
  
  Serial.printf("Auto-Helligkeit: %s, Manuelle Helligkeit: %d\n", 
                displaySettings.autoBrightness ? "AN" : "AUS", 
                displaySettings.brightness);
  
  // Text verarbeitung bleibt gleich...
  if(server.hasArg("text")) {
    String text = server.arg("text");
    text.toCharArray(customText, 64);
    validateAndConvertText(customText);
  }
  
  saveSettings();
  Serial.println("Settings mit Helligkeit gespeichert");
  
  // Response
  String html = "<!DOCTYPE html><html><head><title>Gespeichert</title>";
  html += "<meta charset='UTF-8'><meta http-equiv='refresh' content='3;url=/'></head>";
  html += "<body style='font-family:Arial;text-align:center;margin-top:50px;'>";
  html += "<h1>✅ Einstellungen gespeichert!</h1>";
  html += "<p>Helligkeit: <strong>" + String(getCurrentBrightness()) + "</strong></p>";
  html += "<p>Die Seite lädt automatisch neu...</p>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}


void handleNotFound() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// NTP Sync Function
// GEÄNDERT: Erweitert um Sommer-/Winterzeit Automatik
bool syncTimeFromNTP() {
  WiFiUDP ntpUDP;
  NTPClient timeClient(ntpUDP, "0.at.pool.ntp.org", 0); // UTC Zeit holen
  
  timeClient.begin();
  if(timeClient.forceUpdate()) {
    time_t epochTime = timeClient.getEpochTime();
    
    // *** NEU: Automatische Sommer-/Winterzeit Berechnung ***
    struct tm *utcTime = gmtime(&epochTime);
    int year = utcTime->tm_year + 1900;
    int month = utcTime->tm_mon + 1;
    int day = utcTime->tm_mday;
    int hour = utcTime->tm_hour;
    
    // Berechne Sommerzeit (letzter Sonntag im März bis letzter Sonntag im Oktober)
    bool isDST = false;
    
    if(month > 3 && month < 10) {
      isDST = true;  // Definitiv Sommerzeit
    } else if(month == 3) {
      // März: Prüfe ob nach letztem Sonntag
      int lastSundayMarch = 31 - ((5 * year / 4 + 4) % 7);
      if(day > lastSundayMarch || (day == lastSundayMarch && hour >= 1)) {
        isDST = true;
      }
    } else if(month == 10) {
      // Oktober: Prüfe ob vor letztem Sonntag
      int lastSundayOctober = 31 - ((5 * year / 4 + 1) % 7);
      if(day < lastSundayOctober || (day == lastSundayOctober && hour < 1)) {
        isDST = true;
      }
    }
    
    // Zeitzone anwenden: UTC+1 (Winter) oder UTC+2 (Sommer)
    int timeOffset = isDST ? 7200 : 3600;  // 2 Stunden oder 1 Stunde
    epochTime += timeOffset;
    
    struct tm *localTime = gmtime(&epochTime);
    
    // RTC Struktur füllen
    t.hour = (uint8_t)localTime->tm_hour;
    t.min = (uint8_t)localTime->tm_min;
    t.sec = (uint8_t)localTime->tm_sec;
    t.mon = (uint8_t)(localTime->tm_mon + 1);
    t.mday = (uint8_t)localTime->tm_mday;
    t.year = (uint16_t)(localTime->tm_year + 1900);
    t.wday = (uint8_t)localTime->tm_wday;
    
    // Zeit in RTC schreiben
    DS3231_set(t);
    
    displaySettings.lastNtpUpdate = millis() / 1000;
    saveSettings();
    
    Serial.println("NTP Zeit mit Sommerzeit-Automatik in RTC geschrieben:");
    Serial.printf("Zeitzone: %s\n", isDST ? "Sommerzeit (UTC+2)" : "Winterzeit (UTC+1)");
    Serial.printf("Datum: %02d.%02d.%04d\n", t.mday, t.mon, t.year);
    Serial.printf("Zeit: %02d:%02d:%02d\n", t.hour, t.min, t.sec);
    
    timeClient.end();
    return true;
  }
  
  Serial.println("NTP Sync fehlgeschlagen!");
  timeClient.end();
  return false;
}

// Modified setup() function - ADD these lines to existing setup():
// GEÄNDERT: setupWebServer mit besserer Fehlerbehandlung
// GEÄNDERT: Immer AP-Modus starten, WiFi nur für Sync verwenden
// **KOMPLETT GEÄNDERT** - Setup Web Server
// **GEÄNDERT** - Setup mit besserer Initialisierung
void setupWebServer() {
  Serial.println("=== SmartDisplay Web Server Setup ===");
  
  // **NEU**: EEPROM mit Fehlerbehandlung
  EEPROM.begin(EEPROM_SIZE);
  Serial.println("EEPROM Initialisierung fehlgeschlagen!");
    
  
  
  loadWiFiCredentials();
  loadSettings();
  
  // **GEÄNDERT**: Startup-Sync nur wenn explizit gewünscht
  Serial.println("Überspringe Start-WiFi-Sync für stabileren AP");
  
  // **GEÄNDERT**: Direkt AP starten
  startAccessPoint();
  
  // **GEÄNDERT**: Web Server mit erweiterten Routen
  server.on("/", HTTP_GET, handleRoot);
  server.on("/wifi", HTTP_POST, handleWiFi);
  server.on("/settings", HTTP_POST, handleSettings);
  
  // **NEU**: Zusätzliche Debug-Route
  server.on("/status", HTTP_GET, []() {
    String status = "AP IP: " + WiFi.softAPIP().toString();
    status += "\nClients: " + String(WiFi.softAPgetStationNum());
    status += "\nUptime: " + String(millis()/1000) + "s";
    server.send(200, "text/plain", status);
  });
  
  server.onNotFound(handleNotFound);
  
  // **GEÄNDERT**: Server mit Verzögerung starten
  delay(1000);
  server.begin();
  
  Serial.println("=== Web Server gestartet ===");
  Serial.printf("Zugriff über: http://%s\n", WiFi.softAPIP().toString().c_str());
  Serial.println("SSID: SmartDisplay-Config (ohne Passwort)");
  Serial.println("====================================");
}


// **NEU HINZUGEFÜGT** - WiFi-Modus sauber trennen
void stopWiFiClient() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(100);
  Serial.println("WiFi Client gestoppt");
}


// 5. NEU: Funktion für aktuelle Helligkeit
unsigned char getCurrentBrightness() {
  if(displaySettings.autoBrightness) {
    DS3231_get(&t);
    // Automatische Helligkeit basierend auf Uhrzeit
    if (t.hour >= 19 || t.hour < 7) {
      return 50;  // Nacht: sehr dunkel
    } else if (t.hour >= 7 && t.hour < 9) {
      return 150; // Morgen: mittel
    } else if (t.hour >= 17 && t.hour < 19) {
      return 180; // Abend: etwas heller
    } else {
      return 220; // Tag: hell
    }
  } else {
    return displaySettings.brightness; // Manuelle Helligkeit
  }
}



/** @brief 
* startup function
* @return void
* @addtogroup mainFunction 
* 
*/
void setup() {

  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  dht.humidity().getSensor(&sensor);

  // END of Trinket-specific code.
  Wire.begin();               //start i2c (required for connection)
  //DS3231_init(DS3231_INTCN);  //register the ds3231 (DS3231_INTCN is the default address of ds3231, this is set by macro for no performance loss)

  pixels.begin();  // INITIALIZE NeoPixel strip object (REQUIRED)

  dht.begin();

  Serial.begin(115200);


   setupWebServer();
  
  // *** ERGÄNZT: Automatischer NTP Sync beim Start wenn WiFi verfügbar ***
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi verbunden - starte NTP Sync...");
    if(syncTimeFromNTP()) {
      Serial.println("NTP Sync beim Start erfolgreich");
    } else {
      Serial.println("NTP Sync beim Start fehlgeschlagen");
    }
  }
  
  delayMS = 1000; // Sensor delay
}


/** @brief
*  loop function which is used after the starup function
* @return void
* @addtogroup mainFunction
*
*/
void loop() {
  // pixels.clear(); // Set all pixel colors to 'off'

  // The first NeoPixel in a strand is #0, second is 1, all the way up
  // to the count of pixels minus one.

  //dateAndClock();
  //fillMatrix(100, 0, 10); delay(3000);
  //testMatrix(0,50,100,10,100,10);

  unsigned char brightness = 250;
  int runs = 1;
  int colorIterator = 0;
  int ms = 30;

  static unsigned long lastWebServerCheck = 0;
  static unsigned long lastStatusPrint = 0;
  static unsigned long lastClientCheck = 0;  // **NEU**: Client-Überwachung
  
    // **GEÄNDERT**: Web Server häufiger prüfen für bessere Responsivität
  if(millis() - lastWebServerCheck > 5) {  // **GEÄNDERT**: Von 10ms auf 5ms
    server.handleClient();
    if(isAPMode) {  // **NEU**: DNS nur im AP-Modus
      dnsServer.processNextRequest();
    }
    lastWebServerCheck = millis();
  }
  
  // **NEU**: Client-Status regelmäßig prüfen
  if(millis() - lastClientCheck > 5000) {
    int clients = WiFi.softAPgetStationNum();
    if(clients > 0) {
      Serial.printf("AP aktiv - %d Client(s) verbunden\n", clients);
    }
    lastClientCheck = millis();
  }
  
  // **GEÄNDERT**: Status-Print seltener
  if(millis() - lastStatusPrint > 60000) {  // **GEÄNDERT**: Alle 60 Sekunden
    Serial.printf("System Status - Uptime: %lu min, AP-IP: %s\n", 
                  millis()/60000, WiFi.softAPIP().toString().c_str());
    lastStatusPrint = millis();
  }

  DS3231_get(&t);
  
  // Brightness adjustment
  brightness = getCurrentBrightness();
  
  // Special occasions
  if(t.mon == 12 && (t.mday == 24 || t.mday == 25)) {
    shiftTextV3("FROHE WEIHNACHTEN\0", &colorIterator, brightness, ms);
  }
  else if(t.mon == 1 && t.mday == 1) {
    shiftTextV3("NEUJAHR\0", &colorIterator, brightness, ms);
  }
  else if (t.mon == 12 && t.mday == 31) {
    shiftTextV3("SILVESTER\0", &colorIterator, brightness, ms);
  }
  else if(t.mon == 1 && t.mday >= 20 && (t.wday == 5 || t.wday == 6)) {
    shiftTextV3("WILLKOMMEN AN DER HTL BULME\0", &colorIterator, brightness, ms);
  }
  
  // Regular display sequence based on settings
  if(displaySettings.showWeekday) {
    shiftTextV3(wday[t.wday], &colorIterator, brightness, ms);
  }
  
  if(displaySettings.showDate) {
    datetime(&colorIterator, runs, brightness, ms);
  }
  
  if(displaySettings.showTime) {
    RTCToMatrix(&colorIterator, runs, brightness, ms);
  }
  
  if(displaySettings.showYear) {
    showYear(&colorIterator, runs, brightness, ms);
  }
  
  if(displaySettings.showTemp) {
    printTemperature(&colorIterator, runs, brightness, ms);
  }
  
  if(displaySettings.showHumidity) {
    humidity(&colorIterator, runs, brightness, ms);
  }
  
  if(displaySettings.showCustomText && strlen(customText) > 0) {
    shiftTextV3(customText, &colorIterator, brightness, ms);
  }
  if(displaySettings.showName)
  { 
    shiftTextV3("SMART DISPLAY\0", &colorIterator, brightness, ms);
  }

}