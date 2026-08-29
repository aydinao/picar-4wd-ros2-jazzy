// Standalone hardware smoke test. No ROS, no URDF, no controller_manager.
//
//   twitch <pwm_channel> <dir_gpio> <power -100..100> <seconds>
//   twitch 13 23 60 2          # left front forward at 60% for 2s
//
// Channel/GPIO pairs for the PiCar-4WD (see the repo README):
//   left front  13 / 23      right front 12 / 24
//   left rear    8 / 13      right rear   9 / 20
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "picar_hw/hat.hpp"
#include "picar_hw/hat_registers.hpp"
#include "picar_hw/motor.hpp"

int main(int argc, char ** argv)
{
    if (argc != 5) {
        std::fprintf(stderr,
            "usage: %s <pwm_channel> <dir_gpio> <power -100..100> <seconds>\n", argv[0]);
        return 2;
    }

    const auto channel = static_cast<std::uint8_t>(std::atoi(argv[1]));
    const auto dir_gpio = static_cast<std::uint8_t>(std::atoi(argv[2]));
    const auto power = static_cast<std::int8_t>(std::atoi(argv[3]));
    const double seconds = std::atof(argv[4]);

    picar_hw::PiCar4WDHAT pwm(1, picar_hw::hat::kDefaultAddress, channel);
    picar_hw::Motor motor(pwm, dir_gpio);

    if (!motor.ok()) {
        std::fprintf(stderr, "failed to claim GPIO %u -- is another process holding it?\n",
                     static_cast<unsigned>(dir_gpio));
        return 1;
    }

    pwm.set_frequency(50);
    std::printf("channel %u, gpio %u, power %d, %.2fs\n",
                static_cast<unsigned>(channel), static_cast<unsigned>(dir_gpio),
                static_cast<int>(power), seconds);

    motor.set_power(power);
    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
    motor.set_power(0);
    return 0;
}
