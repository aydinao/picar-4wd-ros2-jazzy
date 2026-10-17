#include "picar_hw/motor.hpp"

#include <gpiod.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace picar_hw {

    namespace {
        constexpr const char * kConsumer = "picar_hw";
    }

    Motor::Motor(Hat & pwm, const std::string & chip_path, unsigned int dir_line,
                 bool is_reversed)
        : pwm_(pwm), dir_line_(dir_line), is_reversed_(is_reversed)
    {
        chip_ = gpiod_chip_open(chip_path.c_str());
        if (!chip_) {
            std::perror("gpiod_chip_open");
            return;
        }

        // v2 builds a request from three objects: what the line should do
        // (settings), which offsets get those settings (line config), and who is
        // asking (request config). All three are freed once the request is made.
        gpiod_line_settings *settings = gpiod_line_settings_new();
        gpiod_line_config *line_cfg = gpiod_line_config_new();
        gpiod_request_config *req_cfg = gpiod_request_config_new();

        if (settings && line_cfg && req_cfg) {
            gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
            gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

            const unsigned int offset = dir_line_;
            if (gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings) == 0) {
                gpiod_request_config_set_consumer(req_cfg, kConsumer);
                dir_request_ = gpiod_chip_request_lines(chip_, req_cfg, line_cfg);
            }
        }

        if (req_cfg)  gpiod_request_config_free(req_cfg);
        if (line_cfg) gpiod_line_config_free(line_cfg);
        if (settings) gpiod_line_settings_free(settings);

        if (!dir_request_) {
            std::perror("gpiod_chip_request_lines");
            gpiod_chip_close(chip_);
            chip_ = nullptr;
        }
    }

    Motor::~Motor() {
        pwm_.set_duty_cycle(0.0f);
        if (dir_request_) gpiod_line_request_release(dir_request_);
        if (chip_) gpiod_chip_close(chip_);
    }

    void Motor::set_power(int8_t power) {
        if (power > 100) power = 100;
        if (power < -100) power = -100;

        bool direction = power < 0;
        uint8_t abs_power = static_cast<uint8_t>(std::abs(power));
        if (abs_power != 0)
        {
            abs_power = static_cast<uint8_t>(abs_power / 2) + 50;
        }

        if (is_reversed_)
        {
            direction = !direction;
        }

        if (dir_request_) {
            gpiod_line_request_set_value(
                dir_request_, dir_line_,
                direction ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
        }
        pwm_.set_duty_cycle(static_cast<float>(abs_power));
    }

}
