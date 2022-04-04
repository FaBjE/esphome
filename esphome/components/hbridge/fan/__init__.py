import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components import fan, output, hbridge
from esphome.const import (
    CONF_ID,
    CONF_SPEED_COUNT,
    CONF_OSCILLATION_OUTPUT,
)

CODEOWNERS = ["@FaBjE"]
AUTO_LOAD = ["hbridge"]

HBridgeFan = hbridge.hbridge_ns.class_("HBridgeFan", fan.Fan, hbridge.HBridge)

CONFIG_SCHEMA = fan.FAN_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(HBridgeFan),
        cv.GenerateID(CONF_ID): cv.declare_id(HBridgeFan),
        cv.Optional(CONF_SPEED_COUNT, default=100): cv.int_range(min=1),
        cv.Optional(CONF_OSCILLATION_OUTPUT): cv.use_id(output.BinaryOutput),
    }
).extend(hbridge.HBRIDGE_CONFIG_SCHEMA)

# Actions
BrakeAction = hbridge.hbridge_ns.class_("BrakeAction", automation.Action)


@automation.register_action(
    "fan.hbridge.brake",
    BrakeAction,
    maybe_simple_id({cv.Required(CONF_ID): cv.use_id(HBridgeFan)}),
)
async def fan_hbridge_brake_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_SPEED_COUNT],
    )
    await cg.register_component(var, config)
    await fan.register_fan(var, config)

    if CONF_OSCILLATION_OUTPUT in config:
        oscillation_output = await cg.get_variable(config[CONF_OSCILLATION_OUTPUT])
        cg.add(var.set_oscillation_output(oscillation_output))

    # HBridge driver config
    await hbridge.hbridge_setup(config, var)
