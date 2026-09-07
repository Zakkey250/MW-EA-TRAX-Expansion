#include "VgmSource.h"

#include "Utilities.h"

extern "C" {
#include "libvgmstream.h"
}

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

namespace eatrax {
namespace {

libvgmstream_t* OpenVgm(const std::filesystem::path& path, const int subsong,
                        std::string* error) {
    const std::string ansiPath = WideToAnsi(path.wstring());
    if (ansiPath.empty()) {
        if (error) *error = "path cannot be represented in the Windows ANSI code page";
        return nullptr;
    }
    libstreamfile_t* streamFile = libstreamfile_open_from_stdio(ansiPath.c_str());
    if (streamFile == nullptr) {
        if (error) *error = "vgmstream could not open the MPF file";
        return nullptr;
    }

    libvgmstream_config_t config{};
    config.ignore_loop = true;
    config.force_sfmt = LIBVGMSTREAM_SFMT_PCM16;
    libvgmstream_t* decoder = libvgmstream_create(streamFile, subsong, &config);
    libstreamfile_close(streamFile);
    if (decoder == nullptr) {
        if (error) *error = "vgmstream rejected the requested subsong";
    }
    return decoder;
}

bool IsImportedFormat(const libvgmstream_format_t* format) {
    if (format == nullptr || format->codec_name == nullptr || format->channels != 2 ||
        format->sample_format != LIBVGMSTREAM_SFMT_PCM16) {
        return false;
    }
    const bool modifiedEaXa =
        format->sample_rate == 44100 && std::strstr(format->codec_name, "EA-XA") != nullptr;
    const bool stockEaLayer3 =
        format->sample_rate == 32000 && std::strstr(format->codec_name, "EALayer3") != nullptr;
    return modifiedEaXa || stockEaLayer3;
}

}  // namespace

VgmDecoder::VgmDecoder() = default;

VgmDecoder::~VgmDecoder() {
    if (handle_ != nullptr) {
        libvgmstream_free(static_cast<libvgmstream_t*>(handle_));
    }
}

bool VgmDecoder::Open(const std::filesystem::path& mpfPath, const int subsong,
                      std::string* error) {
    if (handle_ != nullptr) {
        libvgmstream_free(static_cast<libvgmstream_t*>(handle_));
        handle_ = nullptr;
    }
    libvgmstream_t* decoder = OpenVgm(mpfPath, subsong, error);
    if (decoder == nullptr) {
        return false;
    }
    const bool pursuitSps = (mpfPath.extension() == L".sps" || mpfPath.extension() == L".SPS") &&
        decoder->format != nullptr && decoder->format->channels == 2 &&
        decoder->format->sample_rate == 48000 &&
        decoder->format->sample_format == LIBVGMSTREAM_SFMT_PCM16 &&
        decoder->format->codec_name != nullptr &&
        std::strstr(decoder->format->codec_name, "EALayer3") != nullptr &&
        decoder->format->play_samples > 0;
    if (!pursuitSps && !IsImportedFormat(decoder->format)) {
        if (error) *error = "subsong is not a supported stereo MusicSFx stream";
        libvgmstream_free(decoder);
        return false;
    }
    handle_ = decoder;
    return true;
}

int VgmDecoder::ReadFrames(std::int16_t* destination, const int frameCount) {
    if (handle_ == nullptr || destination == nullptr || frameCount <= 0) {
        return 0;
    }
    libvgmstream_t* decoder = static_cast<libvgmstream_t*>(handle_);
    if (libvgmstream_fill(decoder, destination, frameCount) < 0) {
        return -1;
    }
    return decoder->decoder->buf_samples;
}

void VgmDecoder::Seek(const std::uint64_t frame) {
    if (handle_ == nullptr) return;
    const std::uint64_t clamped = std::min(frame, Length());
    auto* decoder = static_cast<libvgmstream_t*>(handle_);
    // This bundled API retains decode_done after seeking past an EOF. Reset
    // before rewinding so end-to-end pursuit loops can actually decode again.
    if (clamped == 0 || decoder->decoder->done) libvgmstream_reset(decoder);
    libvgmstream_seek(decoder, static_cast<std::int64_t>(clamped));
}

std::uint64_t VgmDecoder::Cursor() const {
    if (handle_ == nullptr) return 0;
    const std::int64_t position =
        libvgmstream_get_play_position(static_cast<libvgmstream_t*>(handle_));
    return position < 0 ? 0 : static_cast<std::uint64_t>(position);
}

std::uint64_t VgmDecoder::Length() const {
    if (handle_ == nullptr) return 0;
    const auto* format = static_cast<libvgmstream_t*>(handle_)->format;
    return format->play_samples < 0 ? 0 : static_cast<std::uint64_t>(format->play_samples);
}

int VgmDecoder::Channels() const {
    return handle_ == nullptr ? 0 : static_cast<libvgmstream_t*>(handle_)->format->channels;
}

int VgmDecoder::SampleRate() const {
    return handle_ == nullptr ? 0 : static_cast<libvgmstream_t*>(handle_)->format->sample_rate;
}

bool VgmDecoder::AtEnd() const {
    return handle_ == nullptr || static_cast<libvgmstream_t*>(handle_)->decoder->done;
}

MusicSfxInspection InspectMusicSfx(const std::filesystem::path& mpfPath) {
    MusicSfxInspection result;
    if (mpfPath.extension() != L".mpf" && mpfPath.extension() != L".MPF") {
        result.error = "input must be MusicSFx.mpf";
        return result;
    }
    std::filesystem::path musPath = mpfPath;
    musPath.replace_extension(L".mus");
    if (!std::filesystem::is_regular_file(mpfPath) || !std::filesystem::is_regular_file(musPath)) {
        result.error = "MusicSFx.mpf and MusicSFx.mus must be present as a pair";
        return result;
    }

    std::string error;
    // The licensed-song map deliberately starts at subsong 2. It exposes the archive-wide
    // count for both the stock EALayer3 pair and compatible EA-XA replacement pairs.
    libvgmstream_t* first = OpenVgm(mpfPath, 2, &error);
    if (first == nullptr) {
        result.error = error;
        return result;
    }
    result.subsongCount = first->format->subsong_count;
    libvgmstream_free(first);
    if (result.subsongCount != 43) {
        result.error = "unsupported MPF layout: expected exactly 43 subsongs";
        return result;
    }

    for (const int subsong : ExpectedMusicSfxSubsongs()) {
        libvgmstream_t* decoder = OpenVgm(mpfPath, subsong, &error);
        if (decoder == nullptr) {
            result.error = "failed to inspect subsong " + std::to_string(subsong) + ": " + error;
            return result;
        }
        if (!IsImportedFormat(decoder->format)) {
            result.error = "unsupported MPF stream map at subsong " + std::to_string(subsong) +
                           " (codec=" + decoder->format->codec_name +
                           ", channels=" + std::to_string(decoder->format->channels) +
                           ", rate=" + std::to_string(decoder->format->sample_rate) +
                           ", sample_format=" +
                           std::to_string(static_cast<int>(decoder->format->sample_format)) + ")";
            libvgmstream_free(decoder);
            return result;
        }
        MusicSfxStreamInfo info;
        info.subsong = subsong;
        info.channels = decoder->format->channels;
        info.sampleRate = decoder->format->sample_rate;
        info.sampleCount = decoder->format->play_samples;
        info.codec = decoder->format->codec_name;
        result.importedStreams.push_back(std::move(info));
        libvgmstream_free(decoder);
    }
    if (result.importedStreams.size() != 27) {
        result.error = "unsupported MPF layout: expected exactly 27 supported EA TRAX streams";
        return result;
    }

    result.supported = true;
    return result;
}

bool DecodeMusicSfxPrefix(const std::filesystem::path& mpfPath, const int subsong,
                          const std::uint32_t framesToDecode, std::uint64_t* checksum,
                          std::uint32_t* framesDecoded, std::string* error) {
    VgmDecoder decoder;
    if (!decoder.Open(mpfPath, subsong, error)) {
        return false;
    }
    std::vector<std::int16_t> samples(static_cast<std::size_t>(4096) * decoder.Channels());
    std::uint32_t decoded = 0;
    std::uint64_t hash = 14695981039346656037ull;
    while (decoded < framesToDecode) {
        const int request = static_cast<int>(
            std::min<std::uint32_t>(4096, static_cast<std::uint32_t>(framesToDecode - decoded)));
        const int count = decoder.ReadFrames(samples.data(), request);
        if (count < 0) {
            if (error) *error = "decode error";
            return false;
        }
        if (count == 0) break;
        const auto* bytes = reinterpret_cast<const unsigned char*>(samples.data());
        const std::size_t byteCount = static_cast<std::size_t>(count) * decoder.Channels() * 2;
        for (std::size_t index = 0; index < byteCount; ++index) {
            hash ^= bytes[index];
            hash *= 1099511628211ull;
        }
        decoded += static_cast<std::uint32_t>(count);
    }
    if (checksum) *checksum = hash;
    if (framesDecoded) *framesDecoded = decoded;
    return decoded > 0;
}

}  // namespace eatrax
