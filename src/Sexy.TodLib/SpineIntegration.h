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
//  Factory — create a Reanimation backed by Spine (not legacy .reanim)
// ============================================================

/// Create a Reanimation object that wraps a SpineAnimation internally.
/// Owned by the caller's ReanimationHolder (usually via LawnApp::AddSpineReanimation).
Reanimation* SpineIntegrate_CreateReanim(
    float theX, float theY, int theRenderOrder, SpineAnimationType theSpineType);

// ============================================================
//  Lifecycle forwarders — called by Reanimation::* methods
// ============================================================

void SpineIntegrate_Update(Reanimation* reanim);
void SpineIntegrate_Draw(Graphics* g, Reanimation* reanim);
void SpineIntegrate_DrawRenderGroup(Graphics* g, Reanimation* reanim, int theRenderGroup);
void SpineIntegrate_Die(Reanimation* reanim);
void SpineIntegrate_SetPosition(Reanimation* reanim, float theX, float theY);
void SpineIntegrate_OverrideScale(Reanimation* reanim, float theScaleX, float theScaleY);
void SpineIntegrate_PropogateColor(Reanimation* reanim);

// ============================================================
//  Animation control forwarders
// ============================================================

void SpineIntegrate_PlayReanim(Reanimation* reanim, const char* theTrackName,
                               ReanimLoopType theLoopType, float theAnimRate);
void SpineIntegrate_SetFramesForLayer(Reanimation* reanim, const char* theTrackName);
bool SpineIntegrate_TrackExists(Reanimation* reanim, const char* theTrackName);
bool SpineIntegrate_IsAnimPlaying(Reanimation* reanim, const char* theTrackName);

// ============================================================
//  Transform / bone query — used by game logic (bullet origin, etc.)
// ============================================================

/// Fill a ReanimatorTransform with the world position of the best bullet-track
/// bone (or a sensible fallback).  Used by Reanimation::GetCurrentTransform().
void SpineIntegrate_GetCurrentTransform(Reanimation* reanim,
                                        ReanimatorTransform* outTransform);

/// Get frames-for-layer info for a Spine reanim (always returns 0/0).
void SpineIntegrate_GetFramesForLayer(Reanimation* reanim,
                                      int& outFrameStart, int& outFrameCount);

// ============================================================
//  Seed-type mapping helper
// ============================================================

/// Map a Plant SeedType to its corresponding SpineAnimationType.
/// Returns SPINE_NONE if the seed type has no Spine equivalent.
SpineAnimationType SpineIntegrate_SeedTypeToSpineType(int seedTypeEnumValue);

#endif // SPINE_INTEGRATION_H__
