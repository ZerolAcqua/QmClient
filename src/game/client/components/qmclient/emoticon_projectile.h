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

	// Tee 的身体盒：全宽为物理尺寸 28，位置取角色中心，与客户端渲染位置一致。
	struct SPlayerBox
	{
		int m_ClientId = -1;
		vec2 m_Pos = vec2(0, 0);
		float m_Half = 0;
	};

	class CAlphaMask
	{
		struct SRect
		{
			int m_Left, m_Top, m_Right, m_Bottom;
		};
		struct SMaskAxes
		{
			vec2 m_X;
			vec2 m_Y;
			vec2 m_AbsX;
			vec2 m_AbsY;
		};
		std::vector<SRect> m_vRects;
		int m_Width = 1;
		int m_Height = 1;

		// 三角函数只按候选矩形集合算一次，地图格遍历不会重复展开轮廓轴。
		SMaskAxes BuildAxes(float Angle) const
		{
			SMaskAxes Result;
			Result.m_X = direction(Angle);
			Result.m_Y = vec2(-Result.m_X.y, Result.m_X.x);
			Result.m_AbsX = vec2(std::abs(Result.m_X.x), std::abs(Result.m_X.y));
			Result.m_AbsY = vec2(std::abs(Result.m_Y.x), std::abs(Result.m_Y.y));
			return Result;
		}

		// 旋转像素矩形与轴对齐矩形使用四条分离轴，接触边界不算穿透。
		bool RectHits(vec2 Pos, float Size, const SMaskAxes &Axes, vec2 RectCenter, vec2 RectHalf) const
		{
			const vec2 Delta = RectCenter - Pos;
			for(const auto &Rect : m_vRects)
			{
				const vec2 Half((Rect.m_Right - Rect.m_Left) * Size / (2 * m_Width), (Rect.m_Bottom - Rect.m_Top) * Size / (2 * m_Height));
				const vec2 Local(((Rect.m_Left + Rect.m_Right) / (2.0f * m_Width) - 0.5f) * Size,
					((Rect.m_Top + Rect.m_Bottom) / (2.0f * m_Height) - 0.5f) * Size);
				const vec2 D = Delta - (Axes.m_X * Local.x + Axes.m_Y * Local.y);
				if(std::abs(D.x) < RectHalf.x + Axes.m_AbsX.x * Half.x + Axes.m_AbsY.x * Half.y - 0.0001f &&
					std::abs(D.y) < RectHalf.y + Axes.m_AbsX.y * Half.x + Axes.m_AbsY.y * Half.y - 0.0001f &&
					std::abs(dot(D, Axes.m_X)) < Half.x + RectHalf.x * Axes.m_AbsX.x + RectHalf.y * Axes.m_AbsX.y - 0.0001f &&
					std::abs(dot(D, Axes.m_Y)) < Half.y + RectHalf.x * Axes.m_AbsY.x + RectHalf.y * Axes.m_AbsY.y - 0.0001f)
					return true;
			}
			return false;
		}

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
			const SMaskAxes Axes = BuildAxes(Angle);
			const float Radius = Size * 0.707107f;
			for(int Y = (int)std::floor((Pos.y - Radius) / 32); Y <= (int)std::floor((Pos.y + Radius) / 32); ++Y)
			{
				for(int X = (int)std::floor((Pos.x - Radius) / 32); X <= (int)std::floor((Pos.x + Radius) / 32); ++X)
				{
					if(Solid(X, Y) && RectHits(Pos, Size, Axes, vec2(X * 32 + 16, Y * 32 + 16), vec2(16, 16)))
						return true;
				}
			}
			return false;
		}

		// 与 Tee 身体盒等动态矩形共用同一套轮廓测试，表情只在实际重叠处反弹。
		bool OverlapsBox(vec2 Pos, float Size, float Angle, vec2 BoxCenter, vec2 BoxHalf) const
		{
			if(m_vRects.empty())
				return false;
			return RectHits(Pos, Size, BuildAxes(Angle), BoxCenter, BoxHalf);
		}
	};

	// 发射者自身不参与碰撞，否则弹道会在出生点被自己挡住。
	inline bool OverlapsPlayerBoxes(const CAlphaMask &Mask, vec2 Pos, float Size, float Angle, int OwnerClientId,
		const SPlayerBox *pBoxes, int NumBoxes)
	{
		if(pBoxes == nullptr)
			return false;
		// 两者外接圆不相交时不可能重叠，先用它跳过远处玩家。
		const float MaskRadius = Size * 0.70710678f;
		for(int I = 0; I < NumBoxes; ++I)
		{
			if(pBoxes[I].m_ClientId == OwnerClientId)
				continue;
			if(length(Pos - pBoxes[I].m_Pos) > MaskRadius + pBoxes[I].m_Half * 1.41421356f)
				continue;
			if(Mask.OverlapsBox(Pos, Size, Angle, pBoxes[I].m_Pos, vec2(pBoxes[I].m_Half, pBoxes[I].m_Half)))
				return true;
		}
		return false;
	}
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
	int m_OwnerClientId = -1;
	double m_Accumulator = 0;
	vec2 m_PreviousPos = vec2(0, 0);
	float m_PreviousAngle = 0;
	static constexpr double STEP = 1.0 / 240.0;

	void Init(vec2 Pos, vec2 Vel, int EmoticonID, float SizeScale = 1.0f, int OwnerClientId = -1)
	{
		m_Pos = m_PreviousPos = Pos;
		m_Vel = Vel;
		m_EmoticonID = EmoticonID;
		m_LifeTime = 3.0f;
		m_SizeScale = std::max(SizeScale, 0.1f);
		m_SizeLimit = 128.0f * m_SizeScale;
		m_Active = true;
		m_OwnerClientId = OwnerClientId;
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
		// 贴墙出生时优先就近移出墙体，飞行中不调用，避免跨到墙的另一侧。
		const int MaxRadius = (int)(Size() + 32);
		for(int Radius = 2; Radius <= MaxRadius; Radius += 2)
		{
			for(int I = 0; I < 16; ++I)
			{
				const vec2 Candidate = m_Pos + direction(-pi / 2 + I * pi / 8) * (float)Radius;
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
	void Update(float Dt, const QmEmoticon::CAlphaMask &Mask, const TSolid &Solid,
		const QmEmoticon::SPlayerBox *pPlayerBoxes = nullptr, int NumPlayerBoxes = 0)
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
		// 地图格与在场 Tee 的身体盒都是障碍，两者走同一套轮廓测试。
		const auto Blocked = [&](vec2 Pos, float Size, float Angle) {
			if(Mask.Overlaps(Pos, Size, Angle, Solid))
				return true;
			return QmEmoticon::OverlapsPlayerBoxes(Mask, Pos, Size, Angle, m_OwnerClientId, pPlayerBoxes, NumPlayerBoxes);
		};
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
			// 飞行中不做脱困瞬移：贴墙时把表情挪到最近的空位会直接跨到墙的另一侧。
			// 空间不足时只冻结消失动画的膨胀，仍按原寿命淡出。
			if(Mask.Overlaps(m_Pos, Size(), m_Angle, Solid))
			{
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
					// 撞到地图或 Tee 都在该轴上反弹，与逐轴求解保持一致。
					if(Blocked(Next, Size(), m_Angle))
						Velocity *= -0.6f;
					else
						m_Pos = Next;
				}
				const float NextAngle = m_Angle + m_AngVel * SubDt;
				if(Blocked(m_Pos, Size(), NextAngle))
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
