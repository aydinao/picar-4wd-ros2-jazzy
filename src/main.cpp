#include "include/4WDHAT.hpp"
#include <chrono>
#include <thread>

int main() {
    PiCar_4WD::PiCar4WDHAT pwm(1, 0x14, 12);  // bus 1, addr 0x14, channel P12
    pwm.set_frequency(50);  
    // Equivalent of p.period(1000); p.prescaler(10);


    while (true) {
        for (int i = 0; i < 4095; i += 10) {
            pwm.set_pulse_width(static_cast<uint16_t>(i));
            std::this_thread::sleep_for(std::chrono::microseconds(244)); // ~1/4095 s
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        for (int i = 4095; i > 0; i -= 10) {
            pwm.set_pulse_width(static_cast<uint16_t>(i));
            std::this_thread::sleep_for(std::chrono::microseconds(244));
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}