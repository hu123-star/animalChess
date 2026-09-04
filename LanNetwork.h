#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <cstdint>
#include <memory>
#include <string>

enum class LanRole {
    None,
    Host,
    Guest
};

enum class LanState {
    Stopped,
    Hosting,
    Connecting,
    Connected,
    Disconnected,
    Failed
};

constexpr uint32_t LAN_NOTIFICATION_GENERATION_MASK = 0x00FFFFFFu;

constexpr WPARAM LanPackNotification(uint32_t generation, uint8_t value)
{
    return (static_cast<WPARAM>(generation & LAN_NOTIFICATION_GENERATION_MASK) << 8) |
        static_cast<WPARAM>(value);
}

constexpr uint32_t LanNotificationGeneration(WPARAM notification)
{
    return static_cast<uint32_t>((notification >> 8) & LAN_NOTIFICATION_GENERATION_MASK);
}

constexpr uint8_t LanNotificationValue(WPARAM notification)
{
    return static_cast<uint8_t>(notification & 0xFFu);
}

// WM_APP_LAN_STATE_CHANGED:
//   wParam = LanPackNotification(generation, static_cast<uint8_t>(LanState))
//   lParam = static_cast<LPARAM>(LanRole)
// WM_APP_LAN_MOVE_RECEIVED:
//   wParam = LanPackNotification(generation, source board index (0..62))
//   lParam = destination board index (0..62)
constexpr UINT WM_APP_LAN_STATE_CHANGED = WM_APP + 100;
constexpr UINT WM_APP_LAN_MOVE_RECEIVED = WM_APP + 101;

class LanSession {
public:
    LanSession();
    ~LanSession();

    bool StartHost(HWND notificationWindow, uint16_t port);
    bool Connect(HWND notificationWindow, const std::string& address, uint16_t port);
    void Stop();
    bool SendMove(uint8_t srcIndex, uint8_t dstIndex);

    LanState GetState() const;
    LanRole GetRole() const;
    uint32_t GetGeneration() const;
    std::string GetLastError() const;

    static std::string GetLocalIPv4Address();

    LanSession(const LanSession&) = delete;
    LanSession& operator=(const LanSession&) = delete;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
