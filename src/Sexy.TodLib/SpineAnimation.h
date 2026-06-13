/*
 * Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of PvZ-Portable.
 */

#ifndef SPINE_ANIMATION_H__
#define SPINE_ANIMATION_H__

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include "../SexyAppFramework/graphics/Graphics.h"
#include "../SexyAppFramework/graphics/Image.h"

// Forward-declare spine-c types so we don't leak C headers to the rest of the project.
extern "C" {
    struct spAtlas;
    struct spSkeletonData;
    struct spSkeleton;
    struct spAnimationState;
    struct spAnimationStateData;
    struct spAnimation;
}

enum class SpineAnimationType : int32_t
{
    SPINE_NONE = -1,
    SPINE_PEASHOOTER,
    SPINE_COUNT
};

struct SpineAnimationParams
{
    SpineAnimationType  mType;
    std::string         mJSONPath;
    std::string         mAtlasPath;
    float               mDefaultScale;
    // Visual offset to align Spine skeleton origin with game coordinate origin.
    // Game expects (0,0) at bottom-center of plant (ground level).
    // Spine editor typically puts root bone at center of character.
    // These offsets are added to all vertex positions during Draw().
    float               mRenderOffsetX;
    float               mRenderOffsetY;

    SpineAnimationParams() : mType((SpineAnimationType)0), mDefaultScale(1.0f),
        mRenderOffsetX(0.0f), mRenderOffsetY(0.0f) {}
    SpineAnimationParams(SpineAnimationType t, const std::string& j,
                         const std::string& a, float s,
                         float offX = 0.0f, float offY = 0.0f)
        : mType(t), mJSONPath(j), mAtlasPath(a), mDefaultScale(s),
          mRenderOffsetX(offX), mRenderOffsetY(offY) {}
};

class SpineAnimation
{
public:
    SpineAnimationType                 mSpineType;
    float                              mPosX;
    float                              mPosY;
    float                              mAnimTime;
    float                              mLastAnimTime;
    float                              mFrameDelay;
    int32_t                            mAnimFrame;
    bool                               mFlip;
    bool                               mDead;
    Sexy::Color                        mColorOverride;
    int32_t                            mRenderGroup;
    int32_t                            mRenderOrder;
    float                              mFrameTime;
    float                              mFPS;
    bool                               mExtraAdditiveDraw;
    Sexy::Color                        mExtraAdditiveColor;
    bool                               mExtraOverlayDraw;
    Sexy::Color                        mExtraOverlayColor;
    float                              mRenderOffsetX;
    float                              mRenderOffsetY;

    // spine-c runtime objects
    spAtlas*                           mAtlas;
    spSkeletonData*                    mSkeletonData;
    spSkeleton*                        mSkeleton;
    spAnimationStateData*              mAnimStateData;
    spAnimationState*                  mAnimState;
    SpineAnimationParams*              mSpineParams;

    // Pre-allocated draw buffers (avoid per-frame heap alloc)
    std::vector<float>                 mWorldVertsCache;
    std::vector<Sexy::TriVertex>      mTriBatchCache;

    SpineAnimation()  : mSpineType((SpineAnimationType)0),
        mPosX(0), mPosY(0), mAnimTime(0), mLastAnimTime(0),
        mFrameDelay(0.05f), mAnimFrame(0), mFlip(false), mDead(false),
        mColorOverride(Sexy::Color::White), mRenderGroup(0),
        mRenderOrder(0), mFrameTime(0), mFPS(30.0f),
        mExtraAdditiveDraw(false), mExtraAdditiveColor(Sexy::Color::White),
        mExtraOverlayDraw(false), mExtraOverlayColor(Sexy::Color::White),
        mRenderOffsetX(0), mRenderOffsetY(0),
        mAtlas(nullptr), mSkeletonData(nullptr), mSkeleton(nullptr),
        mAnimStateData(nullptr), mAnimState(nullptr), mSpineParams(nullptr) {}

    ~SpineAnimation();

    void    SpineAnimationInitialize(float theX, float theY, SpineAnimationType theSpineType);
    void    SpineAnimationDie() { mDead = true; }
    void    Update();
    void    Draw(Sexy::Graphics* g);
    void    DrawRenderGroup(Sexy::Graphics* g, int theRenderGroup);
    void    SetAnimation(const char* theAnimName, bool theLoop);
    void    AddAnimation(const char* theAnimName, bool theLoop, float theDelay);
    void    SetTimeScale(float theScale);
    void    SetFlipX(bool theFlip);
    void    SetFlipY(bool theFlip);
    void    SetScale(float theScale);
    void    SetColor(const Sexy::Color& theColor);
    void    SetColorOverride(const Sexy::Color& theColor) { mColorOverride = theColor; }
    void    SetAdditiveColor(const Sexy::Color& theColor) { mExtraAdditiveColor = theColor; }
    void    SetEnableExtraAdditiveDraw(bool theEnable) { mExtraAdditiveDraw = theEnable; }
    void    SetOverlayColor(const Sexy::Color& theColor) { mExtraOverlayColor = theColor; }
    void    SetEnableExtraOverlayDraw(bool theEnable) { mExtraOverlayDraw = theEnable; }
    void    SetPosition(float theX, float theY);
    void    OverrideScale(float theScaleX, float theScaleY);
    void    UpdateSkeletonWorld();
    int     GetNumAnimations();
    const char* GetAnimationName(int theIndex);
    const char* GetCurrentAnimationName();
    bool    IsAnimComplete();

    // Query world position of a named bone after skeleton update.
    // Returns true if bone found, false otherwise.
    bool    GetBoneWorldPosition(const char* boneName, float* outX, float* outY);

    static void InitializeSpineAnimArray();
    static std::vector<SpineAnimationParams>  gSpineAnimArray;
};

class SpineSkeletonDataCache
{
public:
    struct CachedData {
        spAtlas* atlas;
        spSkeletonData* skeletonData;
    };
    static CachedData LoadData(const std::string& jsonPath,
                                const std::string& atlasPath);
    static void ClearAll();
    static std::map<std::string, CachedData> gCache;
};

#endif
