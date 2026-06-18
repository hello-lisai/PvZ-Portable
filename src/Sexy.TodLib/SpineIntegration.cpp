/*
 * Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of PvZ-Portable.
 */

#include "SpineIntegration.h"
#include "Reanimator.h"
#include "TodDebug.h"
#include "EffectSystem.h"

// ============================================================
//  Factory
// ============================================================

Reanimation* SpineIntegrate_CreateReanim(
    float theX, float theY, int theRenderOrder, SpineAnimationType theSpineType)
{
    TOD_ASSERT(theSpineType >= SpineAnimationType(0) &&
               theSpineType < SpineAnimationType::SPINE_COUNT);

    Reanimation* aReanim = gEffectSystem->mReanimationHolder->AllocSpineAsReanimation(
        theX, theY, theRenderOrder, theSpineType);
    return aReanim;
}

// ============================================================
//  Lifecycle
// ============================================================

void SpineIntegrate_Update(Reanimation* reanim)
{
    if (reanim->mSpineAnimation == nullptr) return;

    reanim->mSpineAnimation->mFrameDelay = 0.01f * reanim->mAnimRate;
    reanim->mSpineAnimation->Update();
    reanim->mAnimTime = reanim->mSpineAnimation->mAnimTime;
    reanim->mLastFrameTime = reanim->mAnimTime;
    if (reanim->mSpineAnimation->mDead)
        reanim->mDead = true;
}

void SpineIntegrate_Draw(void* g, Reanimation* reanim)
{
    if (reanim->mSpineAnimation != nullptr)
        reanim->mSpineAnimation->Draw((Sexy::Graphics*)g);
}

void SpineIntegrate_DrawRenderGroup(void* g, Reanimation* reanim, int /*theRenderGroup*/)
{
    if (reanim->mSpineAnimation != nullptr)
        reanim->mSpineAnimation->Draw((Sexy::Graphics*)g);
}

void SpineIntegrate_Die(Reanimation* reanim)
{
    if (reanim->mSpineAnimation == nullptr) return;
    reanim->mSpineAnimation->SpineAnimationDie();
    delete reanim->mSpineAnimation;
    reanim->mSpineAnimation = nullptr;
}

void SpineIntegrate_SetPosition(Reanimation* reanim, float theX, float theY)
{
    if (reanim->mSpineAnimation != nullptr)
        reanim->mSpineAnimation->SetPosition(theX, theY);
}

void SpineIntegrate_OverrideScale(Reanimation* reanim, float theScaleX, float theScaleY)
{
    if (reanim->mSpineAnimation != nullptr)
        reanim->mSpineAnimation->OverrideScale(theScaleX, theScaleY);
}

void SpineIntegrate_PropogateColor(Reanimation* reanim)
{
    if (reanim->mSpineAnimation == nullptr) return;
    reanim->mSpineAnimation->SetColorOverride(reanim->mColorOverride);
    if (reanim->mEnableExtraAdditiveDraw)
        reanim->mSpineAnimation->SetAdditiveColor(reanim->mExtraAdditiveColor);
}

// ============================================================
//  Animation control
// ============================================================

void SpineIntegrate_PlayReanim(Reanimation* reanim, const char* theTrackName,
                               int theLoopType, float theAnimRate)
{
    if (reanim->mSpineAnimation == nullptr) return;

    bool aShouldLoop = (theLoopType == REANIM_LOOP || theLoopType == REANIM_LOOP_FULL_LAST_FRAME);
    if (theAnimRate != 0.0f)
        reanim->mSpineAnimation->SetTimeScale(theAnimRate / 12.0f);
    reanim->mSpineAnimation->SetAnimation(theTrackName, aShouldLoop);
}

void SpineIntegrate_SetFramesForLayer(Reanimation* reanim, const char* theTrackName)
{
    if (reanim->mSpineAnimation == nullptr) return;

    bool aShouldLoop = (reanim->mLoopType == REANIM_LOOP ||
                        reanim->mLoopType == REANIM_LOOP_FULL_LAST_FRAME);
    reanim->mSpineAnimation->SetAnimation(theTrackName, aShouldLoop);
}

bool SpineIntegrate_TrackExists(Reanimation* reanim, const char* theTrackName)
{
    if (reanim->mSpineAnimation == nullptr) return false;

    int aCount = reanim->mSpineAnimation->GetNumAnimations();
    for (int i = 0; i < aCount; i++)
    {
        const char* aName = reanim->mSpineAnimation->GetAnimationName(i);
        if (aName != nullptr && strcasecmp(aName, theTrackName) == 0)
            return true;
    }
    return false;
}

bool SpineIntegrate_IsAnimPlaying(Reanimation* reanim, const char* theTrackName)
{
    if (reanim->mSpineAnimation == nullptr) return false;
    return strcasecmp(reanim->mSpineAnimation->GetCurrentAnimationName(), theTrackName) == 0;
}

// ============================================================
//  Transform / bone query
// ============================================================

void SpineIntegrate_GetCurrentTransform(Reanimation* reanim,
                                        ReanimatorTransform* outTransform)
{
    if (reanim->mSpineAnimation == nullptr) return;

    float boneX = 0.0f, boneY = 0.0f;
    bool found = false;
    SpineAnimation* sp = reanim->mSpineAnimation;

    // 1st: Try bulletTrack from config
    if (sp->mSpineParams != nullptr && !sp->mSpineParams->mBulletTrack.empty())
    {
        found = sp->GetBoneWorldPosition(
            sp->mSpineParams->mBulletTrack.c_str(), &boneX, &boneY);
    }

    // 2nd: Fall back to candidate search
    if (!found) {
        const char* candidates[] = {
            "head", "stem", "body", "gun", "muzzle",
            "bone10", "bone11", "bone3", "bone6",
            "root", nullptr
        };
        for (int i = 0; candidates[i] != nullptr && !found; i++)
            found = sp->GetBoneWorldPosition(candidates[i], &boneX, &boneY);
    }
    if (!found)
        sp->GetBoneWorldPosition("root", &boneX, &boneY);

    outTransform->mTransX   = boneX;
    outTransform->mTransY   = boneY;
    outTransform->mSkewX    = 0.0f;
    outTransform->mSkewY    = 0.0f;
    outTransform->mScaleX   = 1.0f;
    outTransform->mScaleY   = 1.0f;
    outTransform->mAlpha    = 1.0f;
    outTransform->mFrame    = 0.0f;
    outTransform->mImage    = nullptr;
    outTransform->mFont     = nullptr;
    outTransform->mText     = "";
}

void SpineIntegrate_GetFramesForLayer(Reanimation* /*reanim*/,
                                      int& outFrameStart, int& outFrameCount)
{
    outFrameStart = 0;
    outFrameCount = 0;
}

// ============================================================
//  Seed-type mapping
// ============================================================

SpineAnimationType SpineIntegrate_SeedTypeToSpineType(int seedTypeEnumValue)
{
    switch (seedTypeEnumValue)
    {
    case 1:  // SEED_PEASHOOTER (approximate enum value)
        return SpineAnimationType::SPINE_PEASHOOTER;
    default:
        break;
    }
    return SpineAnimationType::SPINE_NONE;
}
