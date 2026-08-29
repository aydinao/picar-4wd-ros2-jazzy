#include "picar_hw/hat.hpp"
#include "picar_hw/hat_registers.hpp"

#include <string>
#include <utility>
#include <cmath>
#include <vector>
#include <algorithm>

namespace picar_hw{

    // The filename form is the real constructor; the bus form delegates to it,
    // because bus N *is* /dev/i2c-N (see I2CPP::open_adapter).
    Hat::Hat(std::string filename, uint_fast8_t address, uint8_t channel)
        : Device(std::move(filename), address),
          channel_(channel),
          timer_(hat::timer_for_channel(channel)) { }

    Hat::Hat(int bus, uint_fast8_t address, uint8_t channel)
        : Hat("/dev/i2c-" + std::to_string(bus), address, channel) { }

    void Hat::set_duty_cycle(float duty_cycle) {
        // duty_cycle expected as 0.0–100.0
        if (duty_cycle < 0.0f) duty_cycle = 0.0f;
        if (duty_cycle > 100.0f) duty_cycle = 100.0f;
        float fraction = duty_cycle / 100.0f;
        auto pw = static_cast<uint16_t>(fraction * static_cast<float>(period_));
        set_pulse_width(pw);
    }

    void Hat::set_prescaler(uint16_t prescaler) {
        if (!prescaler) {
            prescaler = 1;
        }
        prescaler = prescaler - 1;          
        uint8_t reg = static_cast<uint8_t>(REG_PSC + timer_);
        
        uint_fast8_t buffer[3] = {
            reg,
            static_cast<uint_fast8_t>(prescaler >> 8),
            static_cast<uint_fast8_t>(prescaler & 0xff)
        };
        this->write_i2c(buffer, 3);

    }

    void Hat::set_period(uint16_t period) {
        if (!period) {
            period = 999;
        }
        period_ = static_cast<uint16_t>(period - 1);;

        uint8_t reg = static_cast<uint8_t>(REG_ARR + timer_);
        uint_fast8_t buffer[3] = {
            reg,
            static_cast<uint_fast8_t>(period_ >> 8),
            static_cast<uint_fast8_t>(period_ & 0xff)
        };
        this->write_i2c(buffer, 3);
    
    }

    void Hat::set_pulse_width(uint16_t pulse_width){
        if (!pulse_width){
            pulse_width = 0;
        }
        uint8_t reg = static_cast<uint8_t>(REG_CHN + channel_);
                uint_fast8_t buffer[3] = {
            reg,
            static_cast<uint_fast8_t>(pulse_width >> 8),
            static_cast<uint_fast8_t>(pulse_width & 0xff)
        };
        this->write_i2c(buffer, 3);
    }

    /**
     * If CLOCK / frequency were always a perfect square 
     * and always divided evenly into integers,
     * the square root would give the exact answer every time.
     * 
     * sqrt returns a float, and even when the sqrt is an integer,
     * CLOCK / frequency might not divide evenly.
     * 
     * Given that it can't always be exact, finding 
     * the integer pair for the presecaler (PSC) and the
     * period (ARR) whose product is closest to CLOCK / frequency
     * gives the best accuracy here.
     */
    void Hat::set_frequency(uint16_t frequency) 
    {
        if (!frequency)
        {
            frequency = 50;
        }
    
        int start = std::sqrt(CLOCK / frequency);
        start = start - 5; 
        // prevent negative value
        if (start <= 0) 
        {
            start = 1;
        }
        std::vector<std::pair< int, int >> candidates;
        std::vector< int > accuracy; 

        for (int prescaler = start; prescaler < start + 10; prescaler++)
        {
            int val = CLOCK / frequency / prescaler;
            candidates.push_back({prescaler, val});
            accuracy.push_back(std::abs(frequency - CLOCK / prescaler / val));
        }
        
        std::vector<int>::iterator result = std::min_element(std::begin(accuracy), std::end(accuracy));
        auto index = std::distance(accuracy.begin(), result);
        int prescaler = candidates[index].first;
        int period = candidates[index].second;

        set_prescaler(static_cast<uint16_t>(prescaler));
        set_period(static_cast<uint16_t>(period));

    }

}