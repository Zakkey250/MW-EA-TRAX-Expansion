#include "AudioEngine.h"

#include "VgmSource.h"
#include "Loudness.h"

#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>

namespace eatrax {
namespace {

struct VgmDataSource {
    ma_data_source_base base{};
    std::unique_ptr<VgmDecoder> decoder;
};

ma_result VgmRead(ma_data_source* source, void* output, const ma_uint64 frameCount,
                  ma_uint64* framesRead) {
    auto* vgm = reinterpret_cast<VgmDataSource*>(source);
    ma_uint64 completed = 0;
    auto* destination = static_cast<std::int16_t*>(output);
    while (completed < frameCount) {
        const ma_uint64 remaining = frameCount - completed;
        const int request = static_cast<int>(std::min<ma_uint64>(remaining, 16384));
        const int decoded = vgm->decoder->ReadFrames(destination + completed * 2, request);
        if (decoded < 0) {
            if (framesRead) *framesRead = completed;
            return MA_ERROR;
        }
        if (decoded == 0) break;
        completed += static_cast<ma_uint64>(decoded);
        if (decoded < request) break;
    }
    if (framesRead) *framesRead = completed;
    return completed == 0 && frameCount != 0 ? MA_AT_END : MA_SUCCESS;
}

ma_result VgmSeek(ma_data_source* source, const ma_uint64 frame) {
    auto* vgm = reinterpret_cast<VgmDataSource*>(source);
    vgm->decoder->Seek(frame);
    return MA_SUCCESS;
}

ma_result VgmFormat(ma_data_source* source, ma_format* format, ma_uint32* channels,
                    ma_uint32* sampleRate, ma_channel* channelMap, const size_t channelMapCap) {
    auto* vgm = reinterpret_cast<VgmDataSource*>(source);
    if (format) *format = ma_format_s16;
    if (channels) *channels = static_cast<ma_uint32>(vgm->decoder->Channels());
    if (sampleRate) *sampleRate = static_cast<ma_uint32>(vgm->decoder->SampleRate());
    if (channelMap != nullptr && channelMapCap > 0) {
        ma_channel_map_init_standard(ma_standard_channel_map_default, channelMap, channelMapCap,
                                     static_cast<ma_uint32>(vgm->decoder->Channels()));
    }
    return MA_SUCCESS;
}

ma_result VgmCursor(ma_data_source* source, ma_uint64* cursor) {
    if (cursor == nullptr) return MA_INVALID_ARGS;
    *cursor = reinterpret_cast<VgmDataSource*>(source)->decoder->Cursor();
    return MA_SUCCESS;
}

ma_result VgmLength(ma_data_source* source, ma_uint64* length) {
    if (length == nullptr) return MA_INVALID_ARGS;
    *length = reinterpret_cast<VgmDataSource*>(source)->decoder->Length();
    return MA_SUCCESS;
}

ma_data_source_vtable g_vgmVtable = {VgmRead, VgmSeek, VgmFormat, VgmCursor,
                                     VgmLength, nullptr, 0};

bool InitializeVgmDataSource(VgmDataSource* source, const std::filesystem::path& path,
                             const int subsong, std::string* error) {
    ma_data_source_config config = ma_data_source_config_init();
    config.vtable = &g_vgmVtable;
    if (ma_data_source_init(&config, &source->base) != MA_SUCCESS) {
        if (error) *error = "miniaudio data-source initialization failed";
        return false;
    }
    source->decoder = std::make_unique<VgmDecoder>();
    if (!source->decoder->Open(path, subsong, error)) {
        source->decoder.reset();
        ma_data_source_uninit(&source->base);
        std::memset(&source->base, 0, sizeof(source->base));
        return false;
    }
    return true;
}

void UninitializeVgmDataSource(VgmDataSource* source) {
    if (source->base.vtable != nullptr) {
        ma_data_source_uninit(&source->base);
    }
    source->decoder.reset();
    std::memset(&source->base, 0, sizeof(source->base));
}

}  // namespace

struct AudioEngine::Impl {
    mutable std::mutex mutex;
    ma_engine engine{};
    ma_sound sound{};
    VgmDataSource vgm{};
    bool engineReady = false;
    bool soundReady = false;
    bool paused = false;
    float loudnessGain = 1.0f;
    std::atomic<unsigned> limiterGeneration{0};
    std::atomic<bool> limiterEnabled{false};
    unsigned renderedGeneration = ~0u;
    LoudnessLimiter limiter;

    static void Process(void* user, float* output, ma_uint64 frames) {
        auto* self = static_cast<Impl*>(user);
        const auto generation = self->limiterGeneration.load();
        if (generation != self->renderedGeneration) {
            self->limiter.Reset(ma_engine_get_sample_rate(&self->engine));
            self->renderedGeneration = generation;
        }
        if (self->limiterEnabled.load()) self->limiter.Process(output, frames);
    }

    void DestroySound() {
        if (soundReady) {
            ma_sound_stop(&sound);
            ma_sound_uninit(&sound);
            std::memset(&sound, 0, sizeof(sound));
            soundReady = false;
        }
        UninitializeVgmDataSource(&vgm);
        paused = false;
        loudnessGain = 1.0f;
        limiterEnabled.store(false);
        ++limiterGeneration;
    }
};

AudioEngine::AudioEngine() : impl_(std::make_unique<Impl>()) {}

AudioEngine::~AudioEngine() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->DestroySound();
    if (impl_->engineReady) {
        ma_engine_uninit(&impl_->engine);
        impl_->engineReady = false;
    }
}

bool AudioEngine::Initialize(std::string* error, const bool noDevice) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->engineReady) return true;
    ma_engine_config config = ma_engine_config_init();
    config.channels = 2;
    config.onProcess = Impl::Process;
    config.pProcessUserData = impl_.get();
    if (noDevice) {
        config.noDevice = MA_TRUE;
        config.sampleRate = 48000;
    }
    if (ma_engine_init(&config, &impl_->engine) != MA_SUCCESS) {
        if (error) *error = "miniaudio could not open the default playback device";
        return false;
    }
    impl_->engineReady = true;
    return true;
}

bool AudioEngine::Play(const Track& track, const float volume, std::string* error) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->engineReady) {
        if (error) *error = "audio engine is not initialized";
        return false;
    }
    impl_->DestroySound();
    ma_result result = MA_ERROR;
    // Offline renders have no real-time device pacing. Decode synchronously there
    // so the streaming worker cannot be outrun and insert artificial silence.
    const ma_uint32 flags = MA_SOUND_FLAG_NO_SPATIALIZATION |
        (impl_->engine.pDevice == nullptr ? MA_SOUND_FLAG_DECODE : MA_SOUND_FLAG_STREAM);
    if (track.sourceKind == SourceKind::External) {
        result = ma_sound_init_from_file_w(&impl_->engine, track.sourcePath.c_str(), flags, nullptr,
                                           nullptr, &impl_->sound);
    } else {
        if (!InitializeVgmDataSource(&impl_->vgm, track.sourcePath, track.subsong, error)) {
            return false;
        }
        result = ma_sound_init_from_data_source(
            &impl_->engine, reinterpret_cast<ma_data_source*>(&impl_->vgm),
            MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &impl_->sound);
    }
    if (result != MA_SUCCESS) {
        UninitializeVgmDataSource(&impl_->vgm);
        if (error && error->empty()) *error = "miniaudio rejected the selected source";
        return false;
    }
    impl_->soundReady = true;
    impl_->loudnessGain = track.loudnessMeasured && std::isfinite(track.loudnessGain) &&
        track.loudnessGain > 0.0f && track.loudnessGain <= 4.0f ? track.loudnessGain : 1.0f;
    ++impl_->limiterGeneration;
    impl_->limiterEnabled.store(track.loudnessMeasured);
    ma_sound_set_looping(&impl_->sound, track.loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(&impl_->sound, ApplyLoudnessGain(volume, impl_->loudnessGain));
    ma_sound_seek_to_pcm_frame(&impl_->sound, 0);
    if (ma_sound_start(&impl_->sound) != MA_SUCCESS) {
        impl_->DestroySound();
        if (error) *error = "miniaudio could not start playback";
        return false;
    }
    return true;
}

void AudioEngine::Stop() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->DestroySound();
}

void AudioEngine::Pause() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->soundReady && !impl_->paused) {
        ma_sound_stop(&impl_->sound);
        impl_->paused = true;
    }
}

void AudioEngine::Resume() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->soundReady && impl_->paused) {
        ma_sound_start(&impl_->sound);
        impl_->paused = false;
    }
}

void AudioEngine::SetVolume(const float volume) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->soundReady) {
        ma_sound_set_volume(&impl_->sound, ApplyLoudnessGain(volume, impl_->loudnessGain));
    }
}

float AudioEngine::OutputVolume() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->soundReady ? ma_sound_get_volume(&impl_->sound) : 0.0f;
}

bool AudioEngine::RenderFrames(float* output, const std::uint64_t frames) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->engineReady || impl_->engine.pDevice != nullptr) return false;
    ma_uint64 completed = 0;
    return ma_engine_read_pcm_frames(&impl_->engine, output, frames, &completed) == MA_SUCCESS &&
        completed == frames;
}

std::int32_t AudioEngine::RemainingMilliseconds() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->soundReady) return 0;
    float cursor = 0.0f;
    float length = 0.0f;
    if (ma_sound_get_cursor_in_seconds(&impl_->sound, &cursor) != MA_SUCCESS ||
        ma_sound_get_length_in_seconds(&impl_->sound, &length) != MA_SUCCESS) {
        return 1000;
    }
    const float remaining = std::max(0.0f, length - cursor);
    return static_cast<std::int32_t>(remaining * 1000.0f);
}

bool AudioEngine::IsActive() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->soundReady && ma_sound_at_end(&impl_->sound) == MA_FALSE;
}

bool AudioEngine::HasEnded() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->soundReady && ma_sound_at_end(&impl_->sound) == MA_TRUE;
}

bool AudioEngine::IsPaused() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->paused;
}

bool AudioEngine::ProbeExternalFile(const std::filesystem::path& path, std::string* error) {
    ma_decoder decoder{};
    const ma_decoder_config config = ma_decoder_config_init_default();
    if (ma_decoder_init_file_w(path.c_str(), &config, &decoder) != MA_SUCCESS) {
        if (error) *error = "miniaudio decoder rejected the file";
        return false;
    }
    ma_uint64 length = 0;
    const ma_result lengthResult = ma_decoder_get_length_in_pcm_frames(&decoder, &length);
    ma_decoder_uninit(&decoder);
    if (lengthResult != MA_SUCCESS || length == 0) {
        if (error) *error = "audio file has no decodable PCM frames";
        return false;
    }
    return true;
}

}  // namespace eatrax
