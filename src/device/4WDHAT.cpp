#include "4WDHAT.hpp"
#include <cmath>
#include <ranges>
#include <vector>

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
    
        int start = sqrt(CLOCK / frequency);
        start = start - 5; 
        // prevent negative value
        if (start <= 0) 
        {
            start = 1;
        }
        std::vector< int > arr;
        std::vector< int > accuracy; 

        for (int prescaler = start; prescaler < start + 10; prescaler++)
        {
            int val = CLOCK / frequency / prescaler;
            arr.push_back(val);
            accuracy.push_back(abs(frequency - CLOCK / prescaler / val));
        }
        
    
    }

}