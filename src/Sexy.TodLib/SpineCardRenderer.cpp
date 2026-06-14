/*
 * Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of PvZ-Portable.
 */

#include "SpineCardRenderer.h"
#include "SpineIntegration.h"
#include "../Lawn/Plant.h"           // for SeedType enum
#include "../SexyAppFramework/SexyAppBase.h"
#include "../SexyAppFramework/graphics/Graphics.h"
#include "TodCommon.h"               // for TodDrawImageScaledF

// ============================================================
//  Card image path lookup
// ============================================================

const std::string& SpineCardRenderer_GetImagePath(SpineAnimationType spineType)
{
    static const std::string sEmpty;
    if ((size_t)spineType < SpineAnimation::gSpineAnimArray.size())
        return SpineAnimation::gSpineAnimArray[(size_t)spineType].mCardImage;
    return sEmpty;
}

// ============================================================
//  Card drawing
// ============================================================

bool SpineCardRenderer_DrawSeedCard(
    void* g,
    int   seedTypeEnumValue,
    float thePosX,
    float thePosY,
    float theOffsetX,
    float theOffsetY,
    float theScaleX,
    float theScaleY)
{
    Graphics* gfx = (Graphics*)g;

    // Map SeedType integer → SpineAnimationType.
    // Extend this mapping as more plants get Spine support.
    SpineAnimationType spineType = SpineAnimationType::SPINE_NONE;
    if (seedTypeEnumValue == (int)SeedType::SEED_PEASHOOTER)
        spineType = SpineAnimationType::SPINE_PEASHOOTER;

    if (spineType == SpineAnimationType::SPINE_NONE)
        return false;

    if ((size_t)spineType >= SpineAnimation::gSpineAnimArray.size())
        return false;

    const SpineAnimationParams& params =
        SpineAnimation::gSpineAnimArray[(size_t)spineType];

    if (params.mCardImage.empty())
        return false;

    SharedImageRef cardImg = gSexyAppBase->GetSharedImage(params.mCardImage);
    Image* rawImg = cardImg;
    if ((Image*)cardImg == nullptr)
        return false;

    float imgW = (float)rawImg->mWidth;
    float imgH = (float)rawImg->mHeight;

    TodDrawImageScaledF(gfx, rawImg,
        thePosX + theOffsetX - imgW * 0.5f * theScaleX,
        thePosY + theOffsetY - imgH * 0.5f * theScaleY,
        theScaleX, theScaleY);

    return true;
}
