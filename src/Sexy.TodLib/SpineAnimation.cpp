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
    if (f == nullptr)
        return result;

    // determine file size
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
//  We register custom loader functions so spAtlas_create uses
//  the project's ImageLib instead of direct file I/O.
//  page->rendererObject is set to a Sexy::Image*.
// ============================================================
static void _pvz_spine_createTexture(spAtlasPage* self, const char* path)
{
    ImageLib::Image* loaded = ImageLib::GetImage(path, false);
    if (loaded == nullptr) {
        self->rendererObject = nullptr;
        self->width = 1;
        self->height = 1;
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
    Sexy::MemoryImage* img = (Sexy::MemoryImage*)self->rendererObject;
    delete img;
    self->rendererObject = nullptr;
}

// ============================================================
//  spine-c requires platform callbacks declared in <spine/extension.h>.
//  These must have external (C) linkage and be provided by exactly one
//  translation unit. They delegate to the project's ImageLib / PakInterface.
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
    spAtlas* atlas = spAtlas_create(data, length, dir, nullptr);
    if (atlas == nullptr) return nullptr;

    for (spAtlasPage* page = atlas->pages; page != nullptr; page = page->next) {
        std::string texPath;
        if (dir != nullptr && dir[0] != '\0') {
            texPath = std::string(dir) + "/" + page->name;
        } else {
            texPath = page->name;
        }
        _pvz_spine_createTexture(page, texPath.c_str());
    }
    return atlas;
}

static std::string extractDirectory(const std::string& path)
{
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos)
        return std::string();
    return path.substr(0, pos);
}

// ============================================================
//  Skeleton data cache – share atlas + skeleton-data across
//  multiple SpineAnimation instances that reference the same
//  (jsonPath, atlasPath) pair.
// ============================================================
SpineSkeletonDataCache::CachedData SpineSkeletonDataCache::LoadData(
    const std::string& jsonPath,
    const std::string& atlasPath)
{
    std::string key = jsonPath + "|" + atlasPath;
    auto it = gCache.find(key);
    if (it != gCache.end())
        return it->second;

    // Load atlas from memory (via pvzCreateAtlas which registers our texture loaders)
    std::vector<char> atlasData = readSpineFile(atlasPath);
    CachedData data = { nullptr, nullptr };
    if (atlasData.empty()) {
        gCache[key] = data;
        return data;
    }

    std::string atlasDir = extractDirectory(atlasPath);
    spAtlas* atlas = pvzCreateAtlas(atlasData.data(), (int)atlasData.size() - 1,
                                     atlasDir.empty() ? nullptr : atlasDir.c_str());
    if (atlas == nullptr) {
        gCache[key] = data;
        return data;
    }

    // Load skeleton JSON from memory
    std::vector<char> jsonData = readSpineFile(jsonPath);
    if (jsonData.empty()) {
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
        spAtlas_dispose(atlas);
        if (error) {
            // Silently fail – callers will see nullptr and can log.
        }
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
                "spinedemo/GatlingPea.json", "spinedemo/GatlingPea.atlas", 1.0f));
    }
}

// ============================================================
//  Destructor – dispose spine-c objects.
//  Note: we do NOT dispose atlas/skeletonData because they are
//  shared in the cache.
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
    // mSkeletonData and mAtlas are owned by the cache.
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

    // Lazily populate the animation registry the first time any
    // SpineAnimation is initialized.
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
        }
    }
}

// ============================================================
//  Animation control.
// ============================================================
void SpineAnimation::SetAnimation(const char* theAnimName, bool theLoop)
{
    if (mAnimState == nullptr || mSkeletonData == nullptr)
        return;

    // Try the specified animation; fall back to the first animation in the skeleton
    // data if not found (handles mismatches between JSON content and caller expectations).
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
    if (mAnimState == nullptr) return;
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

    // mFrameDelay is driven by the Reanimation system (Reanimator.cpp sets it
    // proportional to mAnimRate).  Treat mFrameDelay as delta-time in seconds.
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

// ============================================================
//  Transform helpers.
// ============================================================
void SpineAnimation::SetFlipX(bool theFlip)
{
    if (mSkeleton == nullptr) return;
    // spine-c 4.x no longer has flipX/flipY on spSkeleton; emulate via sign of scaleX.
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
//  Walks the skeleton's slots and emits triangle batches using
//  the project's existing Graphics::DrawTrianglesTex API.
// ============================================================
static Sexy::TriVertex makeVert(float x, float y, float u, float v, uint32_t color)
{
    return Sexy::TriVertex(x, y, u, v, color);
}

static uint32_t colorToUInt(float r, float g, float b, float a)
{
    // ARGB layout matching the project's TriVertex.color field.
    uint8_t A = (uint8_t)(a * 255.0f);
    uint8_t R = (uint8_t)(r * 255.0f);
    uint8_t G = (uint8_t)(g * 255.0f);
    uint8_t B = (uint8_t)(b * 255.0f);
    return ((uint32_t)A << 24) | ((uint32_t)R << 16) | ((uint32_t)G << 8) | (uint32_t)B;
}

void SpineAnimation::Draw(Sexy::Graphics* g)
{
    if (mSkeleton == nullptr)
        return;

    // Determine the base color (slot color is multiplied with skeleton color).
    uint32_t baseColor = 0; // 0 means "use the color from Graphics::mColor"
    if (mColorOverride.mRed != 255 || mColorOverride.mGreen != 255 ||
        mColorOverride.mBlue != 255 || mColorOverride.mAlpha != 255) {
        baseColor = ((uint32_t)mColorOverride.mAlpha << 24) |
                    ((uint32_t)mColorOverride.mRed << 16) |
                    ((uint32_t)mColorOverride.mGreen << 8) |
                    (uint32_t)mColorOverride.mBlue;
    }

    float skelR = mSkeleton->color.r;
    float skelG = mSkeleton->color.g;
    float skelB = mSkeleton->color.b;
    float skelA = mSkeleton->color.a;

    // Render each slot in order (draw order is determined by slot order in skeleton data).
    for (int i = 0; i < mSkeleton->slotsCount; i++) {
        spSlot* slot = mSkeleton->slots[i];
        if (slot == nullptr) continue;

        spAttachment* attachment = slot->attachment;
        if (attachment == nullptr) continue;

        // Compute slot color (multiplied with skeleton color).
        float sr = slot->color.r * skelR;
        float sg = slot->color.g * skelG;
        float sb = slot->color.b * skelB;
        float sa = slot->color.a * skelA;
        uint32_t slotColor = baseColor != 0 ? baseColor : colorToUInt(sr, sg, sb, sa);

        // --- Region attachment (simple 4-corner quad). ---
        if (attachment->type == SP_ATTACHMENT_REGION) {
            spRegionAttachment* region = (spRegionAttachment*)attachment;
            spAtlasRegion* regionData = (spAtlasRegion*)region->rendererObject;
            Sexy::Image* tex = nullptr;
            if (regionData != nullptr && regionData->page != nullptr)
                tex = (Sexy::Image*)regionData->page->rendererObject;
            if (tex == nullptr) continue;

            float worldVerts[8];
            spRegionAttachment_computeWorldVertices(region, slot, worldVerts, 0, 2);

            // spine-c stores UVs as (u,v) pairs in region->uvs[8].
            // Vertex order in spine-c is: TL, TR, BR, BL  (counter-clockwise)
            // Build two triangles from the quad.
            Sexy::TriVertex tri[2][3];

            // Triangle 1: TL, TR, BR
            tri[0][0] = makeVert(worldVerts[0], worldVerts[1], region->uvs[0], region->uvs[1], slotColor);
            tri[0][1] = makeVert(worldVerts[2], worldVerts[3], region->uvs[2], region->uvs[3], slotColor);
            tri[0][2] = makeVert(worldVerts[4], worldVerts[5], region->uvs[4], region->uvs[5], slotColor);

            // Triangle 2: TL, BR, BL
            tri[1][0] = makeVert(worldVerts[0], worldVerts[1], region->uvs[0], region->uvs[1], slotColor);
            tri[1][1] = makeVert(worldVerts[4], worldVerts[5], region->uvs[4], region->uvs[5], slotColor);
            tri[1][2] = makeVert(worldVerts[6], worldVerts[7], region->uvs[6], region->uvs[7], slotColor);

            g->DrawTrianglesTex(tex, tri, 2);
        }
        // --- Mesh attachment (arbitrary polygon, possibly with deform). ---
        else if (attachment->type == SP_ATTACHMENT_MESH) {
            spMeshAttachment* mesh = (spMeshAttachment*)attachment;
            spAtlasRegion* regionData = (spAtlasRegion*)mesh->rendererObject;
            Sexy::Image* tex = nullptr;
            if (regionData != nullptr && regionData->page != nullptr)
                tex = (Sexy::Image*)regionData->page->rendererObject;
            if (tex == nullptr) continue;

            int numVerts = mesh->super.worldVerticesLength / 2;
            if (numVerts <= 0) continue;

            std::vector<float> worldVerts((size_t)mesh->super.worldVerticesLength);
            spVertexAttachment_computeWorldVertices(
                &mesh->super, slot, 0, mesh->super.worldVerticesLength,
                worldVerts.data(), 0, 2);

            int numTris = mesh->trianglesCount / 3;
            if (numTris <= 0) continue;

            // Emit triangles using DrawTrianglesTex.
            std::vector<Sexy::TriVertex> triBatch((size_t)numTris * 3);
            for (int t = 0; t < numTris; t++) {
                int i0 = mesh->triangles[t * 3];
                int i1 = mesh->triangles[t * 3 + 1];
                int i2 = mesh->triangles[t * 3 + 2];

                float u0 = mesh->uvs[i0 * 2], v0 = mesh->uvs[i0 * 2 + 1];
                float u1 = mesh->uvs[i1 * 2], v1 = mesh->uvs[i1 * 2 + 1];
                float u2 = mesh->uvs[i2 * 2], v2 = mesh->uvs[i2 * 2 + 1];

                triBatch[t * 3]     = makeVert(worldVerts[i0 * 2], worldVerts[i0 * 2 + 1], u0, v0, slotColor);
                triBatch[t * 3 + 1] = makeVert(worldVerts[i1 * 2], worldVerts[i1 * 2 + 1], u1, v1, slotColor);
                triBatch[t * 3 + 2] = makeVert(worldVerts[i2 * 2], worldVerts[i2 * 2 + 1], u2, v2, slotColor);
            }

            g->DrawTrianglesTex(tex, (const Sexy::TriVertex(*)[3])triBatch.data(), numTris);
        }
        // Linked mesh – delegate to the linked source mesh.
        // In spine-c 4.x, linked mesh attachments are represented as spMeshAttachment
        // with parentMesh pointing to the source mesh (deform) data.
        else if (attachment->type == SP_ATTACHMENT_LINKED_MESH) {
            spMeshAttachment* linked = (spMeshAttachment*)attachment;
            spMeshAttachment* source = linked->parentMesh;
            spMeshAttachment* mesh = (source != nullptr) ? source : linked;
            spAtlasRegion* regionData = (spAtlasRegion*)mesh->region;
            Sexy::Image* tex = nullptr;
            if (regionData != nullptr && regionData->page != nullptr)
                tex = (Sexy::Image*)regionData->page->rendererObject;
            if (tex == nullptr) continue;

            int numVerts = mesh->super.worldVerticesLength / 2;
            if (numVerts <= 0) continue;

            std::vector<float> worldVerts((size_t)mesh->super.worldVerticesLength);
            spVertexAttachment_computeWorldVertices(
                &mesh->super, slot, 0, mesh->super.worldVerticesLength,
                worldVerts.data(), 0, 2);

            int numTris = mesh->trianglesCount / 3;
            if (numTris <= 0) continue;

            std::vector<Sexy::TriVertex> triBatch((size_t)numTris * 3);
            for (int t = 0; t < numTris; t++) {
                int i0 = mesh->triangles[t * 3];
                int i1 = mesh->triangles[t * 3 + 1];
                int i2 = mesh->triangles[t * 3 + 2];

                float u0 = mesh->uvs[i0 * 2], v0 = mesh->uvs[i0 * 2 + 1];
                float u1 = mesh->uvs[i1 * 2], v1 = mesh->uvs[i1 * 2 + 1];
                float u2 = mesh->uvs[i2 * 2], v2 = mesh->uvs[i2 * 2 + 1];

                triBatch[t * 3]     = makeVert(worldVerts[i0 * 2], worldVerts[i0 * 2 + 1], u0, v0, slotColor);
                triBatch[t * 3 + 1] = makeVert(worldVerts[i1 * 2], worldVerts[i1 * 2 + 1], u1, v1, slotColor);
                triBatch[t * 3 + 2] = makeVert(worldVerts[i2 * 2], worldVerts[i2 * 2 + 1], u2, v2, slotColor);
            }

            g->DrawTrianglesTex(tex, (const Sexy::TriVertex(*)[3])triBatch.data(), numTris);
        }
        // Other attachment types (bounding box, path, point, clipping) are non-renderable.
    }
}

void SpineAnimation::DrawRenderGroup(Sexy::Graphics* g, int theRenderGroup)
{
    (void)theRenderGroup;
    Draw(g);
}
