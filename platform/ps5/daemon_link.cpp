/*
 * Accord - PS5 side of the title-to-daemon link probe (T2.2b).
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The fallback variant of T2.3c sends PCM and a heartbeat from the daemon to the
 * title over 127.0.0.1, so the title has to be able to open that connection at
 * all. This is the smallest thing that answers it: one GET /status, timed, with
 * the HTTP status or the failure kept as a value the caller can show on screen
 * and write to klog.
 *
 * Deliberately not the daemon's own client. In a title several POSIX symbols
 * link but resolve to a null import at run time: the harness's own networking
 * example (third_party/ps5-native-app-boilerplate/examples/update-check/
 * console_curl.c) exists because of exactly that, and the first version of this
 * probe jumped to address zero through getaddrinfo. So the address is written
 * numerically, the name resolver is never called, and poll, fcntl and strerror
 * stay out too -- sockets, setsockopt, connect, send, recv and close are the
 * whole surface, which is the set a title's libcurl already proves works.
 */

#include "platform/ps5/daemon_link.hpp"

#include "ui/kit/platform/ps5/system.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace accord::ps5
{
namespace
{
constexpr std::size_t kMaxAnswerBytes = 8u << 10;
constexpr int kReceiveTimeoutSeconds = 2;
// 127.0.0.1 in network order: no resolver, no inet_pton, no getaddrinfo.
constexpr std::uint32_t kLoopback = 0x7f000001u;

std::string error_name(int code)
{
    // The few a localhost connection can produce, named here rather than through
    // strerror, which is another symbol a title cannot rely on.
    switch (code)
    {
    case ECONNREFUSED:
        return "connection refused";
    case ETIMEDOUT:
        return "timed out";
    case EAGAIN:
        return "nothing arrived in time";
    case ENETUNREACH:
        return "network unreachable";
    case EPIPE:
        return "the daemon hung up";
    default:
        break;
    }
    char text[32];
    std::snprintf(text, sizeof text, "error %d", code);
    return text;
}

std::string head_of(const std::string &body)
{
    constexpr std::size_t kDetailBytes = 80;
    if (body.size() <= kDetailBytes)
        return body;
    return body.substr(0, kDetailBytes) + "...";
}

// Only what the one line needs: the status code and the body. The answer is
// HTTP/1.1 with a body that ends when the connection does.
bool parse_answer(const std::string &answer, int &status, std::string &body)
{
    const std::size_t first_line_end = answer.find("\r\n");
    if (first_line_end == std::string::npos)
        return false;
    const std::string first_line = answer.substr(0, first_line_end);
    const std::size_t code_at = first_line.find(' ');
    if (code_at == std::string::npos)
        return false;
    status = std::atoi(first_line.c_str() + code_at + 1);
    if (status < 100 || status > 599)
        return false;

    const std::size_t separator = answer.find("\r\n\r\n");
    if (separator == std::string::npos)
        return false;
    body = answer.substr(separator + 4);
    return true;
}

double since(const std::int64_t started)
{
    return static_cast<double>(hui::sys::monotonic_us() - started) / 1000.0;
}
} // namespace

accord::ui::DaemonLinkResult probe_daemon_link(int timeout_ms)
{
    accord::ui::DaemonLinkResult result;
    result.probed = true;
    (void)timeout_ms; // the receive window below is what bounds the exchange

    const std::int64_t started = hui::sys::monotonic_us();

    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        result.milliseconds = since(started);
        result.detail = "socket: " + error_name(errno);
        return result;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(accord::ui::kDaemonPort);
    address.sin_addr.s_addr = htonl(kLoopback);

    timeval window{};
    window.tv_sec = kReceiveTimeoutSeconds;
    (void)::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &window, sizeof window);

    if (::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof address) != 0)
    {
        const int code = errno;
        ::close(fd);
        result.milliseconds = since(started);
        result.detail = "connect: " + error_name(code);
        return result;
    }

    const std::string port = std::to_string(accord::ui::kDaemonPort);
    const std::string request = "GET /status HTTP/1.1\r\nHost: 127.0.0.1:" + port +
                                "\r\nConnection: close\r\n\r\n";

    std::size_t written = 0;
    while (written < request.size())
    {
        const ssize_t sent = ::send(fd, request.data() + written, request.size() - written, 0);
        if (sent <= 0)
        {
            const int code = errno;
            ::close(fd);
            result.milliseconds = since(started);
            result.detail = "send: " + error_name(code);
            return result;
        }
        written += static_cast<std::size_t>(sent);
    }

    std::string answer;
    for (;;)
    {
        char buffer[512];
        const ssize_t got = ::recv(fd, buffer, sizeof buffer, 0);
        if (got == 0)
            break;
        if (got < 0)
        {
            // A window that ran out with a whole answer already in hand is not a
            // failure of the link; anything else is reported with its name.
            if ((errno == EAGAIN || errno == EWOULDBLOCK) && !answer.empty())
                break;
            const int code = errno;
            ::close(fd);
            result.milliseconds = since(started);
            result.detail = "recv: " + error_name(code);
            return result;
        }
        answer.append(buffer, static_cast<std::size_t>(got));
        if (answer.size() >= kMaxAnswerBytes)
            break;
    }
    ::close(fd);

    result.milliseconds = since(started);

    std::string body;
    if (!parse_answer(answer, result.status, body))
    {
        result.detail = answer.empty() ? "the daemon sent nothing" : "unreadable answer: " +
                                                                        head_of(answer);
        return result;
    }

    result.answered = true;
    result.detail = head_of(body);
    return result;
}
} // namespace accord::ps5
