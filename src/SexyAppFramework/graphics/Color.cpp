#include "Color.h"

using namespace Sexy;

Color Color::Black(0, 0, 0);
Color Color::White(255, 255, 255);

Color::Color() :
	mRed(0),
	mGreen(0),
	mBlue(0),
	mAlpha(255)
{
}

Color::Color(int32_t theColor) :
	mRed((theColor >> 16) & 0xFF),
	mGreen((theColor >> 8) & 0xFF),
	mBlue(theColor & 0xFF),
	mAlpha((theColor >> 24) & 0xFF)
{
	if (mAlpha == 0)
		mAlpha = 0xff;
}

Color::Color(int32_t theColor, int32_t theAlpha) :
	mRed((theColor >> 16) & 0xFF),
	mGreen((theColor >> 8) & 0xFF),
	mBlue(theColor & 0xFF),
	mAlpha(theAlpha)
{
}

Color::Color(int32_t theRed, int32_t theGreen, int32_t theBlue) :
	mRed(theRed),
	mGreen(theGreen),
	mBlue(theBlue),
	mAlpha(0xFF)
{
}

Color::Color(int32_t theRed, int32_t theGreen, int32_t theBlue, int32_t theAlpha) :
	mRed(theRed),
	mGreen(theGreen),
	mBlue(theBlue),
	mAlpha(theAlpha)
{
}

Color::Color(const uchar* theElements) :
	mRed(theElements[0]),
	mGreen(theElements[1]),
	mBlue(theElements[2]),
	mAlpha(0xFF)
{
}

Color::Color(const int32_t* theElements) :
	mRed(theElements[0]),
	mGreen(theElements[1]),
	mBlue(theElements[2]),
	mAlpha(0xFF)
{
}

int32_t	Color::GetRed() const
{
	return mRed;	
}

int32_t Color::GetGreen() const
{
	return mGreen;
}

int32_t	Color::GetBlue() const
{
	return mBlue;
}

int32_t	Color::GetAlpha() const
{
	return mAlpha;
}

int32_t& Color::operator[](int32_t theIdx)
{
	static int32_t aJunk = 0;

	switch (theIdx)
	{
	case 0:
		return mRed;
	case 1:
		return mGreen;
	case 2:
		return mBlue;
	case 3:
		return mAlpha;
	default:
		return aJunk;
	}
}

int32_t Color::operator[](int32_t theIdx) const
{
	switch (theIdx)
	{
	case 0:
		return mRed;
	case 1:
		return mGreen;
	case 2:
		return mBlue;
	case 3:
		return mAlpha;
	default:
		return 0;
	}
}

// Returns standard ARGB format (0xAARRGGBB)
uint32_t Color::ToInt() const
{
	return
		(static_cast<uint32_t>(mAlpha) << 24) |
		(static_cast<uint32_t>(mRed) << 16) |
		(static_cast<uint32_t>(mGreen) << 8) |
		(static_cast<uint32_t>(mBlue));
}

// For GL_RGBA/GL_UNSIGNED_BYTE: RGBA in memory <=> little-endian's 0xAABBGGRR
uint32_t Color::ToGLColor() const
{
	uint32_t aGLColor = (static_cast<uint32_t>(mAlpha) << 24) |
						(static_cast<uint32_t>(mBlue) << 16) |
						(static_cast<uint32_t>(mGreen) << 8) |
						(static_cast<uint32_t>(mRed));
	return ToLE32(aGLColor);
}

bool Sexy::operator==(const Color& theColor1, const Color& theColor2)
{
	return 
		(theColor1.mRed == theColor2.mRed) &&
		(theColor1.mGreen == theColor2.mGreen) &&
		(theColor1.mBlue == theColor2.mBlue) && 
		(theColor1.mAlpha == theColor2.mAlpha);
}

bool Sexy::operator!=(const Color& theColor1, const Color& theColor2)
{
	return 
		(theColor1.mRed != theColor2.mRed) ||
		(theColor1.mGreen != theColor2.mGreen) ||
		(theColor1.mBlue != theColor2.mBlue) ||
		(theColor1.mAlpha != theColor2.mAlpha);
}
