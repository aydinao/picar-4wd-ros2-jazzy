#include "motor.hpp"
#include <chrono>
#include <thread>

int main() {
    uint8_t left_front_gpio = 23;
    uint8_t right_front_gpio = 24;
    uint8_t left_rear_gpio = 13;
    uint8_t right_rear_gpio = 20;
    
    uint8_t left_front_pwm = 13;
    uint8_t right_front_pwm = 12;
    uint8_t left_rear_pwm = 8;
    uint8_t right_rear_pwm = 9;

    PiCar_4WD::PiCar4WDHAT pwm_left_front_obj(1, 0x14, left_front_pwm); 
    PiCar_4WD::PiCar4WDHAT pwm_right_front_obj(1, 0x14, right_front_pwm); 
    PiCar_4WD::PiCar4WDHAT pwm_left_rear_obj(1, 0x14, left_rear_pwm); 
    PiCar_4WD::PiCar4WDHAT pwm_right_rear_obj(1, 0x14, right_rear_pwm); 

    PiCar_4WD::Motor left_front(pwm_left_front_obj, left_front_gpio);
    PiCar_4WD::Motor right_front(pwm_right_front_obj, right_front_gpio);
    PiCar_4WD::Motor left_rear(pwm_left_rear_obj, left_rear_gpio);
    PiCar_4WD::Motor right_rear(pwm_right_rear_obj, right_rear_gpio);
    
    pwm_left_front_obj.set_frequency(50);
    pwm_right_front_obj.set_frequency(50);
    pwm_left_rear_obj.set_frequency(50);
    pwm_right_rear_obj.set_frequency(50);

    left_front.set_power(60);
    right_front.set_power(60);
    left_rear.set_power(60);
    right_rear.set_power(60);
    std::this_thread::sleep_for(std::chrono::seconds(3));

    left_front.set_power(0);
    right_front.set_power(0);
    left_rear.set_power(0);
    right_rear.set_power(0);



}
