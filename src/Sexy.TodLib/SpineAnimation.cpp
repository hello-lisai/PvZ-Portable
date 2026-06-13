/*
 * Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of PvZ-Portable.
 */

#include "SpineAnimation.h"
#include "../SexyAppFramework/imagelib/ImageLib.h"
#include "../SexyAppFramework/paklib/PakInterface.h"
#include "../SexyAppFramework/graphics/TriVertex.h"
#include "../SexyAppFramework/graphics/MemoryImage.h"

#include <spine/spine.h>
#include <spine/extension.h>

#include <cstdlib>
#include <cstring>
#include <vector>
#include <cstdio>
#include <ctime>
#include <cstdarg>
#include <algorithm>

// ============================================================
//  Minimal debug logging – only errors and warnings.
//  No per-frame or per-slot logging to avoid I/O overhead.
// ============================================================
static FILE* gLogFile = nullptr;

static void spineLogOpen()
{
    if (gLogFile == nullptr) {
        gLogFile = fopen("spine_debug.log", "w");
        if (gLogFile) {
            time_t now = time(nullptr);
            fprintf(gLogFile, "=== Spine Debug Log Started: %s", ctime(&now));
            fflush(gLogFile);
        }
    }
}

static void spineLog(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    // Console output
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    // File output
    if (gLogFile) {
        va_list args2;
        va_copy(args2, args);
        vfprintf(gLogFile, fmt, args2);
        fprintf(gLogFile, "\n");
        fflush(gLogFile);
        va_end(args2);
    }

    va_end(args);
}

#define SPINE_ERR(...) do { spineLogOpen(); spineLog("[SPINE ERROR] " __VA_ARGS__); } while(0)
#define SPINE_WARN(...) do { spineLogOpen(); spineLog("[SPINE WARN] " __VA_ARGS__); } while(0)

// ============================================================
//  Static variables
// ============================================================
std::vector<SpineAnimationParams> SpineAnimation::gSpineAnimArray;
std::map<std::string, SpineSkeletonDataCache::CachedData> SpineSkeletonDataCache::gCache;

// ============================================================
//  Helper: read a Spine file (atlas or JSON) through the project's
//  PakInterface (p_fopen) so it works both with loose files and
//  assets packed inside main.pak.
// ============================================================
static std::vector<char> readSpineFile(const std::string& path)
{
    std::vector<char> result;
    PFILE* f = p_fopen(path.c_str(), "rb");
    if (f == nullptr) {
        SPINE_ERR("Cannot open file: %s", path.c_str());
        return result;
    }

    long pos = p_ftell(f);
    p_fseek(f, 0, SEEK_END);
    long size = p_ftell(f);
    p_fseek(f, pos, SEEK_SET);

    result.resize((size_t)size + 1);
    if (size > 0)
        p_fread(&result[0], 1, (size_t)size, f);
    result[size] = '\0';
    p_fclose(f);

    return result;
}

// ============================================================
//  spine-c texture-loading callbacks.
// ============================================================
static void _pvz_spine_createTexture(spAtlasPage* self, const char* path)
{
    if (path == nullptr) {
        self->rendererObject = nullptr;
        self->width = 1;
        self->height = 1;
        SPINE_ERR("createTexture: null path");
        return;
    }

    ImageLib::Image* loaded = ImageLib::GetImage(path, false);
    if (loaded == nullptr) {
        self->rendererObject = nullptr;
        self->width = 1;
        self->height = 1;
        SPINE_ERR("createTexture: ImageLib::GetImage failed for %s", path);
        return;
    }

    Sexy::MemoryImage* img = new Sexy::MemoryImage();
    img->mFilePath = path;
    img->SetBits(loaded->GetBits(), loaded->GetWidth(), loaded->GetHeight(), true);
    delete loaded;
    self->rendererObject = img;
    self->width = img->mWidth;
    self->height = img->mHeight;
}

static void _pvz_spine_disposeTexture(spAtlasPage* self)
{
    if (self == nullptr) return;
    Sexy::MemoryImage* img = (Sexy::MemoryImage*)self->rendererObject;
    if (img)
        delete img;
    self->rendererObject = nullptr;
}

// ============================================================
//  spine-c requires platform callbacks declared in <spine/extension.h>.
// ============================================================
extern "C" {

void _spAtlasPage_createTexture(spAtlasPage* self, const char* path)
{
    _pvz_spine_createTexture(self, path);
}

void _spAtlasPage_disposeTexture(spAtlasPage* self)
{
    _pvz_spine_disposeTexture(self);
}

char* _spUtil_readFile(const char* path, int* length)
{
    std::vector<char> buf = readSpineFile(std::string(path));
    if (buf.empty() || length == nullptr) {
        if (length != nullptr) *length = 0;
        return nullptr;
    }
    int len = (int)(buf.size() - 1);
    *length = len;
    char* out = (char*)malloc((size_t)len + 1);
    if (out == nullptr) {
        *length = 0;
        return nullptr;
    }
    if (len > 0) memcpy(out, buf.data(), (size_t)len);
    out[len] = '\0';
    return out;
}

} // extern "C"

static spAtlas* pvzCreateAtlas(const char* data, int length, const char* dir)
{
    return spAtlas_create(data, length, dir, nullptr);
}

static std::string extractDirectory(const std::string& path)
{
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos)
        return std::string();
    return path.substr(0, pos);
}

// ============================================================
//  Skeleton data cache
// ============================================================
SpineSkeletonDataCache::CachedData SpineSkeletonDataCache::LoadData(
    const std::string& jsonPath,
    const std::string& atlasPath)
{
    std::string key = jsonPath + "|" + atlasPath;
    auto it = gCache.find(key);
    if (it != gCache.end())
        return it->second;

    CachedData data = { nullptr, nullptr };

    std::vector<char> atlasData = readSpineFile(atlasPath);
    if (atlasData.empty()) {
        SPINE_ERR("LoadData: atlas file empty: %s", atlasPath.c_str());
        gCache[key] = data;
        return data;
    }

    std::string atlasDir = extractDirectory(atlasPath);
    spAtlas* atlas = pvzCreateAtlas(atlasData.data(), (int)atlasData.size() - 1,
                                     atlasDir.empty() ? nullptr : atlasDir.c_str());
    if (atlas == nullptr) {
        SPINE_ERR("LoadData: spAtlas_create failed for %s", atlasPath.c_str());
        gCache[key] = data;
        return data;
    }

    std::vector<char> jsonData = readSpineFile(jsonPath);
    if (jsonData.empty()) {
        SPINE_ERR("LoadData: json file empty: %s", jsonPath.c_str());
        spAtlas_dispose(atlas);
        gCache[key] = data;
        return data;
    }

    spSkeletonJson* json = spSkeletonJson_create(atlas);
    if (json == nullptr) {
        spAtlas_dispose(atlas);
        gCache[key] = data;
        return data;
    }
    json->scale = 1.0f;

    spSkeletonData* skeletonData = spSkeletonJson_readSkeletonData(json, jsonData.data());
    const char* error = json->error;
    spSkeletonJson_dispose(json);

    if (skeletonData == nullptr) {
        SPINE_ERR("LoadData: readSkeletonData error: %s", error ? error : "(null)");
        spAtlas_dispose(atlas);
        gCache[key] = data;
        return data;
    }

    data.atlas = atlas;
    data.skeletonData = skeletonData;
    gCache[key] = data;
    return data;
}

void SpineSkeletonDataCache::ClearAll()
{
    for (auto& kv : gCache) {
        if (kv.second.skeletonData)
            spSkeletonData_dispose(kv.second.skeletonData);
        if (kv.second.atlas)
            spAtlas_dispose(kv.second.atlas);
    }
    gCache.clear();
}

// ============================================================
//  Initialize the registry of known Spine animations.
// ============================================================
void SpineAnimation::InitializeSpineAnimArray()
{
    if (gSpineAnimArray.empty()) {
        gSpineAnimArray.push_back(
            SpineAnimationParams(SpineAnimationType::SPINE_PEASHOOTER,
                "spinedemo/GatlingPea.json", "spinedemo/GatlingPea.atlas", 1.0f,
                // Render offset: uniform shift applied after coordinate transform.
                // With the unified Y-flip system, (0,0) means skeleton root bone
                // renders exactly where SetPosition() places it.
                // Tune these if the character needs nudging after testing.
                0.0f,    // mRenderOffsetX
                0.0f));  // mRenderOffsetY
    }
}

// ============================================================
//  Destructor
// ============================================================
SpineAnimation::~SpineAnimation()
{
    if (mAnimState) {
        spAnimationState_dispose(mAnimState);
        mAnimState = nullptr;
    }
    if (mAnimStateData) {
        spAnimationStateData_dispose(mAnimStateData);
        mAnimStateData = nullptr;
    }
    if (mSkeleton) {
        spSkeleton_dispose(mSkeleton);
        mSkeleton = nullptr;
    }
}

// ============================================================
//  Initialize from a SpineAnimationType.
// ============================================================
void SpineAnimation::SpineAnimationInitialize(float theX, float theY, SpineAnimationType theSpineType)
{
    mPosX = theX;
    mPosY = theY;
    mAnimTime = 0.0f;
    mLastAnimTime = 0.0f;
    mFrameDelay = 0.05f;
    mAnimFrame = 0;
    mFlip = false;
    mDead = false;
    mColorOverride = Sexy::Color::White;
    mRenderGroup = 0;
    mRenderOrder = 0;
    mFrameTime = 0;
    mFPS = 30.0f;
    mSpineType = theSpineType;

    mAtlas = nullptr;
    mSkeletonData = nullptr;
    mSkeleton = nullptr;
    mAnimStateData = nullptr;
    mAnimState = nullptr;
    mSpineParams = nullptr;

    if (gSpineAnimArray.empty())
        InitializeSpineAnimArray();

    if (theSpineType >= SpineAnimationType(0) &&
        (size_t)theSpineType < gSpineAnimArray.size()) {
        mSpineParams = &gSpineAnimArray[(size_t)theSpineType];

        SpineSkeletonDataCache::CachedData cached =
            SpineSkeletonDataCache::LoadData(mSpineParams->mJSONPath, mSpineParams->mAtlasPath);
        mAtlas = cached.atlas;
        mSkeletonData = cached.skeletonData;

        if (mSkeletonData != nullptr) {
            mSkeleton = spSkeleton_create(mSkeletonData);
            mAnimStateData = spAnimationStateData_create(mSkeletonData);
            mAnimState = spAnimationState_create(mAnimStateData);

            if (mSkeleton != nullptr) {
                mSkeleton->x = theX;
                mSkeleton->y = theY;
                mSkeleton->scaleX = mSpineParams->mDefaultScale;
                mSkeleton->scaleY = mSpineParams->mDefaultScale;
                spSkeleton_setToSetupPose(mSkeleton);
                spSkeleton_updateWorldTransform(mSkeleton, (spPhysics)0);
            }
        } else {
            SPINE_ERR("SpineAnimationInitialize: skeletonData is null for type %d", (int)theSpineType);
        }
    }

    // Copy render offset from params (aligns skeleton origin with game coords)
    if (mSpineParams != nullptr)
    {
        mRenderOffsetX = mSpineParams->mRenderOffsetX;
        mRenderOffsetY = mSpineParams->mRenderOffsetY;
    }
}

// ============================================================
//  Animation control.
// ============================================================
void SpineAnimation::SetAnimation(const char* theAnimName, bool theLoop)
{
    if (mAnimState == nullptr || mSkeletonData == nullptr)
        return;

    if (spSkeletonData_findAnimation(mSkeletonData, theAnimName) != nullptr) {
        spAnimationState_setAnimationByName(mAnimState, 0, theAnimName, theLoop ? 1 : 0);
    } else if (mSkeletonData->animationsCount > 0) {
        spAnimation* first = mSkeletonData->animations[0];
        spAnimationState_setAnimation(mAnimState, 0, first, theLoop ? 1 : 0);
    }

    if (mSkeleton != nullptr)
        spSkeleton_updateWorldTransform(mSkeleton, (spPhysics)0);
}

void SpineAnimation::AddAnimation(const char* theAnimName, bool theLoop, float theDelay)
{
    if (mAnimState == nullptr)
        return;
    spAnimationState_addAnimationByName(mAnimState, 0, theAnimName, theLoop ? 1 : 0, theDelay);
}

void SpineAnimation::SetTimeScale(float theScale)
{
    if (mAnimState == nullptr) return;
    mAnimState->timeScale = theScale;
}

// ============================================================
//  Per-frame update.
// ============================================================
void SpineAnimation::Update()
{
    if (mAnimState == nullptr || mSkeleton == nullptr)
        return;

    float dt = mFrameDelay;
    if (dt <= 0.0f) dt = 1.0f / 60.0f;

    spAnimationState_update(mAnimState, dt);
    spAnimationState_apply(mAnimState, mSkeleton);
    spSkeleton_updateWorldTransform(mSkeleton, (spPhysics)0);

    mAnimTime += dt;
    mAnimFrame++;
}

void SpineAnimation::UpdateSkeletonWorld()
{
    if (mSkeleton != nullptr)
        spSkeleton_updateWorldTransform(mSkeleton, (spPhysics)0);
}

bool SpineAnimation::GetBoneWorldPosition(const char* boneName, float* outX, float* outY)
{
    if (mSkeleton == nullptr || boneName == nullptr || outX == nullptr || outY == nullptr)
        return false;

    spBone* bone = spSkeleton_findBone(mSkeleton, boneName);
    if (bone == nullptr)
        return false;

    // Return position as OFFSET from skeleton origin, in GAME coordinates (Y-down).
    // This matches legacy Reanimator behavior where GetCurrentTransform() returns
    // mTransX/mTransY as offsets from the reanim position — used by Fire(),
    // GetPeaHeadOffset(), and other logic code to compute bullet spawn points etc.
    //
    // Spine bone worldX/worldY are relative to skeleton origin in Y-up space.
    // Convert: X stays same, Y flips sign (Spine +up → screen +up = smaller Y).
    *outX = bone->worldX;
    *outY = -(bone->worldY);
    return true;
}

// ============================================================
//  Transform helpers.
// ============================================================
void SpineAnimation::SetFlipX(bool theFlip)
{
    if (mSkeleton == nullptr) return;
    float magnitude = mSkeleton->scaleX < 0 ? -mSkeleton->scaleX : mSkeleton->scaleX;
    if (magnitude == 0.0f) magnitude = 1.0f;
    mSkeleton->scaleX = theFlip ? -magnitude : magnitude;
}

void SpineAnimation::SetFlipY(bool theFlip)
{
    if (mSkeleton == nullptr) return;
    float magnitude = mSkeleton->scaleY < 0 ? -mSkeleton->scaleY : mSkeleton->scaleY;
    if (magnitude == 0.0f) magnitude = 1.0f;
    mSkeleton->scaleY = theFlip ? -magnitude : magnitude;
}

void SpineAnimation::SetScale(float theScale)
{
    if (mSkeleton == nullptr) return;
    mSkeleton->scaleX = theScale;
    mSkeleton->scaleY = theScale;
}

void SpineAnimation::OverrideScale(float theScaleX, float theScaleY)
{
    if (mSkeleton == nullptr) return;
    mSkeleton->scaleX = theScaleX;
    mSkeleton->scaleY = theScaleY;
}

void SpineAnimation::SetPosition(float theX, float theY)
{
    if (mSkeleton == nullptr) return;
    mSkeleton->x = theX;
    mSkeleton->y = theY;
}

void SpineAnimation::SetColor(const Sexy::Color& theColor)
{
    if (mSkeleton == nullptr) return;
    mSkeleton->color.r = theColor.mRed / 255.0f;
    mSkeleton->color.g = theColor.mGreen / 255.0f;
    mSkeleton->color.b = theColor.mBlue / 255.0f;
    mSkeleton->color.a = theColor.mAlpha / 255.0f;
}

// ============================================================
//  Query helpers.
// ============================================================
int SpineAnimation::GetNumAnimations()
{
    if (mSkeletonData == nullptr) return 0;
    return mSkeletonData->animationsCount;
}

const char* SpineAnimation::GetAnimationName(int theIndex)
{
    if (mSkeletonData == nullptr || theIndex < 0 || theIndex >= mSkeletonData->animationsCount)
        return "";
    return mSkeletonData->animations[theIndex]->name;
}

const char* SpineAnimation::GetCurrentAnimationName()
{
    if (mAnimState == nullptr) return "";
    if (mAnimState->tracks == nullptr) return "";
    spTrackEntry* entry = mAnimState->tracks[0];
    if (entry == nullptr || entry->animation == nullptr) return "";
    return entry->animation->name;
}

bool SpineAnimation::IsAnimComplete()
{
    if (mAnimState == nullptr) return true;
    if (mAnimState->tracks == nullptr) return true;
    spTrackEntry* entry = mAnimState->tracks[0];
    if (entry == nullptr) return true;
    return (entry->loop == 0) && (entry->trackLast >= entry->animationEnd);
}

// ============================================================
//  Draw – the core rendering function.
//
//  Passes world vertices and UV coordinates from Spine directly
//  to the engine's DrawTrianglesTex. No coordinate transformation
//  is applied here – the skeleton position (x, y) is set via
//  SetPosition() in game coordinates, and Spine's bone transforms
//  produce vertices in that same space.
// ============================================================
static inline uint32_t colorToUInt(float r, float g, float b, float a)
{
    return ((uint32_t)(a * 255.0f) << 24) |
           ((uint32_t)(r * 255.0f) << 16) |
           ((uint32_t)(g * 255.0f) << 8)  |
           (uint32_t)(b * 255.0f);
}

// Apply additive (eaten flash) and overlay (beghouled) visual effects
// to a per-vertex color.  These are set by PropogateColorToAttachments()
// which is called every frame from Plant::Animate() / Reanimation::Draw().
// This is the single point where ALL Spine visual effects are applied —
// no per-animation or per-plant special-casing needed.
static inline uint32_t applyVisualEffects(
    float r, float g, float b, float a,
    uint32_t baseColor,
    bool enableAdditive, const Sexy::Color& additiveCol,
    bool enableOverlay, const Sexy::Color& overlayCol)
{
    // Start from base color if overridden, otherwise compute from spine colors
    uint32_t finalColor;
    if (baseColor != 0) {
        finalColor = baseColor;
    } else {
        finalColor = colorToUInt(r, g, b, a);
    }

    // Additive blend: eaten flash (white flash when zombie bites plant)
    // Adds color on top of existing — simulates DRAWMODE_ADDITIVE.
    if (enableAdditive) {
        int addR = (int)(finalColor >> 16 & 0xFF) + additiveCol.mRed;
        int addG = (int)(finalColor >> 8  & 0xFF) + additiveCol.mGreen;
        int addB = (int)(finalColor       & 0xFF) + additiveCol.mBlue;
        finalColor = ((uint32_t)(std::min(addR, 255)) << 16) |
                     ((uint32_t)(std::min(addG, 255)) << 8)  |
                     ((uint32_t)(std::min(addB, 255)));
        // Keep original alpha
    }

    // Overlay blend: beghouled flash (glowing outline effect)
    // Blends overlay color into existing using overlay alpha as factor.
    if (enableOverlay && overlayCol.mAlpha > 0) {
        float ovAlpha = overlayCol.mAlpha / 255.0f;
        float invA = 1.0f - ovAlpha;
        int curR = (int)(finalColor >> 16 & 0xFF);
        int curG = (int)(finalColor >> 8  & 0xFF);
        int curB = (int)(finalColor       & 0xFF);
        int ovR = (int)(curR * invA + overlayCol.mRed   * ovAlpha);
        int ovG = (int)(curG * invA + overlayCol.mGreen * ovAlpha);
        int ovB = (int)(curB * invA + overlayCol.mBlue  * ovAlpha);
        finalColor = (finalColor & 0xFF000000) |  // preserve alpha
                     ((uint32_t)(std::clamp(ovR, 0, 255)) << 16) |
                     ((uint32_t)(std::clamp(ovG, 0, 255)) << 8)  |
                     ((uint32_t)(std::clamp(ovB, 0, 255)));
    }

    return finalColor;
}

void SpineAnimation::Draw(Sexy::Graphics* g)
{
    if (g == nullptr || mSkeleton == nullptr)
        return;

    if (mSkeleton->drawOrder == nullptr || mSkeleton->slotsCount <= 0)
        return;

    // Pre-compute skeleton color once
    float skelR = mSkeleton->color.r;
    float skelG = mSkeleton->color.g;
    float skelB = mSkeleton->color.b;
    float skelA = mSkeleton->color.a;

    // ============================================================
    //  Unified coordinate transform: Spine Y-up → Game Y-down
    //  ============================================================
    // Spine computes world vertices in Y-up space (positive Y = up).
    // The game engine uses Y-down screen coordinates (positive Y = down).
    // sp*_*computeWorldVertices() outputs vertices that INCLUDE mSkeleton->x/y.
    //
    // Conversion formula (flip Y around skeleton position):
    //   screenX = worldVertX                          (X unchanged)
    //   screenY = 2 * mSkeleton->y - worldVertY       (Y flipped around skelY)
    //
    // This is the SAME transform logic that GetBoneWorldPosition() uses
    // for logical coordinates (bullet spawn points, etc.), ensuring visual
    // and logical positions are always consistent.
    //
    // mRenderOffsetX/Y are added as a final uniform shift to align the
    // skeleton origin with the expected game position (e.g., root bone
    // may not be at the plant's bottom-center where the game expects).
    // ============================================================
    const float skelX = mSkeleton->x;
    const float skelY = mSkeleton->y;

    uint32_t baseColor = 0;
    if (mColorOverride.mRed != 255 || mColorOverride.mGreen != 255 ||
        mColorOverride.mBlue != 255 || mColorOverride.mAlpha != 255) {
        baseColor = ((uint32_t)mColorOverride.mAlpha << 24) |
                    ((uint32_t)mColorOverride.mRed << 16) |
                    ((uint32_t)mColorOverride.mGreen << 8) |
                    (uint32_t)mColorOverride.mBlue;
    }

    const int slotCount = mSkeleton->slotsCount;
    for (int i = 0; i < slotCount; i++) {
        spSlot* slot = mSkeleton->drawOrder[i];
        if (slot == nullptr) continue;

        spAttachment* attachment = slot->attachment;
        if (attachment == nullptr) continue;

        // --- Region attachment ---
        if (attachment->type == SP_ATTACHMENT_REGION) {
            spRegionAttachment* region = (spRegionAttachment*)attachment;
            if (region == nullptr) continue;

            spAtlasRegion* regionData = (spAtlasRegion*)region->region;
            Sexy::Image* tex = nullptr;
            if (regionData != nullptr && regionData->page != nullptr)
                tex = (Sexy::Image*)regionData->page->rendererObject;
            if (tex == nullptr) continue;

            float r = region->color.r * slot->color.r * skelR;
            float gr = region->color.g * slot->color.g * skelG;
            float b = region->color.b * slot->color.b * skelB;
            float a = region->color.a * slot->color.a * skelA;
            uint32_t vertColor = applyVisualEffects(r, gr, b, a, baseColor,
                mExtraAdditiveDraw, mExtraAdditiveColor,
                mExtraOverlayDraw, mExtraOverlayColor);

            float worldVerts[8];
            spRegionAttachment_computeWorldVertices(region, slot, worldVerts, 0, 2);

            // Two triangles forming a quad: (0,1,2) and (0,2,3)
            // Vertex order from Spine: top-left, top-right, bottom-right, bottom-left
            // Apply unified Y-flip around skeleton position + render offset
            Sexy::TriVertex tri[2][3] = {{
                { worldVerts[0] + mRenderOffsetX, 2.0f * skelY - worldVerts[1] + mRenderOffsetY, region->uvs[0], region->uvs[1], vertColor },
                { worldVerts[2] + mRenderOffsetX, 2.0f * skelY - worldVerts[3] + mRenderOffsetY, region->uvs[2], region->uvs[3], vertColor },
                { worldVerts[4] + mRenderOffsetX, 2.0f * skelY - worldVerts[5] + mRenderOffsetY, region->uvs[4], region->uvs[5], vertColor },
            }, {
                { worldVerts[0] + mRenderOffsetX, 2.0f * skelY - worldVerts[1] + mRenderOffsetY, region->uvs[0], region->uvs[1], vertColor },
                { worldVerts[4] + mRenderOffsetX, 2.0f * skelY - worldVerts[5] + mRenderOffsetY, region->uvs[4], region->uvs[5], vertColor },
                { worldVerts[6] + mRenderOffsetX, 2.0f * skelY - worldVerts[7] + mRenderOffsetY, region->uvs[6], region->uvs[7], vertColor },
            }};

            g->DrawTrianglesTex(tex, tri, 2);
        }
        // --- Mesh attachment ---
        else if (attachment->type == SP_ATTACHMENT_MESH ||
                 attachment->type == SP_ATTACHMENT_LINKED_MESH) {
            spMeshAttachment* mesh = (spMeshAttachment*)attachment;
            if (mesh == nullptr) continue;

            spAtlasRegion* regionData = (spAtlasRegion*)mesh->region;
            Sexy::Image* tex = nullptr;
            if (regionData != nullptr && regionData->page != nullptr)
                tex = (Sexy::Image*)regionData->page->rendererObject;
            if (tex == nullptr) continue;

            const int worldVertsLen = mesh->super.worldVerticesLength;
            if (worldVertsLen <= 0) continue;

            float r = mesh->color.r * slot->color.r * skelR;
            float gr = mesh->color.g * slot->color.g * skelG;
            float b = mesh->color.b * slot->color.b * skelB;
            float a = mesh->color.a * slot->color.a * skelA;
            uint32_t vertColor = applyVisualEffects(r, gr, b, a, baseColor,
                mExtraAdditiveDraw, mExtraAdditiveColor,
                mExtraOverlayDraw, mExtraOverlayColor);

            // Reuse pre-allocated buffer (avoid per-frame heap alloc)
            mWorldVertsCache.resize((size_t)worldVertsLen);
            spVertexAttachment_computeWorldVertices(
                &mesh->super, slot, 0, worldVertsLen, mWorldVertsCache.data(), 0, 2);

            const int trianglesCount = mesh->trianglesCount;
            if (trianglesCount <= 0 || (trianglesCount % 3) != 0) continue;
            const int numTris = trianglesCount / 3;

            if (mesh->uvs == nullptr || mesh->triangles == nullptr) continue;

            const int maxIdx = worldVertsLen / 2;

            // Reuse pre-allocated buffer
            mTriBatchCache.resize((size_t)numTris * 3);
            int validTris = 0;
            for (int t = 0; t < numTris; t++) {
                const int i0 = mesh->triangles[t * 3];
                const int i1 = mesh->triangles[t * 3 + 1];
                const int i2 = mesh->triangles[t * 3 + 2];
                if (i0 < 0 || i0 >= maxIdx || i1 < 0 || i1 >= maxIdx || i2 < 0 || i2 >= maxIdx)
                    continue;

                // Apply unified Y-flip around skeleton position + render offset for mesh vertices
                mTriBatchCache[validTris * 3]     = { mWorldVertsCache[i0 * 2] + mRenderOffsetX, 2.0f * skelY - mWorldVertsCache[i0 * 2 + 1] + mRenderOffsetY,
                                                      mesh->uvs[i0 * 2], mesh->uvs[i0 * 2 + 1], vertColor };
                mTriBatchCache[validTris * 3 + 1] = { mWorldVertsCache[i1 * 2] + mRenderOffsetX, 2.0f * skelY - mWorldVertsCache[i1 * 2 + 1] + mRenderOffsetY,
                                                      mesh->uvs[i1 * 2], mesh->uvs[i1 * 2 + 1], vertColor };
                mTriBatchCache[validTris * 3 + 2] = { mWorldVertsCache[i2 * 2] + mRenderOffsetX, 2.0f * skelY - mWorldVertsCache[i2 * 2 + 1] + mRenderOffsetY,
                                                      mesh->uvs[i2 * 2], mesh->uvs[i2 * 2 + 1], vertColor };
                validTris++;
            }

            if (validTris > 0) {
                g->DrawTrianglesTex(tex, (const Sexy::TriVertex(*)[3])mTriBatchCache.data(), validTris);
            }
        }
    }
}

void SpineAnimation::DrawRenderGroup(Sexy::Graphics* g, int theRenderGroup)
{
    (void)theRenderGroup;
    Draw(g);
}
