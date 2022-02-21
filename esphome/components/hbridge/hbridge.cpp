#include "hbridge.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cmath>

namespace esphome {
namespace hbridge {

static const char *const TAG = "hbridge";

void HBridge::setup() {
  // Always start in off-state
  set_state(HBridgeMode::OFF, 0);
}

void HBridge::loop() {
  float new_dutycycle = 0;
  bool dutycycle_transition_done = false;

  // If we are in a transition
  if (this->transition_state_ != TransitionState::OFF) {
    // Calculate the time between this iteration and the previous one
    uint32_t mode_duration = millis() - this->transition_mode_start_time_;
    uint32_t ms_since_last_step = millis() - this->transition_last_step_time_;

    switch (this->transition_state_) {
      case TransitionState::SHORTING_BUILDUP:
        // Apply next step
        this->transition_shorting_dutycycle_ +=
            (this->transition_shorting_dutycycle_delta_per_ms_ * ms_since_last_step);
        set_output_state_(HBridgeMode::SHORT, this->transition_shorting_dutycycle_);

        // If reason to go to next mode (Timeout, or target duty cycle reached)
        if (mode_duration > this->transition_shorting_buildup_duration_ms_ ||
            (this->transition_target_mode_ == HBridgeMode::SHORT &&
             this->transition_shorting_dutycycle_ >= this->transition_target_mode_dutycycle_)) {
          // Determine next mode
          if (transition_full_short_duration_ms_ > 0) {
            // We need to full short for a while
            this->transition_state_ = TransitionState::FULL_SHORT;
            ESP_LOGD(TAG, "Transition mode (short buildup > full short)");
          } else if (this->transition_target_mode_ == HBridgeMode::SHORT) {
            // We have reached our target mode
            set_output_state_(this->transition_target_mode_, this->transition_target_mode_dutycycle_);
            this->transition_state_ = TransitionState::OFF;
            ESP_LOGD(TAG, "Transition done (buildup >> short)");
          } else {
            // We need to transition to new (active) dutycycle
            this->transition_state_ = TransitionState::DUTYCYCLE_TRANSITIONING;
            ESP_LOGD(TAG, "Transition mode (short buildup > duty change)");
          }
          this->transition_mode_start_time_ = millis();
        }
        break;

      case TransitionState::FULL_SHORT:
        // Apply full short
        set_output_state_(HBridgeMode::SHORT, (float) std::fabs(this->transition_target_relative_dutycycle_));

        // If reason to go to next mode (timeout)
        if (mode_duration > this->transition_full_short_duration_ms_) {
          if (this->transition_target_mode_ == HBridgeMode::SHORT) {
            // We have reached our target mode
            set_output_state_(this->transition_target_mode_, this->transition_target_mode_dutycycle_);
            this->transition_state_ = TransitionState::OFF;
            ESP_LOGD(TAG, "Transition done (full short >> short)");
          } else {
            // We need to transition to new (active) dutycycle
            this->transition_state_ = TransitionState::DUTYCYCLE_TRANSITIONING;
            ESP_LOGD(TAG, "Transition mode (full short > duty change)");
          }
          this->transition_mode_start_time_ = millis();
        }
        break;

      case TransitionState::DUTYCYCLE_TRANSITIONING:
        // Apply next step
        new_dutycycle = this->current_relative_dutycycle_ +
                        (this->transition_relative_dutycycle_delta_per_ms_ * ms_since_last_step);
        dutycycle_transition_done = false;

        // Range limit by target relative dutycycle
        if (this->transition_relative_dutycycle_delta_per_ms_ >= 0) {
          // Incrementing transition, limit to upper
          if (new_dutycycle >= this->transition_target_relative_dutycycle_) {
            new_dutycycle = this->transition_target_relative_dutycycle_;
            dutycycle_transition_done = true;
          }
        } else {
          // Decrementing transition, limit to lower
          if (new_dutycycle <= this->transition_target_relative_dutycycle_) {
            new_dutycycle = this->transition_target_relative_dutycycle_;
            dutycycle_transition_done = true;
          }
        }

        set_output_state_by_relative_dutycycle_(new_dutycycle);

        // If reason to go to next mode
        if (dutycycle_transition_done) {
          // Set final mode
          set_output_state_(this->transition_target_mode_, this->transition_target_mode_dutycycle_);
          this->transition_state_ = TransitionState::OFF;
          ESP_LOGD(TAG, "Transition done (duty change)");
        }
        break;

      default:
      case TransitionState::OFF:
        // Nothing to do here (Supress warning)
        break;
    }

    // Store last step time
    this->transition_last_step_time_ = millis();
  }
}

void HBridge::dump_config() {
  ESP_LOGCONFIG(TAG, "HBridge:");
  ESP_LOGCONFIG(TAG, "   Transition delta per ms: %f ", this->setting_transition_delta_per_ms_);
  ESP_LOGCONFIG(TAG, "   Transition shorting buildup duration: %d ms",
                this->setting_transition_shorting_buildup_duration_ms_);
  ESP_LOGCONFIG(TAG, "   Transition full short duration: %d ms", this->setting_transition_full_short_duration_ms_);
}

void HBridge::transition_to_state(HBridgeMode target_mode, float target_dutycycle, float dutycycle_delta_per_ms,
                                  uint32_t shorting_buildup_duration_ms = 0, uint32_t full_short_duration_ms = 0) {
  // --- Behaviour Examples:
  // Transition - Direction A 100% to Direction A 20% (No shorting is applied regardless of values):
  // |           A100% >> A20%            |
  // |< ---  transition_duration_ms  --- >|
  //
  // Transition - Direction A 100% to Direction B 100%, No shorting (duration set to 0):
  // | A100% >> A0% | Idle | B0% >> B100% |
  // |< ---   transition_duration_ms --- >|
  //
  // Transition - Direction A 100% to 0%, With shorting (duration set > 0):
  // | A0% |  Shorting 0% >> Shorting 100%  |       Shorting 100%      |
  //       |< shorting_buildup_duration_ms >|
  //                                        |< full_short_duration_ms >|
  //
  // Transition - Direction A 100% to Direction B 100%, With shorting (duration set > 0):
  // | A0% |  Shorting 0% >> Shorting 100%  |       Shorting 100%      |       B0% >> B100%       |
  //       |< shorting_buildup_duration_ms >|
  //                                        |< full_short_duration_ms >|
  //                                                                   |< transition_duration_ms >|

  // Store targets
  this->transition_target_mode_ = target_mode;
  this->transition_target_mode_dutycycle_ = target_dutycycle;

  // Calculate relative dutycycle based on target mode/dutycycle
  if (this->transition_target_mode_ == HBridgeMode::OFF || this->transition_target_mode_ == HBridgeMode::SHORT) {
    this->transition_target_relative_dutycycle_ = 0;
  } else if (this->transition_target_mode_ == HBridgeMode::DIRECTION_A) {
    this->transition_target_relative_dutycycle_ = target_dutycycle * -1;
  } else if (this->transition_target_mode_ == HBridgeMode::DIRECTION_B) {
    this->transition_target_relative_dutycycle_ = target_dutycycle;
  }

  // Check if transition crosses zero point and shorting procedure is wished by the given function arguments
  bool transition_crosses_zero_point_with_shorting = false;
  if (((this->current_relative_dutycycle_ > 0 && this->transition_target_relative_dutycycle_ <= 0) ||
       (this->current_relative_dutycycle_ < 0 && this->transition_target_relative_dutycycle_ >= 0)) &&
      (shorting_buildup_duration_ms > 0 || full_short_duration_ms > 0)) {
    // Enable (possible) shorting for this procedure
    transition_crosses_zero_point_with_shorting = true;
    this->transition_shorting_buildup_duration_ms_ = shorting_buildup_duration_ms;
    this->transition_full_short_duration_ms_ = full_short_duration_ms;

    // Calculate shorting buildup step size
    if (this->transition_shorting_buildup_duration_ms_ > 0) {
      if (this->transition_target_mode_ == HBridgeMode::SHORT) {
        // If short is our target mode, transition to given dutycycle
        this->transition_shorting_dutycycle_delta_per_ms_ =
            (float) (this->transition_target_mode_dutycycle_ / (float) this->transition_shorting_buildup_duration_ms_);
      } else {
        // Otherwise transition to full short
        this->transition_shorting_dutycycle_delta_per_ms_ =
            (float) (1.0f / (float) this->transition_shorting_buildup_duration_ms_);
      }
    } else {
      // No shorting buildup
      this->transition_shorting_dutycycle_delta_per_ms_ = 0;
    }

    ESP_LOGD(TAG, "Transition: SBuildup: %d ms (%f /ms), FullS: %d", this->transition_shorting_buildup_duration_ms_,
             this->transition_shorting_dutycycle_delta_per_ms_, this->transition_full_short_duration_ms_);
  } else {
    // No shorting procedure for this transition
    transition_crosses_zero_point_with_shorting = false;
    this->transition_shorting_buildup_duration_ms_ = 0;
    this->transition_full_short_duration_ms_ = 0;
    this->transition_shorting_dutycycle_delta_per_ms_ = 0;
  }

  // Calculate step delta (direction)
  if (transition_crosses_zero_point_with_shorting) {
    // If shorting we start from 0 dutycycle
    if (this->transition_target_relative_dutycycle_ < 0) {
      // We need to decrement
      this->transition_relative_dutycycle_delta_per_ms_ = (dutycycle_delta_per_ms * -1);
    } else {
      // We need to increment
      this->transition_relative_dutycycle_delta_per_ms_ = dutycycle_delta_per_ms;
    }
  } else {
    // If no shorting we start from current dutycycle
    if (this->transition_target_relative_dutycycle_ < this->current_relative_dutycycle_) {
      // We need to decrement
      this->transition_relative_dutycycle_delta_per_ms_ = (dutycycle_delta_per_ms * -1);
    } else {
      // We need to increment
      this->transition_relative_dutycycle_delta_per_ms_ = dutycycle_delta_per_ms;
    }
  }

  // Determine first step of transition
  if (transition_crosses_zero_point_with_shorting && this->transition_shorting_buildup_duration_ms_ > 0) {
    // If the transition start with shorting buildup, disable output now
    set_output_state_(HBridgeMode::OFF, 0);

    // Set start state
    this->transition_shorting_dutycycle_ = 0;
    this->transition_state_ = TransitionState::SHORTING_BUILDUP;
  } else if (transition_crosses_zero_point_with_shorting && this->transition_full_short_duration_ms_ > 0) {
    // If the transition starts with full short, short output now
    set_output_state_(HBridgeMode::SHORT, 1);

    // Set start state
    this->transition_shorting_buildup_duration_ms_ = 0;
    this->transition_state_ = TransitionState::FULL_SHORT;
  } else if (this->transition_relative_dutycycle_delta_per_ms_ <
                 (float) std::fabs(this->current_relative_dutycycle_ - this->transition_target_relative_dutycycle_) &&
             this->transition_relative_dutycycle_delta_per_ms_ != 0) {
    // If the transition starts with only dutycycle transitioning, dont change anything now

    // Set state
    this->transition_shorting_buildup_duration_ms_ = 0;
    this->transition_full_short_duration_ms_ = 0;
    this->transition_state_ = TransitionState::DUTYCYCLE_TRANSITIONING;
  } else {
    // This is actually not a transition, just a hard state change.

    // call-off transition
    this->transition_state_ = TransitionState::OFF;
    this->transition_shorting_buildup_duration_ms_ = 0;
    this->transition_full_short_duration_ms_ = 0;
    this->transition_shorting_dutycycle_delta_per_ms_ = 0;

    ESP_LOGD(TAG, "Transition omitted, setting state");

    // Set new state
    set_state(target_mode, target_dutycycle);
  }

  if (this->transition_state_ != TransitionState::OFF) {
    // Set mode start time
    this->transition_mode_start_time_ = millis();
    this->transition_last_step_time_ = millis();

    ESP_LOGD(TAG,
             "Transition from dutycycle: %f to dutycycle: %f (%f per ms) [Shorting buildup: %d ms (%f per ms), Full "
             "short for: %d ms]",
             this->current_relative_dutycycle_, this->transition_target_relative_dutycycle_,
             this->transition_relative_dutycycle_delta_per_ms_, this->transition_shorting_buildup_duration_ms_,
             this->transition_shorting_dutycycle_delta_per_ms_, this->transition_full_short_duration_ms_);
  }
}

void HBridge::set_state(HBridgeMode mode, float dutycycle) {
  // Print info on mode change
  ESP_LOGD(TAG, "Set mode %d - Dutycycle: %.2f", mode, dutycycle);

  // Cancel possible ongoing ramp
  this->transition_state_ = TransitionState::OFF;

  // Set state to outputs
  set_output_state_(mode, dutycycle);
}

void HBridge::set_output_state_by_relative_dutycycle_(float relative_dutycycle) {
  // Note: Relative speed is between -1 and 1.
  // -1 = Full duty direction A
  // 0 = Idle
  // 1  = Full duty direction B

  // Clap to range
  relative_dutycycle = clamp(relative_dutycycle, -1.0f, 1.0f);

  if (relative_dutycycle == 0) {
    set_output_state_(HBridgeMode::OFF, 0);
  } else if (relative_dutycycle < 0) {
    set_output_state_(HBridgeMode::DIRECTION_A, (relative_dutycycle * -1));
  } else if (relative_dutycycle > 0) {
    set_output_state_(HBridgeMode::DIRECTION_B, relative_dutycycle);
  }
}

void HBridge::set_output_state_(HBridgeMode mode, float dutycycle) {
  HBridgeMode new_mode = mode;
  float new_dutycycle = dutycycle;

  if (new_dutycycle <= 0) {
    // If dutycycle is 0 or negative, force off
    new_mode = HBridgeMode::OFF;
    new_dutycycle = 0;
  } else if (new_dutycycle > 1.0f) {
    // If dutycycle is above 1, cap to 1
    new_dutycycle = 1.0f;
  }

  // Set pin states + duty cycle according to mode
  switch (new_mode) {
    case HBridgeMode::OFF:
      if (this->enable_pin_ != nullptr) {
        this->enable_pin_->set_level(0);
      }
      this->pin_a_->set_level(0);
      this->pin_b_->set_level(0);
      this->current_relative_dutycycle_ = 0;
      break;

    case HBridgeMode::DIRECTION_A:
      // Set states dependent on current decay mode
      if (current_decay_mode_ == CurrentDecayMode::SLOW) {
        this->pin_b_->set_level(1.0f - new_dutycycle);
        this->pin_a_->set_level(1.0f);
      } else if (current_decay_mode_ == CurrentDecayMode::FAST) {
        this->pin_b_->set_level(0);
        this->pin_a_->set_level(new_dutycycle);
      }

      if (this->enable_pin_ != nullptr) {
        this->enable_pin_->set_level(1);
      }
      this->current_relative_dutycycle_ = new_dutycycle * -1;
      break;

    case HBridgeMode::DIRECTION_B:
      // Set states dependent on current decay mode
      if (current_decay_mode_ == CurrentDecayMode::SLOW) {
        this->pin_a_->set_level(1.0f - new_dutycycle);
        this->pin_b_->set_level(1.0f);
      } else if (current_decay_mode_ == CurrentDecayMode::FAST) {
        this->pin_a_->set_level(0);
        this->pin_b_->set_level(new_dutycycle);
      }

      if (this->enable_pin_ != nullptr) {
        this->enable_pin_->set_level(1);
      }
      this->current_relative_dutycycle_ = new_dutycycle;
      break;

    case HBridgeMode::SHORT:
      this->pin_a_->set_level(dutycycle);
      this->pin_b_->set_level(dutycycle);
      if (this->enable_pin_ != nullptr) {
        this->enable_pin_->set_level(1);
      }
      this->current_relative_dutycycle_ = 0;
      break;
  }

  // Store current values
  this->current_mode_ = new_mode;
}

}  // namespace hbridge
}  // namespace esphome
