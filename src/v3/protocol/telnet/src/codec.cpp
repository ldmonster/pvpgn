// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol/telnet/codec.hpp"

#include "core/error.hpp"

namespace pvpgn::protocol::telnet {

core::Result<FramedLine> try_parse_line(std::string_view buf) noexcept {
    const auto nl = buf.find('\n');
    if (nl == std::string_view::npos) {
        return core::fail(core::Error{
            core::StatusCode::OutOfRange, "telnet: incomplete line"});
    }
    const std::size_t consumed = nl + 1;
    std::size_t end = nl;
    if (end > 0 && buf[end - 1] == '\r') --end;
    return FramedLine{buf.substr(0, end), consumed};
}

Command tokenise(std::string_view line) {
    Command c;
    std::size_t i = 0;
    auto is_space = [](char ch) { return ch == ' ' || ch == '\t'; };
    while (i < line.size() && is_space(line[i])) ++i;
    auto start = i;
    while (i < line.size() && !is_space(line[i])) ++i;
    if (i > start) c.verb.assign(line.substr(start, i - start));
    while (i < line.size()) {
        while (i < line.size() && is_space(line[i])) ++i;
        if (i >= line.size()) break;
        const auto s = i;
        while (i < line.size() && !is_space(line[i])) ++i;
        c.args.emplace_back(line.substr(s, i - s));
    }
    return c;
}

void write_line(Writer& w, std::string_view line) {
    w.write_bytes(core::ByteView{
        reinterpret_cast<const std::byte*>(line.data()), line.size()});
    static constexpr std::byte crlf[] = {std::byte{'\r'}, std::byte{'\n'}};
    w.write_bytes(core::ByteView{crlf, 2});
}

}  // namespace pvpgn::protocol::telnet
