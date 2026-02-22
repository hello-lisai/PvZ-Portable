#ifndef __LAWNMOWER_H__
#define __LAWNMOWER_H__

#include <cstdint>
#include "../ConstEnums.h"
#include "misc/Rect.h"

class LawnApp;
class Board;
class Zombie;
namespace Sexy
{
    class Graphics;
};
using namespace Sexy;

class LawnMower
{
public:
    LawnApp*            mApp;                   //+0x0
    Board*              mBoard;                 //+0x4
    float               mPosX;                  //+0x8
    float               mPosY;                  //+0xC
    int32_t             mRenderOrder;           //+0x10
    int32_t             mRow;                   //+0x14
    int32_t             mAnimTicksPerFrame;     //+0x18
    ReanimationID       mReanimID;              //+0x1C
    int32_t             mChompCounter;          //+0x20
    int32_t             mRollingInCounter;      //+0x24
    int32_t             mSquishedCounter;       //+0x28
    LawnMowerState      mMowerState;            //+0x2C
    bool                mDead;                  //+0x30
    bool                mVisible;               //+0x31
    LawnMowerType       mMowerType;             //+0x34
    float               mAltitude;              //+0x38
    MowerHeight         mMowerHeight;           //+0x3C
    int32_t             mLastPortalX;           //+0x40

public:
    void                LawnMowerInitialize(int theRow);
    void                StartMower();
    void                Update();
    void                Draw(Graphics* g);
    void                Die();
    Rect                GetLawnMowerAttackRect();
    void                UpdatePool();
    void                MowZombie(Zombie* theZombie);
    void                SquishMower();
    /*inline*/ void     EnableSuperMower(bool theEnable);
};

#endif
