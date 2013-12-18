// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_switches.h"

namespace switches {

// Allow users to specify a custom buffer size for debugging purpose.
const char kAudioBufferSize[] = "audio-buffer-size";

#if defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_SOLARIS)
// The Alsa device to use when opening an audio stream.
const char kAlsaOutputDevice[] = "alsa-output-device";
// The Alsa device to use when opening an audio input stream.
const char kAlsaInputDevice[] = "alsa-input-device";
#endif

#if defined(USE_CRAS)
// Use CRAS, the ChromeOS audio server.
const char kUseCras[] = "use-cras";
#endif

#if defined(OS_WIN)
// Use exclusive mode audio streaming for Windows Vista and higher.
// Leads to lower latencies for audio streams which uses the
// AudioParameters::AUDIO_PCM_LOW_LATENCY audio path.
// See http://msdn.microsoft.com/en-us/library/windows/desktop/dd370844(v=vs.85).aspx
// for details.
const char kEnableExclusiveAudio[] = "enable-exclusive-audio";

// Use Windows WaveOut/In audio API even if Core Audio is supported.
const char kForceWaveAudio[] = "force-wave-audio";
#endif

// Disable automatic fallback from low latency to high latency path.
const char kDisableAudioFallback[] = "disable-audio-fallback";

// Disable AudioOutputResampler for automatic audio resampling and rebuffering.
const char kDisableAudioOutputResampler[] = "disable-audio-output-resampler";

// Controls renderer side mixing and low latency audio path for media elements.
const char kDisableRendererSideMixing[] = "disable-renderer-side-mixing";

// Enable browser-side audio mixer.
const char kEnableAudioMixer[] = "enable-audio-mixer";

// Set number of threads to use for video decoding.
const char kVideoThreads[] = "video-threads";

// Disables support for Encrypted Media Extensions.
const char kDisableEncryptedMedia[] = "disable-encrypted-media";

// Enables Opus playback in media elements.
const char kEnableOpusPlayback[] = "enable-opus-playback";

// Enables VP9 playback in media elements.
const char kEnableVp9Playback[] = "enable-vp9-playback";

// Enables VP8 Alpha playback in media elements.
const char kEnableVp8AlphaPlayback[] = "enable-vp8-alpha-playback";

#if defined(OS_WIN)
const char kWaveOutBuffers[] = "waveout-buffers";
#endif

}  // namespace switches
