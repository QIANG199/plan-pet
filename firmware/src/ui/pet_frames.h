#pragma once

#include <stdint.h>

/* Layout contract with firmware/tools/bake_pet.py — keep both sides in sync:
 * pet_frames.bin = [theme 0=dark 1=light][state][frame] of PET_W x PET_H
 * RGB565 little-endian frames, baked from the clawd GIFs. */
extern "C" const uint8_t pet_frames_bin[];

constexpr int PET_W = 160;
constexpr int PET_H = 160;
constexpr int PET_FRAME_COUNT = 6;
constexpr int PET_STATE_COUNT = 7; /* idle..sleeping + poke */
constexpr int PET_POKE_IDX = 6;
constexpr uint32_t PET_FRAME_BYTES = (uint32_t)PET_W * PET_H * 2;
