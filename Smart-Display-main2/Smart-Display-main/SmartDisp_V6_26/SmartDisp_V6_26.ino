/** @mainpage
* The SmartDisp.ino program implements an application that
* displays times, date, humidity and temperatures on a Matrix
*
* @author  Lukas Christian
* @version V8.0 2020/04/14
* @since   2019-09
* @date 2019/Sep/09 - 2022/Dez/10
*/

/* Web server*/
#include <ESP8266WebServer.h>
#include <EEPROM.h>

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


//const char *ssid = "Dialog WLAN 9ABC";
//const char *password = "05156476454093850903";

// WiFi Einstellungen - werden jetzt aus EEPROM geladen Claude 3
char ssid[32] = "Dialog WLN 9ABC";
char password[64] = "0515647645409385090";

// WiFi Status und Webserver
bool wifiConnected = false;
bool useRTCOnly = false;
ESP8266WebServer server(80);

// EEPROM Adressen für WiFi-Daten
#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 32
#define WIFI_COUNT_ADDR 96

// Struktur für mehrere WiFi-Netzwerke
struct WiFiNetwork {
    char ssid[32];
    char password[64];
};

WiFiNetwork storedNetworks[3]; // Bis zu 3 gespeicherte Netzwerke
int networkCount = 0;
/* Claude 3 ende*/

long utcOffsetInSeconds = 3600;

//char wday[7][4] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
char wday[7][12] = { "Sonntag\0", "Montag\0", "Dienstag\0", "Mittwoch\0", "Donnerstag\0", "Freitag\0", "Samstag\0" };


// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "0.at.pool.ntp.org", utcOffsetInSeconds);


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
  for (; x > (txtRowLen); (*colorIterator)++) {

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
/** @brief
* Claude 3
* Neue Funktion zur Berechnung der Zeitumstellung (EU-Regeln)
* @return void
* @param year current year
* @param month current month
* @param day current day
* @param hour current hour
* @addtogroup application
*/
bool isDST(int year, int month, int day, int hour) {
    // Sommerzeit: Letzter Sonntag im März 2:00 bis letzter Sonntag im Oktober 3:00
    
    if (month < 3 || month > 10) return false; // Jan, Feb, Nov, Dez = Winterzeit
    if (month > 3 && month < 10) return true;  // Apr-Sep = Sommerzeit
    
    // März und Oktober brauchen spezielle Berechnung
    int lastSunday = 31 - ((5 * year / 4 + 4) % 7); // Letzter Sonntag im März
    if (month == 3) {
        return (day > lastSunday || (day == lastSunday && hour >= 2));
    }
    
    lastSunday = 31 - ((5 * year / 4 + 1) % 7); // Letzter Sonntag im Oktober
    if (month == 10) {
        return (day < lastSunday || (day == lastSunday && hour < 3));
    }
    
    return false;
}

/** @brief
* Claude 3
* Neue Funktion zur Aktualisierung der Zeitzone
* @return void
* @addtogroup application
*/
void updateTimeZone() {
    if (!wifiConnected) return;
    
    timeClient.update();
    int year = timeClient.getYear();
    int month = timeClient.getMonth();
    int day = timeClient.getDate();
    int hour = timeClient.getHours();
    
    if (isDST(year, month, day, hour)) 
    {
      utcOffsetInSeconds = 7200;
    } 
    else 
    {
      utcOffsetInSeconds = 3600;
    }
    
    timeClient.setTimeOffset(utcOffsetInSeconds);
    timeClient.forceUpdate();
}


// EEPROM Funktionen
void saveWiFiToEEPROM() {
    EEPROM.write(WIFI_COUNT_ADDR, networkCount);
    for (int i = 0; i < networkCount && i < 3; i++) {
        for (int j = 0; j < 32; j++) {
            EEPROM.write(SSID_ADDR + (i * 96) + j, storedNetworks[i].ssid[j]);
        }
        for (int j = 0; j < 64; j++) {
            EEPROM.write(PASS_ADDR + (i * 96) + j, storedNetworks[i].password[j]);
        }
    }
    EEPROM.commit();
}

void loadWiFiFromEEPROM() {
    networkCount = EEPROM.read(WIFI_COUNT_ADDR);
    if (networkCount > 3) networkCount = 0; // Sicherheitscheck
    
    for (int i = 0; i < networkCount; i++) {
        for (int j = 0; j < 32; j++) {
            storedNetworks[i].ssid[j] = EEPROM.read(SSID_ADDR + (i * 96) + j);
        }
        for (int j = 0; j < 64; j++) {
            storedNetworks[i].password[j] = EEPROM.read(PASS_ADDR + (i * 96) + j);
        }
    }
}

// WiFi Verbindung mit 4 Versuchen
bool connectToWiFi() {
    // Erst gespeicherte Netzwerke probieren
    for (int net = 0; net < networkCount; net++) {
        Serial.printf("Versuche Verbindung zu: %s\n", storedNetworks[net].ssid);
        
        for (int attempt = 1; attempt <= 4; attempt++) {
            WiFi.begin(storedNetworks[net].ssid, storedNetworks[net].password);
            Serial.printf("Versuch %d/4...", attempt);
            
            int timeout = 0;
            while (WiFi.status() != WL_CONNECTED && timeout < 10) {
                delay(500);
                Serial.print(".");
                timeout++;
            }
            
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("\nVerbunden mit %s!\n", storedNetworks[net].ssid);
                Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
                return true;
            }
            Serial.printf(" Fehlgeschlagen\n");
        }
    }
    
    // Fallback: Standard-Netzwerk probieren
    Serial.printf("Versuche Standard-Netzwerk: %s\n", ssid);
    for (int attempt = 1; attempt <= 4; attempt++) {
        WiFi.begin(ssid, password);
        Serial.printf("Versuch %d/4...", attempt);
        
        int timeout = 0;
        while (WiFi.status() != WL_CONNECTED && timeout < 10) {
            delay(500);
            Serial.print(".");
            timeout++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\nVerbunden mit Standard-Netzwerk!\n");
            return true;
        }
        Serial.printf(" Fehlgeschlagen\n");
    }
    
    Serial.println("Alle WiFi-Versuche fehlgeschlagen. Verwende nur RTC.");
    return false;
}

// Webserver Funktionen
void handleRoot() {
    String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>SmartDisp WiFi Manager</title>
    <meta charset='utf-8'>
    <style>
        body { font-family: Arial; margin: 40px; background: #f0f0f0; }
        .container { background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #333; }
        input[type=text], input[type=password] { 
            width: 300px; padding: 10px; margin: 5px 0; border: 1px solid #ddd; border-radius: 5px; 
        }
        button { 
            background: #007cba; color: white; padding: 12px 25px; border: none; border-radius: 5px; cursor: pointer; margin: 10px 5px;
        }
        button:hover { background: #005a87; }
        .status { padding: 10px; margin: 10px 0; border-radius: 5px; }
        .connected { background: #d4edda; border: 1px solid #c3e6cb; color: #155724; }
        .disconnected { background: #f8d7da; border: 1px solid #f5c6cb; color: #721c24; }
        .network-list { margin: 20px 0; }
        .network-item { background: #f8f9fa; padding: 10px; margin: 5px 0; border-radius: 5px; border: 1px solid #dee2e6; }
    </style>
</head>
<body>
    <div class='container'>
        <h1>SmartDisp WiFi Manager</h1>
        
        <div class='status )";
        
    if (wifiConnected) {
        html += "connected'>✓ Verbunden mit: " + WiFi.SSID() + "<br>IP: " + WiFi.localIP().toString();
    } else {
        html += "disconnected'>✗ Nicht verbunden - verwende nur RTC";
    }
    
    html += R"(</div>
        
        <h2>Neues WLAN hinzufügen</h2>
        <form action='/save' method='POST'>
            <p>
                <label>SSID (Netzwerkname):</label><br>
                <input type='text' name='ssid' required>
            </p>
            <p>
                <label>Passwort:</label><br>
                <input type='password' name='password'>
            </p>
            <button type='submit'>Speichern & Verbinden</button>
        </form>
        
        <h2>Gespeicherte Netzwerke</h2>
        <div class='network-list'>)";
    
    if (networkCount == 0) {
        html += "<p>Keine gespeicherten Netzwerke</p>";
    } else {
        for (int i = 0; i < networkCount; i++) {
            html += "<div class='network-item'>";
            html += "<strong>" + String(storedNetworks[i].ssid) + "</strong>";
            html += " <button onclick=\"location.href='/delete?id=" + String(i) + "'\">Löschen</button>";
            html += "</div>";
        }
    }
    
    html += R"(
        </div>
        
        <p>
            <button onclick="location.href='/scan'">Netzwerke scannen</button>
            <button onclick="location.href='/reconnect'">Neu verbinden</button>
            <button onclick="location.reload()\">Aktualisieren</button>
        </p>
        
        <hr>
        <p><small>SmartDisp v8.1 - Aktuelle Zeit: )" + String(t.hour) + ":" + String(t.min) + R"(</small></p>
    </div>
</body>
</html>
)";
    
    server.send(200, "text/html", html);
}

void handleSave() {
    String newSSID = server.arg("ssid");
    String newPassword = server.arg("password");
    
    if (newSSID.length() > 0 && networkCount < 3) {
        strcpy(storedNetworks[networkCount].ssid, newSSID.c_str());
        strcpy(storedNetworks[networkCount].password, newPassword.c_str());
        networkCount++;
        saveWiFiToEEPROM();
        
        // Sofort versuchen zu verbinden
        WiFi.begin(newSSID.c_str(), newPassword.c_str());
        delay(5000);
        
        if (WiFi.status() == WL_CONNECTED) {
            wifiConnected = true;
            server.send(200, "text/html", "<html><body><h1>Erfolgreich verbunden!</h1><a href='/'>Zurück</a></body></html>");
        } else {
            server.send(200, "text/html", "<html><body><h1>Gespeichert, aber Verbindung fehlgeschlagen</h1><a href='/'>Zurück</a></body></html>");
        }
    } else {
        server.send(400, "text/html", "<html><body><h1>Fehler beim Speichern</h1><a href='/'>Zurück</a></body></html>");
    }
}

void handleReconnect() {
    wifiConnected = connectToWiFi();
    server.send(200, "text/html", "<html><body><h1>Verbindungsversuch abgeschlossen</h1><a href='/'>Zurück</a></body></html>");
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

  // EEPROM initialisieren
    EEPROM.begin(EEPROM_SIZE);
    loadWiFiFromEEPROM();
    
    // WiFi Verbindung versuchen
    wifiConnected = connectToWiFi();

  if (wifiConnected) {
        // Zeit vom Internet holen
        timeClient.begin();
        updateTimeZone();
        
        t.hour = (uint8_t)timeClient.getHours();
        t.min = (uint8_t)timeClient.getMinutes();
        t.sec = (uint8_t)timeClient.getSeconds();
        t.mon = (uint8_t)timeClient.getMonth();
        t.mday = (uint8_t)timeClient.getDate();
        t.year = (uint16_t)timeClient.getYear();
        t.wday = (uint8_t)timeClient.getDay();
        
        DS3231_set(t);
        Serial.println("Zeit von NTP synchronisiert");
    } else {
        // Nur RTC verwenden
        useRTCOnly = true;
        Serial.println("Verwende nur RTC - keine Internetzeit verfügbar");
        
        // Access Point für Konfiguration starten
        WiFi.softAP("SmartDisp-Config", "12345678");
        WiFi.softAPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
        Serial.println("Access Point gestartet: SmartDisp-Config");
        Serial.println("Verbinde dich und gehe zu 192.168.1.1");
    }
    
    // Webserver starten
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/reconnect", handleReconnect);
    server.begin();
    Serial.println("Webserver gestartet");
    
    delayMS = sensor.min_delay / 1000;
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
  server.handleClient(); // Webserver bedienen
  unsigned char brightness = 250;
  int runs = 1;
  int colorIterator = 0;
  int ms = 30;
  
  static unsigned long lastTimeUpdate = 0;
  const unsigned long TIME_UPDATE_INTERVAL = 3600000; // Jede Stunde

  while (1) {
    server.handleClient(); // Auch im Loop den Server bedienen
    
    // Zeitupdate nur wenn WiFi verbunden ist
    if (wifiConnected && !useRTCOnly && (millis() - lastTimeUpdate > TIME_UPDATE_INTERVAL)) 
    {
      updateTimeZone();
      
      t.hour = (uint8_t)timeClient.getHours();
      t.min = (uint8_t)timeClient.getMinutes();
      t.sec = (uint8_t)timeClient.getSeconds();
      t.mon = (uint8_t)timeClient.getMonth();
      t.mday = (uint8_t)timeClient.getDate();
      t.year = (uint16_t)timeClient.getYear();
      t.wday = (uint8_t)timeClient.getDay();
      
      DS3231_set(t);
      lastTimeUpdate = millis();
    } 
    else if (useRTCOnly) 
    {
        // Zeit aus RTC lesen
        DS3231_get(&t);
    }
    
    // Helligkeit
    if (t.hour >= 19 || t.hour < 7) 
    {
        brightness = 125;
    } else 
    {
        brightness = 250;
    }
    shiftTextV3(wday[t.wday], &colorIterator, brightness, ms);
    datetime(&colorIterator, runs, brightness, ms);

    RTCToMatrix(&colorIterator, runs, brightness, ms);
    showYear(&colorIterator, runs, brightness, ms);
    printTemperature(&colorIterator, runs, brightness, ms);
    humidity(&colorIterator, runs, brightness, ms);
    if(t.mon == 12 && t.mday == 24 || t.mon == 12 && t.mday == 25)
    {
      shiftTextV3("FROHE WEIHNACHTEN\0", &colorIterator, brightness, ms);
    }
    else if(t.mon == 1 && t.mday == 1)
    {
      shiftTextV3("NEUJAHR\0", &colorIterator, brightness, ms);
    }
    else if (t.mon == 12 && t.mday == 31) 
    {
    	shiftTextV3("SILVESTER\0", &colorIterator, brightness, ms);
    }
    else if(t.mon == 1 && t.mday >= 20 && t.wday == 5 || t.mon == 1 && t.mday >= 20 && t.wday == 6)
    {
      shiftTextV3("WILLKOMMEN AN DER HTL BULME\0", &colorIterator, brightness, ms);
    }
    else if(t.mon == 8 && t.wday == 6)
    {
      shiftTextV3("WOCHENTAG\0", &colorIterator, brightness, ms);
    }
    if(t.mon == 8 && t.mday == 2)
    {
      shiftTextV3("MONAT\0", &colorIterator, brightness, ms);
    }

    shiftTextV3("SMART DISPLAY\0", &colorIterator, brightness, ms);
  }
}