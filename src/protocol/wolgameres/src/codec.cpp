// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol/wolgameres/codec.hpp"

#include <algorithm>
#include <cstring>

#include "core/error.hpp"
#include "protocol/common/reader.hpp"

namespace pvpgn::protocol::wolgameres {

namespace {

bool valid_data_type(std::uint16_t v) noexcept {
    switch (v) {
        case 1: case 2: case 5: case 6: case 7: case 20:
            return true;
        default:
            return false;
    }
}

}  // namespace

core::Result<Header> parse_header(core::ByteView buf) {
    Reader r{buf};
    auto sz = r.read_be<std::uint16_t>();
    if (!sz) return core::fail(sz.error());
    auto rg = r.read_be<std::uint16_t>();
    if (!rg) return core::fail(rg.error());
    if (sz.value() < Header::kSize) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "wolgameres: size < header"});
    }
    return Header{sz.value(), rg.value()};
}

core::Result<Report> decode(core::ByteView buf) {
    auto hdr = parse_header(buf);
    if (!hdr) return core::fail(hdr.error());
    if (buf.size() < hdr.value().size) {
        return core::fail(core::Error{
            core::StatusCode::OutOfRange, "wolgameres: incomplete"});
    }

    auto body = buf.subspan(Header::kSize,
                            static_cast<std::size_t>(hdr.value().size) -
                                Header::kSize);
    Report out{};
    out.header = hdr.value();

    Reader r{body};

    // Optional 4-byte zero prefix preceding the TLV list (RNDG marker).
    if (r.remaining() >= 4) {
        auto peek = core::read_be<std::uint32_t>(r.tail().subspan(0, 4));
        if (peek && peek.value() == 0u) {
            out.has_rndg_prefix = true;
            (void)r.skip(4);
        }
    }

    while (!r.empty()) {
        auto tag = r.read_be<std::uint32_t>();
        if (!tag) return core::fail(tag.error());
        auto type = r.read_be<std::uint16_t>();
        if (!type) return core::fail(type.error());
        if (!valid_data_type(type.value())) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument,
                "wolgameres: unknown data_type"});
        }
        auto len = r.read_be<std::uint16_t>();
        if (!len) return core::fail(len.error());
        auto data = r.read_bytes(len.value());
        if (!data) return core::fail(data.error());

        Entry e;
        e.tag  = tag.value();
        e.type = static_cast<DataType>(type.value());
        e.data.assign(data.value().begin(), data.value().end());
        out.entries.push_back(std::move(e));

        // WOL gameres records are padded so each value advance lands on a
        // 4-byte boundary (original: `datalen = 4*((datalen+3)/4)`). Skip
        // the trailing pad bytes so the TLV walk stays aligned. Tolerate a
        // final record whose pad bytes are missing because the buffer ends
        // exactly on the value (don't error on a truncated final pad).
        const std::size_t pad = (4u - (len.value() & 3u)) & 3u;
        if (pad != 0) {
            (void)r.skip(std::min<std::size_t>(pad, r.remaining()));
        }
    }

    return out;
}

core::Status<> encode(Writer& w, const Report& r) {
    // Build body first to compute total size.
    Writer body;
    if (r.has_rndg_prefix) {
        body.write_be<std::uint32_t>(0u);
    }
    for (const auto& e : r.entries) {
        body.write_be<std::uint32_t>(e.tag);
        body.write_be<std::uint16_t>(static_cast<std::uint16_t>(e.type));
        if (e.data.size() > 0xFFFFu) {
            return core::fail(core::Error{
                core::StatusCode::OutOfRange,
                "wolgameres: entry too long"});
        }
        body.write_be<std::uint16_t>(
            static_cast<std::uint16_t>(e.data.size()));
        body.write_bytes(core::ByteView{e.data.data(), e.data.size()});

        // Pad the value out to a 4-byte boundary to match the original wire
        // layout (`datalen = 4*((datalen+3)/4)`) and keep encode/decode
        // symmetric with the alignment skip above.
        const std::size_t pad = (4u - (e.data.size() & 3u)) & 3u;
        for (std::size_t i = 0; i < pad; ++i) body.write_u8(0u);
    }
    const auto total = body.view().size() + Header::kSize;
    if (total > 0xFFFFu) {
        return core::fail(core::Error{
            core::StatusCode::OutOfRange, "wolgameres: too large"});
    }
    w.write_be<std::uint16_t>(static_cast<std::uint16_t>(total));
    w.write_be<std::uint16_t>(r.header.rngd_size);
    w.write_bytes(body.view());
    return core::ok();
}

core::Result<std::uint8_t> read_byte(const Entry& e) {
    if (e.data.size() < 1) {
        return core::fail(core::Error{
            core::StatusCode::OutOfRange, "wolgameres: byte entry empty"});
    }
    return static_cast<std::uint8_t>(e.data.front());
}

core::Result<std::uint32_t> read_int(const Entry& e) {
    return core::read_be<std::uint32_t>(
        core::ByteView{e.data.data(),
                       std::min<std::size_t>(4, e.data.size())});
}

core::Result<std::uint64_t> read_bigint(const Entry& e) {
    if (e.data.size() < 8) {
        return core::fail(core::Error{
            core::StatusCode::OutOfRange, "wolgameres: bigint too short"});
    }
    return core::read_be<std::uint64_t>(
        core::ByteView{e.data.data(), 8u});
}

std::string_view read_string(const Entry& e) {
    if (e.data.empty()) return {};
    const auto* begin = reinterpret_cast<const char*>(e.data.data());
    std::size_t n = e.data.size();
    // Strip any trailing NUL byte (gameres strings are NUL-terminated).
    while (n > 0 && static_cast<std::uint8_t>(begin[n - 1]) == 0u) --n;
    return std::string_view{begin, n};
}

}  // namespace pvpgn::protocol::wolgameres
