// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/comtrade/indexed_dat.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <optional>
#include <string_view>
#include <system_error>
#include <type_traits>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace ardirec::comtrade {
namespace {

constexpr std::size_t kBinaryHeaderBytes = sizeof(std::uint32_t) * 2u;
constexpr std::size_t kCancellationInterval = 4096u;
constexpr std::size_t kBaseLodBlockFrames = 256u;
constexpr std::size_t kMaximumLodCells = 2'000'000u;

std::string_view trim_view(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) {
        value.remove_suffix(1);
    }
    return value;
}

std::string_view field_view(std::string_view line, std::size_t wanted) {
    std::size_t field = 0;
    std::size_t begin = 0;
    for (std::size_t i = 0; i <= line.size(); ++i) {
        if (i == line.size() || line[i] == ',') {
            if (field == wanted) return trim_view(line.substr(begin, i - begin));
            ++field;
            begin = i + 1;
        }
    }
    return {};
}

void split_views(std::string_view line, std::vector<std::string_view>& fields) {
    fields.clear();
    std::size_t begin = 0;
    for (std::size_t i = 0; i <= line.size(); ++i) {
        if (i == line.size() || line[i] == ',') {
            fields.push_back(trim_view(line.substr(begin, i - begin)));
            begin = i + 1;
        }
    }
}

bool parse_u32(std::string_view text, std::uint32_t& value) {
    text = trim_view(text);
    if (text.empty()) return false;
    std::uint64_t wide = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), wide, 10);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()
        || wide > std::numeric_limits<std::uint32_t>::max()) return false;
    value = static_cast<std::uint32_t>(wide);
    return true;
}

bool parse_double(std::string_view text, double& value) {
    text = trim_view(text);
    if (text.empty()) return false;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value,
                                        std::chars_format::general);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()
           && std::isfinite(value);
}

bool parse_bool(std::string_view text, bool& value) {
    std::uint32_t parsed = 0;
    if (!parse_u32(text, parsed)) return false;
    value = parsed != 0;
    return true;
}

template <typename T>
T byte_swap(T value) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    std::array<unsigned char, sizeof(T)> source{};
    std::array<unsigned char, sizeof(T)> target{};
    std::memcpy(source.data(), &value, sizeof(T));
    for (std::size_t i = 0; i < sizeof(T); ++i) target[i] = source[sizeof(T) - 1u - i];
    std::memcpy(&value, target.data(), sizeof(T));
    return value;
}

template <typename T>
T read_le_bytes(const std::byte* bytes) noexcept {
    T value{};
    std::memcpy(&value, bytes, sizeof(T));
    if constexpr (std::endian::native == std::endian::big) value = byte_swap(value);
    return value;
}

bool missing_i16(std::int16_t value, int revision) noexcept {
    if (revision <= 1991) return static_cast<std::uint16_t>(value) == 0xFFFFu;
    return value == std::numeric_limits<std::int16_t>::min();
}

std::size_t sample_width(DataFormat format) noexcept {
    switch (format) {
    case DataFormat::Binary16: return sizeof(std::int16_t);
    case DataFormat::Binary32: return sizeof(std::int32_t);
    case DataFormat::Float32: return sizeof(float);
    default: return 0;
    }
}

bool is_binary_family(DataFormat format) noexcept {
    return format == DataFormat::Binary16 || format == DataFormat::Binary32
           || format == DataFormat::Float32;
}

std::size_t binary_frame_size(const RecordConfig& config) noexcept {
    const std::size_t width = sample_width(config.data_format);
    if (width == 0) return 0;
    const std::size_t statusWords = (config.status_channels.size() + 15u) / 16u;
    if (statusWords > (std::numeric_limits<std::size_t>::max() - kBinaryHeaderBytes) / 2u) return 0;
    const std::size_t statusBytes = statusWords * sizeof(std::uint16_t);
    if (config.analog_channels.size()
        > (std::numeric_limits<std::size_t>::max() - kBinaryHeaderBytes - statusBytes) / width) return 0;
    return kBinaryHeaderBytes + config.analog_channels.size() * width + statusBytes;
}

double decode_raw_analog(DataFormat format,
                         int revision,
                         const AnalogChannel& definition,
                         const std::byte* bytes) noexcept {
    if (!bytes) return std::numeric_limits<double>::quiet_NaN();
    double raw = std::numeric_limits<double>::quiet_NaN();
    bool missing = false;
    if (format == DataFormat::Binary16) {
        const auto value = read_le_bytes<std::int16_t>(bytes);
        missing = missing_i16(value, revision);
        raw = static_cast<double>(value);
    } else if (format == DataFormat::Binary32) {
        const auto value = read_le_bytes<std::int32_t>(bytes);
        missing = value == std::numeric_limits<std::int32_t>::min();
        raw = static_cast<double>(value);
    } else if (format == DataFormat::Float32) {
        const auto value = read_le_bytes<float>(bytes);
        raw = static_cast<double>(value);
        missing = !std::isfinite(raw) || value == std::numeric_limits<float>::lowest();
    }
    if (missing || !std::isfinite(raw)) return std::numeric_limits<double>::quiet_NaN();
    return definition.a * raw + definition.b;
}

double decode_binary_analog(const RecordConfig& config,
                            const std::byte* frame,
                            std::size_t channel) noexcept {
    if (!frame || channel >= config.analog_channels.size()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const std::size_t width = sample_width(config.data_format);
    return decode_raw_analog(config.data_format, config.revision_year,
                             config.analog_channels[channel],
                             frame + kBinaryHeaderBytes + channel * width);
}

bool decode_binary_status(const RecordConfig& config,
                          const std::byte* frame,
                          std::size_t channel) noexcept {
    if (!frame || channel >= config.status_channels.size()) return false;
    const std::size_t width = sample_width(config.data_format);
    const std::size_t statusBase = kBinaryHeaderBytes + config.analog_channels.size() * width;
    const std::size_t word = channel / 16u;
    const std::size_t bit = channel % 16u;
    const auto packed = read_le_bytes<std::uint16_t>(frame + statusBase + word * sizeof(std::uint16_t));
    return (packed & static_cast<std::uint16_t>(1u << bit)) != 0u;
}

float lod_value(double value) noexcept {
    constexpr double maximum = static_cast<double>(std::numeric_limits<float>::max());
    if (value > maximum) return std::numeric_limits<float>::max();
    if (value < -maximum) return -std::numeric_limits<float>::max();
    return static_cast<float>(value);
}

std::shared_ptr<AnalogLodIndex> make_lod_index(std::size_t frames,
                                               std::size_t channels) {
    if (frames == 0 || channels == 0) return {};
    std::size_t blockSize = std::min(kBaseLodBlockFrames, frames);
    if (blockSize == 0) blockSize = 1;

    std::size_t blockCount = (frames - 1u) / blockSize + 1u;
    while (blockCount > kMaximumLodCells / channels) {
        if (blockSize >= frames) break;
        if (blockSize > std::numeric_limits<std::size_t>::max() / 2u) {
            blockSize = frames;
        } else {
            blockSize = std::min(frames, blockSize * 2u);
        }
        blockCount = (frames - 1u) / blockSize + 1u;
    }
    if (blockCount > std::numeric_limits<std::size_t>::max() / channels) return {};
    const std::size_t cells = blockCount * channels;
    if (cells > kMaximumLodCells) return {};

    auto lod = std::make_shared<AnalogLodIndex>();
    lod->block_size = blockSize;
    lod->block_count = blockCount;
    lod->channel_count = channels;
    lod->minima.assign(cells, std::numeric_limits<float>::infinity());
    lod->maxima.assign(cells, -std::numeric_limits<float>::infinity());
    return lod;
}

void update_lod(AnalogLodIndex* lod,
                std::size_t frame,
                std::size_t channel,
                double value) noexcept {
    if (!lod || lod->block_size == 0 || channel >= lod->channel_count || !std::isfinite(value)) return;
    const std::size_t block = frame / lod->block_size;
    if (block >= lod->block_count) return;
    const std::size_t cell = block * lod->channel_count + channel;
    if (cell >= lod->minima.size() || cell >= lod->maxima.size()) return;
    const float visual = lod_value(value);
    lod->minima[cell] = std::min(lod->minima[cell], visual);
    lod->maxima[cell] = std::max(lod->maxima[cell], visual);
}

class ReadOnlyMapping final {
public:
    ReadOnlyMapping() = default;
    ~ReadOnlyMapping() { close(); }
    ReadOnlyMapping(const ReadOnlyMapping&) = delete;
    ReadOnlyMapping& operator=(const ReadOnlyMapping&) = delete;

    bool open(const std::filesystem::path& path, std::string& reason) {
        close();
#ifdef _WIN32
        file_ = CreateFileW(path.c_str(), GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file_ == INVALID_HANDLE_VALUE) {
            reason = "CreateFileW failed (" + std::to_string(GetLastError()) + ")";
            return false;
        }
        LARGE_INTEGER fileSize{};
        if (!GetFileSizeEx(file_, &fileSize) || fileSize.QuadPart < 0) {
            reason = "GetFileSizeEx failed (" + std::to_string(GetLastError()) + ")";
            close();
            return false;
        }
        size_ = static_cast<std::uint64_t>(fileSize.QuadPart);
        if (size_ == 0) return true;
        mapping_ = CreateFileMappingW(file_, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!mapping_) {
            reason = "CreateFileMappingW failed (" + std::to_string(GetLastError()) + ")";
            close();
            return false;
        }
        data_ = static_cast<const std::byte*>(MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0));
        if (!data_) {
            reason = "MapViewOfFile failed (" + std::to_string(GetLastError()) + ")";
            close();
            return false;
        }
        return true;
#else
        fd_ = ::open(path.c_str(), O_RDONLY);
        if (fd_ < 0) {
            reason = "open() failed";
            return false;
        }
        struct stat info {};
        if (::fstat(fd_, &info) != 0 || info.st_size < 0) {
            reason = "fstat() failed";
            close();
            return false;
        }
        size_ = static_cast<std::uint64_t>(info.st_size);
        if (size_ == 0) return true;
        void* mapped = ::mmap(nullptr, static_cast<std::size_t>(size_), PROT_READ, MAP_PRIVATE, fd_, 0);
        if (mapped == MAP_FAILED) {
            reason = "mmap() failed";
            data_ = nullptr;
            close();
            return false;
        }
        data_ = static_cast<const std::byte*>(mapped);
        return true;
#endif
    }

    void close() noexcept {
#ifdef _WIN32
        if (data_) UnmapViewOfFile(data_);
        data_ = nullptr;
        if (mapping_) CloseHandle(mapping_);
        mapping_ = nullptr;
        if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_);
        file_ = INVALID_HANDLE_VALUE;
#else
        if (data_ && size_ > 0) ::munmap(const_cast<std::byte*>(data_), static_cast<std::size_t>(size_));
        data_ = nullptr;
        if (fd_ >= 0) ::close(fd_);
        fd_ = -1;
#endif
        size_ = 0;
    }

    [[nodiscard]] const std::byte* data() const noexcept { return data_; }

private:
    const std::byte* data_{nullptr};
    std::uint64_t size_{0};
#ifdef _WIN32
    HANDLE file_{INVALID_HANDLE_VALUE};
    HANDLE mapping_{nullptr};
#else
    int fd_{-1};
#endif
};

} // namespace

bool AnalogLodIndex::blockExtrema(std::size_t channel,
                                  std::size_t block,
                                  double& minimum,
                                  double& maximum) const noexcept {
    if (channel >= channel_count || block >= block_count || channel_count == 0) return false;
    if (block > (std::numeric_limits<std::size_t>::max() - channel) / channel_count) return false;
    const std::size_t cell = block * channel_count + channel;
    if (cell >= minima.size() || cell >= maxima.size()) return false;
    const float low = minima[cell];
    const float high = maxima[cell];
    if (!std::isfinite(low) || !std::isfinite(high) || low > high) return false;
    minimum = static_cast<double>(low);
    maximum = static_cast<double>(high);
    return true;
}

struct IndexedDatFile::Impl {
    RecordConfig config;
    std::filesystem::path dat_path;
    ReadOnlyMapping mapping;
    std::uint64_t file_size{0};
    std::size_t frame_size{0};
    std::size_t frame_count{0};
    std::vector<std::uint64_t> ascii_offsets;
    mutable std::mutex fallback_mutex;
    mutable std::ifstream fallback_stream;

    [[nodiscard]] bool binaryFamily() const noexcept { return is_binary_family(config.data_format); }

    bool readBytes(std::uint64_t offset, void* destination, std::size_t count) const noexcept {
        if (!destination || count == 0) return count == 0;
        if (offset > file_size || count > file_size - offset) return false;
        if (mapping.data()) {
            std::memcpy(destination, mapping.data() + static_cast<std::size_t>(offset), count);
            return true;
        }
        std::lock_guard lock(fallback_mutex);
        if (!fallback_stream.is_open()) fallback_stream.open(dat_path, std::ios::binary);
        if (!fallback_stream) return false;
        fallback_stream.clear();
        fallback_stream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        fallback_stream.read(static_cast<char*>(destination), static_cast<std::streamsize>(count));
        return fallback_stream.gcount() == static_cast<std::streamsize>(count);
    }

    bool asciiLine(std::size_t frame, std::string& scratch, std::string_view& line) const noexcept {
        if (frame >= ascii_offsets.size()) return false;
        const std::uint64_t offset = ascii_offsets[frame];
        if (mapping.data()) {
            const auto* begin = reinterpret_cast<const char*>(mapping.data() + static_cast<std::size_t>(offset));
            const auto remaining = static_cast<std::size_t>(file_size - offset);
            const void* newline = std::memchr(begin, '\n', remaining);
            const auto length = newline ? static_cast<std::size_t>(static_cast<const char*>(newline) - begin)
                                        : remaining;
            line = std::string_view(begin, length);
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            return true;
        }
        std::lock_guard lock(fallback_mutex);
        if (!fallback_stream.is_open()) fallback_stream.open(dat_path, std::ios::binary);
        if (!fallback_stream) return false;
        fallback_stream.clear();
        fallback_stream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        if (!std::getline(fallback_stream, scratch)) return false;
        if (!scratch.empty() && scratch.back() == '\r') scratch.pop_back();
        line = scratch;
        return true;
    }
};

IndexedDatFile::IndexedDatFile(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}
IndexedDatFile::~IndexedDatFile() = default;
IndexedDatFile::IndexedDatFile(IndexedDatFile&&) noexcept = default;
IndexedDatFile& IndexedDatFile::operator=(IndexedDatFile&&) noexcept = default;

IndexedDatFile::OpenResult IndexedDatFile::open(const RecordConfig& config,
                                                const std::filesystem::path& dat_path) {
    OpenResult result;
    auto impl = std::make_unique<Impl>();
    impl->config = config;
    impl->dat_path = dat_path;

    std::error_code ec;
    const auto diskSize = std::filesystem::file_size(dat_path, ec);
    if (ec) {
        result.diagnostics.emplace_back("Cannot stat DAT file: " + ec.message());
        return result;
    }
    impl->file_size = static_cast<std::uint64_t>(diskSize);

    std::string mapReason;
    if (!impl->mapping.open(dat_path, mapReason)) {
        result.diagnostics.emplace_back("DAT memory mapping unavailable; using bounded streaming fallback: " + mapReason);
        impl->fallback_stream.open(dat_path, std::ios::binary);
        if (!impl->fallback_stream) {
            result.diagnostics.emplace_back("Cannot open DAT streaming fallback.");
            return result;
        }
    } else if (impl->mapping.data()) {
        result.diagnostics.emplace_back("DAT access: read-only memory map.");
    }

    if (config.data_format == DataFormat::Ascii) {
        auto acceptLine = [&](std::uint64_t offset, std::string_view line) {
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            line = trim_view(line);
            if (line.empty()) return false;
            std::uint32_t sample = 0;
            std::uint32_t timestamp = 0;
            if (!parse_u32(field_view(line, 0), sample) || !parse_u32(field_view(line, 1), timestamp)) return false;
            impl->ascii_offsets.push_back(offset);
            return true;
        };

        std::size_t rejectedRows = 0;
        if (impl->mapping.data()) {
            const char* bytes = reinterpret_cast<const char*>(impl->mapping.data());
            std::uint64_t begin = 0;
            for (std::uint64_t i = 0; i <= impl->file_size; ++i) {
                if (i == impl->file_size || bytes[i] == '\n') {
                    std::string_view line(bytes + static_cast<std::size_t>(begin),
                                          static_cast<std::size_t>(i - begin));
                    if (!trim_view(line).empty() && !acceptLine(begin, line)) ++rejectedRows;
                    begin = i + 1u;
                }
            }
        } else {
            std::ifstream stream(dat_path, std::ios::binary);
            std::string line;
            while (stream) {
                const auto position = stream.tellg();
                if (position < 0 || !std::getline(stream, line)) break;
                if (!trim_view(line).empty() && !acceptLine(static_cast<std::uint64_t>(position), line)) ++rejectedRows;
            }
        }
        impl->frame_count = impl->ascii_offsets.size();
        if (rejectedRows > 0) {
            result.diagnostics.emplace_back("ASCII DAT: skipped " + std::to_string(rejectedRows)
                                            + " row(s) with invalid sample/timestamp fields.");
        }
    } else if (is_binary_family(config.data_format)) {
        impl->frame_size = binary_frame_size(config);
        if (impl->frame_size == 0) {
            result.diagnostics.emplace_back("Invalid binary DAT frame layout.");
            return result;
        }
        const std::uint64_t completeFrames = impl->file_size / impl->frame_size;
        if (completeFrames > std::numeric_limits<std::size_t>::max()) {
            result.diagnostics.emplace_back("DAT contains more frames than this process can index.");
            return result;
        }
        impl->frame_count = static_cast<std::size_t>(completeFrames);
        const std::uint64_t remainder = impl->file_size % impl->frame_size;
        if (remainder != 0) {
            result.diagnostics.emplace_back("Binary DAT truncated tail: ignored " + std::to_string(remainder)
                                            + " byte(s) after " + std::to_string(impl->frame_count)
                                            + " complete frame(s).");
        }
    } else {
        result.diagnostics.emplace_back("Unsupported/unknown DAT format.");
        return result;
    }

    if (impl->frame_count == 0) {
        result.diagnostics.emplace_back("DAT contains no valid complete frames.");
        return result;
    }

    result.file = std::shared_ptr<IndexedDatFile>(new IndexedDatFile(std::move(impl)));
    return result;
}

std::size_t IndexedDatFile::frameCount() const noexcept { return m_impl ? m_impl->frame_count : 0; }
std::size_t IndexedDatFile::analogCount() const noexcept { return m_impl ? m_impl->config.analog_channels.size() : 0; }
std::size_t IndexedDatFile::statusCount() const noexcept { return m_impl ? m_impl->config.status_channels.size() : 0; }
bool IndexedDatFile::memoryMapped() const noexcept { return m_impl && m_impl->mapping.data() != nullptr; }
bool IndexedDatFile::binaryFamily() const noexcept { return m_impl && m_impl->binaryFamily(); }
DataFormat IndexedDatFile::dataFormat() const noexcept { return m_impl ? m_impl->config.data_format : DataFormat::Unknown; }
const std::filesystem::path& IndexedDatFile::path() const noexcept {
    static const std::filesystem::path empty;
    return m_impl ? m_impl->dat_path : empty;
}

std::uint32_t IndexedDatFile::sampleNumber(std::size_t frame) const noexcept {
    if (!m_impl || frame >= m_impl->frame_count) return 0;
    if (m_impl->binaryFamily()) {
        if (m_impl->mapping.data()) return read_le_bytes<std::uint32_t>(m_impl->mapping.data() + frame * m_impl->frame_size);
        std::array<std::byte, sizeof(std::uint32_t)> bytes{};
        if (!m_impl->readBytes(static_cast<std::uint64_t>(frame) * m_impl->frame_size, bytes.data(), bytes.size())) return 0;
        return read_le_bytes<std::uint32_t>(bytes.data());
    }
    std::string scratch;
    std::string_view line;
    std::uint32_t value = 0;
    return m_impl->asciiLine(frame, scratch, line) && parse_u32(field_view(line, 0), value) ? value : 0;
}

std::uint32_t IndexedDatFile::rawTimestamp(std::size_t frame) const noexcept {
    if (!m_impl || frame >= m_impl->frame_count) return 0;
    if (m_impl->binaryFamily()) {
        const std::uint64_t offset = static_cast<std::uint64_t>(frame) * m_impl->frame_size + sizeof(std::uint32_t);
        if (m_impl->mapping.data()) return read_le_bytes<std::uint32_t>(m_impl->mapping.data() + static_cast<std::size_t>(offset));
        std::array<std::byte, sizeof(std::uint32_t)> bytes{};
        if (!m_impl->readBytes(offset, bytes.data(), bytes.size())) return 0;
        return read_le_bytes<std::uint32_t>(bytes.data());
    }
    std::string scratch;
    std::string_view line;
    std::uint32_t value = 0;
    return m_impl->asciiLine(frame, scratch, line) && parse_u32(field_view(line, 1), value) ? value : 0;
}

double IndexedDatFile::analogValue(std::size_t frame, std::size_t channel) const noexcept {
    if (!m_impl || frame >= m_impl->frame_count || channel >= m_impl->config.analog_channels.size()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (m_impl->binaryFamily()) {
        const std::size_t width = sample_width(m_impl->config.data_format);
        const std::uint64_t offset = static_cast<std::uint64_t>(frame) * m_impl->frame_size
                                     + kBinaryHeaderBytes + channel * width;
        if (m_impl->mapping.data()) {
            return decode_raw_analog(m_impl->config.data_format, m_impl->config.revision_year,
                                     m_impl->config.analog_channels[channel],
                                     m_impl->mapping.data() + static_cast<std::size_t>(offset));
        }
        std::array<std::byte, sizeof(std::int32_t)> bytes{};
        if (!m_impl->readBytes(offset, bytes.data(), width)) return std::numeric_limits<double>::quiet_NaN();
        return decode_raw_analog(m_impl->config.data_format, m_impl->config.revision_year,
                                 m_impl->config.analog_channels[channel], bytes.data());
    }

    std::string scratch;
    std::string_view line;
    if (!m_impl->asciiLine(frame, scratch, line)) return std::numeric_limits<double>::quiet_NaN();
    double raw = 0.0;
    if (!parse_double(field_view(line, 2u + channel), raw)) return std::numeric_limits<double>::quiet_NaN();
    const auto& definition = m_impl->config.analog_channels[channel];
    return definition.a * raw + definition.b;
}

std::optional<bool> IndexedDatFile::statusState(std::size_t frame, std::size_t channel) const noexcept {
    if (!m_impl || frame >= m_impl->frame_count || channel >= m_impl->config.status_channels.size()) {
        return std::nullopt;
    }
    if (m_impl->binaryFamily()) {
        if (m_impl->mapping.data()) {
            return decode_binary_status(m_impl->config,
                                        m_impl->mapping.data() + frame * m_impl->frame_size,
                                        channel);
        }
        const std::size_t width = sample_width(m_impl->config.data_format);
        const std::size_t statusBase = kBinaryHeaderBytes + m_impl->config.analog_channels.size() * width;
        const std::size_t word = channel / 16u;
        const std::uint64_t offset = static_cast<std::uint64_t>(frame) * m_impl->frame_size
                                     + statusBase + word * sizeof(std::uint16_t);
        std::array<std::byte, sizeof(std::uint16_t)> bytes{};
        if (!m_impl->readBytes(offset, bytes.data(), bytes.size())) return std::nullopt;
        const auto packed = read_le_bytes<std::uint16_t>(bytes.data());
        return (packed & static_cast<std::uint16_t>(1u << (channel % 16u))) != 0u;
    }

    std::string scratch;
    std::string_view line;
    if (!m_impl->asciiLine(frame, scratch, line)) return std::nullopt;
    bool value = false;
    const std::size_t field = 2u + m_impl->config.analog_channels.size() + channel;
    if (!parse_bool(field_view(line, field), value)) return std::nullopt;
    return value;
}

bool IndexedDatFile::statusValue(std::size_t frame, std::size_t channel) const noexcept {
    if (!m_impl || channel >= m_impl->config.status_channels.size()) return false;
    const auto state = statusState(frame, channel);
    return state.value_or(m_impl->config.status_channels[channel].normal_state != 0);
}

void IndexedDatFile::copyAnalogRange(std::size_t channel,
                                     std::size_t first,
                                     std::size_t end,
                                     std::vector<double>& destination) const {
    destination.clear();
    if (!m_impl || channel >= m_impl->config.analog_channels.size() || first >= m_impl->frame_count) return;
    end = std::min(end, m_impl->frame_count);
    if (end <= first) return;
    destination.reserve(end - first);

    if (m_impl->binaryFamily() && !m_impl->mapping.data()) {
        std::ifstream stream(m_impl->dat_path, std::ios::binary);
        if (!stream) return;
        std::vector<std::byte> frameBuffer(m_impl->frame_size);
        stream.seekg(static_cast<std::streamoff>(first * m_impl->frame_size), std::ios::beg);
        for (std::size_t index = first; index < end; ++index) {
            stream.read(reinterpret_cast<char*>(frameBuffer.data()), static_cast<std::streamsize>(frameBuffer.size()));
            if (stream.gcount() != static_cast<std::streamsize>(frameBuffer.size())) break;
            destination.push_back(decode_binary_analog(m_impl->config, frameBuffer.data(), channel));
        }
        return;
    }

    for (std::size_t index = first; index < end; ++index) destination.push_back(analogValue(index, channel));
}

DatIndexSummary IndexedDatFile::buildIndex(double timestamp_scale_seconds,
                                           const std::atomic_bool* cancel) const {
    DatIndexSummary summary;
    if (!m_impl || m_impl->frame_count == 0 || !std::isfinite(timestamp_scale_seconds)
        || timestamp_scale_seconds <= 0.0) {
        summary.diagnostics.emplace_back("DAT index could not be built because the timestamp scale is invalid.");
        return summary;
    }

    summary.time_seconds.reserve(m_impl->frame_count);
    summary.analog_abs_peaks.assign(m_impl->config.analog_channels.size(), 0.0);
    summary.status_active.assign(m_impl->config.status_channels.size(), std::uint8_t{0});
    auto lod = make_lod_index(m_impl->frame_count, m_impl->config.analog_channels.size());
    if (!lod && !m_impl->config.analog_channels.empty()) {
        summary.diagnostics.emplace_back("Analog visual LOD cache could not be allocated within its bounded safety budget.");
    }

    std::vector<std::uint8_t> previousStatus(m_impl->config.status_channels.size(), std::uint8_t{0});
    std::vector<std::uint8_t> havePreviousStatus(m_impl->config.status_channels.size(), std::uint8_t{0});
    std::size_t invalidAnalogFields = 0;
    std::size_t invalidStatusFields = 0;
    std::size_t missingBinaryAnalogValues = 0;

    std::vector<std::byte> frameBuffer;
    std::ifstream binaryFallback;
    if (m_impl->binaryFamily() && !m_impl->mapping.data()) {
        binaryFallback.open(m_impl->dat_path, std::ios::binary);
        if (!binaryFallback) {
            summary.diagnostics.emplace_back("DAT streaming fallback could not be opened while building the index.");
            return summary;
        }
        frameBuffer.resize(m_impl->frame_size);
    }

    std::ifstream asciiFallback;
    std::string asciiScratch;
    std::vector<std::string_view> fields;
    fields.reserve(2u + m_impl->config.analog_channels.size() + m_impl->config.status_channels.size());
    if (m_impl->config.data_format == DataFormat::Ascii && !m_impl->mapping.data()) {
        asciiFallback.open(m_impl->dat_path, std::ios::binary);
        if (!asciiFallback) {
            summary.diagnostics.emplace_back("ASCII DAT streaming fallback could not be opened while building the index.");
            return summary;
        }
    }

    for (std::size_t frameIndex = 0; frameIndex < m_impl->frame_count; ++frameIndex) {
        if (cancel && frameIndex % kCancellationInterval == 0u && cancel->load(std::memory_order_relaxed)) {
            summary.cancelled = true;
            return summary;
        }

        std::uint32_t timestamp = 0;
        bool anyDigitalEdge = false;

        if (m_impl->binaryFamily()) {
            const std::byte* frame = nullptr;
            if (m_impl->mapping.data()) {
                frame = m_impl->mapping.data() + frameIndex * m_impl->frame_size;
            } else {
                binaryFallback.read(reinterpret_cast<char*>(frameBuffer.data()),
                                    static_cast<std::streamsize>(frameBuffer.size()));
                if (binaryFallback.gcount() != static_cast<std::streamsize>(frameBuffer.size())) {
                    summary.diagnostics.emplace_back("DAT streaming fallback ended before the indexed complete-frame count.");
                    break;
                }
                frame = frameBuffer.data();
            }

            timestamp = read_le_bytes<std::uint32_t>(frame + sizeof(std::uint32_t));
            for (std::size_t channel = 0; channel < m_impl->config.analog_channels.size(); ++channel) {
                const double value = decode_binary_analog(m_impl->config, frame, channel);
                if (std::isfinite(value)) {
                    summary.analog_abs_peaks[channel] = std::max(summary.analog_abs_peaks[channel], std::abs(value));
                    update_lod(lod.get(), frameIndex, channel, value);
                } else {
                    ++missingBinaryAnalogValues;
                }
            }
            for (std::size_t channel = 0; channel < m_impl->config.status_channels.size(); ++channel) {
                const auto state = static_cast<std::uint8_t>(decode_binary_status(m_impl->config, frame, channel) ? 1 : 0);
                if (state != 0) summary.status_active[channel] = 1;
                if (havePreviousStatus[channel] != 0 && state != previousStatus[channel]) anyDigitalEdge = true;
                previousStatus[channel] = state;
                havePreviousStatus[channel] = 1;
            }
        } else {
            std::string_view line;
            if (m_impl->mapping.data()) {
                const std::uint64_t offset = m_impl->ascii_offsets[frameIndex];
                const char* begin = reinterpret_cast<const char*>(m_impl->mapping.data() + static_cast<std::size_t>(offset));
                const auto remaining = static_cast<std::size_t>(m_impl->file_size - offset);
                const void* newline = std::memchr(begin, '\n', remaining);
                const auto length = newline ? static_cast<std::size_t>(static_cast<const char*>(newline) - begin) : remaining;
                line = std::string_view(begin, length);
                if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            } else {
                asciiFallback.clear();
                asciiFallback.seekg(static_cast<std::streamoff>(m_impl->ascii_offsets[frameIndex]), std::ios::beg);
                if (!std::getline(asciiFallback, asciiScratch)) {
                    summary.diagnostics.emplace_back("ASCII DAT changed or became unreadable while building the index.");
                    break;
                }
                if (!asciiScratch.empty() && asciiScratch.back() == '\r') asciiScratch.pop_back();
                line = asciiScratch;
            }

            split_views(line, fields);
            if (fields.size() < 2 || !parse_u32(fields[1], timestamp)) {
                // This should be unreachable because open() indexed only rows
                // with valid sample/timestamp fields. Treat mutation as damage.
                summary.diagnostics.emplace_back("ASCII DAT row lost a valid timestamp after indexing; valid prefix retained.");
                break;
            }
            for (std::size_t channel = 0; channel < m_impl->config.analog_channels.size(); ++channel) {
                const std::size_t field = 2u + channel;
                double raw = 0.0;
                if (field >= fields.size() || !parse_double(fields[field], raw)) {
                    ++invalidAnalogFields;
                    continue;
                }
                const auto& definition = m_impl->config.analog_channels[channel];
                const double value = definition.a * raw + definition.b;
                if (std::isfinite(value)) {
                    summary.analog_abs_peaks[channel] = std::max(summary.analog_abs_peaks[channel], std::abs(value));
                    update_lod(lod.get(), frameIndex, channel, value);
                } else {
                    ++invalidAnalogFields;
                }
            }
            const std::size_t statusBase = 2u + m_impl->config.analog_channels.size();
            for (std::size_t channel = 0; channel < m_impl->config.status_channels.size(); ++channel) {
                bool stateBool = false;
                const std::size_t field = statusBase + channel;
                if (field >= fields.size() || !parse_bool(fields[field], stateBool)) {
                    // Unknown is deliberately not coerced to 0: doing so could
                    // synthesize a transition that never existed in the record.
                    ++invalidStatusFields;
                    continue;
                }
                const auto state = static_cast<std::uint8_t>(stateBool ? 1 : 0);
                if (state != 0) summary.status_active[channel] = 1;
                if (havePreviousStatus[channel] != 0 && state != previousStatus[channel]) anyDigitalEdge = true;
                previousStatus[channel] = state;
                havePreviousStatus[channel] = 1;
            }
        }

        const double time = static_cast<double>(timestamp) * timestamp_scale_seconds;
        summary.time_seconds.push_back(time);
        if (anyDigitalEdge) summary.digital_edge_times.push_back(time);
    }

    if (invalidAnalogFields > 0) {
        summary.diagnostics.emplace_back("ASCII DAT: " + std::to_string(invalidAnalogFields)
                                         + " invalid/missing analog field(s) mapped to NaN.");
    }
    if (invalidStatusFields > 0) {
        summary.diagnostics.emplace_back("ASCII DAT: " + std::to_string(invalidStatusFields)
                                         + " invalid/missing digital field(s) treated as unknown; no synthetic edges created.");
    }
    if (missingBinaryAnalogValues > 0) {
        summary.diagnostics.emplace_back("Binary DAT: " + std::to_string(missingBinaryAnalogValues)
                                         + " missing/non-finite analog value(s) retained as NaN.");
    }
    if (cancel && cancel->load(std::memory_order_relaxed)) {
        summary.cancelled = true;
        return summary;
    }
    summary.analog_lod = std::move(lod);
    return summary;
}

} // namespace ardirec::comtrade
