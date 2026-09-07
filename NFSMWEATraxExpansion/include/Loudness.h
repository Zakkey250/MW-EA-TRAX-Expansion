#pragma once
#include <algorithm>
#include <cmath>
#include <array>
#include <cstdint>

namespace eatrax {
// Constant gain preserves musical dynamics and subsequent game fades.
// Peak protection runs after the game volume, so quiet playback is not attenuated
// merely because the original file contains a high peak.
inline float LoudnessGainDb(float target, float integrated) {
    return std::clamp(target - integrated, -60.0f, 12.0f);
}
inline float ApplyLoudnessGain(float gameVolume, float gain) {
    if (!std::isfinite(gameVolume)) return 0.0f;
    return std::clamp(gameVolume, 0.0f, 2.0f) * gain;
}

// Stereo-linked, 5 ms lookahead sample-peak limiter. No allocation or locks in
// the audio callback. This is not a true-peak/oversampling limiter.
class LoudnessLimiter {
    std::array<float, 2048 * 2> delay_{};
    unsigned cursor_ = 0, delayFrames_ = 240, hold_ = 0;
    float gain_ = 1.0f, release_ = 0.999584f;
public:
    void Reset(unsigned sampleRate) {
        delay_.fill(0.0f);
        cursor_ = hold_ = 0;
        gain_ = 1.0f;
        delayFrames_ = std::clamp(sampleRate / 200, 1u, 2048u);
        release_ = std::exp(-1.0f / (0.05f * std::max(sampleRate, 1u)));
    }
    void Process(float* samples, std::uint64_t frames) {
        constexpr float ceiling = 0.891250938f; // -1 dBFS sample peak
        for (std::uint64_t i = 0; i < frames; ++i) {
            const float left = std::isfinite(samples[i * 2]) ? samples[i * 2] : 0.0f;
            const float right = std::isfinite(samples[i * 2 + 1]) ? samples[i * 2 + 1] : 0.0f;
            const float peak = std::max(std::abs(left), std::abs(right));
            const float needed = peak > ceiling ? ceiling / peak : 1.0f;
            if (needed <= gain_) { gain_ = needed; hold_ = delayFrames_; }
            else if (hold_ > 0) --hold_;
            else gain_ = 1.0f - (1.0f - gain_) * release_;
            samples[i * 2] = delay_[cursor_ * 2] * gain_;
            samples[i * 2 + 1] = delay_[cursor_ * 2 + 1] * gain_;
            delay_[cursor_ * 2] = left;
            delay_[cursor_ * 2 + 1] = right;
            cursor_ = (cursor_ + 1) % delayFrames_;
        }
    }
};
}
