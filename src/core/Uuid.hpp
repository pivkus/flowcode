#pragma once

#include <array>
#include <cstring>
#include <chrono>
#include <random>
#include <optional>
#include <charconv>

struct Uuid {
    // Represents the 128-bit number (Uuid), big endian, zero initialized
    std::array<uint8_t, 16> bytes{};

    constexpr Uuid() noexcept = default;

    static Uuid generate_v7(){
        Uuid u;

        // Get the timestamp
        using namespace std::chrono;
        uint64_t timestamp = duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()
        ).count();

        thread_local std::mt19937_64 generator{ std::random_device{}() };
        thread_local std::uniform_int_distribution<uint64_t> distribution{};

        // Generate the random bytes
        uint64_t hi = distribution(generator);
        uint64_t lo = distribution(generator);

        // fill the "bytes" array, most significant byte first
        u.fill_bytes(hi, 8, 0);
        u.fill_bytes(lo, 8, 8);

        // replace first 6 bytes with timestamp
        u.fill_bytes(timestamp, 6, 0);

        // set the version
        u.bytes[6] = (u.bytes[6] & 0x0f) | 0x70;

        // set the variant
        u.bytes[8] = (u.bytes[8] & 0x3F) | 0x80;

        return u;
    }

    static std::optional<Uuid> from_string(std::string str) {
        // 8-4-4-4-12
        // ex. 01a0155c-1838-714c-9957-2ae8bdee6429
        if (str.size() != 36) return std::nullopt;
        // Validate separators
        for (size_t i : {8, 13, 18, 23}) if (str[i] != '-') return std::nullopt;

        Uuid u;

        char* start = str.data();
        size_t shift = 0;
        for (const size_t off : {8, 4, 4, 4, 12}){
            uint64_t val{};
            if (auto [ptr, ec] = std::from_chars(start, start + off, val, 16);
                ec != std::errc{} || ptr != (start + off)) return std::nullopt;

            u.fill_bytes(val, off / 2, shift);
            start += off + 1;
            shift += off / 2;
        }
        
        return u;
    }

    std::string to_string() const;
    friend bool operator==(const Uuid&, const Uuid&) noexcept = default;

private:
    void fill_bytes(uint64_t val, size_t nbytes, size_t shift){
        for (size_t i = 0; i < nbytes; i++) bytes[shift + i] = (val >> (nbytes - 1 - i)*8) & 0xff;
    }
};

namespace std {
    // specialize the std::hash struct for Uuid type
    // this is used to support unorderd_map and QHash
    template<> struct hash<Uuid> {
        // std::hash<Uuid>(u, seed)
        size_t operator()(const Uuid& u, size_t seed = 0) const {
            uint64_t hi, lo;
            memcpy(&hi, u.bytes.data(), 8);
            memcpy(&lo, u.bytes.data() + 8, 8);
            return static_cast<size_t>(seed ^ (hi * 1099511628211ull) ^ lo);
        }
    };
}

namespace std{
    template<> struct formatter<Uuid> : formatter<string_view>{
        auto format(const Uuid& uuid, auto& ctx) const {
            return apply([&ctx](auto... bytes){
                return std::format_to(ctx.out(),
                    "{:02x}{:02x}{:02x}{:02x}-"
                    "{:02x}{:02x}-"
                    "{:02x}{:02x}-"
                    "{:02x}{:02x}-"
                    "{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
                bytes...);
            }, uuid.bytes);
        }
    };
}

inline std::string Uuid::to_string() const {
    return std::format("{}", *this);
}