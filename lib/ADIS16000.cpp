////////////////////////////////////////////////////////////////////////////////////////////////////////
//  June 2015
//  Author: Juan Jose Chong <juan.chong@analog.com>
////////////////////////////////////////////////////////////////////////////////////////////////////////
//  ADIS16000.cpp
////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
//  This library provides all the functions necessary to interface the ADIS16000 Digital MEMS 
//  Vibration Sensor with Embedded RF Transceiver to an 8-Bit Atmel-based Arduino development board. 
//  Functions for SPI configuration, reads and writes, and scaling are included. 
//
//  This example is free software. You can redistribute it and/or modify it
//  under the terms of the GNU Lesser Public License as published by the Free Software
//  Foundation, either version 3 of the License, or any later version.
//
//  This example is distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS 
//  FOR A PARTICULAR PURPOSE.  See the GNU Lesser Public License for more details.
//
//  You should have received a copy of the GNU Lesser Public License along with 
//  this example.  If not, see <http://www.gnu.org/licenses/>.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ADIS16000"

////////////////////////////////////////////////////////////////////////////
// Constructor with configurable CS and RST
////////////////////////////////////////////////////////////////////////////
// CS - Chip select pin
// RST - Hardware reset pin
////////////////////////////////////////////////////////////////////////////
ADIS16000::ADIS16000(int CS, int RST) {
  _CS = CS;
  _RST = RST;

  SPI.begin(); // Initialize SPI bus
  configSPI(); // Configure SPI

//Set default pin states
  pinMode(_CS, OUTPUT); // Set CS pin to be an output
  pinMode(_RST, OUTPUT); // Set RST pin to be an output
  digitalWrite(_CS, HIGH); // Initialize CS pin to be high
  digitalWrite(_RST, HIGH); // Initialize RST pin to be high
}

////////////////////////////////////////////////////////////////////////////
// Destructor
////////////////////////////////////////////////////////////////////////////
ADIS16000::~ADIS16000() {
  // Close SPI bus
  SPI.end();
}

////////////////////////////////////////////////////////////////////////////
// Performs a hardware reset by setting _RST pin low for delay (in ms).
////////////////////////////////////////////////////////////////////////////
int ADIS16000::resetDUT(uint8_t ms) {
  digitalWrite(_RST, LOW);
  delay(100);
  digitalWrite(_RST, HIGH);
  delay(ms);
  return 1;
}

////////////////////////////////////////////////////////////////////////////
// Sets SPI bit order, clock divider, and data mode. This function is useful
// when there are multiple SPI devices using different settings.
// Returns 1 when complete.
////////////////////////////////////////////////////////////////////////////
int ADIS16000::configSPI() {
  SPI.setBitOrder(MSBFIRST); // Per the datasheet
  SPI.setClockDivider(SPI_CLOCK_DIV8); // Config for  1MHz (ADIS16448 max 2MHz)
  SPI.setDataMode(SPI_MODE3); // Clock base at one, sampled on falling edge
  return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Reads two bytes (one word) in two sequential registers over SPI
////////////////////////////////////////////////////////////////////////////////////////////
// regAddr - address of register to be read
// return - (int) signed 16 bit 2's complement number
////////////////////////////////////////////////////////////////////////////////////////////
int16_t ADIS16000::regRead(uint8_t regAddr) {
//Read registers using SPI
  
  // Write register address to be read
  digitalWrite(_CS, LOW); // Set CS low to enable device
  SPI.transfer(regAddr); // Write address over SPI bus
  SPI.transfer(0x00); // Write 0x00 to the SPI bus fill the 16 bit transaction requirement
  digitalWrite(_CS, HIGH); // Set CS high to disable device

  delayMicroseconds(15); // Delay to not violate read rate (40us)

  // Read data from requested register
  digitalWrite(_CS, LOW); // Set CS low to enable device
  uint8_t _msbData = SPI.transfer(0x00); // Send (0x00) and place upper byte into variable
  uint8_t _lsbData = SPI.transfer(0x00); // Send (0x00) and place lower byte into variable
  digitalWrite(_CS, HIGH); // Set CS high to disable device

  delayMicroseconds(15); // Delay to not violate read rate (40us)
  
  int16_t _dataOut = (_msbData << 8) | (_lsbData & 0xFF); // Concatenate upper and lower bytes
  // Shift MSB data left by 8 bits, mask LSB data with 0xFF, and OR both bits.

#ifdef DEBUG 
  Serial.print("Register 0x");
  Serial.print((unsigned char)regAddr, HEX);
  Serial.print(" reads: ");
  Serial.println(_dataOut);
#endif

  return(_dataOut);
}

////////////////////////////////////////////////////////////////////////////
// Writes one byte of data to the specified register over SPI.
// Returns 1 when complete.
////////////////////////////////////////////////////////////////////////////
// regAddr - address of register to be written
// regData - data to be written to the register
////////////////////////////////////////////////////////////////////////////
int ADIS16000::regWrite(uint8_t regAddr,uint16_t regData) {

  // Write register address and data
  uint16_t addr = (((regAddr & 0x7F) | 0x80) << 8); // Toggle sign bit, and check that the address is 8 bits
  uint16_t lowWord = (addr | (regData & 0xFF)); // OR Register address (A) with data(D) (AADD)
  uint16_t highWord = ((addr | 0x100) | ((regData >> 8) & 0xFF)); // OR Register address with data and increment address
  digitalWrite(_CS, LOW); // Set CS low to enable device
  SPI.transfer((uint8_t)lowWord); // Write low byte over SPI bus
  SPI.transfer((uint8_t)highWord); //Write high byte over SPI bus
  digitalWrite(_CS, HIGH); // Set CS high to disable device
  
  delayMicroseconds(25); // Delay to not violate read rate (40us)

  #ifdef DEBUG
    Serial.print("Wrote 0x");
    Serial.println(regData);
    Serial.print(" to register: 0x");
    Serial.print((unsigned char)regAddr, HEX);
  #endif

  return 1;
}

int addSensor(uint8_t sensorAddr) {
	regWrite(GLOB_CMD_G, 0x01);
	delayMicroseconds(500);
	regWrite(CMD_DATA, sensorAddr);
	return 1;
}

int ADIS16000::removeSensor(uint8_t sensorAddr) {
	regWrite(CMD_DATA, sensorAddr);
	regWrite(GLOB_CMD_G, 0x100);
	return 1;
}

int ADIS16000::saveGatewaySettings() {
	regWrite(PAGE_ID, 0x00);
	regWrite(GLOB_CMD_G, 0x40);
	return 1;
}

int ADIS16000::saveSensorSettings(uint8_t sensorAddr) {
	regWrite(PAGE_ID, sensorAddr);
	regWrite(GLOB_CMD_S, 0x40);
	regWrite(PAGE_ID, 0x00);
	regWrite(GLOB_CMD_G, 0x02);
	return 1;
}

int16_t * ADIS16000::readFFTBuffer(uint8_t sensorAddr) {
	int16_t buffer [512];
	regWrite(PAGE_ID, sensorAddr);
  regWrite(BUF_PNTR, 0x00);
  regWrite(GLOB_CMD_S, 0x800); // Start data acquisition
  regWrite(GLOB_CMD_G, 0X2); // Send data to sensor
	for (int i = 0; i < 511, i++) {
		bufferx[i] = regRead(X_BUF);
    buffery[i + 256] = regRead(Y_BUF);
	}
	return buffer;
}

int16_t * ADIS16000::readFFT(uint8_t sample, uint8_t sensorAddr) {
	int16_t buffer [2];
	regWrite(PAGE_ID, sensorAddr);
	regWrite(BUF_PNTR, sample);
	buffer[1] = regRead(X_BUF);
	buffer[2] = regRead(Y_BUF);
	return buffer;
}

int ADIS16000::setDataReady(uint8_t dio) {
	regWrite(PAGE_ID, 0x00);
	if (dio == 1){
		regWrite(GPO_CTRL, 0x08);
		return dio;
	}
		
	if (dio == 2){
		regWrite(GPO,CTRL, 0x20);
		return dio;
	}

	if (dio != 1 || 2)
		return 0;
}

int ADIS16000::setPeriodicMode(uint16_t interval, uint8_t scalefactor, uint8_t sensorAddr) {
  regWrite(PAGE_ID, sensorAddr);
  regWrite(UPDAT_INT, interval);
  regWrite(INT_SCL, scalefactor);
  regWrite(GLOB_CMD_S, 0x800);
  return 1;
}

float ADIS16000::scaleTime(int16_t sensorData, int gRange {
  int lsbrange = 0;
  int signedData = 0;
  int isNeg = sensorData & 0x8000;
  if (isNeg == 0x8000) // If the number is negative, scale and sign the output
    signedData = sensorData - 0xFFFF;
  else
    signedData = sensorData; // Else return the raw number
  switch (gRange) {
    case 1:
      lsbrange = 0.0305;
      break;
    case 5:
      lsbrange = 0.1526;
      break;
    case 10:
      lsbrange = 0.3052;
      break;
    case 20:
      lsbrange = 0.6104;
      break;
    default:
      lsbrange = 0.0305;
      break;
  }
  float finalData = signedData * lsbrange;
  return finalData;
}

float ADIS16000::scaleFFT(int16_t sensorData, int gRange {
  int lsbrange = 0;
  int signedData = 0;
  int isNeg = sensorData & 0x8000;
  if (isNeg == 0x8000) // If the number is negative, scale and sign the output
    signedData = sensorData - 0xFFFF;
  else
    signedData = sensorData; // Else return the raw number
  switch (gRange) {
    case 1:
      lsbrange = 0.0153;
      break;
    case 5:
      lsbrange = 0.0763;
      break;
    case 10:
      lsbrange = 0.1526;
      break;
    case 20:
      lsbrange = 0.3052;
      break;
    default:
      lsbrange = 0.0153;
      break;
  }
  float finalData = signedData * lsbrange;
  return finalData;
}

float ADIS16000::scaleSupply(int16_t sensorData)
{
  int signedData = 0;
  int isNeg = sensorData & 0x8000;
  if (isNeg == 0x8000) // If the number is negative, scale and sign the output
    signedData = sensorData - 0xFFFF;
  else
    signedData = sensorData; // Else return the raw number
  float finalData = signedData * 0.00044; // Multiply by accel sensitivity (250 uG/LSB)
  return finalData;
}

float ADIS16000::scaleTemp(int16_t sensorData)
{
  int signedData = 0;
  int isNeg = sensorData & 0x8000;
  if (isNeg == 0x8000) // If the number is negative, scale and sign the output
    signedData = sensorData - 0xFFFF;
  else
    signedData = sensorData; // Else return the raw number
  float finalData = signedData * 0.0815; // Multiply by accel sensitivity (250 uG/LSB)
  return finalData;
}
