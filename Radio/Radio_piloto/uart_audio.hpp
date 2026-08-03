#pragma once

#include <Arduino.h>

bool uart_audio_init();
bool uart_send_audio(uint8_t frame_seq, const int8_t *audio_data, uint16_t audio_len);
void uart_process_received_audio(bool reproduce);
void uart_discard_received_audio();
bool uart_box_ptt_active();
void uart_audio_debug_report();
