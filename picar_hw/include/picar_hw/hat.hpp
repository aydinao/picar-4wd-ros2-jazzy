/**
 * @file 4WDHAT.hpp
 * @author Aydin Orhan @aydinao
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "i2cpp/device.hpp"

namespace picar_hw

  
{
   
    class Hat : public i2cpp::Device
    {
        private:
            static constexpr int REG_CHN = 0x20;
            static constexpr int REG_FRE = 0x30;
            static constexpr int REG_PSC = 0x40;
            static constexpr int REG_ARR = 0x44;
            static constexpr int CLOCK = 72000000;
            uint8_t channel_;
            uint16_t timer_;
            uint16_t period_ = 0;

            
            /**
             * Set a prescaler for the internal clock
             * @param prescaler prescaler value (e.g. 1, 8, 256)
             */
            void set_prescaler(uint16_t prescaler);

            /** 
             * Set a time period.
             * The Period (T) is the total time it takes for one complete PWM cycle.
             * T = 1 / frequency
             * T = (Prescaler) * (Period Value + 1) / System Clock (keep or remove this?)
            */
            void set_period(uint16_t period);

        public:
            using SharedPtr = std::shared_ptr<Hat>;

            /**
             * Construct a Hat with the given bus and address.
             * @param bus I2C interface number to use
             * @param address Address of the device on the I2C network
             * @param channel PWM channel P0-P13; selects the pulse-width register,
             *        and via channel / 4 the timer sharing prescaler and period
             */
            Hat(int bus, uint_fast8_t address, uint8_t channel);
            
            /**
             * Construct a Hat against a device file instead of a bus number.
             * Bus N is shorthand for /dev/i2c-N, so this is the same thing said
             * differently. Tests point it at a temp file to capture register writes.
             * @param filename Path to the I2C device file
             * @param address Address of the device on the I2C network
             * @param channel PWM channel P0-P13
             */
            Hat(std::string filename, uint_fast8_t address, uint8_t channel);
            
            /**
             * The output PWM frequency
             * PWM Frequency = system clock / prescaler * (Counter Max Value + 1)
             * @param frequency frequency for PWM output
             */
            void set_frequency(uint16_t frequency);

           
            /**
             * Set pulse width
             * The Pulse Width (t_on) is the absolute duration
             * of time that the PWM signal remains high during 
             * a single period.
             * 
             * t_on = (Prescaler) * (Pulse Width Value) / System Clock
             * @param pulse_width the pulse width
             */
            void set_pulse_width(uint16_t pulse_width);

            /**
             * Set pulse width percentage (duty cycle, D)
             * Represents the ratio of the active time to
             * the total time period, expressed as a percentage
             * 
             * D = (t_on / T) * 100 = (Pulse Width Value / Period Value + 1) * 100
             */
            void set_duty_cycle(float duty_cycle);
    };
}
            