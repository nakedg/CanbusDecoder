#include <inttypes.h>
#include "Car.h"

class CanboxRaiseHandler
{
    private:
        Car *_car;
        void DoorProcess();
        void CarInfoProcess();
        void WheelInfoProcess();
        void ExecuteCmd(const uint8_t *rxBuffer, uint8_t len);
    public:
        void CmdProcess(uint8_t ch);
        void SetCar(Car *car);
        void SendCarState();
        void SendCanboxMessage(uint8_t type, uint8_t *msg, uint8_t size);
};