#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace daikin_ags {

// Values for Daikin ARC43XXX IR Controllers
// Temperature
const uint8_t DAIKIN_AGS_TEMP_MIN = 16;  // Celsius
const uint8_t DAIKIN_AGS_TEMP_MAX = 30;  // Celsius

// Modes
const uint8_t DAIKIN_AGS_MODE_AUTO = 0b1010;
const uint8_t DAIKIN_AGS_MODE_COOL = 0b0010;
const uint8_t DAIKIN_AGS_MODE_HEAT = 0b1000;
const uint8_t DAIKIN_AGS_MODE_DRY = 0b0001;
const uint8_t DAIKIN_AGS_MODE_FAN = 0b0100;
const uint8_t DAIKIN_AGS_MODE_OFF = 0x00;
const uint8_t DAIKIN_AGS_MODE_ON = 0x01;

// Fan Speed
const uint8_t DAIKIN_AGS_FAN_AUTO = 0b0001;
const uint8_t DAIKIN_AGS_FAN_SILENT = 0b1001;
const uint8_t DAIKIN_AGS_FAN_1 = 0b1000;
const uint8_t DAIKIN_AGS_FAN_2 = 0b0100;
const uint8_t DAIKIN_AGS_FAN_3 = 0b0010;
const uint8_t DAIKIN_AGS_FAN_4 = 0b0011;

// IR Transmission
const uint32_t DAIKIN_AGS_IR_FREQUENCY = 38000;
const uint32_t DAIKIN_AGS_LEADER_MARK = 9800;
const uint32_t DAIKIN_AGS_LEADER_SPACE = 9700;
const uint32_t DAIKIN_AGS_HEADER_MARK = 4600;
const uint32_t DAIKIN_AGS_HEADER_SPACE = 2500;
const uint32_t DAIKIN_AGS_BIT_MARK = 390;
const uint32_t DAIKIN_AGS_ONE_SPACE = 950;
const uint32_t DAIKIN_AGS_ZERO_SPACE = 380;
const uint32_t DAIKIN_AGS_MESSAGE_SPACE = 20300;
const uint32_t DAIKIN_AGS_FOOTER_MARK = 4600;

// State Frame size
const uint8_t DAIKIN_AGS_STATE_FRAME_SIZE = 8;

class DaikinAgsClimate : public climate_ir::ClimateIR {
 public:
  DaikinAgsClimate()
      : climate_ir::ClimateIR(DAIKIN_AGS_TEMP_MIN, DAIKIN_AGS_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}

 protected:
  // Transmit via IR the state of this climate controller.
  void transmit_state() override;
  uint8_t operation_mode_();
  uint8_t fan_speed_();
  uint8_t temperature_();
  uint8_t extra_modes_();
  uint8_t calculate_checksum_(const uint8_t frame[]);
  // Overide control to check if togling power is required
  void control(const climate::ClimateCall &call) override;

  // Handle received IR Buffer
  bool parse_state_frame_(const uint8_t frame[]);
  bool on_receive(remote_base::RemoteReceiveData data);

  // Others
  uint8_t bcdToUint8(const uint8_t bcd);
  uint8_t uint8ToBcd(const uint8_t dec);
  bool shouldTogglePower = false;
};

}  // namespace daikin_ags
}  // namespace esphome