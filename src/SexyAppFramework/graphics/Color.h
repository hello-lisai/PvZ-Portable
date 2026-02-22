#ifndef __COLOR_H__
#define __COLOR_H__

#include "Common.h"

namespace Sexy
{

class Color
{
public:
	int32_t mRed;
	int32_t mGreen;
	int32_t mBlue;
	int32_t mAlpha;

	static Color Black;
	static Color White;

public:
	Color();
	Color(int32_t theColor);
	Color(int32_t theColor, int32_t theAlpha);
	Color(int32_t theRed, int32_t theGreen, int32_t theBlue);
	Color(int32_t theRed, int32_t theGreen, int32_t theBlue, int32_t theAlpha);
	Color(const uchar* theElements);	
	Color(const int32_t* theElements);

	int32_t					GetRed() const;
	int32_t					GetGreen() const;
	int32_t					GetBlue() const;
	int32_t					GetAlpha() const;
	uint32_t				ToInt() const;
	uint32_t				ToGLColor() const;

	int32_t&				operator[](int32_t theIdx);
	int32_t					operator[](int32_t theIdx) const;	
};

bool operator==(const Color& theColor1, const Color& theColor2);
bool operator!=(const Color& theColor1, const Color& theColor2);

}

#endif //__COLOR_H__