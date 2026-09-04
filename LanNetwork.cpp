#include "pch.h"
#include "LanNetwork.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <iphlpapi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstring>
#include <exception>
#include <mutex>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

namespace {

constexpr uint8_t kMagic0 = 0x55;
constexpr uint8_t kMagic1 = 0xAA;
constexpr uint16_t kNetJoinReq = 0x0010;
constexpr uint16_t kNetJoinAck = 0x0011;
constexpr uint16_t kNetMoveSync = 0x0012;
constexpr uint32_t kMaxBodyLength = 256;
constexpr size_t kHeaderLength = 8;
constexpr uint8_t kGuestCamp = 1;
constexpr std::chrono::milliseconds kPollInterval(100);
constexpr std::chrono::seconds kConnectTimeout(10);
constexpr std::chrono::seconds kHandshakeTimeout(10);
constexpr std::chrono::seconds kSendTimeout(5);

enum class IoResult {
    Ok,
    Closed,
    Cancelled,
    Timeout,
    Error
};

struct Frame {
    uint16_t messageId = 0;
    std::vector<uint8_t> body;
};

std::string FormatSocketError(const char* operation, int errorCode)
{
    wchar_t* systemMessage = nullptr;
    const DWORD length = ::FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        static_cast<DWORD>(errorCode),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&systemMessage),
        0,
        nullptr);

    std::string message;
    if (length != 0 && systemMessage != nullptr) {
        std::wstring wideMessage(systemMessage, length);
        ::LocalFree(systemMessage);
        while (!wideMessage.empty() &&
            (wideMessage.back() == L'\r' || wideMessage.back() == L'\n' || wideMessage.back() == L' ')) {
            wideMessage.pop_back();
        }

        if (!wideMessage.empty()) {
            const int utf8Length = ::WideCharToMultiByte(CP_UTF8, 0,
                wideMessage.data(), static_cast<int>(wideMessage.size()),
                nullptr, 0, nullptr, nullptr);
            if (utf8Length > 0) {
                message.resize(static_cast<size_t>(utf8Length));
                ::WideCharToMultiByte(CP_UTF8, 0,
                    wideMessage.data(), static_cast<int>(wideMessage.size()),
                    &message[0], utf8Length, nullptr, nullptr);
            }
        }
    }

    std::ostringstream stream;
    stream << operation << " failed (" << errorCode;
    if (!message.empty()) {
        stream << ": " << message;
    }
    stream << ')';
    return stream.str();
}

bool IsPeerCloseError(int errorCode)
{
    return errorCode == WSAECONNRESET ||
        errorCode == WSAECONNABORTED ||
        errorCode == WSAESHUTDOWN ||
        errorCode == WSAENOTCONN;
}

void CloseNativeSocket(SOCKET socketHandle)
{
    if (socketHandle == INVALID_SOCKET) {
        return;
    }

    ::shutdown(socketHandle, SD_BOTH);
    ::closesocket(socketHandle);
}

bool SetNonBlocking(SOCKET socketHandle)
{
    u_long enabled = 1;
    return ::ioctlsocket(socketHandle, FIONBIO, &enabled) == 0;
}

void ConfigureConnectedSocket(SOCKET socketHandle)
{
    BOOL keepAlive = TRUE;
    ::setsockopt(socketHandle, SOL_SOCKET, SO_KEEPALIVE,
        reinterpret_cast<const char*>(&keepAlive), sizeof(keepAlive));

    BOOL noDelay = TRUE;
    ::setsockopt(socketHandle, IPPROTO_TCP, TCP_NODELAY,
        reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));

    tcp_keepalive keepAliveSettings = {};
    keepAliveSettings.onoff = 1;
    keepAliveSettings.keepalivetime = 10000;
    keepAliveSettings.keepaliveinterval = 2000;
    DWORD bytesReturned = 0;
    ::WSAIoctl(socketHandle, SIO_KEEPALIVE_VALS,
        &keepAliveSettings, sizeof(keepAliveSettings),
        nullptr, 0, &bytesReturned, nullptr, nullptr);
}

} // namespace

struct LanSession::Impl {
    Impl()
    {
        WSADATA data = {};
        const int result = ::WSAStartup(MAKEWORD(2, 2), &data);
        if (result == 0) {
            wsaReady = true;
        }
        else {
            SetLastError(FormatSocketError("WSAStartup", result));
        }
    }

    ~Impl()
    {
        std::lock_guard<std::mutex> lifecycleLock(lifecycleMutex);
        AdvanceGeneration();
        ShutdownWorker();
        if (wsaReady) {
            ::WSACleanup();
        }
    }

    bool StartHost(HWND notificationWindow, uint16_t port)
    {
        std::lock_guard<std::mutex> lifecycleLock(lifecycleMutex);
        const uint32_t sessionGeneration = AdvanceGeneration();
        ShutdownWorker();
        if (!PrepareStart(sessionGeneration, notificationWindow, port,
            LanRole::Host, LanState::Hosting)) {
            return false;
        }

        return LaunchWorker(sessionGeneration,
            [this, sessionGeneration, port]() { HostWorker(sessionGeneration, port); });
    }

    bool Connect(HWND notificationWindow, const std::string& address, uint16_t port)
    {
        std::lock_guard<std::mutex> lifecycleLock(lifecycleMutex);
        const uint32_t sessionGeneration = AdvanceGeneration();
        ShutdownWorker();
        if (!PrepareStart(sessionGeneration, notificationWindow, port,
            LanRole::Guest, LanState::Connecting)) {
            return false;
        }
        if (address.empty()) {
            FailAndAbort(sessionGeneration, "The host IPv4 address is empty");
            return false;
        }

        return LaunchWorker(sessionGeneration,
            [this, sessionGeneration, address, port]() {
                GuestWorker(sessionGeneration, address, port);
            });
    }

    void Stop()
    {
        std::lock_guard<std::mutex> lifecycleLock(lifecycleMutex);
        const uint32_t stopGeneration = AdvanceGeneration();
        ShutdownWorker();
        role.store(LanRole::None);
        SetState(stopGeneration, LanState::Stopped);
    }

    bool SendMove(uint8_t srcIndex, uint8_t dstIndex)
    {
        const uint32_t sessionGeneration = generation.load();
        if (srcIndex >= 63 || dstIndex >= 63) {
            SetLastErrorIfCurrent(sessionGeneration, "Move indexes must be in the range 0..62");
            return false;
        }
        if (!IsCurrent(sessionGeneration) ||
            state.load() != LanState::Connected || stopRequested.load()) {
            SetLastErrorIfCurrent(sessionGeneration, "There is no connected LAN peer");
            return false;
        }

        const std::array<uint8_t, 2> body = { srcIndex, dstIndex };
        std::lock_guard<std::mutex> lock(sendMutex);
        if (!IsCurrent(sessionGeneration) ||
            state.load() != LanState::Connected || stopRequested.load()) {
            SetLastErrorIfCurrent(sessionGeneration, "There is no connected LAN peer");
            return false;
        }
        const SOCKET socketHandle = peerSocket.load();
        if (socketHandle == INVALID_SOCKET) {
            SetLastErrorIfCurrent(sessionGeneration, "There is no connected LAN peer");
            return false;
        }

        std::string ioError;
        const IoResult result = SendFrame(sessionGeneration, socketHandle,
            kNetMoveSync, body.data(), body.size(), ioError);
        if (result == IoResult::Ok) {
            return true;
        }

        if (result != IoResult::Cancelled) {
            if (ioError.empty()) {
                ioError = "The LAN peer disconnected while sending a move";
            }
            FailAndAbort(sessionGeneration, ioError);
        }
        return false;
    }

    uint32_t AdvanceGeneration()
    {
        std::lock_guard<std::mutex> notificationLock(notificationMutex);
        uint32_t next = (generation.load() & LAN_NOTIFICATION_GENERATION_MASK) + 1u;
        if (next > LAN_NOTIFICATION_GENERATION_MASK) {
            next = 1u;
        }
        generation.store(next);
        return next;
    }

    void ShutdownWorker()
    {
        stopRequested.store(true);
        CloseSocket(peerSocket);
        CloseSocket(listenSocket);
        if (worker.joinable()) {
            worker.join();
        }
    }

    bool PrepareStart(uint32_t sessionGeneration, HWND notificationWindow,
        uint16_t port, LanRole newRole, LanState initialState)
    {
        ownerWindow.store(notificationWindow);
        role.store(newRole);
        ClearLastError();

        if (!wsaReady) {
            if (GetLastError().empty()) {
                SetLastError("Windows Sockets is unavailable");
            }
            SetState(sessionGeneration, LanState::Failed);
            return false;
        }
        if (notificationWindow == nullptr || !::IsWindow(notificationWindow)) {
            SetLastError("The notification window is invalid");
            SetState(sessionGeneration, LanState::Failed);
            return false;
        }
        if (port == 0) {
            SetLastError("The LAN port must be between 1 and 65535");
            SetState(sessionGeneration, LanState::Failed);
            return false;
        }

        stopRequested.store(false);
        SetState(sessionGeneration, initialState);
        return true;
    }

    template<typename WorkerFunction>
    bool LaunchWorker(uint32_t sessionGeneration, WorkerFunction&& function)
    {
        try {
            worker = std::thread(std::forward<WorkerFunction>(function));
            return true;
        }
        catch (const std::exception& error) {
            FailAndAbort(sessionGeneration,
                std::string("Unable to start the LAN worker: ") + error.what());
            return false;
        }
        catch (...) {
            FailAndAbort(sessionGeneration, "Unable to start the LAN worker");
            return false;
        }
    }

    void HostWorker(uint32_t sessionGeneration, uint16_t port)
    {
        SOCKET listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener == INVALID_SOCKET) {
            FailFromWorker(sessionGeneration, FormatSocketError("socket", ::WSAGetLastError()));
            return;
        }
        if (!PublishSocket(sessionGeneration, listenSocket, listener)) {
            return;
        }

        BOOL exclusiveAddress = TRUE;
        if (::setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
            reinterpret_cast<const char*>(&exclusiveAddress), sizeof(exclusiveAddress)) == SOCKET_ERROR) {
            const int errorCode = ::WSAGetLastError();
            ReleaseSocket(listenSocket, listener);
            FailFromWorker(sessionGeneration, FormatSocketError("setsockopt", errorCode));
            return;
        }
        if (!SetNonBlocking(listener)) {
            const int errorCode = ::WSAGetLastError();
            ReleaseSocket(listenSocket, listener);
            FailFromWorker(sessionGeneration, FormatSocketError("ioctlsocket", errorCode));
            return;
        }

        sockaddr_in localAddress = {};
        localAddress.sin_family = AF_INET;
        localAddress.sin_port = ::htons(port);
        localAddress.sin_addr.s_addr = ::htonl(INADDR_ANY);

        if (::bind(listener, reinterpret_cast<const sockaddr*>(&localAddress), sizeof(localAddress)) == SOCKET_ERROR) {
            const int errorCode = ::WSAGetLastError();
            ReleaseSocket(listenSocket, listener);
            FailFromWorker(sessionGeneration, FormatSocketError("bind", errorCode));
            return;
        }
        if (::listen(listener, 1) == SOCKET_ERROR) {
            const int errorCode = ::WSAGetLastError();
            ReleaseSocket(listenSocket, listener);
            FailFromWorker(sessionGeneration, FormatSocketError("listen", errorCode));
            return;
        }

        while (!IsCancelled(sessionGeneration)) {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(listener, &readSet);
            timeval timeout = {};
            timeout.tv_usec = static_cast<long>(kPollInterval.count() * 1000);

            const int selected = ::select(0, &readSet, nullptr, nullptr, &timeout);
            if (selected == SOCKET_ERROR) {
                const int errorCode = ::WSAGetLastError();
                if (IsCancelled(sessionGeneration)) {
                    ReleaseSocket(listenSocket, listener);
                    return;
                }
                ReleaseSocket(listenSocket, listener);
                FailFromWorker(sessionGeneration, FormatSocketError("select", errorCode));
                return;
            }
            if (selected == 0) {
                continue;
            }

            SOCKET candidate = ::accept(listener, nullptr, nullptr);
            if (candidate == INVALID_SOCKET) {
                const int errorCode = ::WSAGetLastError();
                if (errorCode == WSAEWOULDBLOCK || errorCode == WSAECONNRESET ||
                    errorCode == WSAECONNABORTED) {
                    continue;
                }
                if (IsCancelled(sessionGeneration)) {
                    ReleaseSocket(listenSocket, listener);
                    return;
                }
                ReleaseSocket(listenSocket, listener);
                FailFromWorker(sessionGeneration, FormatSocketError("accept", errorCode));
                return;
            }
            if (!SetNonBlocking(candidate)) {
                CloseNativeSocket(candidate);
                continue;
            }
            ConfigureConnectedSocket(candidate);
            if (!PublishSocket(sessionGeneration, peerSocket, candidate)) {
                ReleaseSocket(listenSocket, listener);
                return;
            }

            Frame joinRequest;
            std::string ioError;
            IoResult result = ReceiveFrame(sessionGeneration, candidate,
                joinRequest, kHandshakeTimeout, ioError);
            const bool validJoin = result == IoResult::Ok &&
                joinRequest.messageId == kNetJoinReq && joinRequest.body.empty();
            if (!validJoin) {
                ReleaseSocket(peerSocket, candidate);
                if (IsCancelled(sessionGeneration)) {
                    ReleaseSocket(listenSocket, listener);
                    return;
                }
                continue;
            }

            const std::array<uint8_t, 2> acknowledgement = { 1, kGuestCamp };
            {
                std::lock_guard<std::mutex> lock(sendMutex);
                result = SendFrame(sessionGeneration, candidate, kNetJoinAck,
                    acknowledgement.data(), acknowledgement.size(), ioError);
            }
            if (result != IoResult::Ok) {
                ReleaseSocket(peerSocket, candidate);
                if (IsCancelled(sessionGeneration)) {
                    ReleaseSocket(listenSocket, listener);
                    return;
                }
                continue;
            }

            // The listening socket remains available until a complete JOIN/ACK
            // exchange succeeds, so an accidental or malformed connection cannot
            // destroy the room.
            ReleaseSocket(listenSocket, listener);
            if (!SetState(sessionGeneration, LanState::Connected)) {
                ReleaseSocket(peerSocket, candidate);
                return;
            }
            ReceiveMoveLoop(sessionGeneration, candidate);
            ReleaseSocket(peerSocket, candidate);
            return;
        }

        ReleaseSocket(listenSocket, listener);
    }

    void GuestWorker(uint32_t sessionGeneration, const std::string& address, uint16_t port)
    {
        in_addr parsedAddress = {};
        if (::InetPtonA(AF_INET, address.c_str(), &parsedAddress) != 1) {
            FailFromWorker(sessionGeneration, "The host address is not a valid IPv4 address");
            return;
        }

        SOCKET peer = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (peer == INVALID_SOCKET) {
            FailFromWorker(sessionGeneration, FormatSocketError("socket", ::WSAGetLastError()));
            return;
        }
        if (!PublishSocket(sessionGeneration, peerSocket, peer)) {
            return;
        }
        if (!SetNonBlocking(peer)) {
            const int errorCode = ::WSAGetLastError();
            ReleaseSocket(peerSocket, peer);
            FailFromWorker(sessionGeneration, FormatSocketError("ioctlsocket", errorCode));
            return;
        }

        sockaddr_in remoteAddress = {};
        remoteAddress.sin_family = AF_INET;
        remoteAddress.sin_port = ::htons(port);
        remoteAddress.sin_addr = parsedAddress;

        int connectResult = ::connect(peer,
            reinterpret_cast<const sockaddr*>(&remoteAddress), sizeof(remoteAddress));
        if (connectResult == SOCKET_ERROR) {
            const int errorCode = ::WSAGetLastError();
            if (errorCode != WSAEWOULDBLOCK && errorCode != WSAEINPROGRESS && errorCode != WSAEINVAL) {
                ReleaseSocket(peerSocket, peer);
                FailFromWorker(sessionGeneration, FormatSocketError("connect", errorCode));
                return;
            }

            std::string connectError;
            const IoResult waitResult = WaitForConnect(sessionGeneration,
                peer, kConnectTimeout, connectError);
            if (waitResult != IoResult::Ok) {
                ReleaseSocket(peerSocket, peer);
                HandleWorkerIoFailure(sessionGeneration, waitResult,
                    connectError, "Unable to connect to the host");
                return;
            }
        }

        ConfigureConnectedSocket(peer);

        std::string ioError;
        IoResult result;
        {
            std::lock_guard<std::mutex> lock(sendMutex);
            result = SendFrame(sessionGeneration, peer, kNetJoinReq, nullptr, 0, ioError);
        }
        if (result != IoResult::Ok) {
            ReleaseSocket(peerSocket, peer);
            HandleWorkerIoFailure(sessionGeneration, result,
                ioError, "The host disconnected during the handshake");
            return;
        }

        Frame acknowledgement;
        result = ReceiveFrame(sessionGeneration, peer,
            acknowledgement, kHandshakeTimeout, ioError);
        if (result != IoResult::Ok) {
            ReleaseSocket(peerSocket, peer);
            HandleWorkerIoFailure(sessionGeneration, result,
                ioError, "The host disconnected before completing the handshake");
            return;
        }
        if (acknowledgement.messageId != kNetJoinAck || acknowledgement.body.size() != 2) {
            ReleaseSocket(peerSocket, peer);
            FailFromWorker(sessionGeneration, "Invalid JOIN_ACK frame");
            return;
        }
        if (acknowledgement.body[0] != 1) {
            ReleaseSocket(peerSocket, peer);
            FailFromWorker(sessionGeneration, "The host rejected the join request");
            return;
        }
        if (acknowledgement.body[1] != kGuestCamp) {
            ReleaseSocket(peerSocket, peer);
            FailFromWorker(sessionGeneration, "The host assigned an unsupported camp");
            return;
        }

        if (!SetState(sessionGeneration, LanState::Connected)) {
            ReleaseSocket(peerSocket, peer);
            return;
        }
        ReceiveMoveLoop(sessionGeneration, peer);
        ReleaseSocket(peerSocket, peer);
    }

    void ReceiveMoveLoop(uint32_t sessionGeneration, SOCKET peer)
    {
        while (!IsCancelled(sessionGeneration)) {
            Frame frame;
            std::string ioError;
            const IoResult result = ReceiveFrame(sessionGeneration, peer,
                frame, std::chrono::milliseconds::max(), ioError);
            if (result != IoResult::Ok) {
                HandleWorkerIoFailure(sessionGeneration, result,
                    ioError, "The LAN peer disconnected");
                return;
            }
            if (frame.messageId != kNetMoveSync || frame.body.size() != 2) {
                FailFromWorker(sessionGeneration, "Unexpected LAN protocol frame");
                return;
            }
            if (frame.body[0] >= 63 || frame.body[1] >= 63) {
                FailFromWorker(sessionGeneration, "The LAN peer sent an invalid board index");
                return;
            }
            PostMove(sessionGeneration, frame.body[0], frame.body[1]);
        }
    }

    IoResult WaitForConnect(uint32_t sessionGeneration, SOCKET socketHandle,
        std::chrono::milliseconds timeout, std::string& error)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!IsCancelled(sessionGeneration)) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                error = "Connection attempt timed out";
                return IoResult::Timeout;
            }

            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
            const auto waitTime = (std::min)(remaining, kPollInterval);

            fd_set writeSet;
            fd_set errorSet;
            FD_ZERO(&writeSet);
            FD_ZERO(&errorSet);
            FD_SET(socketHandle, &writeSet);
            FD_SET(socketHandle, &errorSet);
            timeval selectTimeout = {};
            selectTimeout.tv_sec = static_cast<long>(waitTime.count() / 1000);
            selectTimeout.tv_usec = static_cast<long>((waitTime.count() % 1000) * 1000);

            const int selected = ::select(0, nullptr, &writeSet, &errorSet, &selectTimeout);
            if (selected == SOCKET_ERROR) {
                if (IsCancelled(sessionGeneration)) {
                    return IoResult::Cancelled;
                }
                error = FormatSocketError("select", ::WSAGetLastError());
                return IoResult::Error;
            }
            if (selected == 0) {
                continue;
            }

            int socketError = 0;
            int optionLength = sizeof(socketError);
            if (::getsockopt(socketHandle, SOL_SOCKET, SO_ERROR,
                reinterpret_cast<char*>(&socketError), &optionLength) == SOCKET_ERROR) {
                if (IsCancelled(sessionGeneration)) {
                    return IoResult::Cancelled;
                }
                error = FormatSocketError("getsockopt", ::WSAGetLastError());
                return IoResult::Error;
            }
            if (socketError != 0) {
                error = FormatSocketError("connect", socketError);
                return IsPeerCloseError(socketError) ? IoResult::Closed : IoResult::Error;
            }
            return IoResult::Ok;
        }
        return IoResult::Cancelled;
    }

    IoResult WaitForSocket(uint32_t sessionGeneration, SOCKET socketHandle, bool waitForWrite,
        std::chrono::milliseconds timeout, std::string& error)
    {
        fd_set descriptorSet;
        FD_ZERO(&descriptorSet);
        FD_SET(socketHandle, &descriptorSet);
        timeval selectTimeout = {};
        selectTimeout.tv_sec = static_cast<long>(timeout.count() / 1000);
        selectTimeout.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

        const int selected = waitForWrite
            ? ::select(0, nullptr, &descriptorSet, nullptr, &selectTimeout)
            : ::select(0, &descriptorSet, nullptr, nullptr, &selectTimeout);
        if (selected == SOCKET_ERROR) {
            if (IsCancelled(sessionGeneration)) {
                return IoResult::Cancelled;
            }
            error = FormatSocketError("select", ::WSAGetLastError());
            return IoResult::Error;
        }
        return selected == 0 ? IoResult::Timeout : IoResult::Ok;
    }

    IoResult SendAll(uint32_t sessionGeneration, SOCKET socketHandle,
        const uint8_t* bytes, size_t byteCount, std::string& error)
    {
        const auto deadline = std::chrono::steady_clock::now() + kSendTimeout;
        size_t sent = 0;
        while (sent < byteCount) {
            if (IsCancelled(sessionGeneration)) {
                return IoResult::Cancelled;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                error = "Sending the LAN frame timed out";
                return IoResult::Timeout;
            }

            const size_t remaining = byteCount - sent;
            const int chunkLength = static_cast<int>((std::min<size_t>)(remaining, INT_MAX));
            const int result = ::send(socketHandle,
                reinterpret_cast<const char*>(bytes + sent), chunkLength, 0);
            if (result > 0) {
                sent += static_cast<size_t>(result);
                continue;
            }
            if (result == 0) {
                return IoResult::Closed;
            }

            const int errorCode = ::WSAGetLastError();
            if (errorCode == WSAEWOULDBLOCK) {
                const IoResult waitResult = WaitForSocket(sessionGeneration,
                    socketHandle, true, kPollInterval, error);
                if (waitResult == IoResult::Timeout) {
                    continue;
                }
                if (waitResult != IoResult::Ok) {
                    return waitResult;
                }
                continue;
            }
            if (IsCancelled(sessionGeneration)) {
                return IoResult::Cancelled;
            }
            if (IsPeerCloseError(errorCode)) {
                return IoResult::Closed;
            }
            error = FormatSocketError("send", errorCode);
            return IoResult::Error;
        }
        return IoResult::Ok;
    }

    IoResult ReceiveAll(uint32_t sessionGeneration, SOCKET socketHandle,
        uint8_t* bytes, size_t byteCount,
        std::chrono::milliseconds timeout, std::string& error)
    {
        const bool hasDeadline = timeout != std::chrono::milliseconds::max();
        const auto deadline = hasDeadline
            ? std::chrono::steady_clock::now() + timeout
            : std::chrono::steady_clock::time_point::max();

        size_t received = 0;
        while (received < byteCount) {
            if (IsCancelled(sessionGeneration)) {
                return IoResult::Cancelled;
            }

            const size_t remaining = byteCount - received;
            const int chunkLength = static_cast<int>((std::min<size_t>)(remaining, INT_MAX));
            const int result = ::recv(socketHandle,
                reinterpret_cast<char*>(bytes + received), chunkLength, 0);
            if (result > 0) {
                received += static_cast<size_t>(result);
                continue;
            }
            if (result == 0) {
                return IoResult::Closed;
            }

            const int errorCode = ::WSAGetLastError();
            if (errorCode == WSAEWOULDBLOCK) {
                std::chrono::milliseconds waitTime = kPollInterval;
                if (hasDeadline) {
                    const auto now = std::chrono::steady_clock::now();
                    if (now >= deadline) {
                        error = "LAN protocol operation timed out";
                        return IoResult::Timeout;
                    }
                    const auto remainingTime = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
                    waitTime = (std::min)(waitTime, remainingTime);
                }

                const IoResult waitResult = WaitForSocket(sessionGeneration,
                    socketHandle, false, waitTime, error);
                if (waitResult == IoResult::Timeout) {
                    continue;
                }
                if (waitResult != IoResult::Ok) {
                    return waitResult;
                }
                continue;
            }
            if (IsCancelled(sessionGeneration)) {
                return IoResult::Cancelled;
            }
            if (IsPeerCloseError(errorCode)) {
                return IoResult::Closed;
            }
            error = FormatSocketError("recv", errorCode);
            return IoResult::Error;
        }
        return IoResult::Ok;
    }

    IoResult SendFrame(uint32_t sessionGeneration, SOCKET socketHandle, uint16_t messageId,
        const uint8_t* body, size_t bodyLength, std::string& error)
    {
        if (bodyLength > kMaxBodyLength || bodyLength > UINT32_MAX) {
            error = "LAN frame body is too large";
            return IoResult::Error;
        }

        std::array<uint8_t, kHeaderLength> header = {};
        header[0] = kMagic0;
        header[1] = kMagic1;
        const uint16_t networkMessageId = ::htons(messageId);
        const uint32_t networkBodyLength = ::htonl(static_cast<uint32_t>(bodyLength));
        std::memcpy(header.data() + 2, &networkMessageId, sizeof(networkMessageId));
        std::memcpy(header.data() + 4, &networkBodyLength, sizeof(networkBodyLength));

        IoResult result = SendAll(sessionGeneration,
            socketHandle, header.data(), header.size(), error);
        if (result != IoResult::Ok || bodyLength == 0) {
            return result;
        }
        return SendAll(sessionGeneration, socketHandle, body, bodyLength, error);
    }

    IoResult ReceiveFrame(uint32_t sessionGeneration, SOCKET socketHandle, Frame& frame,
        std::chrono::milliseconds timeout, std::string& error)
    {
        std::array<uint8_t, kHeaderLength> header = {};
        IoResult result = ReceiveAll(sessionGeneration,
            socketHandle, header.data(), header.size(), timeout, error);
        if (result != IoResult::Ok) {
            return result;
        }
        if (header[0] != kMagic0 || header[1] != kMagic1) {
            error = "Invalid LAN protocol magic";
            return IoResult::Error;
        }

        uint16_t networkMessageId = 0;
        uint32_t networkBodyLength = 0;
        std::memcpy(&networkMessageId, header.data() + 2, sizeof(networkMessageId));
        std::memcpy(&networkBodyLength, header.data() + 4, sizeof(networkBodyLength));
        frame.messageId = ::ntohs(networkMessageId);
        const uint32_t bodyLength = ::ntohl(networkBodyLength);
        if (bodyLength > kMaxBodyLength) {
            error = "LAN frame body exceeds the maximum length";
            return IoResult::Error;
        }

        frame.body.assign(bodyLength, 0);
        if (bodyLength == 0) {
            return IoResult::Ok;
        }
        return ReceiveAll(sessionGeneration,
            socketHandle, frame.body.data(), frame.body.size(), timeout, error);
    }

    void HandleWorkerIoFailure(uint32_t sessionGeneration, IoResult result,
        const std::string& ioError, const char* closedMessage)
    {
        if (result == IoResult::Cancelled || IsCancelled(sessionGeneration)) {
            return;
        }
        if (result == IoResult::Closed) {
            SetStateWithError(sessionGeneration, LanState::Disconnected, closedMessage);
            return;
        }
        if (!ioError.empty()) {
            FailFromWorker(sessionGeneration, ioError);
        }
        else if (result == IoResult::Timeout) {
            FailFromWorker(sessionGeneration, "LAN protocol operation timed out");
        }
        else {
            FailFromWorker(sessionGeneration, "LAN network operation failed");
        }
    }

    bool PublishSocket(uint32_t sessionGeneration,
        std::atomic<SOCKET>& slot, SOCKET socketHandle)
    {
        slot.store(socketHandle);
        if (!IsCancelled(sessionGeneration)) {
            return true;
        }

        SOCKET expected = socketHandle;
        if (slot.compare_exchange_strong(expected, INVALID_SOCKET)) {
            CloseNativeSocket(socketHandle);
        }
        return false;
    }

    void ReleaseSocket(std::atomic<SOCKET>& slot, SOCKET socketHandle)
    {
        SOCKET expected = socketHandle;
        if (slot.compare_exchange_strong(expected, INVALID_SOCKET)) {
            CloseNativeSocket(socketHandle);
        }
    }

    void CloseSocket(std::atomic<SOCKET>& slot)
    {
        const SOCKET socketHandle = slot.exchange(INVALID_SOCKET);
        CloseNativeSocket(socketHandle);
    }

    void FailAndAbort(uint32_t sessionGeneration, const std::string& message)
    {
        std::lock_guard<std::mutex> notificationLock(notificationMutex);
        if (generation.load() != sessionGeneration) {
            return;
        }
        SetLastError(message);
        state.store(LanState::Failed);
        stopRequested.store(true);
        CloseSocket(peerSocket);
        CloseSocket(listenSocket);
        PostStateChangedLocked(sessionGeneration, LanState::Failed, role.load());
    }

    void FailFromWorker(uint32_t sessionGeneration, const std::string& message)
    {
        std::lock_guard<std::mutex> notificationLock(notificationMutex);
        if (generation.load() != sessionGeneration || stopRequested.load()) {
            return;
        }
        SetLastError(message);
        state.store(LanState::Failed);
        PostStateChangedLocked(sessionGeneration, LanState::Failed, role.load());
    }

    bool SetState(uint32_t sessionGeneration, LanState newState)
    {
        std::lock_guard<std::mutex> notificationLock(notificationMutex);
        if (generation.load() != sessionGeneration) {
            return false;
        }
        state.store(newState);
        PostStateChangedLocked(sessionGeneration, newState, role.load());
        return true;
    }

    void SetStateWithError(uint32_t sessionGeneration, LanState newState,
        const std::string& message)
    {
        std::lock_guard<std::mutex> notificationLock(notificationMutex);
        if (generation.load() != sessionGeneration || stopRequested.load()) {
            return;
        }
        SetLastError(message);
        state.store(newState);
        PostStateChangedLocked(sessionGeneration, newState, role.load());
    }

    void PostMove(uint32_t sessionGeneration, uint8_t srcIndex, uint8_t dstIndex)
    {
        std::lock_guard<std::mutex> notificationLock(notificationMutex);
        if (generation.load() != sessionGeneration || stopRequested.load() ||
            state.load() != LanState::Connected) {
            return;
        }
        const HWND target = ownerWindow.load();
        if (target != nullptr) {
            ::PostMessage(target, WM_APP_LAN_MOVE_RECEIVED,
                LanPackNotification(sessionGeneration, srcIndex),
                static_cast<LPARAM>(dstIndex));
        }
    }

    void PostStateChangedLocked(uint32_t sessionGeneration,
        LanState newState, LanRole currentRole) const
    {
        const HWND target = ownerWindow.load();
        if (target != nullptr) {
            ::PostMessage(target, WM_APP_LAN_STATE_CHANGED,
                LanPackNotification(sessionGeneration, static_cast<uint8_t>(newState)),
                static_cast<LPARAM>(currentRole));
        }
    }

    bool IsCurrent(uint32_t sessionGeneration) const
    {
        return sessionGeneration != 0 && generation.load() == sessionGeneration;
    }

    bool IsCancelled(uint32_t sessionGeneration) const
    {
        return !IsCurrent(sessionGeneration) || stopRequested.load();
    }

    void SetLastErrorIfCurrent(uint32_t sessionGeneration, const std::string& message)
    {
        std::lock_guard<std::mutex> notificationLock(notificationMutex);
        if (generation.load() == sessionGeneration) {
            SetLastError(message);
        }
    }

    void SetLastError(const std::string& message)
    {
        std::lock_guard<std::mutex> lock(errorMutex);
        lastError = message;
    }

    void ClearLastError()
    {
        std::lock_guard<std::mutex> lock(errorMutex);
        lastError.clear();
    }

    std::string GetLastError() const
    {
        std::lock_guard<std::mutex> lock(errorMutex);
        return lastError;
    }

    std::atomic<LanState> state{ LanState::Stopped };
    std::atomic<LanRole> role{ LanRole::None };
    std::atomic<uint32_t> generation{ 0 };
    std::atomic<HWND> ownerWindow{ nullptr };
    std::atomic<bool> stopRequested{ true };
    std::atomic<SOCKET> listenSocket{ INVALID_SOCKET };
    std::atomic<SOCKET> peerSocket{ INVALID_SOCKET };
    std::mutex lifecycleMutex;
    std::mutex notificationMutex;
    mutable std::mutex errorMutex;
    std::mutex sendMutex;
    std::string lastError;
    std::thread worker;
    bool wsaReady = false;
};

LanSession::LanSession()
    : m_impl(std::make_unique<Impl>())
{
}

LanSession::~LanSession() = default;

bool LanSession::StartHost(HWND notificationWindow, uint16_t port)
{
    return m_impl->StartHost(notificationWindow, port);
}

bool LanSession::Connect(HWND notificationWindow, const std::string& address, uint16_t port)
{
    return m_impl->Connect(notificationWindow, address, port);
}

void LanSession::Stop()
{
    m_impl->Stop();
}

bool LanSession::SendMove(uint8_t srcIndex, uint8_t dstIndex)
{
    return m_impl->SendMove(srcIndex, dstIndex);
}

LanState LanSession::GetState() const
{
    return m_impl->state.load();
}

LanRole LanSession::GetRole() const
{
    return m_impl->role.load();
}

uint32_t LanSession::GetGeneration() const
{
    return m_impl->generation.load();
}

std::string LanSession::GetLastError() const
{
    return m_impl->GetLastError();
}

std::string LanSession::GetLocalIPv4Address()
{
    WSADATA data = {};
    if (::WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        return "127.0.0.1";
    }

    std::vector<std::string> addresses;
    ULONG bufferLength = 0;
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST |
        GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG resultCode = ::GetAdaptersAddresses(AF_INET, flags,
        nullptr, nullptr, &bufferLength);
    if (resultCode == ERROR_BUFFER_OVERFLOW && bufferLength != 0) {
        std::vector<uint8_t> buffer(bufferLength);
        auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        resultCode = ::GetAdaptersAddresses(AF_INET, flags,
            nullptr, adapters, &bufferLength);
        if (resultCode == NO_ERROR) {
            for (const IP_ADAPTER_ADDRESSES* adapter = adapters;
                adapter != nullptr; adapter = adapter->Next) {
                if (adapter->OperStatus != IfOperStatusUp) {
                    continue;
                }
                for (const IP_ADAPTER_UNICAST_ADDRESS* unicast = adapter->FirstUnicastAddress;
                    unicast != nullptr; unicast = unicast->Next) {
                    if (unicast->Address.lpSockaddr == nullptr ||
                        unicast->Address.iSockaddrLength < static_cast<int>(sizeof(sockaddr_in)) ||
                        unicast->Address.lpSockaddr->sa_family != AF_INET) {
                        continue;
                    }

                    const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(
                        unicast->Address.lpSockaddr);
                    const uint32_t hostOrderAddress = ::ntohl(ipv4->sin_addr.s_addr);
                    const bool loopback =
                        (hostOrderAddress & 0xFF000000u) == 0x7F000000u;
                    if (loopback || hostOrderAddress == 0) {
                        continue;
                    }

                    char text[INET_ADDRSTRLEN] = {};
                    if (::InetNtopA(AF_INET,
                        const_cast<in_addr*>(&ipv4->sin_addr), text, sizeof(text)) == nullptr) {
                        continue;
                    }
                    const std::string address(text);
                    if (std::find(addresses.begin(), addresses.end(), address) == addresses.end()) {
                        addresses.push_back(address);
                    }
                }
            }
        }
    }

    ::WSACleanup();
    if (addresses.empty()) {
        return "127.0.0.1";
    }

    std::ostringstream joined;
    for (size_t index = 0; index < addresses.size(); ++index) {
        if (index != 0) {
            joined << " / ";
        }
        joined << addresses[index];
    }
    return joined.str();
}
