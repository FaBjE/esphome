#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace hbridge {

enum ValveActuatorRestoreMode {
  HBRIDGE_VALVE_ACTUATOR_RESTORE_DEFAULT_OFF,
  HBRIDGE_VALVE_ACTUATOR_RESTORE_DEFAULT_ON,
  HBRIDGE_VALVE_ACTUATOR_ALWAYS_OFF,
  HBRIDGE_VALVE_ACTUATOR_ALWAYS_ON,
  HBRIDGE_VALVE_ACTUATOR_RESTORE_INVERTED_DEFAULT_OFF,
  HBRIDGE_VALVE_ACTUATOR_RESTORE_INVERTED_DEFAULT_ON,
};

class HBridgeValveActuator : public switch_::Switch, public Component {
 public:
  void set_pina_pin(GPIOPin *pina_pin) { pina_pin_ = pina_pin; }
  void set_pinb_pin(GPIOPin *pinb_pin) { pinb_pin_ = pinb_pin; }
  void set_actuator_switching_time(uint32_t actuator_switching_time) { actuator_switching_time_ = actuator_switching_time; }
  void set_restore_mode(ValveActuatorRestoreMode restore_mode);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  float get_setup_priority() const override;

  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;

  GPIOPin *pina_pin_;
  GPIOPin *pinb_pin_;

  ValveActuatorRestoreMode restore_mode_{HBRIDGE_VALVE_ACTUATOR_RESTORE_DEFAULT_OFF};
  
  uint32_t actuator_switching_time_{15000}; //Default 15 seconds, these actuators are really slow due to gearing
};

}  // namespace hbridge
}  // namespace esphome
