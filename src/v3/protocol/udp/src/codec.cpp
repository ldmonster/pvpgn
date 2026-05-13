// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol/udp/codec.hpp"

#include <variant>

#include "core/error.hpp"
#include "protocol/common/reader.hpp"

namespace pvpgn::protocol::udp {

core::Result<Datagram> decode(core::ByteView buf) {
    Reader r{buf};
    auto type = r.read_le<std::uint32_t>();
    if (!type) return core::fail(type.error());
    switch (type.value()) {
        case kServerUdpTest: {
            auto v = r.read_le<std::uint32_t>();
            if (!v) return core::fail(v.error());
            return Datagram{UdpTest{v.value()}};
        }
        case kClientUdpPing: {
            auto v = r.read_le<std::uint32_t>();
            if (!v) return core::fail(v.error());
            return Datagram{UdpPing{v.value()}};
        }
        case kClientSessionAddr1: {
            auto v = r.read_le<std::uint32_t>();
            if (!v) return core::fail(v.error());
            return Datagram{SessionAddr1{v.value()}};
        }
        case kClientSessionAddr2: {
            auto k = r.read_le<std::uint32_t>();
            if (!k) return core::fail(k.error());
            auto n = r.read_le<std::uint32_t>();
            if (!n) return core::fail(n.error());
            return Datagram{SessionAddr2{k.value(), n.value()}};
        }
        default:
            return core::fail(core::Error{
                core::StatusCode::Unimplemented,
                "udp codec: unknown type"});
    }
}

namespace {
struct Visitor {
    Writer& w;
    void operator()(const UdpTest& m) const {
        w.write_le<std::uint32_t>(kServerUdpTest);
        w.write_le<std::uint32_t>(m.bnettag);
    }
    void operator()(const UdpPing& m) const {
        w.write_le<std::uint32_t>(kClientUdpPing);
        w.write_le<std::uint32_t>(m.cookie);
    }
    void operator()(const SessionAddr1& m) const {
        w.write_le<std::uint32_t>(kClientSessionAddr1);
        w.write_le<std::uint32_t>(m.session_key);
    }
    void operator()(const SessionAddr2& m) const {
        w.write_le<std::uint32_t>(kClientSessionAddr2);
        w.write_le<std::uint32_t>(m.session_key);
        w.write_le<std::uint32_t>(m.session_number);
    }
};
}  // namespace

core::Status<> encode(Writer& w, const Datagram& dg) {
    std::visit(Visitor{w}, dg);
    return core::ok();
}

}  // namespace pvpgn::protocol::udp
