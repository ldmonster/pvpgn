// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol/irc/codec.hpp"

#include <cctype>
#include <cstring>
#include <string>

#include "core/error.hpp"

namespace pvpgn::protocol::irc {

namespace {

core::Failure<core::Error> bad(std::string_view what) {
    return core::fail(
        core::Error{core::StatusCode::InvalidArgument, std::string{what}});
}

constexpr char to_upper_ascii(char c) noexcept {
    return (c >= 'a' && c <= 'z')
               ? static_cast<char>(c - ('a' - 'A'))
               : c;
}

}  // namespace

core::Result<FramedLine> try_parse_line(std::string_view buf) noexcept {
    // Look for the first '\n'; tolerate optional preceding '\r'.
    const auto nl = buf.find('\n');
    if (nl == std::string_view::npos) {
        return core::fail(core::Error{
            core::StatusCode::OutOfRange, "irc: incomplete line"});
    }
    const std::size_t consumed = nl + 1;
    std::size_t end = nl;
    if (end > 0 && buf[end - 1] == '\r') --end;
    return FramedLine{buf.substr(0, end), consumed};
}

core::Result<Message> decode(std::string_view line) {
    if (line.empty()) {
        return bad("irc: empty line");
    }
    Message msg;
    std::size_t i = 0;

    // Optional prefix: ":" word SPACE
    if (line[0] == ':') {
        ++i;
        const auto sp = line.find(' ', i);
        if (sp == std::string_view::npos) {
            return bad("irc: prefix without command");
        }
        msg.prefix.assign(line.substr(i, sp - i));
        i = sp + 1;
        // Skip any extra spaces.
        while (i < line.size() && line[i] == ' ') ++i;
        if (i >= line.size()) return bad("irc: trailing prefix only");
    }

    // Command: word (alpha or 3-digit numeric). Upper-cased for compare.
    const auto cmd_start = i;
    while (i < line.size() && line[i] != ' ') ++i;
    if (i == cmd_start) return bad("irc: missing command");
    msg.command.assign(line.substr(cmd_start, i - cmd_start));
    for (auto& c : msg.command) c = to_upper_ascii(c);

    // Params
    while (i < line.size()) {
        // Eat separators.
        while (i < line.size() && line[i] == ' ') ++i;
        if (i >= line.size()) break;
        if (line[i] == ':') {
            // Trailing param — rest of line, may contain spaces.
            ++i;
            msg.params.emplace_back(line.substr(i));
            i = line.size();
            break;
        }
        const auto p_start = i;
        while (i < line.size() && line[i] != ' ') ++i;
        msg.params.emplace_back(line.substr(p_start, i - p_start));
    }

    return msg;
}

core::Status<> encode(Writer& w, const Message& msg) {
    if (msg.command.empty()) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument, "irc: empty command"});
    }
    std::string out;
    out.reserve(msg.prefix.size() + msg.command.size() + 16);
    if (!msg.prefix.empty()) {
        out.push_back(':');
        out.append(msg.prefix);
        out.push_back(' ');
    }
    out.append(msg.command);
    for (std::size_t k = 0; k < msg.params.size(); ++k) {
        const auto& p = msg.params[k];
        const bool last = (k + 1 == msg.params.size());
        const bool needs_trailing =
            last && (p.empty() || p.find(' ') != std::string::npos ||
                     (!p.empty() && p[0] == ':'));
        out.push_back(' ');
        if (needs_trailing) out.push_back(':');
        out.append(p);
    }
    out.append("\r\n");
    w.write_bytes(core::ByteView{
        reinterpret_cast<const std::byte*>(out.data()), out.size()});
    return core::ok();
}

std::string encode_to_string(const Message& msg) {
    Writer w;
    (void)encode(w, msg);
    const auto v = w.view();
    return std::string{reinterpret_cast<const char*>(v.data()), v.size()};
}

}  // namespace pvpgn::protocol::irc
