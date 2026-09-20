// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef ENGINE_CLIENT_GLYPH_OUTLINE_H
#define ENGINE_CLIENT_GLYPH_OUTLINE_H

#include <base/dbg.h>
#include <base/vmath.h>

#include <algorithm>
#include <array>

// 字形描边半径由字号决定，当前最大为 4；输入输出使用不同缓冲。
inline void QmGrowGlyphOutline(const unsigned char *pIn, unsigned char *pOut, int Width, int Height, int Radius)
{
	dbg_assert(Radius >= 0 && Radius <= 4, "unsupported glyph outline radius");
	std::array<float, 81> aWeights;
	const int Diameter = Radius * 2 + 1;
	for(int dy = -Radius; dy <= Radius; ++dy)
		for(int dx = -Radius; dx <= Radius; ++dx)
			aWeights[(dy + Radius) * Diameter + dx + Radius] = 1.f - std::clamp(length(vec2(dx, dy)) - Radius, 0.f, 1.f);

	for(int y = 0; y < Height; ++y)
	{
		for(int x = 0; x < Width; ++x)
		{
			int Value = pIn[y * Width + x];
			// 最大值已饱和时，剩余邻居不可能再改变结果。
			for(int dy = std::max(-Radius, -y); dy <= std::min(Radius, Height - 1 - y) && Value < 255; ++dy)
			{
				for(int dx = std::max(-Radius, -x); dx <= std::min(Radius, Width - 1 - x) && Value < 255; ++dx)
				{
					const float Weight = aWeights[(dy + Radius) * Diameter + dx + Radius];
					Value = std::max(Value, int(pIn[(y + dy) * Width + x + dx] * Weight));
				}
			}
			pOut[y * Width + x] = Value;
		}
	}
}

#endif
