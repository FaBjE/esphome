#include "hbridge_valve_actuator.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hbridge {

static const char *const TAG = "hbridge.valve-actuator";

float HBridgeValveActuator::get_setup_priority() const { return setup_priority::HARDWARE; }

void HBridgeValveActuator::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Valve Actuator: '%s'...", this->name_.c_str());

  bool initial_state = false;
  switch (this->restore_mode_) {
    case HBRIDGE_VALVE_ACTUATOR_RESTORE_DEFAULT_OFF:
      initial_state = this->get_initial_state().value_or(false);
      break;
    case HBRIDGE_VALVE_ACTUATOR_RESTORE_DEFAULT_ON:
      initial_state = this->get_initial_state().value_or(true);
      break;
    case HBRIDGE_VALVE_ACTUATOR_RESTORE_INVERTED_DEFAULT_OFF:
      initial_state = !this->get_initial_state().value_or(true);
      break;
    case HBRIDGE_VALVE_ACTUATOR_RESTORE_INVERTED_DEFAULT_ON:
      initial_state = !this->get_initial_state().value_or(false);
      break;
    case HBRIDGE_VALVE_ACTUATOR_ALWAYS_OFF:
      initial_state = false;
      break;
    case HBRIDGE_VALVE_ACTUATOR_ALWAYS_ON:
      initial_state = true;
      break;
  }

  // write state before setup
  if (initial_state)
    this->turn_on();
  else
    this->turn_off();
  this->pin_->setup();
  // write after setup again for other IOs
  if (initial_state)
    this->turn_on();
  else
    this->turn_off();
}


void HBridgeValveActuator::dump_config() {
  LOG_SWITCH("", "HBridge Valve Actuator", this);
  LOG_PIN("  Pin A: ", this->pina_pin_);
  LOG_PIN("  Pin B: ", this->pinb_pin_);
  const LogString *restore_mode = LOG_STR("");
  switch (this->restore_mode_) {
    case HBRIDGE_VALVE_ACTUATOR_RESTORE_DEFAULT_OFF:
      restore_mode = LOG_STR("Restore (Defaults to OFF)");
      break;
    case HBRIDGE_VALVE_ACTUATOR_RESTORE_DEFAULT_ON:
      restore_mode = LOG_STR("Restore (Defaults to ON)");
      break;
    case HBRIDGE_VALVE_ACTUATOR_RESTORE_INVERTED_DEFAULT_ON:
      restore_mode = LOG_STR("Restore inverted (Defaults to ON)");
      break;
    case HBRIDGE_VALVE_ACTUATOR_RESTORE_INVERTED_DEFAULT_OFF:
      restore_mode = LOG_STR("Restore inverted (Defaults to OFF)");
      break;
    case HBRIDGE_VALVE_ACTUATOR_ALWAYS_OFF:
      restore_mode = LOG_STR("Always OFF");
      break;
    case HBRIDGE_VALVE_ACTUATOR_ALWAYS_ON:
      restore_mode = LOG_STR("Always ON");
      break;
  }
  ESP_LOGCONFIG(TAG, "  Restore Mode: %s", LOG_STR_ARG(restore_mode));
  if (!this->interlock_.empty()) {
    ESP_LOGCONFIG(TAG, "  Interlocks:");
    for (auto *lock : this->interlock_) {
      if (lock == this)
        continue;
      ESP_LOGCONFIG(TAG, "    %s", lock->get_name().c_str());
    }
  }
}


void HBridgeValveActuator::write_state(bool state) {

  // cancel any pending "state changes"
  this->cancel_timeout("switching-actuator");

  //Set output states to desired state (direction is relative, can be inverted by config or wiring)
  this->pin_->pina_pin_(state);
  this->pin_->pinb_pin_(!state);

  //The actuator takes some time to switch to its new state, set a timeout to publish new state
  this->set_timeout("switching-actuator", this->actuator_switching_time_, [this, state] {
    //After the switching delay, publish new state
    this->publish_state(state);
  });  

}

void HBridgeValveActuator::set_restore_mode(ValveActuatorRestoreMode restore_mode) { this->restore_mode_ = restore_mode; }

}  // namespace gpio
}  // namespace esphome
