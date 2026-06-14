/*
 * Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of PvZ-Portable.
 *
 * SpineCardRenderer — Independent card / preview image rendering for
 * plants that use Spine animations.
 */

#ifndef SPINE_CARD_RENDERER_H__
#define SPINE_CARD_RENDERER_H__

#include "SpineAnimation.h"

// ============================================================
//  Card image lookup
// ============================================================

const std::string& SpineCardRenderer_GetImagePath(SpineAnimationType spineType);

// ============================================================
//  Card drawing
// ============================================================

/// Draw a custom seed-packet card for a Spine plant.
/// @param seedTypeEnumValue  integer value of the SeedType enum (e.g. SEED_PEASHOOTER)
/// @return true if drawn (caller should skip legacy DrawCachedPlant).
bool SpineCardRenderer_DrawSeedCard(
    void* g,                    // Graphics* — use void* to avoid header dependency
    int   seedTypeEnumValue,
    float thePosX,
    float thePosY,
    float theOffsetX,
    float theOffsetY,
    float theScaleX,
    float theScaleY);

#endif // SPINE_CARD_RENDERER_H__
