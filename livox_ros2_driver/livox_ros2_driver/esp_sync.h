// ESP32/PC UTC bridge for ONE legacy Livox Horizon. Local integration, 2026.
// No second SDK process is created. UTC is an epoch label for the last PPS.
#pragma once
#include <atomic>
#include <cstdint>
#include <ctime>
#include <memory>
#include "livox_sdk.h"

namespace livox_ros {
// Legacy SDK releases differ in the spelling of this wire-compatible field.
template <typename Request>
auto SetUtcMicroseconds(Request &req, uint32_t value, int)
    -> decltype(req.microsecond = value, void()) { req.microsecond = value; }
template <typename Request>
auto SetUtcMicroseconds(Request &req, uint32_t value, long)
    -> decltype(req.mircrosecond = value, void()) { req.mircrosecond = value; }

inline int64_t EspMonoNs() {
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return int64_t(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

class EspSync {
 public:
  static EspSync &Get() { static EspSync value; return value; }
  std::atomic<bool> enabled{false}, running{false};
  std::atomic<int> handle{-1}, last_type{-1};
  std::atomic<bool> multiple{false};
  std::atomic<int64_t> last_packet{0}, last_command{0}, last_ack{0};
  std::atomic<uint64_t> pps_epoch{0}, ack_epoch{0}, first_epoch{0};
  std::atomic<uint64_t> accepted{0}, rejected{0};

  void Observe(uint8_t device_handle, uint8_t device_type, uint8_t timestamp_type) {
    if (!enabled || device_type != kDeviceTypeLidarHorizon) return;
    int expected = -1;
    handle.compare_exchange_strong(expected, int(device_handle));
    if (handle != int(device_handle)) { multiple = true; return; }
    last_type = timestamp_type;
    last_packet = EspMonoNs();
  }

  bool Ready() const {
    return handle >= 0 && !multiple && EspMonoNs() - last_packet < 1000000000LL;
  }

  bool Accept(uint8_t device_handle, uint8_t type, uint64_t stamp) {
    if (!enabled) return true;
    const int64_t now = EspMonoNs();
    const uint64_t epoch = pps_epoch.load();
    const uint64_t first = first_epoch.load();
    const int64_t delta = int64_t(stamp) - int64_t(epoch);
    const bool valid = running && !multiple && handle == int(device_handle)
        && type == kTimestampTypePpsGps && first && epoch >= first + 2000000000ULL
        && ack_epoch >= first && now - last_ack < 1500000000LL
        && now - last_command < 1500000000LL
        && delta >= -150000000LL && delta < 1500000000LL;
    if (valid) ++accepted; else ++rejected;
    return valid;
  }

  struct Request { uint64_t epoch; };
  static void UtcAck(livox_status status, uint8_t, uint8_t response, void *data) {
    std::unique_ptr<Request> request(static_cast<Request *>(data));
    if (status == kStatusSuccess && response == 0) {
      Get().ack_epoch = request->epoch;
      Get().last_ack = EspMonoNs();
    }
  }

  // Python monotonic_ns() and CLOCK_MONOTONIC share the PC's monotonic clock.
  // Reject queued DDS commands so an old epoch cannot label the next PPS.
  int SetUtc(uint64_t epoch_ns, int64_t sent_mono, int64_t edge_mono) {
    const int64_t now = EspMonoNs();
    if (!enabled || !running || !Ready()) return -100;
    if (now < sent_mono || now - sent_mono > 100000000LL
        || now - edge_mono < 10000000LL || now - edge_mono > 450000000LL
        || epoch_ns % 1000000000ULL != 0) return -101;
    if (pps_epoch && epoch_ns != pps_epoch + 1000000000ULL) return -102;
    const time_t seconds = epoch_ns / 1000000000ULL;
    tm utc{};
    if (!gmtime_r(&seconds, &utc) || utc.tm_year < 100 || utc.tm_year > 355) return -103;
    LidarSetUtcSyncTimeRequest req{};
    req.year = utc.tm_year - 100;
    req.month = utc.tm_mon + 1;
    req.day = utc.tm_mday;
    req.hour = utc.tm_hour;
    SetUtcMicroseconds(req, uint32_t((utc.tm_min * 60 + utc.tm_sec) * 1000000ULL), 0);
    // Record the command before SDK dispatch; the callback can be asynchronous.
    if (!first_epoch) first_epoch = epoch_ns;
    pps_epoch = epoch_ns;
    last_command = now;
    auto *context = new Request{epoch_ns};
    const int status = LidarSetUtcSyncTime(uint8_t(handle.load()), &req, UtcAck, context);
    if (status != kStatusSuccess) delete context;
    return status;
  }
};
}  // namespace livox_ros
