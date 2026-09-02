#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

struct gpiod_chip;
struct gpiod_line_request;
struct gpiod_edge_event_buffer;

namespace picar_hw
{

/// Counts rising edges on one slotted-disc encoder.
///
/// The disc has 20 slots and a single photo-interrupter, so the line reports
/// that a slot passed and nothing else: no direction, no absolute position, no
/// distinction between a wheel driven forward and the same wheel dragged
/// backwards. This class exposes exactly that and no more -- a monotonically
/// increasing edge count, and the kernel's timestamp for the most recent edge.
///
/// It deliberately stops there. Turning counts into a wheel velocity needs a
/// time base and a radius; recovering the SIGN needs information this sensor
/// does not carry. Both are modelling decisions, and neither belongs to the
/// device driver.
///
/// A background thread blocks on the kernel's edge-event queue, so edges are
/// not lost between reads regardless of how slowly the control loop runs.
/// count() and last_event_ns() are relaxed atomic loads -- no lock, no syscall,
/// no allocation -- and are safe to call from the real-time path.
class Encoder
{
public:
    /// @param chip_path GPIO character device, e.g. /dev/gpiochip0
    /// @param line      BCM line number of the encoder pin
    Encoder(const std::string & chip_path, unsigned int line);
    ~Encoder();

    // Owns a line request, an event buffer and a running thread.
    Encoder(const Encoder &) = delete;
    Encoder & operator=(const Encoder &) = delete;
    Encoder(Encoder &&) = delete;
    Encoder & operator=(Encoder &&) = delete;

    /// True while the line is claimed and the counting thread is healthy.
    /// Goes false if the event wait fails, so a dead counter is visible to the
    /// caller rather than silently reporting a frozen count.
    bool ok() const
    {
        return request_ != nullptr && !faulted_.load(std::memory_order_relaxed);
    }

    /// Rising edges seen since construction. Real-time safe.
    uint64_t count() const { return count_.load(std::memory_order_relaxed); }

    /// CLOCK_MONOTONIC timestamp of the most recent edge, in nanoseconds, as
    /// recorded by the kernel at interrupt time. Zero until the first edge.
    /// Real-time safe.
    ///
    /// This is a better time base than the control loop's own period: it is
    /// taken when the slot actually passed, not when the loop got around to
    /// looking.
    uint64_t last_event_ns() const
    {
        return last_event_ns_.load(std::memory_order_relaxed);
    }

private:
    void run();

    gpiod_chip * chip_ = nullptr;
    gpiod_line_request * request_ = nullptr;
    gpiod_edge_event_buffer * buffer_ = nullptr;
    unsigned int line_;

    std::atomic<uint64_t> count_{0};
    std::atomic<uint64_t> last_event_ns_{0};
    std::atomic<bool> running_{false};
    std::atomic<bool> faulted_{false};
    std::thread thread_;
};

}  // namespace picar_hw
