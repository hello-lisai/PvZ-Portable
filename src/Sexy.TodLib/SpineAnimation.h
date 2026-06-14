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
    float               mRenderOffsetX;      // Visual offset X
    float               mRenderOffsetY;      // Visual offset Y (positive = down)
    std::string         mAnchorBone;        // Anchor bone name (e.g. "bone2")
    std::string         mBulletTrack;       // Bullet launch bone name (e.g. "bone10")
    std::string         mCardImage;         // Card custom image path (e.g. "spinedemo/123.png")

    SpineAnimationParams()
        : mType((SpineAnimationType)0), mDefaultScale(1.0f),
          mRenderOffsetX(0.0f), mRenderOffsetY(0.0f) {}

    SpineAnimationParams(SpineAnimationType t, const std::string& j,
                         const std::string& a, float s,
                         float offX = 0.0f, float offY = 0.0f,
                         const std::string& anchor = "",
                         const std::string& bullet = "",
                         const std::string& card = "")
        : mType(t), mJSONPath(j), mAtlasPath(a), mDefaultScale(s),
          mRenderOffsetX(offX), mRenderOffsetY(offY),
          mAnchorBone(anchor), mBulletTrack(bullet), mCardImage(card) {}
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

    // Anchor bone world position cache
    float                              mAnchorX;     // Anchor bone world coordinate X
    float                              mAnchorY;     // Anchor bone world coordinate Y

    // Pre-allocated vertex buffers to avoid per-frame allocation
    std::vector<float>                 mWorldVertsCache;  // Pre-allocated vertex buffer
    std::vector<Sexy::TriVertex>      mTriBatchCache;    // Pre-allocated triangle buffer

    SpineAnimation()  : mSpineType((SpineAnimationType)0),
        mPosX(0), mPosY(0), mAnimTime(0), mLastAnimTime(0),
        mFrameDelay(0.05f), mAnimFrame(0), mFlip(false), mDead(false),
        mColorOverride(Sexy::Color::White), mRenderGroup(0),
        mRenderOrder(0), mFrameTime(0), mFPS(30.0f),
        mExtraAdditiveDraw(false), mExtraAdditiveColor(Sexy::Color::White),
        mExtraOverlayDraw(false), mExtraOverlayColor(Sexy::Color::White),
        mRenderOffsetX(0.0f), mRenderOffsetY(0.0f),
        mAtlas(nullptr), mSkeletonData(nullptr), mSkeleton(nullptr),
        mAnimStateData(nullptr), mAnimState(nullptr), mSpineParams(nullptr),
        mAnchorX(0.0f), mAnchorY(0.0f) {}

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
    void    SetPosition(float theX, float theY);
    void    OverrideScale(float theScaleX, float theScaleY);
    bool    GetBoneWorldPosition(const char* boneName, float* outX, float* outY);
    void    SetEnableExtraAdditiveDraw(bool theEnable) { mExtraAdditiveDraw = theEnable; }
    void    SetAdditiveColor(const Sexy::Color& theColor) { mExtraAdditiveColor = theColor; }
    void    SetEnableExtraOverlayDraw(bool theEnable) { mExtraOverlayDraw = theEnable; }
    void    SetOverlayColor(const Sexy::Color& theColor) { mExtraOverlayColor = theColor; }
    void    UpdateSkeletonWorld();
    int     GetNumAnimations();
    const char* GetAnimationName(int theIndex);
    const char* GetCurrentAnimationName();
    bool    IsAnimComplete();

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
