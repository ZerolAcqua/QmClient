#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_EMOTICON_PROJECTILE_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_EMOTICON_PROJECTILE_H

#include <base/vmath.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <vector>

namespace QmEmoticon
{
	inline int SelectedSector(vec2 Mouse, float InnerRadius, int Count)
	{
		if(length(Mouse) <= InnerRadius)
			return -1;
		return ((int)std::round(angle(Mouse) * Count / (2.0f * pi)) + Count) % Count;
	}

	class CAlphaMask
	{
		struct SRect
		{
			int m_Left, m_Top, m_Right, m_Bottom;
		};
		std::vector<SRect> m_vRects;
		int m_Width = 1;
		int m_Height = 1;

	public:
		// 合并相邻行的相同不透明区间，保留透明孔洞，不使用图片外接矩形代替轮廓。
		void Build(const unsigned char *pRgba, int Width, int Height, int Stride = 0)
		{
			m_vRects.clear();
			m_Width = Width;
			m_Height = Height;
			if(Stride == 0)
				Stride = Width * 4;
			for(int Y = 0; Y < Height; ++Y)
			{
				for(int X = 0; X < Width;)
				{
					if(pRgba[Y * Stride + X * 4 + 3] == 0)
					{
						++X;
						continue;
					}
					const int Left = X++;
					while(X < Width && pRgba[Y * Stride + X * 4 + 3] != 0)
						++X;
					auto It = std::find_if(m_vRects.rbegin(), m_vRects.rend(), [&](const SRect &Rect) {
						return Rect.m_Bottom == Y && Rect.m_Left == Left && Rect.m_Right == X;
					});
					if(It != m_vRects.rend())
						It->m_Bottom = Y + 1;
					else
						m_vRects.push_back({Left, Y, X, Y + 1});
				}
			}
		}

		template<typename TSolid>
		bool Overlaps(vec2 Pos, float Size, float Angle, const TSolid &Solid) const
		{
			if(m_vRects.empty())
				return false;
			const vec2 AxisX = direction(Angle);
			const vec2 AxisY(-AxisX.y, AxisX.x);
			const vec2 AbsX(std::abs(AxisX.x), std::abs(AxisX.y));
			const vec2 AbsY(std::abs(AxisY.x), std::abs(AxisY.y));
			const float Radius = Size * 0.707107f;
			for(int Y = (int)std::floor((Pos.y - Radius) / 32); Y <= (int)std::floor((Pos.y + Radius) / 32); ++Y)
			{
				for(int X = (int)std::floor((Pos.x - Radius) / 32); X <= (int)std::floor((Pos.x + Radius) / 32); ++X)
				{
					if(!Solid(X, Y))
						continue;
					const vec2 TileCenter(X * 32 + 16, Y * 32 + 16);
					for(const auto &Rect : m_vRects)
					{
						const vec2 Half((Rect.m_Right - Rect.m_Left) * Size / (2 * m_Width), (Rect.m_Bottom - Rect.m_Top) * Size / (2 * m_Height));
						const vec2 Local(((Rect.m_Left + Rect.m_Right) / (2.0f * m_Width) - 0.5f) * Size,
							((Rect.m_Top + Rect.m_Bottom) / (2.0f * m_Height) - 0.5f) * Size);
						const vec2 Delta = TileCenter - (Pos + AxisX * Local.x + AxisY * Local.y);
						// 旋转像素矩形与地图格使用四条分离轴，接触边界不算穿透。
						if(std::abs(Delta.x) < 16 + AbsX.x * Half.x + AbsY.x * Half.y - 0.0001f &&
							std::abs(Delta.y) < 16 + AbsX.y * Half.x + AbsY.y * Half.y - 0.0001f &&
							std::abs(dot(Delta, AxisX)) < Half.x + 16 * (AbsX.x + AbsX.y) - 0.0001f &&
							std::abs(dot(Delta, AxisY)) < Half.y + 16 * (AbsY.x + AbsY.y) - 0.0001f)
							return true;
					}
				}
			}
			return false;
		}
	};
}

struct CEmoticonProjectile
{
	vec2 m_Pos = vec2(0, 0);
	vec2 m_Vel = vec2(0, 0);
	float m_Angle = 0;
	float m_AngVel = 0;
	int m_EmoticonID = 0;
	float m_LifeTime = 0;
	float m_SizeScale = 1;
	float m_SizeLimit = 128;
	bool m_Active = false;
	double m_Accumulator = 0;
	vec2 m_PreviousPos = vec2(0, 0);
	float m_PreviousAngle = 0;
	static constexpr double STEP = 1.0 / 240.0;

	void Init(vec2 Pos, vec2 Vel, int EmoticonID, float SizeScale = 1.0f)
	{
		m_Pos = m_PreviousPos = Pos;
		m_Vel = Vel;
		m_EmoticonID = EmoticonID;
		m_LifeTime = 3.0f;
		m_SizeScale = std::max(SizeScale, 0.1f);
		m_SizeLimit = 128.0f * m_SizeScale;
		m_Active = true;
		m_Angle = m_PreviousAngle = 0;
		m_AngVel = ((rand() % 100) - 50) / 10.0f;
		m_Accumulator = 0;
	}

	float Size() const { return std::min(m_SizeLimit, 64.0f * m_SizeScale * (1.0f + std::max(0.0f, 0.5f - m_LifeTime) * 2.0f)); }

	template<typename TSolid>
	bool PlaceOutside(const QmEmoticon::CAlphaMask &Mask, const TSolid &Solid)
	{
		if(!Mask.Overlaps(m_Pos, Size(), m_Angle, Solid))
			return true;
		// 贴墙出生或消失动画膨胀时，优先就近移出墙体。
		for(float Radius = 2; Radius <= Size() + 32; Radius += 2)
		{
			for(int I = 0; I < 16; ++I)
			{
				const vec2 Candidate = m_Pos + direction(-pi / 2 + I * pi / 8) * Radius;
				if(!Mask.Overlaps(Candidate, Size(), m_Angle, Solid))
				{
					m_Pos = m_PreviousPos = Candidate;
					return true;
				}
			}
		}
		return false;
	}

	template<typename TSolid>
	void Update(float Dt, const QmEmoticon::CAlphaMask &Mask, const TSolid &Solid)
	{
		if(!m_Active || Dt <= 0)
			return;
		// 长时间失去前台后，已过期的表情直接回收，避免补算整段历史。
		if(Dt >= m_LifeTime)
		{
			m_LifeTime = 0;
			m_Active = false;
			return;
		}
		// 固定步长消除帧率对弹道的影响。
		m_Accumulator += Dt;
		while(m_Accumulator + 1e-9 >= STEP && m_Active)
		{
			m_Accumulator -= STEP;
			const float PreviousSize = Size();
			m_LifeTime -= STEP;
			if(m_LifeTime <= 0)
			{
				m_Active = false;
				break;
			}
			if(!PlaceOutside(Mask, Solid))
			{
				// 空间不足时停止消失动画的膨胀，仍按原寿命淡出。
				if(PreviousSize < Size() && !Mask.Overlaps(m_Pos, PreviousSize, m_Angle, Solid))
					m_SizeLimit = PreviousSize;
				else
				{
					m_Active = false;
					break;
				}
			}
			m_PreviousPos = m_Pos;
			m_PreviousAngle = m_Angle;
			m_Vel.y += 1500.0f * STEP;
			// 连续细分平移和旋转，单次轮廓移动不超过一个世界单位。
			const int Steps = std::max(1, (int)std::ceil((length(m_Vel) + std::abs(m_AngVel) * Size()) * STEP));
			const float SubDt = STEP / Steps;
			for(int I = 0; I < Steps; ++I)
			{
				for(int Axis = 0; Axis < 2; ++Axis)
				{
					vec2 Next = m_Pos;
					float &Velocity = Axis == 0 ? m_Vel.x : m_Vel.y;
					(Axis == 0 ? Next.x : Next.y) += Velocity * SubDt;
					if(Mask.Overlaps(Next, Size(), m_Angle, Solid))
						Velocity *= -0.6f;
					else
						m_Pos = Next;
				}
				const float NextAngle = m_Angle + m_AngVel * SubDt;
				if(Mask.Overlaps(m_Pos, Size(), NextAngle, Solid))
					m_AngVel *= -0.6f;
				else
					m_Angle = NextAngle;
			}
		}
	}
};

namespace QmEmoticon
{
	template<std::size_t N>
	CEmoticonProjectile *ProjectileSlot(CEmoticonProjectile (&aProjectiles)[N])
	{
		auto *pOldest = &aProjectiles[0];
		for(auto &Projectile : aProjectiles)
		{
			if(!Projectile.m_Active)
				return &Projectile;
			if(Projectile.m_LifeTime < pOldest->m_LifeTime)
				pOldest = &Projectile;
		}
		return pOldest;
	}
}

#endif
