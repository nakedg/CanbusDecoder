#include "CanboxRaiseHandler.h"
#include "string.h"
#include "driver/uart.h"
#include "driver/twai.h"
#include "esp_log.h"

#define RX_BUFFER_SIZE 32
static const char *TAG = "CanboxRaiseHandler";

static float scale(float value, float inMin, float inMax, float outMin, float outMax)
{
  float part1 = (value - inMin) * (outMax - outMin);
  float part2 = (inMax - inMin);
  float res = (part1 / part2) + outMin;
  return res;
}

static uint8_t GetChecksum(uint8_t * buf, uint8_t len)
{
	uint8_t sum = 0;
	for (uint8_t i = 0; i < len; i++)
		sum += buf[i];

	sum = sum ^ 0xff;

	return sum;
}

enum rx_state
{
	RX_WAIT_START,
	RX_LEN,
	RX_CMD,
	RX_DATA,
	RX_CRC
};


static uint8_t rx_buffer[RX_BUFFER_SIZE];
static uint8_t rx_idx = 0;
static uint8_t rx_state = RX_WAIT_START;

void CanboxRaiseHandler::CmdProcess(uint8_t ch)
{
    switch (rx_state)
    {
        case RX_WAIT_START:
            if (ch != 0x2e)
                break;

            rx_idx = 0;
            rx_buffer[rx_idx++] = ch;
            rx_state = RX_CMD;
            break;
        
        case RX_CMD:
            rx_buffer[rx_idx++] = ch;
            rx_state = RX_LEN;
            break;

        case RX_LEN:
            rx_buffer[rx_idx++] = ch;
            rx_state = ch ? RX_DATA : RX_CRC;
            break;

        case RX_DATA:
            rx_buffer[rx_idx++] = ch;
            {
                uint8_t len = rx_buffer[2];
                rx_state = ((rx_idx - 2) > len) ? RX_CRC : RX_DATA;
            }
            break;

        case RX_CRC:
            rx_buffer[rx_idx++] = ch;
            rx_state = RX_WAIT_START;
            {
                uint8_t ack = 0xff;
                uart_write_bytes(UART_NUM_1, &ack, 1);

                char buf[64];
                uint8_t cmd = rx_buffer[1];
                snprintf(buf, sizeof(buf), "\r\nnew cmd %" PRIx8 "\r\n", cmd);
                uart_write_bytes(UART_NUM_1, &buf, strlen(buf));
            }
            ExecuteCmd(rx_buffer, rx_idx);
            break;
    }
}

void CanboxRaiseHandler::SetCar(Car *car)
{
    _car = car;
}

void CanboxRaiseHandler::SendCarState()
{
  DoorProcess();
  CarInfoProcess();
  WheelInfoProcess();
}

void CanboxRaiseHandler::SendCanboxMessage(uint8_t type, uint8_t *msg, uint8_t size)
{
    uint8_t buf[4/*header type size ... chksum*/ + size];
    buf[0] = 0x2e;
    buf[1] = type;
    buf[2] = size;
    memcpy(buf + 3, msg, size);
    buf[3 + size] = GetChecksum(buf + 1, size + 2);
    uart_write_bytes(UART_NUM_1, &buf, sizeof(buf));
}

void CanboxRaiseHandler::DoorProcess()
{
    uint8_t fl_door = _car->fl_door;
    uint8_t fr_door = _car->fr_door;
    uint8_t rl_door = _car->rl_door;
    uint8_t rr_door = _car->rr_door;
    uint8_t tailgate = _car->tailgate;
    uint8_t bonnet = _car->bonnet;

    uint8_t state = 0;

    if (bonnet)
        state |= 0x20;
    if (tailgate)
        state |= 0x10;
    if (rr_door)
        state |= 0x08;
    if (rl_door)
        state |= 0x04;
    if (fr_door)
        state |= 0x02;
    if (fl_door)
        state |= 0x01;

    uint8_t buf[] = { 0x01, state };
    SendCanboxMessage(0x41, buf, sizeof(buf));
}

void CanboxRaiseHandler::CarInfoProcess()
{
    uint16_t taho = _car->taho;
    uint8_t t1 = (taho >> 8) & 0xff;
    uint8_t t2 = taho & 0xff;
    uint16_t speed = _car->speed * 100;
    uint8_t t3 = (speed >> 8) & 0xff;
    uint8_t t4 = speed & 0xff;
    uint16_t voltage = 0.12 * 100;
    uint8_t t5 = (voltage >> 8) & 0xff;
    uint8_t t6 = voltage & 0xff;
    uint16_t temp = _car->temp;
    uint8_t t7 = (temp >> 8) & 0xff;
    uint8_t t8 = temp & 0xff;
    uint32_t odo = _car->odometer;
    uint8_t t9 = (odo >> 16) & 0xff;
    uint8_t t10 = (odo >> 8) & 0xff;
    uint8_t t11 = odo & 0xff;
    uint8_t t12 = _car->fuel_lvl;

    uint8_t buf[13] = { 0x02, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12 };

    SendCanboxMessage(0x41, buf, sizeof(buf));

    uint8_t state = 0;
    static uint8_t low_state = 0;

    uint8_t low_voltage = _car->low_voltage;
    uint8_t low_fuel = _car->low_fuel_lvl;

    if (low_fuel)
        state |= 0x80;
    if (low_voltage)
        state |= 0x40;

    uint8_t buf_low[] = { 0x03, state };

    if (state != low_state) {

        low_state = state;
        SendCanboxMessage(0x41, buf_low, sizeof(buf_low));
        ESP_LOGI(TAG, "Send low state %u", state);
    }
}

void CanboxRaiseHandler::WheelInfoProcess()
{
    int16_t sangle = scale(_car->wheel, -100, 100, -540, 540);
    ESP_LOGI(TAG, "Wheel val %i, angle %i", _car->wheel, sangle);
    uint8_t wbuf[] = { (uint8_t)sangle, (uint8_t)(sangle >> 8) };
    SendCanboxMessage(0x26, wbuf, sizeof(wbuf));
}

void CanboxRaiseHandler::ExecuteCmd(const uint8_t *rxBuffer, uint8_t len)
{
    uint8_t cmd = rx_buffer[1];
    if (cmd == 0xEE)
    {
        ESP_LOGI(TAG, "Receive test message");
        uint8_t data[8];
        uint32_t id = 0;
        id = (id << 8) + rx_buffer[3];
        id = (id << 8) + rx_buffer[4];
        id = (id << 8) + rx_buffer[5];
        id = (id << 8) + rx_buffer[6];
        memcpy(&data, rxBuffer + 7, 8);
        twai_message_t message = { 
            .identifier = rx_buffer[4],
            .data = *data,
        };

        _car->ProcessCanMessage(&message);
        SendCarState();
    }
}
