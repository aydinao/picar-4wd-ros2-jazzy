#pragma once

#include <cstdint>
#include <memory>
#include <gpiod.h>

#include "picar_hw/hat.hpp"

namespace picar_hw
{

    class Motor
    {
        private:
            uint8_t STEP = 10;
            float DELAY = 0.1;
            PiCar4WDHAT& pwm_pin_;
            uint8_t dir_pin_;
            bool is_reversed_;
            int8_t power_ = 0;
            uint8_t except_power_ = 0;

            // libgpiod v2: a chip handle plus a *line request*. In v1 you held a
            // `gpiod_line*` obtained from the chip; in v2 you request one or more
            // line offsets together and get back a single request handle that owns
            // them. Values are then set by (request, offset) rather than by line.
            struct gpiod_chip *chip_ = nullptr;
            struct gpiod_line_request *dir_request_ = nullptr;

        public:
            using SharedPtr = std::shared_ptr<Motor>;
            Motor(PiCar4WDHAT& pwm_pin, uint8_t dir_pin, bool is_reversed = false);
            ~Motor();

            void set_power(int8_t power);

            /// True if the direction line was successfully claimed.
            bool ok() const { return dir_request_ != nullptr; }
    };
}
