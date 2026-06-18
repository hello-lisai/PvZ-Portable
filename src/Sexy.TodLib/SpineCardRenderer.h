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

const std::string& SpineCardRenderer_GetImagePath(SpineAnimationType spineType);

/// Draw a custom seed-packet card for a Spine plant.
/// @return true if drawn (caller should skip legacy DrawCachedPlant).
bool SpineCardRenderer_DrawSeedCard(
    void* g,
    int   seedTypeEnumValue,
    float thePosX,
    float thePosY,
    float theOffsetX,
    float theOffsetY,
    float theScaleX,
    float theScaleY);

#endif // SPINE_CARD_RENDERER_H__
