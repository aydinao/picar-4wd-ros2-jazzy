#pragma once

#include <cstdint>
#include <memory>
#include <gpiod.h>

#include "4WDHAT.hpp"

namespace PiCar_4WD
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
            struct gpiod_chip *chip_;
            struct gpiod_line *dir_line_;

        public:
            using SharedPtr = std::shared_ptr<Motor>;
            Motor(PiCar4WDHAT& pwm_pin, uint8_t dir_pin, bool is_reversed = false);
            ~Motor();

            void set_power(int8_t power);
    };
}
