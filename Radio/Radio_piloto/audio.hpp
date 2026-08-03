#pragma once

#include <Arduino.h>

bool audio_init();
void audio_process();

void audio_set_microphone_enabled(bool enabled);
bool audio_capture_frame(const int8_t **out_audio, uint16_t *out_len);

void audio_play_frame_s8(const int8_t *audio, uint16_t len);
void audio_request_playback_stop();
void audio_resume_playback();
void audio_cut_playback();
