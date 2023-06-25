//
// Created by Tim Holzhey on 14.06.23
//

#ifndef FM_SYNTHESIZER_CONFIG_H
#define FM_SYNTHESIZER_CONFIG_H

#include "patch_file.h"

#define AUDIO_SAMPLE_RATE						44100
#define AUDIO_FRAMES_PER_BUFFER					64

#define NUM_VOICES								16
#define NUM_OPERATORS							6

#define DEFAULT_PATCH_FILE						PATCH_FILE_ROM_ROM1A
#define DEFAULT_PATCH_FILE_VOICE				11

#define SAMPLE_BIT_WIDTH						24
#define SAMPLE_MASK								((1 << SAMPLE_BIT_WIDTH) - 1)

#define BASE_PITCH_FREQUENCY_HZ					440
#define BASE_PITCH_MIDI_NOTE					69
#define HALF_TONES_PER_OCTAVE					12
#define NOTE_TO_LOG_FREQ_TABLE_SIZE				128

#define LOG_FREQ_TO_PHASE_TABLE_BIT_WIDTH		20
#define LOG_FREQ_TO_PHASE_TABLE_MASK			((1 << LOG_FREQ_TO_PHASE_TABLE_BIT_WIDTH) - 1)
#define LOG_FREQ_TO_PHASE_TABLE_LOG_SIZE		10
#define LOG_FREQ_TO_PHASE_TABLE_SIZE			(1 << LOG_FREQ_TO_PHASE_TABLE_LOG_SIZE)
#define LOG_FREQ_TO_PHASE_TABLE_SAMPLE_SHIFT	(SAMPLE_BIT_WIDTH - LOG_FREQ_TO_PHASE_TABLE_LOG_SIZE)

#define LOG_SIN_TABLE_BIT_WIDTH					8
#define LOG_SIN_TABLE_MASK						((1 << LOG_SIN_TABLE_BIT_WIDTH) - 1)
#define LOG_SIN_TABLE_LOG_SIZE					8
#define LOG_SIN_TABLE_SIZE						(1 << LOG_SIN_TABLE_LOG_SIZE)

#define EXP_TABLE_BIT_WIDTH						10
#define EXP_TABLE_MASK							((1 << EXP_TABLE_BIT_WIDTH) - 1)
#define EXP_TABLE_LOG_SIZE						8
#define EXP_TABLE_SIZE							(1 << EXP_TABLE_LOG_SIZE)

#define COARSE_LOG_MULT_TABLE_BIT_WIDTH			SAMPLE_BIT_WIDTH
#define COARSE_LOG_MULT_TABLE_MASK				((1 << COARSE_LOG_MULT_TABLE_BIT_WIDTH) - 1)
#define COARSE_LOG_MULT_TABLE_LOG_SIZE			5
#define COARSE_LOG_MULT_TABLE_SIZE				(1 << COARSE_LOG_MULT_TABLE_LOG_SIZE)

#define ALGORITHM_ROUTING_TABLE_SIZE			31
#define OUTPUT_MODE_INDEX_1						(1 << 0)
#define OUTPUT_MODE_INDEX_2						(1 << 1)
#define OUTPUT_MODE_INDEX_3						(1 << 2)
#define OUTPUT_MODE_INDEX_4						(1 << 3)
#define OUTPUT_MODE_INDEX_5						(1 << 4)
#define OUTPUT_MODE_INDEX_6						(1 << 5)
#define OUTPUT_MOD_INDEX_MASTER					(1 << 6)

#endif //FM_SYNTHESIZER_CONFIG_H
