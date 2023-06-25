//
// Created by Tim Holzhey on 14.06.23
//

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "synthesizer.h"
#include "patch_file.h"
#include "visualization.h"
#include "config.h"
#include "read_luts.h"

#define ENVELOPE_BIT_DEPTH				9
#define ENVELOPE_MAX					(1 << ENVELOPE_BIT_DEPTH)

#define INT16_SIGN_BIT					0x8000

#define OSCILLATOR_MODE_RATIO			0
#define OSCILLATOR_MODE_FIXED			1

// LUTS
static uint32_t note_to_log_freq_table[NOTE_TO_LOG_FREQ_TABLE_SIZE];
static uint32_t log_freq_to_phase_table[LOG_FREQ_TO_PHASE_TABLE_SIZE];
static uint16_t log_sin_table[LOG_SIN_TABLE_SIZE];
static uint16_t exp_table[EXP_TABLE_SIZE];
static int32_t coarse_log_mult_table[COARSE_LOG_MULT_TABLE_SIZE];
static uint8_t algorithm_routing_table[ALGORITHM_ROUTING_TABLE_SIZE][NUM_OPERATORS];

synth_data_t synth_data;

ret_code_t synthesizer_init(void) {
	RET_ON_FAIL(READ_LUT("hex_u32_note_to_log_freq.mem", note_to_log_freq_table));
	RET_ON_FAIL(READ_LUT("hex_u32_log_freq_to_phase.mem", log_freq_to_phase_table));
	RET_ON_FAIL(READ_LUT("hex_u16_log_sin.mem", log_sin_table));
	RET_ON_FAIL(READ_LUT("hex_u16_exp.mem", exp_table));
	RET_ON_FAIL(READ_LUT("hex_i32_coarse_log_mult.mem", coarse_log_mult_table));
	RET_ON_FAIL(READ_LUT("hex_u8_algorithm_routing.mem", algorithm_routing_table));

	// Load default patch file
	RET_ON_FAIL(patch_file_load_rom(DEFAULT_PATCH_FILE));

	// Load default sample
	RET_ON_FAIL(patch_file_load_patch(DEFAULT_PATCH_FILE_VOICE - 1, &synth_data.voice_params));

	// Test 1 operator
	voice_params_t params = {
			.algorithm = 4,
			.name = "TEST      ",
			.operators = {
					[0] = {
						.osc = {.frequency_coarse = 1},
						.output_level = 99,
					},
					[1] = {
						.osc = {.frequency_coarse = 14},
						.output_level = 58,
					},/*
					[2] = {
						.osc = {.frequency_coarse = 1},
						.output_level = 99,
					},
					[3] = {
						.osc = {.frequency_coarse = 1},
						.output_level = 88,
					}*/
			}
	};
	 memcpy(&synth_data.voice_params, &params, sizeof(voice_params_t));
	// voice_assign_key(60, 127);

	return RET_CODE_OK;
}

uint32_t get_phase_from_log_frequency(uint32_t log_freq) {
	uint32_t index = (log_freq & SAMPLE_MASK) >> LOG_FREQ_TO_PHASE_TABLE_SAMPLE_SHIFT;
	uint32_t freq_low = log_freq_to_phase_table[index];
	uint32_t freq_high = log_freq_to_phase_table[index + 1];
	uint32_t low_bits = log_freq & ((1 << LOG_FREQ_TO_PHASE_TABLE_SAMPLE_SHIFT) - 1);
	uint32_t high_bits = log_freq >> SAMPLE_BIT_WIDTH;
	uint32_t freq = freq_low + (int32_t) ((((int64_t) (freq_high - freq_low) * (int64_t) low_bits)) >> LOG_FREQ_TO_PHASE_TABLE_SAMPLE_SHIFT);
	return freq >> (LOG_FREQ_TO_PHASE_TABLE_BIT_WIDTH - high_bits);
}

uint32_t get_oscillator_log_frequency(uint8_t midi_note, uint8_t mode, uint8_t coarse, uint8_t fine, uint8_t detune) {
	if (mode == OSCILLATOR_MODE_RATIO) {
		uint32_t log_freq = (int32_t) note_to_log_freq_table[midi_note];

		double detune_ratio = 0.0209 * exp(-0.396 * (((float)log_freq) / (1 << SAMPLE_BIT_WIDTH))) / 7;
		log_freq += (int32_t) detune_ratio * log_freq * (detune - 7);

		log_freq += coarse_log_mult_table[coarse & (COARSE_LOG_MULT_TABLE_SIZE - 1)];
		if (fine) {
			// (1 << 24) / log(2)
			log_freq += (int32_t)floor(24204406.323123 * log(1 + 0.01 * fine) + 0.5);
		}

		return log_freq;
	}

	uint32_t log_freq = (4458616 * ((coarse & 3) * 100 + fine)) >> 3;
	log_freq += detune > 7 ? 13457 * (detune - 7) : 0;
	return log_freq;
}

uint16_t get_log_sin_from_angle(uint16_t phi) {
	uint8_t index = phi & LOG_SIN_TABLE_MASK;
	uint8_t quadrant = (phi >> LOG_SIN_TABLE_BIT_WIDTH) & 3;

	switch (quadrant) {
		case 0:
			// rising quarter wave  Shape A
			return log_sin_table[index];
		case 1:
			// falling quarter wave  Shape B
			return log_sin_table[index ^ (LOG_SIN_TABLE_SIZE - 1)];
		case 2:
			// rising quarter wave -ve  Shape C
			return log_sin_table[index] | INT16_SIGN_BIT;
		default:
			// falling quarter wave -ve  Shape D
			return log_sin_table[index ^ (LOG_SIN_TABLE_SIZE - 1)] | INT16_SIGN_BIT;
	}
}

int32_t get_sin_from_angle(uint32_t phase, uint16_t envelope) {
	uint16_t log_sin = get_log_sin_from_angle(phase >> LOG_FREQ_TO_PHASE_TABLE_SAMPLE_SHIFT) + (envelope << (ENVELOPE_BIT_DEPTH - 6));
	bool is_signed = log_sin & INT16_SIGN_BIT;
	log_sin &= ~INT16_SIGN_BIT;
	uint32_t result = (1 << EXP_TABLE_BIT_WIDTH) + exp_table[(log_sin & (EXP_TABLE_SIZE - 1)) ^ (EXP_TABLE_SIZE - 1)];
	result <<= 1;
	result >>= (log_sin >> EXP_TABLE_LOG_SIZE);

	if (is_signed) {
		return (-result-1) << LOG_FREQ_TO_PHASE_TABLE_SAMPLE_SHIFT;
	}
	return (result) << LOG_FREQ_TO_PHASE_TABLE_SAMPLE_SHIFT;
}

int synthesizer_render(const void *input_buffer, void *output_buffer,
					   unsigned long frames_per_buffer,
					   const PaStreamCallbackTimeInfo *time_info,
					   PaStreamCallbackFlags status_flags,
					   void *user_data) {
	synth_data_t *data = (synth_data_t *) user_data;
	int32_t *out = (int32_t *) output_buffer;

	(void) time_info;
	(void) status_flags;
	(void) input_buffer;

	for (uint32_t frame_idx = 0; frame_idx < frames_per_buffer; frame_idx++) {
		int32_t master_buffer = 0;

		for (uint32_t voice_idx = 0; voice_idx < NUM_VOICES; voice_idx++) {
			if (data->voice_data[voice_idx].gate == 0) {
				continue;
			}

			// Clear input buffers
			for (uint32_t operator_idx = 0; operator_idx < NUM_OPERATORS; operator_idx++) {
				operator_data_t *op_data = &data->voice_data[voice_idx].operator_data[operator_idx];
				op_data->input_mod_buffer = 0;
				op_data->level_in = 0;
			}

			// Sample operators
			for (int32_t operator_idx = NUM_OPERATORS - 1; operator_idx >= 0; operator_idx--) {
				operator_data_t *op_data = &data->voice_data[voice_idx].operator_data[operator_idx];
				operator_params_t *op_params = &data->voice_params.operators[operator_idx];

				// Get frequency and phase increment
				uint32_t log_freq = get_oscillator_log_frequency(data->voice_data[voice_idx].note, op_params->osc.mode, op_params->osc.frequency_coarse, op_params->osc.frequency_fine, op_params->osc.detune);
				uint32_t phase_inc = get_phase_from_log_frequency(log_freq);
				// printf("operator %d, log_freq %d, phase %d\n", operator_idx, log_freq, phase);

				uint16_t op_level = ENVELOPE_MAX - op_params->output_level * ENVELOPE_MAX / 99;

				// Sample sine wave
				int32_t sample = get_sin_from_angle(op_data->phase + op_data->input_mod_buffer, op_level);

				// Increment phase
				op_data->phase += phase_inc;

				// Route output
				const uint8_t *routing = algorithm_routing_table[data->voice_params.algorithm];
				if (routing[operator_idx] & OUTPUT_MOD_INDEX_MASTER) {
					master_buffer += sample;
					// printf("master buffer %d\n", master_buffer);
				}
				for (uint8_t output_index = 0; output_index < NUM_OPERATORS; output_index++) {
					if (routing[operator_idx] & (1 << output_index)) {
						// printf("routing %d to %d\n", operator_idx, output_index);
						data->voice_data[voice_idx].operator_data[output_index].input_mod_buffer += sample;
					}
				}
			}
		}

		// Find lower midi note playing
		uint8_t lowest_note = 127;
		for (uint32_t voice_idx = 0; voice_idx < NUM_VOICES; voice_idx++) {
			if (data->voice_data[voice_idx].gate == 0) {
				continue;
			}
			if (data->voice_data[voice_idx].note < lowest_note) {
				lowest_note = data->voice_data[voice_idx].note;
			}
		}

		// Add sample to visualization, align with midi frequency
		visualization_add_sample(master_buffer, lowest_note);

		// Amplify
		master_buffer <<= 1;

		// Write mono to stereo output buffer
		*out++ = master_buffer;
		*out++ = master_buffer;
	}

	return paContinue;
}
