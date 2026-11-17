// SPDX-License-Identifier: GPL-3.0-or-later
// test-links: dt1
#include <dt1_cache.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

void put_i16(std::vector<std::byte>& bytes, std::size_t offset, std::int16_t value) {
    const auto raw = std::uint16_t(value);
    bytes[offset] = std::byte(raw & 0xff);
    bytes[offset + 1] = std::byte(raw >> 8);
}

void put_i32(std::vector<std::byte>& bytes, std::size_t offset, std::int32_t value) {
    const auto raw = std::uint32_t(value);
    for (std::size_t index = 0; index < 4; ++index) bytes[offset + index] = std::byte((raw >> (index * 8)) & 0xff);
}

std::vector<std::byte> one_tile(std::int32_t width = 1) {
    constexpr std::size_t header = 0x114, tile_header = header, block_header = tile_header + 96, pixels = block_header + 20;
    std::vector<std::byte> bytes(pixels + 3);
    put_i32(bytes, 0, 7);
    put_i32(bytes, 4, 6);
    put_i32(bytes, 0x10c, 1);
    put_i32(bytes, 0x110, int32_t(header));
    put_i32(bytes, tile_header + 8, 1);
    put_i32(bytes, tile_header + 12, width);
    bytes[tile_header + 40] = std::byte{1};
    put_i32(bytes, tile_header + 72, int32_t(block_header));
    put_i32(bytes, tile_header + 80, 1);
    put_i16(bytes, block_header + 8, 0);
    put_i32(bytes, block_header + 10, 3);
    put_i32(bytes, block_header + 16, 20);
    bytes[pixels] = std::byte{0};
    bytes[pixels + 1] = std::byte{1};
    bytes[pixels + 2] = std::byte{42};
    return bytes;
}

}  // namespace

int main() {
    using d2d::dt1::Archive;
    using d2d::dt1::ArchiveCache;
    using d2d::dt1::Pixels;
    const auto bytes = one_tile();

    Archive archive(bytes);
    assert(archive.resident() && archive.size() == 1);
    assert(archive.tiles()[0].pixels == std::vector<std::uint8_t>{42});
    assert(archive.release_pixels() >= 1 && !archive.resident() && archive.tiles()[0].pixels.empty());
    assert(archive.restore_pixels(bytes) && archive.resident());
    assert(archive.tiles()[0].pixels == std::vector<std::uint8_t>{42});

    const auto mismatched = one_tile(2);
    archive.release_pixels();
    assert(!archive.restore_pixels(mismatched) && !archive.resident() && archive.tiles()[0].pixels.empty());

    ArchiveCache cache;
    int reads = 0;
    auto loader = [&](const std::string&) -> std::optional<std::vector<std::byte>> { ++reads; return bytes; };
    auto first = cache.get("data\\global\\tiles\\act1\\blank.dt1", Pixels::decode, loader);
    auto second = cache.get("DATA\\GLOBAL\\TILES\\ACT1\\BLANK.DT1", Pixels::decode, loader);
    assert(first && first == second && reads == 1);

    auto level_a_archive = first;
    auto level_b_archive = second;
    std::weak_ptr<Archive> shared = first;
    first.reset();
    second.reset();
    level_a_archive.reset();
    assert(!shared.expired());
    level_b_archive.reset();
    assert(shared.expired());

    int missing_reads = 0;
    auto missing = cache.get("missing.dt1", Pixels::decode, [&](const std::string&) -> std::optional<std::vector<std::byte>> {
        ++missing_reads;
        return std::nullopt;
    });
    auto retried = cache.get("missing.dt1", Pixels::decode, [&](const std::string&) -> std::optional<std::vector<std::byte>> {
        ++missing_reads;
        return bytes;
    });
    assert(!missing && retried && missing_reads == 2);

    auto decoded = cache.get("mode.dt1", Pixels::decode, loader);
    auto skipped = cache.get("mode.dt1", Pixels::skip, loader);
    assert(decoded && decoded->resident() && !decoded->tiles()[0].pixels.empty());
    assert(skipped && skipped != decoded && !skipped->resident() && skipped->tiles()[0].pixels.empty());
    assert(!skipped->restore_pixels(bytes));
}