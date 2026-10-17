// Standalone encoder read-out. No ROS, no URDF, no controller_manager.
//
//   encoder_probe [seconds]        default: run until Ctrl-C
//
// Prints both encoder counts once a second. The point of it is to answer two
// questions that nothing in the codebase can answer on its own:
//
//   1. WHICH ENCODER IS WHICH SIDE. SunFounder's speed.py uses Speed(25) and
//      Speed(4) without ever labelling them, so the mapping in the URDF is a
//      guess. Spin ONE side by hand and see which column moves. Get this
//      backwards and odometry mirrors every turn.
//
//   2. Does the counter work at all -- are edges arriving, and roughly 20 per
//      revolution as the disc's slot count implies?
//
// Turn a wheel by hand rather than driving it: the motors are not involved
// here, and a hand-turned wheel is easy to count revolutions of.
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "picar_hw/encoder.hpp"

namespace {

// BCM numbers from the ros2_control xacro. Which is which side is exactly what
// this tool exists to establish, so they are labelled by PIN, not by side.
constexpr unsigned int kGpioA = 25;
constexpr unsigned int kGpioB = 4;
constexpr const char * kChip = "/dev/gpiochip0";

std::atomic<bool> g_stop{false};

void on_signal(int) { g_stop.store(true); }

}  // namespace

int main(int argc, char ** argv)
{
    const double seconds = (argc > 1) ? std::atof(argv[1]) : 0.0;  // 0 = forever

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    picar_hw::Encoder a(kChip, kGpioA);
    picar_hw::Encoder b(kChip, kGpioB);

    if (!a.ok() || !b.ok()) {
        // perror above has already printed the real reason. The two that
        // actually happen: another process (ros2_control) holds the lines, or
        // this user is not in the gpio group.
        std::fprintf(stderr,
                     "failed to claim GPIO %u and/or %u -- is ros2_control running, "
                     "or are you not in the gpio group?\n", kGpioA, kGpioB);
        return 1;
    }

    std::printf("Spin ONE side by hand. The column that moves is that side.\n");
    std::printf("20 slots per revolution, so one full turn is ~20 counts.\n\n");
    std::printf("%8s  %12s %12s  %10s %10s\n",
                "elapsed", "BCM25", "BCM4", "d(BCM25)", "d(BCM4)");

    uint64_t prev_a = 0, prev_b = 0;
    const auto started = std::chrono::steady_clock::now();

    while (!g_stop.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        const uint64_t count_a = a.count();
        const uint64_t count_b = b.count();
        const double elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

        std::printf("%7.1fs  %12llu %12llu  %10llu %10llu\n", elapsed,
                    static_cast<unsigned long long>(count_a),
                    static_cast<unsigned long long>(count_b),
                    static_cast<unsigned long long>(count_a - prev_a),
                    static_cast<unsigned long long>(count_b - prev_b));
        std::fflush(stdout);

        prev_a = count_a;
        prev_b = count_b;

        // ok() goes false if the event wait failed, which would otherwise look
        // identical to a wheel that simply is not turning.
        if (!a.ok() || !b.ok()) {
            std::fprintf(stderr, "\nan encoder thread faulted -- counts are no longer live\n");
            return 1;
        }

        if (seconds > 0.0 && elapsed >= seconds) {
            break;
        }
    }

    std::printf("\nfinal: BCM25 = %llu, BCM4 = %llu\n",
                static_cast<unsigned long long>(a.count()),
                static_cast<unsigned long long>(b.count()));
    return 0;
}
