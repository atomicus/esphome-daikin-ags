#include "daikin_ags.h"
#include "esphome/core/log.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome.h"

namespace esphome
{
  namespace daikin_ags
  {

    static const char *const TAG = "daikin_ags.climate";

    void DaikinAgsClimate::transmit_state()
    {
      uint8_t remote_state[8] = {0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

      // Mode and fan
      remote_state[1] = this->operation_mode_() | this->fan_speed_();
      // clock mins
      remote_state[2] = this->uint8ToBcd(00);
      // clock hours
      remote_state[3] = this->uint8ToBcd(12);
      // Timer on
      remote_state[4] = this->uint8ToBcd(00);
      // Timer Off
      remote_state[5] = this->uint8ToBcd(00);
      // Temp
      remote_state[6] = this->temperature_();
      // Additional options
      remote_state[7] = this->extra_modes_();
      // Since we did sent a toogle power, we need to clear it.
      uint8_t checksum = this->calculate_checksum_(remote_state);
      // Special modes & checksum
      remote_state[7] = this->extra_modes_() | checksum;

      auto transmit = this->transmitter_->transmit();
      auto *data = transmit.get_data();
      data->set_carrier_frequency(DAIKIN_AGS_IR_FREQUENCY);

      // Send 4 leader pulses
      data->mark(DAIKIN_AGS_LEADER_MARK);
      data->space(DAIKIN_AGS_LEADER_SPACE);
      data->mark(DAIKIN_AGS_LEADER_MARK);
      data->space(DAIKIN_AGS_LEADER_SPACE);

      // Send header
      data->mark(DAIKIN_AGS_HEADER_MARK);
      data->space(DAIKIN_AGS_HEADER_SPACE);

      // Send first bit constant data
      for (int i = 0; i < 1; i++)
      {
        for (uint8_t mask = 1; mask > 0; mask <<= 1)
        { // iterate through bit mask
          data->mark(DAIKIN_AGS_BIT_MARK);
          bool bit = remote_state[i] & mask;
          data->space(bit ? DAIKIN_AGS_ONE_SPACE : DAIKIN_AGS_ZERO_SPACE);
        }
      }

      for (int i = 1; i < 8; i++)
      {
        for (uint8_t mask = 1; mask > 0; mask <<= 1)
        { // iterate through bit mask
          data->mark(DAIKIN_AGS_BIT_MARK);
          bool bit = remote_state[i] & mask;
          data->space(bit ? DAIKIN_AGS_ONE_SPACE : DAIKIN_AGS_ZERO_SPACE);
        }
      }
      data->mark(DAIKIN_AGS_BIT_MARK);

      // Send footer
      data->space(DAIKIN_AGS_MESSAGE_SPACE);
      data->mark(DAIKIN_AGS_FOOTER_MARK);
      data->space(0);
      // Uncomment follwing line and change pin number to a pin corresponding to your receiver data signal
      // This will disable receiver for time of transmission. Usefull if your HW rec. Diode sees transmit one.
      // pinMode(4, OUTPUT);
      transmit.perform();
      // pinMode(4, INPUT_PULLUP);
      this->shouldTogglePower = false;
    }

    uint8_t DaikinAgsClimate::operation_mode_()
    {
      uint8_t operating_mode = DAIKIN_AGS_MODE_ON;
      switch (this->mode)
      {
      case climate::CLIMATE_MODE_COOL:
        operating_mode = DAIKIN_AGS_MODE_COOL;
        break;
      case climate::CLIMATE_MODE_DRY:
        operating_mode = DAIKIN_AGS_MODE_DRY;
        break;
      case climate::CLIMATE_MODE_HEAT:
        operating_mode = DAIKIN_AGS_MODE_HEAT;
        break;
      case climate::CLIMATE_MODE_HEAT_COOL:
        operating_mode = DAIKIN_AGS_MODE_AUTO;
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
        operating_mode = DAIKIN_AGS_MODE_FAN;
        break;
      case climate::CLIMATE_MODE_OFF:
      default:
        operating_mode = DAIKIN_AGS_MODE_OFF;
        break;
      }

      return operating_mode;
    }

    uint8_t DaikinAgsClimate::fan_speed_()
    {
      uint8_t fan_speed;
      switch (this->fan_mode.value())
      {
      case climate::CLIMATE_FAN_LOW:
        fan_speed = DAIKIN_AGS_FAN_1 << 4;
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        fan_speed = DAIKIN_AGS_FAN_3 << 4;
        break;
      case climate::CLIMATE_FAN_HIGH:
        fan_speed = DAIKIN_AGS_FAN_4 << 4;
        break;
      case climate::CLIMATE_FAN_AUTO:
      default:
        fan_speed = DAIKIN_AGS_FAN_AUTO << 4;
      }
      return fan_speed;
    }

    uint8_t DaikinAgsClimate::temperature_()
    {
      // Force special temperatures depending on the mode
      switch (this->mode)
      {
      case climate::CLIMATE_MODE_FAN_ONLY:
        return 0x32;
      case climate::CLIMATE_MODE_DRY:
        return 0xc0;
      default:
        uint8_t temperature =
            (uint8_t)roundf(clamp<float>(this->target_temperature, DAIKIN_AGS_TEMP_MIN, DAIKIN_AGS_TEMP_MAX));
        return ((temperature / 10) << 4) + (temperature % 10);
      }
    }

    uint8_t DaikinAgsClimate::extra_modes_()
    {
      uint8_t extra_modes = 0x00;
      if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL)
      {
        extra_modes |= 0b00000001;
      }
      if (this->shouldTogglePower)
      {
        extra_modes |= 0b00001000;
      }
      return extra_modes;
    }

    /**
     * Calculates the checksum of a frame.
     * It's sum of int's made of each 4 bits of frame.
     **/
    uint8_t DaikinAgsClimate::calculate_checksum_(const uint8_t frame[DAIKIN_AGS_STATE_FRAME_SIZE])
    {
      uint8_t checksum = 0x00;
      for (int i = 0; i < DAIKIN_AGS_STATE_FRAME_SIZE; i++)
      {
        // Last 4 bytes of frame contain checksum itself, we don't count it.
        if (i != DAIKIN_AGS_STATE_FRAME_SIZE - 1)
        {
          checksum += (frame[i] & 0xF0) >> 4;
        }
        checksum += frame[i] & 0x0F;
      }
      // Use only last bits of checksum
      checksum = checksum & 0x0F;
      // Move it to first bits as it will get inversed
      checksum = checksum << 4;

      return checksum;
    }

    void DaikinAgsClimate::control(const climate::ClimateCall &call)
    {
      // If we want to turn it off, and it's actually non-off, set toggle power

      if (call.get_mode().has_value())
      {
        if (call.get_mode() == climate::CLIMATE_MODE_OFF && this->mode != climate::CLIMATE_MODE_OFF)
        {
          this->shouldTogglePower = true;
          ESP_LOGCONFIG("daikin", "  Requested mode was OFF, but device was ON.");
        }
        // Analogically, if other way round
        if (call.get_mode() != climate::CLIMATE_MODE_OFF && this->mode == climate::CLIMATE_MODE_OFF)
        {
          this->shouldTogglePower = true;
          ESP_LOGCONFIG("daikin", "  Requested mode was ON, but device was OFF. Requested 0x%02X", call.get_mode());
        }
      }
      ClimateIR::control(call);
    }

    bool DaikinAgsClimate::parse_state_frame_(const uint8_t frame[])
    {
      uint8_t clk_checksum = this->calculate_checksum_(frame);
      uint8_t rcv_checksum = frame[7] & 0xF0;

      if (rcv_checksum != clk_checksum)
      {
        return false;
      }

      uint8_t mode = (frame[1] & 0x0F);

      // Now it gets a bit confusing.
      // If power bit is set to 1 we should turn it off if it was on,
      // or turn on, if was off with all correct settings.
      // if power bit is not set, we can't turn it switch the power of device
      // So you can program it, but not power it up.

      bool shouldTooglePower = frame[7] & 0x08;

      if (shouldTooglePower && this->mode != climate::CLIMATE_MODE_OFF)
      {
        // override mode
        mode = DAIKIN_AGS_MODE_OFF;
        ESP_LOGCONFIG("daikin", "  Switching off");
      }
      if (!shouldTooglePower && this->mode == climate::CLIMATE_MODE_OFF)
      {
        // override mode
        mode = DAIKIN_AGS_MODE_OFF;
        ESP_LOGCONFIG("daikin", "  Keeping off");
      }

      switch (mode)
      {
      case DAIKIN_AGS_MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case DAIKIN_AGS_MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case DAIKIN_AGS_MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case DAIKIN_AGS_MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      case DAIKIN_AGS_MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case DAIKIN_AGS_MODE_OFF:
        this->mode = climate::CLIMATE_MODE_OFF;
        break;
      }

      // Read fan
      uint8_t fan = (frame[1] & 0xF0) >> 4;
      switch (fan)
      {
      case DAIKIN_AGS_FAN_SILENT:
      case DAIKIN_AGS_FAN_1:
        this->fan_mode = climate::CLIMATE_FAN_LOW;
        break;
      case DAIKIN_AGS_FAN_2:
        this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
        break;
      case DAIKIN_AGS_FAN_3:
      case DAIKIN_AGS_FAN_4:
        this->fan_mode = climate::CLIMATE_FAN_HIGH;
        break;
      case DAIKIN_AGS_FAN_AUTO:
        this->fan_mode = climate::CLIMATE_FAN_AUTO;
      }

      this->target_temperature = this->bcdToUint8(frame[6]);

      if (frame[7] & 0x01)
      {
        this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
      }
      else
      {
        this->swing_mode = climate::CLIMATE_SWING_OFF;
      }

      this->publish_state();
      return true;
    }

    bool DaikinAgsClimate::on_receive(remote_base::RemoteReceiveData data)
    {
      uint8_t state_frame[DAIKIN_AGS_STATE_FRAME_SIZE] = {};
      if (!data.expect_item(DAIKIN_AGS_LEADER_MARK, DAIKIN_AGS_LEADER_SPACE))
      {
        return false;
      }
      if (!data.expect_item(DAIKIN_AGS_LEADER_MARK, DAIKIN_AGS_LEADER_SPACE))
      {
        return false;
      }
      if (!data.expect_item(DAIKIN_AGS_HEADER_MARK, DAIKIN_AGS_HEADER_SPACE))
      {
        return false;
      }

      for (uint8_t pos = 0; pos < DAIKIN_AGS_STATE_FRAME_SIZE; pos++)
      {
        uint8_t byte = 0;
        for (int8_t bit = 0; bit < 8; bit++)
        {
          if (data.expect_item(DAIKIN_AGS_BIT_MARK, DAIKIN_AGS_ONE_SPACE))
          {
            byte |= 1 << bit;
          }
          else if (!data.expect_item(DAIKIN_AGS_BIT_MARK, DAIKIN_AGS_ZERO_SPACE))
          {
            return false;
          }
        }
        state_frame[pos] = byte;
        if (pos == 0)
        {
          // frame header
          if (byte != 0x16)
            return false;
        }
      }
      ESP_LOGCONFIG("daikin", "  Frame received");

      return this->parse_state_frame_(state_frame);
    }

    uint8_t DaikinAgsClimate::bcdToUint8(const uint8_t bcd)
    {
      if (bcd > 0x99)
        return 255; // Too big.
      return (bcd >> 4) * 10 + (bcd & 0xF);
    }

    uint8_t DaikinAgsClimate::uint8ToBcd(uint8_t val) { return (((val / 10) << 4) + (val % 10)); }

  } // namespace daikin_ags
} // namespace esphome