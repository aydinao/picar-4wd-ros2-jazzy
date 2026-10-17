#include "picar_hw/encoder.hpp"

#include <cerrno>
#include <cstdio>
#include <gpiod.h>

namespace picar_hw
{

namespace {
constexpr const char * kConsumer = "picar_hw";

// One revolution is 20 edges, so even a fast wheel produces edges in the low
// hundreds per second. 32 is several loop periods' worth of headroom.
constexpr size_t kEventBufferCapacity = 32;

// Bounds how long ~Encoder waits for the thread to notice it should stop.
// Nothing is lost by timing out: the kernel queues edges either way.
constexpr int64_t kWaitTimeoutNs = 100'000'000;  // 100 ms
}  // namespace

Encoder::Encoder(const std::string & chip_path, unsigned int line)
    : line_(line)
{
    chip_ = gpiod_chip_open(chip_path.c_str());
    if (!chip_) {
        std::perror("gpiod_chip_open");
        return;
    }

    // Same three-object dance as Motor, but as an input with edge detection.
    // Without set_edge_detection the request succeeds and no events ever
    // arrive -- edge detection is off by default.
    gpiod_line_settings * settings = gpiod_line_settings_new();
    gpiod_line_config * line_cfg = gpiod_line_config_new();
    gpiod_request_config * req_cfg = gpiod_request_config_new();

    if (settings && line_cfg && req_cfg) {
        gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
        gpiod_line_settings_set_edge_detection(settings, GPIOD_LINE_EDGE_RISING);

        const unsigned int offset = line_;
        if (gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings) == 0) {
            gpiod_request_config_set_consumer(req_cfg, kConsumer);
            request_ = gpiod_chip_request_lines(chip_, req_cfg, line_cfg);
        }
    }

    if (req_cfg)  gpiod_request_config_free(req_cfg);
    if (line_cfg) gpiod_line_config_free(line_cfg);
    if (settings) gpiod_line_settings_free(settings);

    if (!request_) {
        std::perror("gpiod_chip_request_lines");
        gpiod_chip_close(chip_);
        chip_ = nullptr;
        return;
    }

    buffer_ = gpiod_edge_event_buffer_new(kEventBufferCapacity);
    if (!buffer_) {
        std::perror("gpiod_edge_event_buffer_new");
        gpiod_line_request_release(request_);
        request_ = nullptr;
        gpiod_chip_close(chip_);
        chip_ = nullptr;
        return;
    }

    running_.store(true, std::memory_order_release);
    thread_ = std::thread(&Encoder::run, this);
}

Encoder::~Encoder()
{
    running_.store(false, std::memory_order_release);
    if (thread_.joinable()) {
        thread_.join();
    }
    if (buffer_)  gpiod_edge_event_buffer_free(buffer_);
    if (request_) gpiod_line_request_release(request_);
    if (chip_)    gpiod_chip_close(chip_);
}

void Encoder::run()
{
    while (running_.load(std::memory_order_acquire)) {
        // Timed wait rather than an indefinite one, so the destructor does not
        // have to interrupt a blocked thread to shut it down.
        const int waited = gpiod_line_request_wait_edge_events(request_, kWaitTimeoutNs);

        if (waited == 0) {
            continue;  // nothing arrived; re-check running_
        }
        if (waited < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::perror("gpiod_line_request_wait_edge_events");
            faulted_.store(true, std::memory_order_relaxed);
            return;
        }

        const int n = gpiod_line_request_read_edge_events(
            request_, buffer_, kEventBufferCapacity);
        if (n <= 0) {
            continue;
        }

        // Only the newest event's timestamp is kept: the count carries how many
        // there were, and a per-edge history has no consumer.
        gpiod_edge_event * last =
            gpiod_edge_event_buffer_get_event(buffer_, static_cast<unsigned long>(n - 1));
        if (last) {
            last_event_ns_.store(
                gpiod_edge_event_get_timestamp_ns(last), std::memory_order_relaxed);
        }

        // Relaxed is sufficient: this counter is the only thing being published
        // to the reader, there is nothing it must be ordered against, and a
        // mutex here would let a non-real-time thread block the control loop.
        count_.fetch_add(static_cast<uint64_t>(n), std::memory_order_relaxed);
    }
}

}  // namespace picar_hw
