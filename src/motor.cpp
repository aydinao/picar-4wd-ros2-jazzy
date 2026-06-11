#include <cstdint>
#include<iostream>

#include "motor.hpp"

namespace PiCar_4WD{

    Motor::Motor(PiCar4WDHAT& pwm_pin, uint8_t dir_pin_, bool is_reversed_) : pwm_pin_(pwm_pin)  { }


    void Motor::set_power(int8_t power){
        if (power > 100) power = 100;
         if (power < -100) power = -100;

        int8_t direction = (power < 0) ? 1 : 0;
        uint8_t abs_power = static_cast<uint8_t>(std::abs(power));
        if (abs_power != 0)
        {
            abs_power = static_cast<uint8_t>( abs_power / 2 ) + 50;
        }
        
        if (is_reversed_)
        {
            direction = !direction;
        }
        dir_pin_ = direction;
        pwm_pin_.set_duty_cycle(static_cast<float>(abs_power));
        
    }

}