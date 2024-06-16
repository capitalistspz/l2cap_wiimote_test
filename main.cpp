#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>

#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

#include "WiimoteDevice.h"
#include "print.h"

std::vector<WiimoteDevice> FindAndConnectWiimotes();

int main() {
  println("Put your Wiimote into pairing mode (Press 1 and 2 together, or Press SYNC)");
  auto motes = FindAndConnectWiimotes();
  if (motes.empty()) {
    println_err("Wiimotes failed to connect or were not found");
    return 1;
  }
  for (auto i = 0; i < motes.size(); ++i) {
    auto& mote = motes[i];
    auto const& addr = mote.BDAddr();
    println("Wiimote {}: {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}", i, addr.b[5],
            addr.b[4], addr.b[3], addr.b[2], addr.b[1], addr.b[0]);

    mote.SetLed(1 << (i % 4));
    mote.SetReportMode(InputReport::Status, true);
  }
  println(
      "Press A to start rumble\n"
      "Press B to stop rumble\n"
      "Press 1 to show battery\n"
      "Press HOME to quit");

  bool quit = false;
  bool updateStatus = false;
  using namespace std::chrono;
  auto lastStatusUpdate = system_clock::now();
  while (!quit) {
    const auto now = system_clock::now();
    if (now - lastStatusUpdate > seconds(2))
    {
      lastStatusUpdate = now;
      updateStatus = true;
    }

    for (auto& mote : motes) {
      mote.Update();
      auto const& status = mote.Status();
      auto button = mote.Buttons();

      if (button & WM_BUTTON_A) {
        mote.SetRumble(true);
      }
      if (button & WM_BUTTON_B) {
        mote.SetRumble(false);
      }
      if (button & WM_BUTTON_HOME) {
        println("...quitting");
        quit = true;
        break;
      }
      if (button & WM_BUTTON_ONE)
      {
        println("Battery: {}", status.batteryLevel);
      }
      if (updateStatus)
      {
        mote.RequestStatus();
      }
    }

    updateStatus = false;
  }

  return 0;
}

bool IsWiimoteName(std::string_view sv) {
  return sv == "Nintendo RVL-CNT-01" || sv == "Nintendo RVL-CNT-01-TR";
}

bool AttemptConnect(int sockFd, sockaddr_l2 const& addr) {
  for (auto i = 0; i < 3; ++i) {
    if (connect(sockFd, reinterpret_cast<sockaddr const*>(&addr),
                sizeof(sockaddr_l2)) == 0)
      return true;
    println("Connection attempt {} failed with error {:x}: {} ", i + 1, errno,
            std::strerror(errno));
    if (i == 2) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
  }
  return false;
}

std::vector<WiimoteDevice> FindAndConnectWiimotes() {
  constexpr static uint8_t liacLap[] = {0x00, 0x8b, 0x9e};
  //! Get default BT device
  auto const hostId = hci_get_route(nullptr);

  // Search for devices
  inquiry_info* infoItr = nullptr;
  auto const respCount =
      hci_inquiry(hostId, 5, 8, liacLap, &infoItr, IREQ_CACHE_FLUSH);

  std::vector<WiimoteDevice> motes;
  char buffer[HCI_MAX_NAME_LENGTH] = {};

  //! Open dev to read name later
  auto const hostDesc = hci_open_dev(hostId);

  for (auto i = 0; i < respCount; ++i) {
    auto& addr = infoItr[i].bdaddr;
    if (hci_read_remote_name(hostDesc, &addr, HCI_MAX_NAME_LENGTH, buffer,
                             2000) != 0)
      continue;
    // Check devices by name,
    // 3rd party controllers often don't have the product and vendor id in SDP
    if (!IsWiimoteName(buffer)) continue;
    println("Found '{}' with address {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
            buffer, addr.b[5], addr.b[4], addr.b[3], addr.b[2], addr.b[1],
            addr.b[0]);

    // Connect to device with L2 CAP

    // Socket for sending data to controller, PSM 0x11
    auto const sendFd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
    if (sendFd < 0) {
      println_err("Failed to open send socket: {}\n", strerror(errno));
      continue;
    }

    sockaddr_l2 sendAddr{};
    sendAddr.l2_family = AF_BLUETOOTH;
    sendAddr.l2_psm = htobs(0x11);
    sendAddr.l2_bdaddr = addr;

    if (!AttemptConnect(sendFd, sendAddr)) {
      close(sendFd);
      continue;
    }

    // Socket for receiving data from controller, PSM 0x13
    auto const recvFd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
    if (recvFd < 0) {
      println_err("Failed to open recv socket: {}\n", strerror(errno));
      continue;
    }
    sockaddr_l2 recvAddr{};
    recvAddr.l2_family = AF_BLUETOOTH;
    recvAddr.l2_psm = htobs(0x13);
    recvAddr.l2_bdaddr = addr;

    if (!AttemptConnect(recvFd, recvAddr)) {
      close(sendFd);
      close(recvFd);
      continue;
    }

    println("Successfully connected");
    motes.emplace_back(addr, sendFd, recvFd);
  }
  hci_close_dev(hostDesc);
  return motes;
}