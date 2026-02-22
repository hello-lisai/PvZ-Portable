#pragma once

#include <cstdint>

#include "../ConstEnums.h"
#include "../SexyAppFramework/graphics/Graphics.h"

using namespace Sexy;

class LawnApp;
class Board;

class GameObject
{
public:
	LawnApp*                        mApp;
	Board*                          mBoard;
    int32_t                         mX;
    int32_t                         mY;
    int32_t                         mWidth;
    int32_t                         mHeight;
    bool                            mVisible;
    int32_t                         mRow;
    int32_t                         mRenderOrder;

public:
    /*inline*/                      GameObject();
    /*inline*/ bool                 BeginDraw(Graphics* g);
    /*inline*/ void                 EndDraw(Graphics* g);
    /*inline*/ void                 MakeParentGraphicsFrame(Graphics* g);
};
