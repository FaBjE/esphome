import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import switch
from esphome.const import (
    CONF_ID,
    CONF_PIN_A,
    CONF_PIN_B,
    CONF_RESTORE_MODE,
)
from .. import hbridge_ns

HBridgeValveActuator = hbridge_ns.class_(
    "HBridgeValveActuator", switch.Switch, cg.Component
)
HBridgeValveActuatorRestoreMode = hbridge_ns.enum("ValveActuatorRestoreMode")

RESTORE_MODES = {
    "RESTORE_DEFAULT_OFF": HBridgeValveActuatorRestoreMode.HBRIDGE_VALVE_ACTUATOR_RESTORE_DEFAULT_OFF,
    "RESTORE_DEFAULT_ON": HBridgeValveActuatorRestoreMode.HBRIDGE_VALVE_ACTUATOR_RESTORE_DEFAULT_ON,
    "ALWAYS_OFF": HBridgeValveActuatorRestoreMode.HBRIDGE_VALVE_ACTUATOR_ALWAYS_OFF,
    "ALWAYS_ON": HBridgeValveActuatorRestoreMode.HBRIDGE_VALVE_ACTUATOR_ALWAYS_ON,
    "RESTORE_INVERTED_DEFAULT_OFF": HBridgeValveActuatorRestoreMode.HBRIDGE_VALVE_ACTUATOR_RESTORE_INVERTED_DEFAULT_OFF,
    "RESTORE_INVERTED_DEFAULT_ON": HBridgeValveActuatorRestoreMode.HBRIDGE_VALVE_ACTUATOR_RESTORE_INVERTED_DEFAULT_ON,
}

CONF_ACTUATOR_SWITCHING_TIME = "actuator_switching_time"
CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(HBridgeValveActuator),
        cv.Required(CONF_PIN_A): pins.gpio_output_pin_schema,
        cv.Required(CONF_PIN_B): pins.gpio_output_pin_schema,
        cv.Optional(CONF_RESTORE_MODE, default="RESTORE_DEFAULT_OFF"): cv.enum(
            RESTORE_MODES, upper=True, space="_"
        ),
        cv.Optional(
            CONF_ACTUATOR_SWITCHING_TIME, default="15000ms"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)

    pina = await cg.gpio_pin_expression(config[CONF_PIN_A])
    cg.add(var.set_pina_pin(pina))

    pinb = await cg.gpio_pin_expression(config[CONF_PIN_B])
    cg.add(var.set_pinb_pin(pinb))

    cg.add(var.set_restore_mode(config[CONF_RESTORE_MODE]))

    if CONF_ACTUATOR_SWITCHING_TIME in config:
        cg.add(var.set_actuator_switching_time(config[CONF_ACTUATOR_SWITCHING_TIME]))
