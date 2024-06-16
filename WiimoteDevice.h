#pragma once

#include <bluetooth/bluetooth.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <cstdint>
#include <utility>

#include "print.h"

#define AS_U8(val) static_cast<uint8_t>(val)

enum class InputReport : uint8_t {
  Status = 0x20,
  ReadMemory = 0x21,
  Buttons = 0x30
};

enum class OutputReport : uint8_t {
  Rumble = 0x10,
  LED = 0x11,
  Report = 0x12,
  Status = 0x15,
  WriteMemory = 0x16,
  ReadMemory = 0x17
};

enum WiimoteButton : uint16_t {
  WM_BUTTON_LEFT = 1 << 0,
  WM_BUTTON_RIGHT = 1 << 1,
  WM_BUTTON_DOWN = 1 << 2,
  WM_BUTTON_UP = 1 << 3,
  WM_BUTTON_PLUS = 1 << 4,

  WM_BUTTON_TWO = 1 << 8,
  WM_BUTTON_ONE = 1 << 9,
  WM_BUTTON_B = 1 << 10,
  WM_BUTTON_A = 1 << 11,
  WM_BUTTON_MINUS = 1 << 12,
  WM_BUTTON_HOME = 1 << 15
};

struct WiimoteStatus {
  bool batteryLow : 1;
  bool extensionConnected : 1;
  bool speakerEnabled : 1;
  bool irEnabled : 1;
  uint8_t ledState : 4;
  uint16_t _zero;
  uint8_t batteryLevel;
};

class WiimoteDevice {
 public:
  WiimoteDevice(bdaddr_t const& addr, int sendFd, int recvFd)
      : m_addr(addr), m_sendFd(sendFd), m_recvFd(recvFd) {}

  WiimoteDevice(WiimoteDevice&& dev) noexcept
      : m_addr(std::exchange(dev.m_addr, {})),
        m_sendFd(std::exchange(dev.m_sendFd, 0)),
        m_recvFd(std::exchange(dev.m_recvFd, 0)) {}

  ~WiimoteDevice() {
    close(m_recvFd);
    close(m_sendFd);
  }

  void SetReportMode(InputReport report, bool continuous) {
    uint8_t msg[] = {0xa2, AS_U8(OutputReport::Report), AS_U8(continuous << 2),
                     AS_U8(report)};
    Send(msg, sizeof(msg));
  }

  void SetLed(uint8_t ledMask) {
    uint8_t msg[] = {0xa2, AS_U8(OutputReport::LED), AS_U8(ledMask << 4)};
    Send(msg, sizeof(msg));
  }

  void SetRumble(bool on) {
    uint8_t msg[] = {0xa2, AS_U8(OutputReport::Rumble), AS_U8(on)};
    rumble = on;
    Send(msg, sizeof(msg));
  }

  void RequestStatus() {
    uint8_t msg[] = {0xa2, AS_U8(OutputReport::Status), 0x00};
    Send(msg, sizeof(msg));
  }

  void Send(uint8_t* msg, size_t size) {
    // Rumble bit must be set on every report
    msg[2] |= rumble ? 1 : 0;
    send(m_sendFd, msg, size, 0);
  }

  void Update() {
    uint8_t buffer[23];
    auto res = recv(m_recvFd, buffer, 23, MSG_DONTWAIT);
    if (res < 2) return;
    assert(buffer[0] == 0xa1);
    auto const* it = buffer + 1;
    auto report = static_cast<InputReport>(*(it++));

    switch (report) {
      using enum InputReport;
      case Buttons: {
        //! Mask out the acceleration bits
        buttons = *reinterpret_cast<uint16_t const*>(it) & ~0x60E0;
        it += 2;
        break;
      }
      case Status: {
        buttons = *reinterpret_cast<uint16_t const*>(it) & ~0x60E0;
        it += 2;
        status = *reinterpret_cast<WiimoteStatus const*>(it);
        break;
      }
      default:
        break;
    }
  }

  bdaddr_t BDAddr() const { return m_addr; }
  uint16_t Buttons() const { return buttons; }

  WiimoteStatus Status() const { return status; }

 private:
  bdaddr_t m_addr;
  int m_sendFd;
  int m_recvFd;

  bool rumble = false;
  uint16_t buttons = 0;
  WiimoteStatus status{};
};