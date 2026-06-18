/*
 * Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of PvZ-Portable.
 *
 * SpineIntegration — Independent bridge layer between the game's Reanimation
 * system and the Spine skeletal animation runtime.
 *
 * All Spine-specific logic that was previously interleaved inside Reanimator.cpp
 * (Reanimation class methods) is now consolidated here.  The Reanimation class
 * keeps only a thin mIsSpine flag and forwards to these free functions.
 */

#ifndef SPINE_INTEGRATION_H__
#define SPINE_INTEGRATION_H__

#include "SpineAnimation.h"
#include "Reanimator.h"

class Reanimation;
struct ReanimatorTransform;

// ============================================================
//  Factory
// ============================================================

Reanimation* SpineIntegrate_CreateReanim(
    float theX, float theY, int theRenderOrder, SpineAnimationType theSpineType);

// ============================================================
//  Lifecycle forwarders
// ============================================================

void SpineIntegrate_Update(Reanimation* reanim);
void SpineIntegrate_Draw(void* g, Reanimation* reanim);
void SpineIntegrate_DrawRenderGroup(void* g, Reanimation* reanim, int theRenderGroup);
void SpineIntegrate_Die(Reanimation* reanim);
void SpineIntegrate_SetPosition(Reanimation* reanim, float theX, float theY);
void SpineIntegrate_OverrideScale(Reanimation* reanim, float theScaleX, float theScaleY);
void SpineIntegrate_PropogateColor(Reanimation* reanim);

// ============================================================
//  Animation control forwarders
// ============================================================

void SpineIntegrate_PlayReanim(Reanimation* reanim, const char* theTrackName,
                               int theLoopType, float theAnimRate);
void SpineIntegrate_SetFramesForLayer(Reanimation* reanim, const char* theTrackName);
bool SpineIntegrate_TrackExists(Reanimation* reanim, const char* theTrackName);
bool SpineIntegrate_IsAnimPlaying(Reanimation* reanim, const char* theTrackName);

// ============================================================
//  Transform / bone query
// ============================================================

void SpineIntegrate_GetCurrentTransform(Reanimation* reanim,
                                        ReanimatorTransform* outTransform);
void SpineIntegrate_GetFramesForLayer(Reanimation* reanim,
                                      int& outFrameStart, int& outFrameCount);

// ============================================================
//  Seed-type mapping
// ============================================================

SpineAnimationType SpineIntegrate_SeedTypeToSpineType(int seedTypeEnumValue);

#endif // SPINE_INTEGRATION_H__
