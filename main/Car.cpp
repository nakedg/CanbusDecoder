#include "Car.h"
#include "esp_log.h"

static uint8_t getBit(const uint8_t *data, uint8_t pos)
{
    uint8_t bytePos = pos / 8;
    uint8_t bitPos = pos % 8;
    return ((*(data + bytePos) >> bitPos) & 0x01);
}

static int16_t scale(int16_t value, int16_t inMin, int16_t inMax, int16_t outMin, int16_t outMax)
{
  int16_t part1 = (value - inMin) * (outMax - outMin);
  int16_t part2 = (inMax - inMin);
  float res = ((float)part1 / (float)part2) + outMin;
  return (int16_t)res;
}

static uint32_t getUInt(const uint8_t *data, uint8_t startPosition, uint8_t endPosition, bool intel = false)
{
    uint32_t result = 0;
    if (intel) 
    {
        for (int n = endPosition; n >= startPosition; n--)
        {
          result = (result << 8) + data[n];
        }
    }
    else
    {
      for (int n = startPosition; n <= endPosition; n++)
        {
          result = (result << 8) + data[n];
        }
    }
    return result;
}

const char hex_asc_upper[] = "0123456789ABCDEF";
#define hex_asc_upper_lo(x)    hex_asc_upper[((x) & 0x0F)]
#define hex_asc_upper_hi(x)    hex_asc_upper[((x) & 0xF0) >> 4]

static inline void put_hex_byte(char *buf, uint8_t byte)
{
    buf[0] = hex_asc_upper_hi(byte);
    buf[1] = hex_asc_upper_lo(byte);
}

void Car::InitCar()
{
  bonnet = 1;
  fl_door = 1;
  fr_door = 0;
  rl_door = 0;
  rr_door  = 1;
  fuel_lvl = 10;
  low_fuel_lvl = 0;
  odometer = 100;
  low_voltage = 0;
  speed = 50;
  taho = 70;
  wheel = 0;
  temp = 30;
  //writeDebugStream(F("Car inited\r\n"));
}

void Car::ProcessCanMessage(const twai_message_t *frame)
{
  switch (frame->identifier)
  {
  case 0x350:
    parseDoors(frame->data);
    break;
  case 0x186:
    parseEngineRpm(frame->data);
    break;
  case 0x5D7:
    parseOdometer(frame->data);
    break;
  case 0x354:
    parseSpeed(frame->data);
    break;
  case 0x6FB:
    parseFuelLevel(frame->data);
    break;
  case 0x5DA:
    parseEngineTemp(frame->data);
    break;
  case 0xC6:
    parseWheelPosition(frame->data);
    break;
  default:
    break;
  }
}

void Car::parseDoors(const uint8_t *data)
{
  fr_door = getBit(data, 51);
  fl_door = getBit(data, 53);
  rl_door = rr_door = getBit(data, 57);
}

void Car::parseEngineRpm(const uint8_t *data)
{
  uint32_t rpm = getUInt(data, 0, 1);
  taho = rpm / 10;
}

void Car::parseSpeed(const uint8_t *data)
{
  uint32_t val = getUInt(data, 0, 1);
  speed = val / 100;
}

void Car::parseOdometer(const uint8_t *data)
{
  int val = getUInt(data, 2, 5);
  odometer = (val >> 4) / 100;
}

void Car::parseEngineTemp(const uint8_t *data)
{
  temp = data[0] - 40;
}

void Car::parseFuelLevel(const uint8_t *data)
{
  uint8_t fuel = data[3];
  fuel_lvl = fuel >> 1;
  low_fuel_lvl = fuel & (1 << 7);
}

void Car::parseWheelPosition(const uint8_t *data)
{
  uint16_t val = getUInt(data, 0, 1);
  int16_t angle = scale(val, 27900, 37900, -100, 100);
  wheel = angle;
}