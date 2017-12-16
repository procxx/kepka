project(libtgvoip)

add_library(tgvoip STATIC
    BlockingQueue.cpp
    BufferInputStream.cpp
    BufferOutputStream.cpp
    BufferPool.cpp
    CongestionControl.cpp
    EchoCanceller.cpp
    JitterBuffer.cpp
    logging.cpp
    MediaStreamItf.cpp
    NetworkSocket.cpp
    OpusDecoder.cpp
    OpusEncoder.cpp
    VoIPController.cpp
    VoIPServerConfig.cpp

    audio/AudioInput.cpp
    audio/AudioOutput.cpp
    audio/Resampler.cpp

    webrtc_dsp/webrtc/base/checks.cc
    webrtc_dsp/webrtc/base/stringutils.cc
    webrtc_dsp/webrtc/common_audio/audio_util.cc
    webrtc_dsp/webrtc/common_audio/channel_buffer.cc
    webrtc_dsp/webrtc/common_audio/fft4g.c
    webrtc_dsp/webrtc/common_audio/ring_buffer.c
    webrtc_dsp/webrtc/common_audio/signal_processing/auto_corr_to_refl_coef.c
    webrtc_dsp/webrtc/common_audio/signal_processing/auto_correlation.c
    webrtc_dsp/webrtc/common_audio/signal_processing/complex_bit_reverse.c
    webrtc_dsp/webrtc/common_audio/signal_processing/complex_fft.c
    webrtc_dsp/webrtc/common_audio/signal_processing/copy_set_operations.c
    webrtc_dsp/webrtc/common_audio/signal_processing/cross_correlation.c
    webrtc_dsp/webrtc/common_audio/signal_processing/division_operations.c
    webrtc_dsp/webrtc/common_audio/signal_processing/dot_product_with_scale.c
    webrtc_dsp/webrtc/common_audio/signal_processing/downsample_fast.c
    webrtc_dsp/webrtc/common_audio/signal_processing/energy.c
    webrtc_dsp/webrtc/common_audio/signal_processing/filter_ar.c
    webrtc_dsp/webrtc/common_audio/signal_processing/filter_ar_fast_q12.c
    webrtc_dsp/webrtc/common_audio/signal_processing/filter_ma_fast_q12.c
    webrtc_dsp/webrtc/common_audio/signal_processing/get_hanning_window.c
    webrtc_dsp/webrtc/common_audio/signal_processing/get_scaling_square.c
    webrtc_dsp/webrtc/common_audio/signal_processing/ilbc_specific_functions.c
    webrtc_dsp/webrtc/common_audio/signal_processing/levinson_durbin.c
    webrtc_dsp/webrtc/common_audio/signal_processing/lpc_to_refl_coef.c
    webrtc_dsp/webrtc/common_audio/signal_processing/min_max_operations.c
    webrtc_dsp/webrtc/common_audio/signal_processing/randomization_functions.c
    webrtc_dsp/webrtc/common_audio/signal_processing/real_fft.c
    webrtc_dsp/webrtc/common_audio/signal_processing/refl_coef_to_lpc.c
    webrtc_dsp/webrtc/common_audio/signal_processing/resample.c
    webrtc_dsp/webrtc/common_audio/signal_processing/resample_48khz.c
    webrtc_dsp/webrtc/common_audio/signal_processing/resample_by_2.c
    webrtc_dsp/webrtc/common_audio/signal_processing/resample_by_2_internal.c
    webrtc_dsp/webrtc/common_audio/signal_processing/resample_fractional.c
    webrtc_dsp/webrtc/common_audio/signal_processing/spl_init.c
    webrtc_dsp/webrtc/common_audio/signal_processing/spl_inl.c
    webrtc_dsp/webrtc/common_audio/signal_processing/spl_sqrt.c
    webrtc_dsp/webrtc/common_audio/signal_processing/spl_sqrt_floor.c
    webrtc_dsp/webrtc/common_audio/signal_processing/splitting_filter_impl.c
    webrtc_dsp/webrtc/common_audio/signal_processing/sqrt_of_one_minus_x_squared.c
    webrtc_dsp/webrtc/common_audio/signal_processing/vector_scaling_operations.c
    webrtc_dsp/webrtc/common_audio/sparse_fir_filter.cc
    webrtc_dsp/webrtc/common_audio/wav_file.cc
    webrtc_dsp/webrtc/common_audio/wav_header.cc
    webrtc_dsp/webrtc/modules/audio_processing/aec/aec_core.cc
    webrtc_dsp/webrtc/modules/audio_processing/aec/aec_core_sse2.cc
    webrtc_dsp/webrtc/modules/audio_processing/aec/aec_resampler.cc
    webrtc_dsp/webrtc/modules/audio_processing/aec/echo_cancellation.cc
    webrtc_dsp/webrtc/modules/audio_processing/aecm/aecm_core.cc
    webrtc_dsp/webrtc/modules/audio_processing/aecm/aecm_core_c.cc
    webrtc_dsp/webrtc/modules/audio_processing/aecm/echo_control_mobile.cc
    webrtc_dsp/webrtc/modules/audio_processing/agc/legacy/analog_agc.c
    webrtc_dsp/webrtc/modules/audio_processing/agc/legacy/digital_agc.c
    webrtc_dsp/webrtc/modules/audio_processing/logging/apm_data_dumper.cc
    webrtc_dsp/webrtc/modules/audio_processing/ns/noise_suppression.c
    webrtc_dsp/webrtc/modules/audio_processing/ns/noise_suppression_x.c
    webrtc_dsp/webrtc/modules/audio_processing/ns/ns_core.c
    webrtc_dsp/webrtc/modules/audio_processing/ns/nsx_core.c
    webrtc_dsp/webrtc/modules/audio_processing/ns/nsx_core_c.c
    webrtc_dsp/webrtc/modules/audio_processing/splitting_filter.cc
    webrtc_dsp/webrtc/modules/audio_processing/three_band_filter_bank.cc
    webrtc_dsp/webrtc/modules/audio_processing/utility/block_mean_calculator.cc
    webrtc_dsp/webrtc/modules/audio_processing/utility/delay_estimator.cc
    webrtc_dsp/webrtc/modules/audio_processing/utility/delay_estimator_wrapper.cc
    webrtc_dsp/webrtc/modules/audio_processing/utility/ooura_fft.cc
    webrtc_dsp/webrtc/modules/audio_processing/utility/ooura_fft_sse2.cc
    webrtc_dsp/webrtc/system_wrappers/source/cpu_features.cc
)

if (WIN32)
    target_compile_definitions(tgvoip PRIVATE -DNOMINMAX -D_USING_V110_SDK71_ -DTGVOIP_WINXP_COMPAT)
    target_sources(tgvoip PRIVATE
        os/windows/NetworkSocketWinsock.cpp
        os/windows/AudioInputWave.cpp
        os/windows/AudioOutputWave.cpp
        os/windows/AudioOutputWASAPI.cpp
        os/windows/AudioInputWASAPI.cpp
    )
endif()

if (APPLE)
    target_compile_definitions(tgvoip PRIVATE -DWEBRTC_MAC -DTARGET_OS_OSX)
    target_sources(tgvoip PRIVATE
        os/darwin/AudioInputAudioUnit.cpp
        os/darwin/AudioOutputAudioUnit.cpp
        os/darwin/AudioInputAudioUnitOSX.cpp
        os/darwin/AudioOutputAudioUnitOSX.cpp
        os/darwin/AudioUnitIO.cpp
        os/darwin/DarwinSpecific.mm
    )
endif()

if (LINUX)
    target_sources(tgvoip PRIVATE
        os/linux/AudioInputALSA.cpp
        os/linux/AudioOutputALSA.cpp
        os/linux/AudioOutputPulse.cpp
        os/linux/AudioInputPulse.cpp
        os/linux/PulseAudioLoader.cpp
    )
endif()

if (NOT WIN32) # POSIX
    target_compile_definitions(tgvoip PRIVATE -DWEBRTC_POSIX)
    target_sources(tgvoip PRIVATE
        os/posix/NetworkSocketPosix.cpp
    )
endif()

target_compile_definitions(tgvoip PRIVATE -DWEBRTC_APM_DEBUG_DUMP=0 -DTGVOIP_USE_DESKTOP_DSP)
target_include_directories(tgvoip PRIVATE webrtc_dsp ${OPUS_INCLUDE_DIR})
