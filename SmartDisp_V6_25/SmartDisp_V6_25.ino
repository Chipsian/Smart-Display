/** @mainpage
* The SmartDisp.ino program implements an application that
* displays times, date, humidity and temperatures on a Matrix
*
* @author  Lukas Christian
* @version V8.0 2020/04/14
* @since   2019-09
* @date 2019/Sep/09 - 2022/Dez/10
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


const char *ssid = "TP-LINK_AAB9";
const char *password = "08728425";

const long utcOffsetInSeconds = 3600;

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
      return -1;
    } else {
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
  DS3231_init(DS3231_INTCN);  //register the ds3231 (DS3231_INTCN is the default address of ds3231, this is set by macro for no performance loss)

  pixels.begin();  // INITIALIZE NeoPixel strip object (REQUIRED)

  dht.begin();

  Serial.begin(115200);


  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\n");
  timeClient.begin();
  timeClient.forceUpdate();
  timeClient.update();
  t.hour = (uint8_t)timeClient.getHours();
  t.min = (uint8_t)timeClient.getMinutes();
  t.sec = (uint8_t)timeClient.getSeconds();
  t.mon = (uint8_t)timeClient.getMonth();
  t.mday = (uint8_t)timeClient.getDate();
  t.year = (uint8_t)timeClient.getYear();
  t.wday = (uint8_t)timeClient.getDay();

  DS3231_set(t);

  delayMS = sensor.min_delay / 1000;
  timeClient.end();
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

  while (1) {
    //weekDays(&colorIterator, runs, brightness, ms);
    if (t.mon == 3 && t.mday == 25 && t.hour >= 2) {
      timeClient.forceUpdate();
      timeClient.update();
     //timeClient(ntpUDP, "0.at.pool.ntp.org", utcOffsetInSeconds + utcOffsetInSeconds);
      t.hour = (uint8_t)timeClient.getHours();
    }

    else if (t.mon == 10 && t.mday >= 25 && t.hour == 3) {
      timeClient.forceUpdate();
      timeClient.update();
     //timeClient(ntpUDP, "0.at.pool.ntp.org", utcOffsetInSeconds);
      t.hour = (uint8_t)timeClient.getHours();
    }

    if (t.hour >= 19 || t.hour < 7) {
      brightness = 125;
    } else {
      brightness = 250;
    }
    shiftTextV3(wday[t.wday], &colorIterator, brightness, ms);
    datetime(&colorIterator, runs, brightness, ms);

    RTCToMatrix(&colorIterator, runs, brightness, ms);
    showYear(&colorIterator, runs, brightness, ms);
    printTemperature(&colorIterator, runs, brightness, ms);
    humidity(&colorIterator, runs, brightness, ms);
    //shiftTextV3("WILLKOMMEN AN DER HTL BULME\0", &colorIterator, brightness, ms);
  }
}