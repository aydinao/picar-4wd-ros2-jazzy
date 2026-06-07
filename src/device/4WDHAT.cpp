#include "4WDHAT.hpp"
#include <cmath>
#include <vector>
#include <algorithm>

namespace PiCar_4WD{
    PiCar4WDHAT::PiCar4WDHAT(int bus, uint_fast8_t address, uint8_t channel) : Device(bus, address), channel_(channel) { }

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
    void PiCar4WDHAT::set_frequency(uint16_t frequency) 
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

        set_prescaler(static_cast<uint8_t>(prescaler));
        set_period(static_cast<uint16_t>(period));

    }

}