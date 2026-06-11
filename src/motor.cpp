#include <cstdint>
#include <iostream>
#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include "motor.hpp"

namespace PiCar_4WD {

    Motor::Motor(PiCar4WDHAT& pwm_pin, uint8_t dir_pin, bool is_reversed)
        : pwm_pin_(pwm_pin), dir_pin_(dir_pin), is_reversed_(is_reversed)
    {
        chip_ = gpiod_chip_open_by_name("gpiochip0");
        if (!chip_) {
            perror("Open chip failed");
            return;
        }

        dir_line_ = gpiod_chip_get_line(chip_, dir_pin_);
        if (!dir_line_) {
            perror("Get line failed");
            gpiod_chip_close(chip_);
            return;
        }

        if (gpiod_line_request_output(dir_line_, "picar", 0) < 0) {
            perror("Request line as output failed");
            gpiod_chip_close(chip_);
        }
    }

    Motor::~Motor() {
        pwm_pin_.set_duty_cycle(0.0f);
        if (dir_line_) gpiod_line_release(dir_line_);
        if (chip_) gpiod_chip_close(chip_);
    }

    void Motor::set_power(int8_t power) {
        if (power > 100) power = 100;
        if (power < -100) power = -100;

        int8_t direction = (power < 0) ? 1 : 0;
        uint8_t abs_power = static_cast<uint8_t>(std::abs(power));
        if (abs_power != 0)
        {
            abs_power = static_cast<uint8_t>(abs_power / 2) + 50;
        }

        if (is_reversed_)
        {
            direction = !direction;
        }

        gpiod_line_set_value(dir_line_, direction);
        pwm_pin_.set_duty_cycle(static_cast<float>(abs_power));
    }

}
