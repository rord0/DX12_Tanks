#include "tanks_math.hpp"

f32 vec2Dot(vec2 a, vec2 b)  { return a.x * b.x + a.y * b.y; }
f32 vec2Dist(vec2 a, vec2 b) { return sqrtf((a.x - b.x)*(a.x - b.x) + (a.y - b.y)*(a.y - b.y)); }
vec2 vec2Norm(vec2 v) { f32 mag = sqrtf(v.x * v.x + v.y * v.y); return {v.x / mag, v.y / mag }; }
vec2 vec2Rotate(vec2 v, f32 angle)
{
	float cosA = cosf(angle);
    float sinA = sinf(angle);
	return {v.x * cosA - v.y * sinA, v.x * sinA + v.y * cosA};
}

mat4 ModelMatrix2D(vec2 pos, float rotation, vec2 scale)
{
    float c = cosf(rotation);
    float s = sinf(rotation);

    mat4 m = {0};

    m.m[0][0] = c * scale.x;   m.m[0][1] = s * scale.x;   m.m[0][2] = 0.0f; m.m[0][3] = 0.0f;
    m.m[1][0] = -s * scale.y;  m.m[1][1] = c * scale.y;   m.m[1][2] = 0.0f; m.m[1][3] = 0.0f;
    m.m[2][0] = 0.0f;          m.m[2][1] = 0.0f;          m.m[2][2] = 1.0f; m.m[2][3] = 0.0f;
    m.m[3][0] = pos.x;         m.m[3][1] = pos.y;         m.m[3][2] = 0.0f; m.m[3][3] = 1.0f;

    return m;
}

void RectGetMinMaxPointsOnAxis(const Collider2D & collider, const vec2 axis, vec2 * min, vec2 * max)
{
	if (collider.type == COLLIDER_RECTANGLE)
	{
		vec2 xAxis = vec2{ cosf(collider.rotation), sinf(collider.rotation)};
		vec2 yAxis = vec2{-sinf(collider.rotation), cosf(collider.rotation)};

		f32 halfExtentX = collider.size.x / 4.0f;
		f32 halfExtentY = collider.size.y / 4.0f;

		vec2 vertices[4];
		vertices[0] = collider.position + (xAxis * halfExtentX) + (yAxis * halfExtentY);
		vertices[1] = collider.position + (xAxis * halfExtentX) - (yAxis * halfExtentY);
		vertices[2] = collider.position - (xAxis * halfExtentX) + (yAxis * halfExtentY);
		vertices[3] = collider.position - (xAxis * halfExtentX) - (yAxis * halfExtentY);

		f32 minProj = vec2Dot(vertices[0], axis);
		f32 maxProj = minProj;
		*min = vertices[0];
		*max = vertices[0];

		for (vec2 v : vertices)
		{
			f32 p = vec2Dot(v, axis);
			if (p < minProj)
			{
				minProj = p;
				*min = v;
			}
			if (p > maxProj)
			{
				maxProj = p;
				*max = v;
			}
		}
	}
}

b32 IsCollidingOnAxis(vec2 axis, const Collider2D & a, const Collider2D & b, CollisionData & collisionData)
{
	vec2 minVertA, minVertB, maxVertA, maxVertB;
	RectGetMinMaxPointsOnAxis(a, axis, &minVertA, &maxVertA);
	RectGetMinMaxPointsOnAxis(b, axis, &minVertB, &maxVertB);

	float minA = vec2Dot(minVertA, axis);
	float maxA = vec2Dot(maxVertA, axis);
	float minB = vec2Dot(minVertB, axis);
	float maxB = vec2Dot(maxVertB, axis);

	if (maxA < minB || maxB < minA) { return false; }

	float overlapA = maxA - minB;
	float overlapB = maxB - minA;

	if (overlapA < overlapB)
	{
		collisionData.normal = -axis;
        collisionData.penetration = abs(overlapA);
	}
	else
	{
		collisionData.normal = axis;
		collisionData.penetration = abs(overlapB);
	}
	collisionData.point = collisionData.normal;


	return true;

}

vec2 ClosestPointOnRect(Collider2D rect, vec2 point)
{
	vec2 yAxis = vec2{-sinf(rect.rotation), cosf(rect.rotation)};
	vec2 xAxis = vec2{ cosf(rect.rotation), sinf(rect.rotation)};

    float hx = rect.size.x * 0.25f;
    float hy = rect.size.y * 0.25f;

	vec2 dir = point - rect.position;
    float px = vec2Dot(dir, xAxis);
    float py = vec2Dot(dir, yAxis);

	px = std::clamp(px, -hx, hx);
    py = std::clamp(py, -hy, hy);

	vec2 p = rect.position + xAxis * px + yAxis * py;
    return p;
}

b32 IsColliding(const Collider2D & a, const Collider2D & b, CollisionData * outData)
{
	if (a.type == COLLIDER_RECTANGLE && b.type == COLLIDER_RECTANGLE)
	{
		vec2 axes[4];
		axes[0] = vec2{-sinf(a.rotation), cosf(a.rotation)};
		axes[1] = vec2{ cosf(a.rotation), sinf(a.rotation)};
		axes[2] = vec2{-sinf(b.rotation), cosf(b.rotation)};
		axes[3] = vec2{ cosf(b.rotation), sinf(b.rotation)};

		CollisionData bestCD;
		CollisionData currentCD;

		bestCD.penetration = 10000.0f;
		for (const vec2 & axis : axes)
		{
			if (!IsCollidingOnAxis(axis, a, b, currentCD))
				return false;

			if (currentCD.penetration <= bestCD.penetration)
				bestCD = currentCD;
		}

		RendererPushCircle(DEBUG_RENDER_CMDS, vec3{bestCD.normal.x, bestCD.normal.y, 0.0f}, 0.0f, {0.1f,0.1f}, {1.0f, 0.0f, 0.0f}, 1.0f, 30);
		if (outData != NULL) *outData = bestCD;
		return true;
	}
	if (a.type == COLLIDER_RECTANGLE && b.type == COLLIDER_CIRCLE)
	{
		vec2 closestPoint = ClosestPointOnRect(a, b.position);
		f32 dist = vec2Dist(closestPoint, b.position);

		if (dist <= 0.5f / 4.0f)
		{
			return true;
		}

		return false;
	}

	return true;
}

bool IsLinesColliding(vec2 startA, vec2 endA, vec2 startB, vec2 endB, vec2 * outIntersection)
{
	float uA = ((endB.x-startB.x)*(startA.y-startB.y) - (endB.y-startB.y)*(startA.x-startB.x)) / ((endB.y-startB.y)*(endA.x-startA.x) - (endB.x-startB.x)*(endA.y-startA.y));
	float uB = ((endA.x-startA.x)*(startA.y-startB.y) - (endA.y-startA.y)*(startA.x-startB.x)) / ((endB.y-startB.y)*(endA.x-startA.x) - (endB.x-startB.x)*(endA.y-startA.y));

	if (uA >= 0 && uA <= 1 && uB >= 0 && uB <= 1)
	{
		float intersectionX = startA.x + (uA * (endA.x-startA.x));
		float intersectionY = startA.y + (uA * (endA.y-startA.y));
		if (outIntersection != NULL) *outIntersection = vec2{intersectionX, intersectionY};
		return true;
	}
	return false;
}

bool IsLineColliding(const Collider2D & collider, vec2 start, vec2 end, vec2 * outPoint)
{
	vec2 xAxis = vec2{ cosf(collider.rotation), sinf(collider.rotation)};
	vec2 yAxis = vec2{-sinf(collider.rotation), cosf(collider.rotation)};

	f32 halfExtentX = collider.size.x / 4.0f;
	f32 halfExtentY = collider.size.y / 4.0f;

	vec2 vertices[4];
	vertices[0] = collider.position + (xAxis * halfExtentX) + (yAxis * halfExtentY);
	vertices[1] = collider.position - (xAxis * halfExtentX) + (yAxis * halfExtentY);
	vertices[2] = collider.position - (xAxis * halfExtentX) - (yAxis * halfExtentY);
	vertices[3] = collider.position + (xAxis * halfExtentX) - (yAxis * halfExtentY);

	bool colliding = false;
	f32 closestDist = std::numeric_limits<float>::max();;
	vec2 closestPoint= {0,0};
	for (int i = 0; i < 4; i++)
	{
		vec2 intersection;
		if (IsLinesColliding(start, end, vertices[i], vertices[(i + 1) % 4], &intersection))
		{
			colliding = true;
			f32 dist= vec2Dist(start, intersection);
			if (dist <= closestDist)
			{
				closestPoint = intersection;
				closestDist = dist;
			}
			RendererPushCircle(DEBUG_RENDER_CMDS, {intersection.x, intersection.y, 0.0f}, 0.0f, {0.1f,0.1f}, {1.0f, 0.0f, 0.0f}, 1.0f, 30);
		}
	}

	if (colliding && outPoint != NULL) { *outPoint = closestPoint; }

	return colliding;
}
