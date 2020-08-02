/* ----------------------------------------------------------------------- *
 * Title:         pcf8574-lcd-demo                                         *
 * Description:   C-code for PCF8574T backpack controlling LCD through I2C *
 *                Tested on Raspberry Pi 3                                 *
 *                20180203 frank4dd                                        *
 *                                                                         *
 * Prerequisites: apt-get libi2c-dev i2c-tools                             *
 * Compilation:   gcc pcf8574-lcd-demo.c -o pcf8574-lcd-demo               *
 *------------------------------------------------------------------------ * 
 * PCF8574T backpack module uses 4-bit mode, LCD pins D0-D3 are not used.  *
 * backpack module wiring:                                                 *
 *                                                                         *
 *  PCF8574T     LCD                                                       *
 *  ========     ======                                                    *
 *     P0        RS                                                        *
 *     P1        RW                                                        *
 *     P2        Enable                                                    *
 *     P3        Led Backlight                                             *
 *     P4        D4                                                        *
 *     P5        D5                                                        *
 *     P6        D6                                                        *
 *     P7        D7                                                        *
 *                                                                         *
 * I2C-byte: D7 D6 D5 D4 BL EN RW RS                                       *
 * ----------------------------------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>

#define I2C_BUS        "/dev/i2c-1" // I2C bus device on a Raspberry Pi 3
#define I2C_ADDR       0x27         // I2C slave address for the LCD module
#define BINARY_FORMAT  " %c  %c  %c  %c  %c  %c  %c  %c\n"
#define BYTE_TO_BINARY(byte) \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 
  
int lcd_backlight;
int debug;
char address; 
int i2cFile;

void i2c_start() {
   if((i2cFile = open(I2C_BUS, O_RDWR)) < 0) {
      printf("Error failed to open I2C bus [%s].\n", I2C_BUS);
      exit(-1);
   }
   // set the I2C slave address for all subsequent I2C device transfers
   if (ioctl(i2cFile, I2C_SLAVE, I2C_ADDR) < 0) {
      printf("Error failed to set I2C address [%s].\n", I2C_ADDR);
      exit(-1);
   }
}

void i2c_stop() { close(i2cFile); }

void i2c_send_byte(unsigned char data) {
   unsigned char byte[1];
   byte[0] = data;
   if(debug) printf(BINARY_FORMAT, BYTE_TO_BINARY(byte[0]));
   write(i2cFile, byte, sizeof(byte)); 
   /* -------------------------------------------------------------------- *
    * Below wait creates 1msec delay, needed by display to catch commands  *
    * -------------------------------------------------------------------- */
   usleep(1000);
}

void main() { 
   i2c_start(); 
   debug=1;

   /* -------------------------------------------------------------------- *
    * Initialize the display, using the 4-bit mode initialization sequence *
    * -------------------------------------------------------------------- */
   if(debug) printf("Init Start:\n");
   if(debug) printf("D7 D6 D5 D4 BL EN RW RS\n");

   usleep(15000);             // wait 15msec
   i2c_send_byte(0b00110100); // D7=0, D6=0, D5=1, D4=1, RS,RW=0 EN=1
   i2c_send_byte(0b00110000); // D7=0, D6=0, D5=1, D4=1, RS,RW=0 EN=0
   usleep(4100);              // wait 4.1msec
   i2c_send_byte(0b00110100); // 
   i2c_send_byte(0b00110000); // same
   usleep(100);               // wait 100usec
   i2c_send_byte(0b00110100); //
   i2c_send_byte(0b00110000); // 8-bit mode init complete
   usleep(4100);              // wait 4.1msec
   i2c_send_byte(0b00100100); //
   i2c_send_byte(0b00100000); // switched now to 4-bit mode

   /* -------------------------------------------------------------------- *
    * 4-bit mode initialization complete. Now configuring the function set *
    * -------------------------------------------------------------------- */
   usleep(40);                // wait 40usec
   i2c_send_byte(0b00100100); //
   i2c_send_byte(0b00100000); // keep 4-bit mode
   i2c_send_byte(0b10000100); //
   i2c_send_byte(0b10000000); // D3=2lines, D2=char5x8

   /* -------------------------------------------------------------------- *
    * Next turn display off                                                *
    * -------------------------------------------------------------------- */
   usleep(40);                // wait 40usec
   i2c_send_byte(0b00000100); //
   i2c_send_byte(0b00000000); // D7-D4=0
   i2c_send_byte(0b10000100); //
   i2c_send_byte(0b10000000); // D3=1 D2=display_off, D1=cursor_off, D0=cursor_blink

   /* -------------------------------------------------------------------- *
    * Display clear, cursor home                                           *
    * -------------------------------------------------------------------- */
   usleep(40);                // wait 40usec
   i2c_send_byte(0b00000100); //
   i2c_send_byte(0b00000000); // D7-D4=0
   i2c_send_byte(0b00010100); //
   i2c_send_byte(0b00010000); // D0=display_clear

   /* -------------------------------------------------------------------- *
    * Set cursor direction                                                 *
    * -------------------------------------------------------------------- */
   usleep(40);                // wait 40usec
   i2c_send_byte(0b00000100); //
   i2c_send_byte(0b00000000); // D7-D4=0
   i2c_send_byte(0b01100100); //
   i2c_send_byte(0b01100000); // print left to right

   /* -------------------------------------------------------------------- *
    * Turn on the display                                                  *
    * -------------------------------------------------------------------- */
   usleep(40);                // wait 40usec
   i2c_send_byte(0b00000100); //
   i2c_send_byte(0b00000000); // D7-D4=0
   i2c_send_byte(0b11100100); //
   i2c_send_byte(0b11100000); // D3=1 D2=display_on, D1=cursor_on, D0=cursor_blink

   if(debug) printf("Init End.\n");
   sleep(1);

   if(debug) printf("Writing HELLO to display\n");
   if(debug) printf("D7 D6 D5 D4 BL EN RW RS\n");

   /* -------------------------------------------------------------------- *
    * Start writing 'H' 'E' 'L' 'L' 'O' chars to the display, with BL=on.  *
    * -------------------------------------------------------------------- */
   i2c_send_byte(0b01001101); //
   i2c_send_byte(0b01001001); // send 0100=4
   i2c_send_byte(0b10001101); //
   i2c_send_byte(0b10001001); // send 1000=8 = 0x48 ='H'

   i2c_send_byte(0b01001101); //
   i2c_send_byte(0b01001001); // send 0100=4
   i2c_send_byte(0b01011101); // 
   i2c_send_byte(0b01011001); // send 0101=1 = 0x41 ='E'

   i2c_send_byte(0b01001101); //
   i2c_send_byte(0b01001001); // send 0100=4
   i2c_send_byte(0b11001101); //
   i2c_send_byte(0b11001001); // send 1100=12 = 0x4D ='L'

   i2c_send_byte(0b01001101); //
   i2c_send_byte(0b01001001); // send 0100=4
   i2c_send_byte(0b11001101); //
   i2c_send_byte(0b11001001); // send 1100=12 = 0x4D ='L'

   i2c_send_byte(0b01001101); //
   i2c_send_byte(0b01001001); // send 0100=4
   i2c_send_byte(0b11111101); //
   i2c_send_byte(0b11111001); // send 1111=15 = 0x4F ='O'

   if(debug) printf("Finished writing to display.\n");
   i2c_stop(); 
}
