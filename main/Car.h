#pragma once
#include <inttypes.h>
#include "driver/twai.h"

class Car
{
  public:
    uint32_t taho;
    uint32_t speed;

    int8_t wheel;

    uint8_t fl_door;
    uint8_t fr_door;
    uint8_t rl_door;
    uint8_t rr_door;
    uint8_t bonnet;
    uint8_t tailgate;

    uint32_t odometer;
    uint32_t voltage;
    uint8_t low_voltage;
    uint32_t temp;
    uint8_t fuel_lvl;
    uint8_t low_fuel_lvl;
    void InitCar();

    void ProcessCanMessage(const twai_message_t *frame);
    void WriteCan(const twai_message_t *frame);
  private:
    void parseDoors(const uint8_t *data);
    void parseEngineRpm(const uint8_t *data);
    void parseSpeed(const uint8_t  *data);
    void parseOdometer(const uint8_t *data);
    void parseEngineTemp(const uint8_t *data);
    void parseFuelLevel(const uint8_t *data);
    void parseWheelPosition(const uint8_t *data);
};