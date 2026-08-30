#include "picar_hw/reset.hpp"

#include <gpiod.h>

#include <thread>

namespace picar_hw
{

bool pulse_reset(
    const std::string & chip_path,
    unsigned int line,
    std::chrono::milliseconds low_time,
    std::chrono::milliseconds settle)
{
    gpiod_chip * chip = gpiod_chip_open(chip_path.c_str());
    if (!chip) {
        return false;
    }

    gpiod_line_settings * settings = gpiod_line_settings_new();
    gpiod_line_config * line_cfg = gpiod_line_config_new();
    gpiod_request_config * req_cfg = gpiod_request_config_new();
    gpiod_line_request * request = nullptr;

    if (settings && line_cfg && req_cfg) {
        gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
        // Start low: requesting the line already asserts reset.
        gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);
        if (gpiod_line_config_add_line_settings(line_cfg, &line, 1, settings) == 0) {
            gpiod_request_config_set_consumer(req_cfg, "picar_hw_reset");
            request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);
        }
    }

    if (req_cfg)  gpiod_request_config_free(req_cfg);
    if (line_cfg) gpiod_line_config_free(line_cfg);
    if (settings) gpiod_line_settings_free(settings);

    if (!request) {
        gpiod_chip_close(chip);
        return false;
    }

    std::this_thread::sleep_for(low_time);
    gpiod_line_request_set_value(request, line, GPIOD_LINE_VALUE_ACTIVE);
    // The MCU needs a moment after release before it answers on the bus.
    std::this_thread::sleep_for(settle);

    gpiod_line_request_release(request);
    gpiod_chip_close(chip);
    return true;
}

}  // namespace picar_hw
