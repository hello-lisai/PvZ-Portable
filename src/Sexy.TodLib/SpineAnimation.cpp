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

// ============================================================
//  Debug logging system – writes to both console and a file
//  so we can diagnose crashes after they happen.
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

#define SPINE_LOG(...) do { spineLogOpen(); spineLog(__VA_ARGS__); } while(0)
#define SPINE_LOG_FN() SPINE_LOG("[SPINE] %s", __FUNCTION__)
#define SPINE_ERR(...) do { spineLogOpen(); spineLog("[ERROR] " __VA_ARGS__); } while(0)
#define SPINE_WARN(...) do { spineLogOpen(); spineLog("[WARN] " __VA_ARGS__); } while(0)

// ============================================================
//  Static variables
// ============================================================
std::vector<SpineAnimationParams> SpineAnimation::gSpineAnimArray;
std::map<std::string, SpineSkeletonDataCache::CachedData> SpineSkeletonDataCache::gCache;

// ============================================================
//  Minimal JSON parser for SpineAnimationConfig.json
//  No third-party library dependency.
// ============================================================
static const char* skipWS(const char* p)
{
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
        p++;
    return p;
}

static std::string parseJsonString(const char** pp)
{
    const char* p = *pp;
    p = skipWS(p);
    if (*p != '"') return "";
    p++; // skip opening quote
    std::string result;
    while (*p && *p != '"') {
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
                case '"':  result += '"'; break;
                case '\\': result += '\\'; break;
                case '/':  result += '/'; break;
                case 'b':  result += '\b'; break;
                case 'f':  result += '\f'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                default:   result += *p; break;
            }
        } else {
            result += *p;
        }
        p++;
    }
    if (*p == '"') p++; // skip closing quote
    *pp = p;
    return result;
}

static float parseJsonNumber(const char** pp)
{
    const char* p = *pp;
    p = skipWS(p);
    char* end = nullptr;
    float val = strtof(p, &end);
    *pp = end ? end : p;
    return val;
}

static bool parseJsonObject(const char* data, SpineAnimationParams& out)
{
    // Find the first '{'
    const char* p = strchr(data, '{');
    if (p == nullptr) return false;
    p++;

    while (*p && *p != '}') {
        p = skipWS(p);
        if (*p != '"') { p++; continue; }
        std::string key = parseJsonString(&p);
        p = skipWS(p);
        if (*p != ':') { p++; continue; }
        p++; // skip ':'
        p = skipWS(p);

        if (key == "type") {
            int typeVal = (int)parseJsonNumber(&p);
            out.mType = (SpineAnimationType)typeVal;
        } else if (key == "json") {
            out.mJSONPath = parseJsonString(&p);
        } else if (key == "atlas") {
            out.mAtlasPath = parseJsonString(&p);
        } else if (key == "scale") {
            out.mDefaultScale = parseJsonNumber(&p);
        } else if (key == "offsetX") {
            out.mRenderOffsetX = parseJsonNumber(&p);
        } else if (key == "offsetY") {
            out.mRenderOffsetY = parseJsonNumber(&p);
        } else if (key == "anchorBone") {
            out.mAnchorBone = parseJsonString(&p);
        } else if (key == "bulletTrack") {
            out.mBulletTrack = parseJsonString(&p);
        } else if (key == "cardImage") {
            out.mCardImage = parseJsonString(&p);
        } else {
            // Skip unknown value
            if (*p == '"') parseJsonString(&p);
            else if (*p == '{' || *p == '[') {
                int depth = 1;
                p++;
                while (*p && depth > 0) {
                    if (*p == '{' || *p == '[') depth++;
                    else if (*p == '}' || *p == ']') depth--;
                    p++;
                }
            } else {
                while (*p && *p != ',' && *p != '}') p++;
            }
        }

        p = skipWS(p);
        if (*p == ',') p++; // skip comma
    }
    return true;
}

// Forward declaration for config loader (defined after readSpineFile)
static bool loadConfigFromFile(const char* path, SpineAnimationParams& out);

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
        SPINE_LOG("[readSpineFile] FAILED to open: %s", path.c_str());
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

    SPINE_LOG("[readSpineFile] OK: %s (size=%ld)", path.c_str(), size);
    return result;
}

// ============================================================
//  spine-c texture-loading callbacks.
// ============================================================
static void _pvz_spine_createTexture(spAtlasPage* self, const char* path)
{
    SPINE_LOG("[_pvz_spine_createTexture] path=%s", path ? path : "(null)");

    if (path == nullptr) {
        self->rendererObject = nullptr;
        self->width = 1;
        self->height = 1;
        SPINE_LOG("[_pvz_spine_createTexture] ERROR: null path");
        return;
    }

    ImageLib::Image* loaded = ImageLib::GetImage(path, false);
    if (loaded == nullptr) {
        self->rendererObject = nullptr;
        self->width = 1;
        self->height = 1;
        SPINE_LOG("[_pvz_spine_createTexture] ERROR: ImageLib::GetImage failed for %s", path);
        return;
    }

    SPINE_LOG("[_pvz_spine_createTexture] Image loaded: %dx%d", loaded->GetWidth(), loaded->GetHeight());

    Sexy::MemoryImage* img = new Sexy::MemoryImage();
    img->mFilePath = path;
    img->SetBits(loaded->GetBits(), loaded->GetWidth(), loaded->GetHeight(), true);
    delete loaded;
    self->rendererObject = img;
    self->width = img->mWidth;
    self->height = img->mHeight;

    SPINE_LOG("[_pvz_spine_createTexture] SUCCESS: %s -> %dx%d", path, img->mWidth, img->mHeight);
}

static void _pvz_spine_disposeTexture(spAtlasPage* self)
{
    SPINE_LOG("[_pvz_spine_disposeTexture] page=%p rendererObject=%p", (void*)self, self ? self->rendererObject : nullptr);
    if (self == nullptr) return;
    Sexy::MemoryImage* img = (Sexy::MemoryImage*)self->rendererObject;
    if (img) {
        SPINE_LOG("[_pvz_spine_disposeTexture] deleting MemoryImage %p", (void*)img);
        delete img;
    }
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

// ============================================================
//  Config file loader — defined here so readSpineFile() is visible
// ============================================================
static bool loadConfigFromFile(const char* path, SpineAnimationParams& out)
{
    std::vector<char> data = readSpineFile(path);
    if (data.empty()) return false;
    SPINE_LOG("[loadConfigFromFile] Loaded config from: %s (size=%zu)", path, data.size());
    return parseJsonObject(data.data(), out);
}

static spAtlas* pvzCreateAtlas(const char* data, int length, const char* dir)
{
    SPINE_LOG("[pvzCreateAtlas] data=%p length=%d dir=%s", (const void*)data, length, dir ? dir : "(null)");
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
    SPINE_LOG("[LoadData] json=%s atlas=%s", jsonPath.c_str(), atlasPath.c_str());

    std::string key = jsonPath + "|" + atlasPath;
    auto it = gCache.find(key);
    if (it != gCache.end()) {
        SPINE_LOG("[LoadData] CACHE HIT");
        return it->second;
    }

    CachedData data = { nullptr, nullptr };

    std::vector<char> atlasData = readSpineFile(atlasPath);
    if (atlasData.empty()) {
        SPINE_LOG("[LoadData] FAILED: atlas file empty or missing");
        gCache[key] = data;
        return data;
    }

    std::string atlasDir = extractDirectory(atlasPath);
    spAtlas* atlas = pvzCreateAtlas(atlasData.data(), (int)atlasData.size() - 1,
                                     atlasDir.empty() ? nullptr : atlasDir.c_str());
    if (atlas == nullptr) {
        SPINE_LOG("[LoadData] FAILED: spAtlas_create returned null");
        gCache[key] = data;
        return data;
    }

    SPINE_LOG("[LoadData] Atlas created OK, pages=%p regions=%p", (void*)atlas->pages, (void*)atlas->regions);

    std::vector<char> jsonData = readSpineFile(jsonPath);
    if (jsonData.empty()) {
        SPINE_LOG("[LoadData] FAILED: json file empty or missing");
        spAtlas_dispose(atlas);
        gCache[key] = data;
        return data;
    }

    spSkeletonJson* json = spSkeletonJson_create(atlas);
    if (json == nullptr) {
        SPINE_LOG("[LoadData] FAILED: spSkeletonJson_create returned null");
        spAtlas_dispose(atlas);
        gCache[key] = data;
        return data;
    }
    json->scale = 1.0f;

    spSkeletonData* skeletonData = spSkeletonJson_readSkeletonData(json, jsonData.data());
    const char* error = json->error;
    spSkeletonJson_dispose(json);

    if (skeletonData == nullptr) {
        SPINE_LOG("[LoadData] FAILED: spSkeletonJson_readSkeletonData error=%s", error ? error : "(null)");
        spAtlas_dispose(atlas);
        gCache[key] = data;
        return data;
    }

    SPINE_LOG("[LoadData] SUCCESS: skeletonData=%p bones=%d slots=%d animations=%d",
              (void*)skeletonData, skeletonData->bonesCount, skeletonData->slotsCount, skeletonData->animationsCount);

    data.atlas = atlas;
    data.skeletonData = skeletonData;
    gCache[key] = data;
    return data;
}

void SpineSkeletonDataCache::ClearAll()
{
    SPINE_LOG("[ClearAll] clearing %zu entries", gCache.size());
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
    SPINE_LOG("[InitializeSpineAnimArray]");
    if (gSpineAnimArray.empty()) {
        SpineAnimationParams params;

        // Try loading from external config file first
        bool loaded = loadConfigFromFile("spinedemo/SpineAnimationConfig.json", params);

        if (loaded && !params.mJSONPath.empty()) {
            // Apply defaults for any missing fields
            if (params.mAnchorBone.empty()) params.mAnchorBone = "bone2";
            if (params.mRenderOffsetY == 0.0f) params.mRenderOffsetY = 50.0f;
            if (params.mBulletTrack.empty()) params.mBulletTrack = "bone10";
            if (params.mCardImage.empty()) params.mCardImage = "spinedemo/123.png";
            if (params.mDefaultScale == 0.0f) params.mDefaultScale = 1.0f;

            gSpineAnimArray.push_back(params);
            SPINE_LOG("[InitializeSpineAnimArray] Loaded from config file: json=%s atlas=%s scale=%f anchor=%s card=%s",
                      params.mJSONPath.c_str(), params.mAtlasPath.c_str(),
                      params.mDefaultScale, params.mAnchorBone.c_str(),
                      params.mCardImage.c_str());
        } else {
            // Fallback to built-in defaults
            gSpineAnimArray.push_back(
                SpineAnimationParams(SpineAnimationType::SPINE_PEASHOOTER,
                    "spinedemo/GatlingPea.json", "spinedemo/GatlingPea.atlas",
                    1.0f, 0.0f, 50.0f, "bone2", "bone10", "spinedemo/123.png"));
            SPINE_WARN("[InitializeSpineAnimArray] Config file not found, using built-in defaults");
        }

        SPINE_LOG("[InitializeSpineAnimArray] Registered %zu animation(s)", gSpineAnimArray.size());
    }
}

// ============================================================
//  Destructor
// ============================================================
SpineAnimation::~SpineAnimation()
{
    SPINE_LOG("[~SpineAnimation] this=%p", (void*)this);
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
    SPINE_LOG("[SpineAnimationInitialize] this=%p x=%f y=%f type=%d", (void*)this, theX, theY, (int)theSpineType);

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
        SPINE_LOG("[SpineAnimationInitialize] Loading: json=%s atlas=%s",
                  mSpineParams->mJSONPath.c_str(), mSpineParams->mAtlasPath.c_str());

        SpineSkeletonDataCache::CachedData cached =
            SpineSkeletonDataCache::LoadData(mSpineParams->mJSONPath, mSpineParams->mAtlasPath);
        mAtlas = cached.atlas;
        mSkeletonData = cached.skeletonData;

        SPINE_LOG("[SpineAnimationInitialize] cached: atlas=%p skeletonData=%p", (void*)mAtlas, (void*)mSkeletonData);

        if (mSkeletonData != nullptr) {
            mSkeleton = spSkeleton_create(mSkeletonData);
            mAnimStateData = spAnimationStateData_create(mSkeletonData);
            mAnimState = spAnimationState_create(mAnimStateData);

            SPINE_LOG("[SpineAnimationInitialize] skeleton=%p animStateData=%p animState=%p",
                      (void*)mSkeleton, (void*)mAnimStateData, (void*)mAnimState);

            if (mSkeleton != nullptr) {
                mSkeleton->x = theX;
                mSkeleton->y = theY;
                mSkeleton->scaleX = mSpineParams->mDefaultScale;
                mSkeleton->scaleY = mSpineParams->mDefaultScale;
                spSkeleton_setToSetupPose(mSkeleton);
                spSkeleton_updateWorldTransform(mSkeleton, (spPhysics)0);

                // Copy render offsets from params
                mRenderOffsetX = mSpineParams->mRenderOffsetX;
                mRenderOffsetY = mSpineParams->mRenderOffsetY;

                // Cache anchor bone world position
                mAnchorX = 0.0f;
                mAnchorY = 0.0f;
                if (!mSpineParams->mAnchorBone.empty()) {
                    spBone* anchorBone = spSkeleton_findBone(mSkeleton, mSpineParams->mAnchorBone.c_str());
                    if (anchorBone != nullptr) {
                        mAnchorX = anchorBone->worldX;
                        mAnchorY = anchorBone->worldY;
                        SPINE_LOG("[SpineAnimationInitialize] Anchor bone '%s' at (%f, %f)",
                                  mSpineParams->mAnchorBone.c_str(), mAnchorX, mAnchorY);
                    } else {
                        SPINE_WARN("[SpineAnimationInitialize] Anchor bone '%s' not found",
                                   mSpineParams->mAnchorBone.c_str());
                    }
                }

                // Pre-allocate vertex buffers for Draw()
                // Iterate slot data (not runtime slots) to find max vertices
                int maxWorldVerts = 0;
                for (int s = 0; s < mSkeletonData->slotsCount; s++) {
                    spSlotData* slotData = mSkeletonData->slots[s];
                    if (slotData == nullptr || slotData->attachmentName == nullptr)
                        continue;
                    // Look up attachment from default skin
                    spAttachment* att = nullptr;
                    if (mSkeletonData->defaultSkin != nullptr) {
                        att = spSkin_getAttachment(mSkeletonData->defaultSkin, s, slotData->attachmentName);
                    }
                    if (att == nullptr && mSkeletonData->skinsCount > 0 && mSkeletonData->skins[0] != nullptr) {
                        att = spSkin_getAttachment(mSkeletonData->skins[0], s, slotData->attachmentName);
                    }
                    if (att == nullptr) continue;
                    if (att->type == SP_ATTACHMENT_REGION)
                        maxWorldVerts = (maxWorldVerts > 8) ? maxWorldVerts : 8;
                    else if (att->type == SP_ATTACHMENT_MESH || att->type == SP_ATTACHMENT_LINKED_MESH) {
                        spMeshAttachment* meshAtt = SUB_CAST(spMeshAttachment, att);
                        if (meshAtt && meshAtt->super.worldVerticesLength > maxWorldVerts)
                            maxWorldVerts = meshAtt->super.worldVerticesLength;
                    }
                }
                mWorldVertsCache.resize((size_t)(maxWorldVerts + 1));
                // Reserve reasonable capacity for triangle batch (will grow if needed)
                mTriBatchCache.reserve(256);

                SPINE_LOG("[SpineAnimationInitialize] Skeleton initialized at (%f, %f) scale=%f",
                          theX, theY, mSpineParams->mDefaultScale);
            }
        } else {
            SPINE_LOG("[SpineAnimationInitialize] FAILED: skeletonData is null");
        }
    } else {
        SPINE_LOG("[SpineAnimationInitialize] FAILED: invalid spine type %d (array size=%zu)",
                  (int)theSpineType, gSpineAnimArray.size());
    }
}

// ============================================================
//  Animation control.
// ============================================================
void SpineAnimation::SetAnimation(const char* theAnimName, bool theLoop)
{
    SPINE_LOG("[SetAnimation] name=%s loop=%d", theAnimName ? theAnimName : "(null)", theLoop);
    if (mAnimState == nullptr || mSkeletonData == nullptr) {
        SPINE_LOG("[SetAnimation] FAILED: animState=%p skeletonData=%p", (void*)mAnimState, (void*)mSkeletonData);
        return;
    }

    if (spSkeletonData_findAnimation(mSkeletonData, theAnimName) != nullptr) {
        spAnimationState_setAnimationByName(mAnimState, 0, theAnimName, theLoop ? 1 : 0);
        SPINE_LOG("[SetAnimation] Set animation: %s", theAnimName);
    } else if (mSkeletonData->animationsCount > 0) {
        spAnimation* first = mSkeletonData->animations[0];
        spAnimationState_setAnimation(mAnimState, 0, first, theLoop ? 1 : 0);
        SPINE_LOG("[SetAnimation] Fallback to first animation: %s", first->name);
    } else {
        SPINE_LOG("[SetAnimation] FAILED: no animations found");
    }

    if (mSkeleton != nullptr)
        spSkeleton_updateWorldTransform(mSkeleton, (spPhysics)0);
}

void SpineAnimation::AddAnimation(const char* theAnimName, bool theLoop, float theDelay)
{
    SPINE_LOG("[AddAnimation] name=%s loop=%d delay=%f", theAnimName ? theAnimName : "(null)", theLoop, theDelay);
    if (mAnimState == nullptr) {
        SPINE_LOG("[AddAnimation] FAILED: animState is null");
        return;
    }
    spAnimationState_addAnimationByName(mAnimState, 0, theAnimName, theLoop ? 1 : 0, theDelay);
}

void SpineAnimation::SetTimeScale(float theScale)
{
    SPINE_LOG("[SetTimeScale] %f", theScale);
    if (mAnimState == nullptr) return;
    mAnimState->timeScale = theScale;
}

// ============================================================
//  Per-frame update.
// ============================================================
void SpineAnimation::Update()
{
    if (mAnimState == nullptr || mSkeleton == nullptr) {
        if (mAnimFrame % 60 == 0) {  // log sparingly
            SPINE_LOG("[Update] SKIPPED: animState=%p skeleton=%p", (void*)mAnimState, (void*)mSkeleton);
        }
        return;
    }

    float dt = mFrameDelay;
    if (dt <= 0.0f) dt = 1.0f / 60.0f;

    SPINE_LOG("[Update] dt=%f frame=%d", dt, mAnimFrame);

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
    float magnitude = mSkeleton->scaleX < 0 ? -mSkeleton->scaleX : mSkeleton->scaleX;
    if (magnitude == 0.0f) magnitude = 1.0f;
    mSkeleton->scaleX = theFlip ? -magnitude : magnitude;
    SPINE_LOG("[SetFlipX] flip=%d scaleX=%f", theFlip, mSkeleton->scaleX);
}

void SpineAnimation::SetFlipY(bool theFlip)
{
    if (mSkeleton == nullptr) return;
    float magnitude = mSkeleton->scaleY < 0 ? -mSkeleton->scaleY : mSkeleton->scaleY;
    if (magnitude == 0.0f) magnitude = 1.0f;
    mSkeleton->scaleY = theFlip ? -magnitude : magnitude;
    SPINE_LOG("[SetFlipY] flip=%d scaleY=%f", theFlip, mSkeleton->scaleY);
}

void SpineAnimation::SetScale(float theScale)
{
    SPINE_LOG("[SetScale] %f", theScale);
    if (mSkeleton == nullptr) return;
    mSkeleton->scaleX = theScale;
    mSkeleton->scaleY = theScale;
}

void SpineAnimation::OverrideScale(float theScaleX, float theScaleY)
{
    if (mSkeleton == nullptr) return;
    float mult = (mSpineParams != nullptr) ? mSpineParams->mDefaultScale : 1.0f;
    mSkeleton->scaleX = theScaleX * mult;
    mSkeleton->scaleY = theScaleY * mult;
    SPINE_LOG("[OverrideScale] (%f, %f) * %f -> (%f, %f)",
              theScaleX, theScaleY, mult, mSkeleton->scaleX, mSkeleton->scaleY);
}

bool SpineAnimation::GetBoneWorldPosition(const char* boneName, float* outX, float* outY)
{
    if (mSkeleton == nullptr || boneName == nullptr || outX == nullptr || outY == nullptr) {
        SPINE_ERR("[GetBoneWorldPosition] Invalid args: skeleton=%p name=%p outX=%p outY=%p",
                  (void*)mSkeleton, (const void*)boneName, (void*)outX, (void*)outY);
        return false;
    }

    spBone* bone = spSkeleton_findBone(mSkeleton, boneName);
    if (bone == nullptr) {
        SPINE_WARN("[GetBoneWorldPosition] Bone '%s' not found", boneName);
        return false;
    }

    // Return offset relative to anchor in game coordinate system (Y-down) + renderOffset
    *outX = (bone->worldX - mAnchorX) + mRenderOffsetX;
    *outY = -(bone->worldY - mAnchorY) + mRenderOffsetY;

    SPINE_LOG("[GetBoneWorldPosition] '%s' -> (%f, %f)", boneName, *outX, *outY);
    return true;
}

void SpineAnimation::SetPosition(float theX, float theY)
{
    SPINE_LOG("[SetPosition] (%f, %f)", theX, theY);
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
// ============================================================
static Sexy::TriVertex makeVert(float x, float y, float u, float v, uint32_t color)
{
    return Sexy::TriVertex(x, y, u, v, color);
}

static uint32_t colorToUInt(float r, float g, float b, float a)
{
    uint8_t A = (uint8_t)(a * 255.0f);
    uint8_t R = (uint8_t)(r * 255.0f);
    uint8_t G = (uint8_t)(g * 255.0f);
    uint8_t B = (uint8_t)(b * 255.0f);
    return ((uint32_t)A << 24) | ((uint32_t)R << 16) | ((uint32_t)G << 8) | (uint32_t)B;
}

// Helper: apply additive/overlay visual effects after main draw pass
static void applyVisualEffects(Sexy::Graphics* g,
                                const SpineAnimation* anim,
                                Sexy::Image* tex,
                                const Sexy::TriVertex tri[][3],
                                int triCount)
{
    // Additive draw for gnawing flash effect
    if (anim->mExtraAdditiveDraw && tex != nullptr && triCount > 0) {
        Sexy::Color prevColor = g->GetColor();
        int prevMode = g->GetDrawMode();
        g->SetColor(anim->mExtraAdditiveColor);
        g->SetDrawMode(Sexy::Graphics::DRAWMODE_ADDITIVE);
        g->DrawTrianglesTex(tex, tri, triCount);
        g->SetDrawMode(prevMode);
        g->SetColor(prevColor);
    }

    // Overlay draw for hypnosis glow effect
    if (anim->mExtraOverlayDraw && tex != nullptr && triCount > 0) {
        Sexy::Color prevColor = g->GetColor();
        int prevMode = g->GetDrawMode();
        g->SetColor(anim->mExtraOverlayColor);
        g->SetDrawMode(Sexy::Graphics::DRAWMODE_NORMAL);
        g->DrawTrianglesTex(tex, tri, triCount);
        g->SetDrawMode(prevMode);
        g->SetColor(prevColor);
    }
}

void SpineAnimation::Draw(Sexy::Graphics* g)
{
    if (g == nullptr || mSkeleton == nullptr) {
        SPINE_LOG("[Draw] EARLY EXIT: g=%p skeleton=%p", (void*)g, (void*)mSkeleton);
        return;
    }

    if (mSkeleton->drawOrder == nullptr || mSkeleton->slotsCount <= 0) {
        SPINE_LOG("[Draw] EARLY EXIT: drawOrder=%p slotsCount=%d",
                  (void*)mSkeleton->drawOrder, mSkeleton->slotsCount);
        return;
    }

    SPINE_LOG("[Draw] BEGIN skeleton=%p slots=%d", (void*)mSkeleton, mSkeleton->slotsCount);

    uint32_t baseColor = 0;
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

    // Anchor-based coordinate transformation:
    // screenX = worldVertX - anchorX + renderOffsetX
    // screenY = 2*skelY - worldVertY + anchorY + renderOffsetY
    const float skelY = mSkeleton->y;

    int drawnSlots = 0;
    int skippedSlots = 0;

    const int slotCount = mSkeleton->slotsCount;
    for (int i = 0; i < slotCount; i++) {
        spSlot* slot = mSkeleton->drawOrder[i];
        if (slot == nullptr) {
            skippedSlots++;
            continue;
        }

        spAttachment* attachment = slot->attachment;
        if (attachment == nullptr) {
            skippedSlots++;
            continue;
        }

        // --- Region attachment ---
        if (attachment->type == SP_ATTACHMENT_REGION) {
            spRegionAttachment* region = (spRegionAttachment*)attachment;
            if (region == nullptr) {
                skippedSlots++;
                continue;
            }

            spAtlasRegion* regionData = (spAtlasRegion*)region->region;
            Sexy::Image* tex = nullptr;
            if (regionData != nullptr && regionData->page != nullptr)
                tex = (Sexy::Image*)regionData->page->rendererObject;
            if (tex == nullptr) {
                SPINE_LOG("[Draw] slot=%d region: NO TEXTURE", i);
                skippedSlots++;
                continue;
            }

            float r = region->color.r * slot->color.r * skelR;
            float gC = region->color.g * slot->color.g * skelG;
            float b = region->color.b * slot->color.b * skelB;
            float a = region->color.a * slot->color.a * skelA;
            uint32_t vertColor = baseColor != 0 ? baseColor : colorToUInt(r, gC, b, a);

            float worldVerts[8];
            spRegionAttachment_computeWorldVertices(region, slot, worldVerts, 0, 2);

            Sexy::TriVertex tri[2][3];

            // Anchor-based transform: screenX = worldVertX - anchorX + renderOffsetX
            tri[0][0] = makeVert(worldVerts[0] - mAnchorX + mRenderOffsetX,
                                  2.0f * skelY - worldVerts[1] + mAnchorY + mRenderOffsetY,
                                  region->uvs[0], 1.0f - region->uvs[1], vertColor);
            tri[0][1] = makeVert(worldVerts[2] - mAnchorX + mRenderOffsetX,
                                  2.0f * skelY - worldVerts[3] + mAnchorY + mRenderOffsetY,
                                  region->uvs[2], 1.0f - region->uvs[3], vertColor);
            tri[0][2] = makeVert(worldVerts[4] - mAnchorX + mRenderOffsetX,
                                  2.0f * skelY - worldVerts[5] + mAnchorY + mRenderOffsetY,
                                  region->uvs[4], 1.0f - region->uvs[5], vertColor);

            tri[1][0] = makeVert(worldVerts[0] - mAnchorX + mRenderOffsetX,
                                  2.0f * skelY - worldVerts[1] + mAnchorY + mRenderOffsetY,
                                  region->uvs[0], 1.0f - region->uvs[1], vertColor);
            tri[1][1] = makeVert(worldVerts[4] - mAnchorX + mRenderOffsetX,
                                  2.0f * skelY - worldVerts[5] + mAnchorY + mRenderOffsetY,
                                  region->uvs[4], 1.0f - region->uvs[5], vertColor);
            tri[1][2] = makeVert(worldVerts[6] - mAnchorX + mRenderOffsetX,
                                  2.0f * skelY - worldVerts[7] + mAnchorY + mRenderOffsetY,
                                  region->uvs[6], 1.0f - region->uvs[7], vertColor);

            SPINE_LOG("[Draw] slot=%d region: verts=(%.1f,%.1f)-(%.1f,%.1f)-(%.1f,%.1f)-(%.1f,%.1f)",
                      i, worldVerts[0], worldVerts[1], worldVerts[2], worldVerts[3],
                      worldVerts[4], worldVerts[5], worldVerts[6], worldVerts[7]);

            g->DrawTrianglesTex(tex, tri, 2);
            applyVisualEffects(g, this, tex, tri, 2);
            drawnSlots++;
        }
        // --- Mesh attachment ---
        else if (attachment->type == SP_ATTACHMENT_MESH ||
                 attachment->type == SP_ATTACHMENT_LINKED_MESH) {
            spMeshAttachment* mesh = (spMeshAttachment*)attachment;
            if (mesh == nullptr) {
                skippedSlots++;
                continue;
            }

            spAtlasRegion* regionData = (spAtlasRegion*)mesh->region;
            Sexy::Image* tex = nullptr;
            if (regionData != nullptr && regionData->page != nullptr)
                tex = (Sexy::Image*)regionData->page->rendererObject;
            if (tex == nullptr) {
                SPINE_LOG("[Draw] slot=%d mesh: NO TEXTURE", i);
                skippedSlots++;
                continue;
            }

            const int worldVertsLen = mesh->super.worldVerticesLength;
            if (worldVertsLen <= 0) {
                skippedSlots++;
                continue;
            }

            float r = mesh->color.r * slot->color.r * skelR;
            float gC = mesh->color.g * slot->color.g * skelG;
            float b = mesh->color.b * slot->color.b * skelB;
            float a = mesh->color.a * slot->color.a * skelA;
            uint32_t vertColor = baseColor != 0 ? baseColor : colorToUInt(r, gC, b, a);

            std::vector<float> worldVerts((size_t)worldVertsLen);
            spVertexAttachment_computeWorldVertices(
                &mesh->super, slot, 0, worldVertsLen, worldVerts.data(), 0, 2);

            const int trianglesCount = mesh->trianglesCount;
            if (trianglesCount <= 0 || (trianglesCount % 3) != 0) {
                skippedSlots++;
                continue;
            }
            const int numTris = trianglesCount / 3;

            if (mesh->uvs == nullptr || mesh->triangles == nullptr) {
                skippedSlots++;
                continue;
            }

            const int maxIdx = worldVertsLen / 2;

            std::vector<Sexy::TriVertex> triBatch((size_t)numTris * 3);
            int validTris = 0;
            for (int t = 0; t < numTris; t++) {
                const int i0 = mesh->triangles[t * 3];
                const int i1 = mesh->triangles[t * 3 + 1];
                const int i2 = mesh->triangles[t * 3 + 2];
                if (i0 < 0 || i0 >= maxIdx || i1 < 0 || i1 >= maxIdx || i2 < 0 || i2 >= maxIdx) {
                    continue;
                }

                const float u0 = mesh->uvs[i0 * 2], v0 = mesh->uvs[i0 * 2 + 1];
                const float u1 = mesh->uvs[i1 * 2], v1 = mesh->uvs[i1 * 2 + 1];
                const float u2 = mesh->uvs[i2 * 2], v2 = mesh->uvs[i2 * 2 + 1];

                triBatch[t * 3]     = makeVert(worldVerts[i0 * 2] - mAnchorX + mRenderOffsetX,
                                                  2.0f * skelY - worldVerts[i0 * 2 + 1] + mAnchorY + mRenderOffsetY,
                                                  u0, 1.0f - v0, vertColor);
                triBatch[t * 3 + 1] = makeVert(worldVerts[i1 * 2] - mAnchorX + mRenderOffsetX,
                                                  2.0f * skelY - worldVerts[i1 * 2 + 1] + mAnchorY + mRenderOffsetY,
                                                  u1, 1.0f - v1, vertColor);
                triBatch[t * 3 + 2] = makeVert(worldVerts[i2 * 2] - mAnchorX + mRenderOffsetX,
                                                  2.0f * skelY - worldVerts[i2 * 2 + 1] + mAnchorY + mRenderOffsetY,
                                                  u2, 1.0f - v2, vertColor);
                validTris++;
            }

            if (validTris > 0) {
                SPINE_LOG("[Draw] slot=%d mesh: tris=%d/%d", i, validTris, numTris);
                g->DrawTrianglesTex(tex, (const Sexy::TriVertex(*)[3])triBatch.data(), numTris);
                applyVisualEffects(g, this, tex, (const Sexy::TriVertex(*)[3])triBatch.data(), numTris);
                drawnSlots++;
            } else {
                skippedSlots++;
            }
        }
        else {
            skippedSlots++;
        }
    }

    SPINE_LOG("[Draw] END drawn=%d skipped=%d", drawnSlots, skippedSlots);
}

void SpineAnimation::DrawRenderGroup(Sexy::Graphics* g, int theRenderGroup)
{
    (void)theRenderGroup;
    Draw(g);
}
