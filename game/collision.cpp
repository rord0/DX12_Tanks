#include "collision.hpp"
#include "tanks_math.hpp"
#include "transforms.hpp"
#include <cmath>
#include <regex>

CollisionSystem2D InitCollisionSystem2D(Arena * arena, u32 maxStaticColliders, u32 maxDynamicColliders)
{
	CollisionSystem2D c;
	c.nextColliderID = 100;
	c.dynamicColliders = ArrayInit(sizeof(Collider2D), maxDynamicColliders, ArenaPush(arena, sizeof(Collider2D) * maxDynamicColliders));
	c.staticColliders = ArrayInit(sizeof(Collider2D), maxStaticColliders, ArenaPush(arena, sizeof(Collider2D) * maxStaticColliders));
	return c;
}

u32 CollisionSystem2D::AddCollider(ColliderType type, u32 transformIndex, bool isStatic)
{
	Collider2D collider = {type, nextColliderID++, transformIndex, isStatic};
	if (isStatic) { ArrayPush(&staticColliders, &collider); }
	else		  { ArrayPush(&dynamicColliders, &collider); }
	return collider.id;
}


bool CollisionSystem2D::RemoveCollider(u32 colliderID)
{
	for (int i = 0; i < staticColliders.count; i++)
	{
		Collider2D * collider = (Collider2D*)staticColliders.elements + i;
		if (collider->id == colliderID)
		{
			ArrayRemove(&staticColliders, i);
			return true;
		}
	}

	for (int i = 0; i < dynamicColliders.count; i++)
	{
		Collider2D * collider = (Collider2D*)dynamicColliders.elements + i;
		if (collider->id == colliderID)
		{
			ArrayRemove(&dynamicColliders, i);
			return true;
		}
	}

	return false;
}

void GetRectWorldVertices(const Transform2D * t, vec2 vertices[4])
{
	vec2 worldPos = { t->world.m[3][0], t->world.m[3][1] };

	vec2 xAxis;
	vec2 yAxis;
	GetAxes2D(&t->world, &xAxis, &yAxis);

	vec2 halfExtent = mat4GetScale2D(&t->world) * 0.5f;

	vertices[0] = worldPos + xAxis * halfExtent.x + yAxis * halfExtent.y;
	vertices[1] = worldPos - xAxis * halfExtent.x + yAxis * halfExtent.y;
	vertices[2] = worldPos - xAxis * halfExtent.x - yAxis * halfExtent.y;
	vertices[3] = worldPos + xAxis * halfExtent.x - yAxis * halfExtent.y;
}

void RectGetMinMaxPointsOnAxis(const Transform2D * transform, const vec2 axis, vec2 * min, vec2 * max)
{
	vec2 vertices[4];
	GetRectWorldVertices(transform, vertices);

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

bool IsCollidingOnAxis(vec2 axis, const Transform2D * a, const Transform2D * b, CollisionData & collisionData)
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
        collisionData.penetration = fabsf(overlapA);
	}
	else
	{
		collisionData.normal = axis;
		collisionData.penetration = fabsf(overlapB);
	}
	collisionData.point = collisionData.normal;

	return true;
}

bool IsCircleRectColliding(const Transform2D * circle, const Transform2D * rect, CollisionData * collisionData)
{
	vec2 circlePos = mat4GetPositionVec2(&circle->world);
	vec2 rectPos = mat4GetPositionVec2(&rect->world);

	mat2 rectRot = GetRotationMat2(rect->world);

	vec2 local = (circlePos - rectPos) * rectRot.transpose();

	vec2 rectHalfExtents = mat4GetScale2D(&rect->world) * 0.5f;
	vec2 circleHalfExtents = mat4GetScale2D(&circle->world) * 0.5f;
    float closestX = fmaxf(-rectHalfExtents.x, fminf(local.x, rectHalfExtents.x));
    float closestY = fmaxf(-rectHalfExtents.y, fminf(local.y, rectHalfExtents.y));

	float diffX = local.x - closestX;
    float diffY = local.y - closestY;
	f32 radius = mat4GetScale2D(&circle->world).x * 0.25;

	f32 distSq = (diffX * diffX + diffY * diffY);

	if (distSq > radius * radius) { return false; }
	
	f32 dist = sqrtf(distSq);

	vec2 normalLocal;
    if (dist > 0.0001f)
    {
        // Circle center is outside the rect (normal case) — push away from closest point
        normalLocal = { diffX / dist, diffY / dist };
    }
	else
	{
        // Circle center is INSIDE the rect — closest-point diff is (0,0), can't normalize.
        // Push out along the axis of least penetration instead.
        float penX = rectHalfExtents.x - fabsf(local.x);
        float penY = rectHalfExtents.y - fabsf(local.y);

        if (penX < penY)
            normalLocal = { local.x < 0.0f ? -1.0f : 1.0f, 0.0f };
        else
            normalLocal = { 0.0f, local.y < 0.0f ? -1.0f : 1.0f };

        dist = 0.0f; // center is inside; treat as fully overlapping the surface
	}

    f32 penetration = radius - dist;
    vec2 normalWorld = normalLocal * rectRot;

	collisionData->normal = normalWorld;
	collisionData->penetration = penetration;
	return true;

}

bool IsColliding(const Collider2D * a, const Collider2D * b, CollisionData * outData, const Transform2D * transformA, const Transform2D * transformB)
{
	if (a->type == COLLIDER_RECTANGLE && b->type == COLLIDER_RECTANGLE)
	{
		vec2 axes[4];
		GetAxes2D(&transformA->world, &axes[0], &axes[1]);
		GetAxes2D(&transformB->world, &axes[2], &axes[3]);

		CollisionData bestCD;
		CollisionData currentCD;

		bestCD.penetration = 10000.0f;
		for (const vec2 & axis : axes)
		{
			if (!IsCollidingOnAxis(axis, transformA, transformB, currentCD))
				return false;

			if (currentCD.penetration <= bestCD.penetration)
				bestCD = currentCD;
		}

		if (outData != NULL) *outData = bestCD;
		return true;
	}
	if (a->type == COLLIDER_RECTANGLE && b->type == COLLIDER_CIRCLE)
	{
		CollisionData CD;
		if (IsCircleRectColliding(transformB, transformA, &CD))
		{
			CD.normal *= -1.0f;
			if (outData != NULL) *outData = CD;
			return true;
		}
	}
	if (a->type == COLLIDER_CIRCLE && b->type == COLLIDER_RECTANGLE)
	{
		CollisionData CD;
		if (IsCircleRectColliding(transformA, transformB, &CD))
		{
			if (outData != NULL) *outData = CD;
			return true;
		}
	}

	return false;
}

void CollisionSystem2D::ResolveCollisions(TransformHierarchy * th)
{
	collisionChecks = 0;
	transforms = th;
	for (int i = 0; i < dynamicColliders.count; i++)
	{
		Collider2D * collider = (Collider2D*)dynamicColliders.elements + i;
		collider->colliding = false;
	}

	for (int i = 0; i < staticColliders.count; i++)
	{
		Collider2D * collider = (Collider2D*)staticColliders.elements + i;
		collider->colliding = false;
	}

	//Dynamic - Static
	for (int i = 0; i < dynamicColliders.count; i++)
	{
		for (int j = 0; j < staticColliders.count; j++)
		{
			Collider2D * colliderA = (Collider2D*)dynamicColliders.elements + i;
			Collider2D * colliderB = (Collider2D*)staticColliders.elements + j;
			Transform2D * transformA = transforms->GetTransform(colliderA->transformIndex);
			Transform2D * transformB = transforms->GetTransform(colliderB->transformIndex);

			CollisionData cd = {0};
			bool colliding = IsColliding(colliderA, colliderB, &cd, transformA, transformB);
			collisionChecks++;
			if (colliding)
			{
				vec2 correction = cd.normal * (cd.penetration * 1.0f);
				Transform2D * transformARoot = transforms->GetRootTransform(colliderA->transformIndex);
				Transform2D * transformBRoot = transforms->GetRootTransform(colliderB->transformIndex);
				colliderA->colliding = true;
				colliderB->colliding = true;
				transformARoot->SetPosition(transformARoot->position + correction);
			}
		}
	}

	// Dynamic - Dynamic 
	for (int i = 0; i < dynamicColliders.count; i++)
	{
		for (int j = i + 1; j < dynamicColliders.count; j++)
		{
			Collider2D * colliderA = (Collider2D*)dynamicColliders.elements + i;
			Collider2D * colliderB = (Collider2D*)dynamicColliders.elements + j;
			Transform2D * transformA = transforms->GetTransform(colliderA->transformIndex);
			Transform2D * transformB = transforms->GetTransform(colliderB->transformIndex);

			CollisionData cd = {0};
			bool colliding = IsColliding(colliderA, colliderB, &cd, transformA, transformB);
			if (colliding)
			{
				vec2 correction = cd.normal * (cd.penetration * 1.0f);
				Transform2D * transformARoot = transforms->GetRootTransform(colliderA->transformIndex);
				Transform2D * transformBRoot = transforms->GetRootTransform(colliderB->transformIndex);
				colliderA->colliding = true;
				colliderB->colliding = true;
				transformARoot->SetPosition(transformARoot->position + correction);
				transformBRoot->SetPosition(transformBRoot->position - correction);
			}
		}
	}
}

bool IsLinesColliding(vec2 startA, vec2 endA, vec2 startB, vec2 endB, vec2 * outIntersection)
{
	float uA = ((endB.x-startB.x) * (startA.y-startB.y) - (endB.y-startB.y) * (startA.x-startB.x)) /
			   ((endB.y-startB.y) * (endA.x-startA.x)   - (endB.x-startB.x) * (endA.y-startA.y));
	float uB = ((endA.x-startA.x) * (startA.y-startB.y) - (endA.y-startA.y) * (startA.x-startB.x)) /
			   ((endB.y-startB.y) * (endA.x-startA.x)   - (endB.x-startB.x) * (endA.y-startA.y));

	if (uA >= 0 && uA <= 1 && uB >= 0 && uB <= 1)
	{
		float intersectionX = startA.x + (uA * (endA.x-startA.x));
		float intersectionY = startA.y + (uA * (endA.y-startA.y));
		if (outIntersection != NULL) *outIntersection = vec2{intersectionX, intersectionY};
		return true;
	}
	return false;
}

bool LineRectColliding(const Collider2D * collider, const Transform2D * transform, vec2 start, vec2 end, vec2 * outHitPos)
{
	bool colliding = false;
	vec2 vertices[4];
	GetRectWorldVertices(transform, vertices);

	f32 closestDist = std::numeric_limits<float>::max();;
	vec2 closestPoint= {0,0};
	for (int i = 0; i < 4; i++)
	{
		vec2 intersection;
		if (IsLinesColliding(start, end, vertices[i], vertices[(i + 1) % 4], &intersection))
		{
			colliding = true;
			f32 dist = vec2Dist(start, intersection);
			if (dist <= closestDist)
			{
				closestPoint = intersection;
				closestDist = dist;
			}
		}
	}

	if (colliding && outHitPos != NULL) { *outHitPos = closestPoint; }
	return colliding;
}

bool LineCircleColliding(vec2 start, vec2 end, vec2 center, f32 radius, vec2 * outHitPoint)
{
	vec2 d = (end - start);
	vec2 f = (start - center);

	f32 a = vec2Dot(d, d);
	f32 b = 2.0f * vec2Dot(f, d);
	f32 c = vec2Dot(f, f) - radius * radius;

	f32 discriminant = b * b - 4.0f * a * c;
	if (discriminant < 0.0f)
		return false; // no intersection at all

	discriminant = sqrtf(discriminant);

	f32 t1 = (-b - discriminant) / (2.0f * a);
	f32 t2 = (-b + discriminant) / (2.0f * a);

	// We want the smallest t in [0,1] — that's the first entry point along the segment
	f32 t = FLT_MAX;
	if (t1 >= 0.0f && t1 <= 1.0f) t = t1;
	else if (t2 >= 0.0f && t2 <= 1.0f) t = t2;
	else
		return false; // circle intersects the infinite line, but not within the segment

	if (outHitPoint) { *outHitPoint = (start + d * t); }

	return true;
}

bool LineColliding(const Collider2D * collider, const Transform2D * transform, vec2 start, vec2 end, vec2 * outHitPos)
{
	if (collider->type == COLLIDER_RECTANGLE)
	{
		return LineRectColliding(collider, transform, start, end, outHitPos);
	}
	else if (collider->type == COLLIDER_CIRCLE)
	{
		return LineCircleColliding(start, end, mat4GetPositionVec2(&transform->world), (mat4GetScale2D(&transform->world).x / 2.0f), outHitPos);
	}
	return false;
}

bool PointRectColliding(const Collider2D * collider, const Transform2D * transform, vec2 point)
{
	vec2 diff = point - mat4GetPositionVec2(&transform->world);

	vec2 pointRectSpace = diff * GetRotationMat2(transform->world);
	vec2 rectSize = mat4GetScale2D(&transform->world);
	return fabsf(pointRectSpace.x) <= (rectSize.x * 0.5f) && fabsf(pointRectSpace.y) <= (rectSize.y * 0.5);
}

bool PointCircleColliding(const Collider2D * collider, const Transform2D * transform, vec2 point)
{
	f32 dist =vec2Dist(point, mat4GetPositionVec2(&transform->world));
	f32 r = (mat4GetScale2D(&transform->world).x) / 4.0f;
	return dist < r;
}

bool PointColliding(const Collider2D * collider, const Transform2D * transform, vec2 point)
{
	if (collider->type == COLLIDER_RECTANGLE)
	{
		return PointRectColliding(collider, transform, point);
	}
	else if (collider->type == COLLIDER_CIRCLE)
	{
		return PointCircleColliding(collider, transform, point);
	}
	return false;
}

bool CollisionSystem2D::RaycastHit(vec2 start, vec2 end, vec2 * outPoint, u32 ignoreID, u32 * outColliderID)
{
	Collider2D * collider;
	bool hit = false;
	f32 nearestHitDist = FLT_MAX;
	vec2 nearestHitPos = {0.0f, 0.0f};
	u32 nearestHitID = 0;

	auto testArray = [&](Array& colliders)
	{
		for (int i = 0; i < colliders.count; i++)
		{
			Collider2D * collider = (Collider2D*)colliders.elements + i;
			if (collider->id == ignoreID) { continue; }
			vec2 currentHitPos;
			if (LineColliding(collider, transforms->GetTransform(collider->transformIndex), start, end, &currentHitPos))
			{
				f32 hitDist = vec2Dist(start, currentHitPos);
				if (hitDist < nearestHitDist)
				{
					nearestHitDist = hitDist;
					nearestHitPos = currentHitPos;
					nearestHitID = collider->id;
					hit = true;
				}
			}
		}
	};

	testArray(dynamicColliders);
	testArray(staticColliders);

	if (hit)
	{
		if (outPoint) *outPoint = nearestHitPos;
		if (outColliderID) *outColliderID = nearestHitID;
	}

	return hit;
}

bool CollisionSystem2D::AABBHits(vec2 pos, vec2 extents, Array * ids)
{
	auto testArray = [&](Array& colliders)
	{
		for (int i = 0; i < colliders.count; i++)
		{
			Collider2D * collider = (Collider2D*)colliders.elements + i;
			Transform2D transformAABB = {pos, extents, 0.0f};

			transformAABB.world = ModelMatrix2D(transformAABB.position, transformAABB.rotation, transformAABB.scale);
			Collider2D AABB = {COLLIDER_RECTANGLE, 0, 0};
			if (IsColliding(collider, &AABB, NULL, transforms->GetTransform(collider->transformIndex), &transformAABB))
			{
				ArrayPush(ids, &collider->id);
			}
		}
	};

	testArray(dynamicColliders);
	testArray(staticColliders);
	return false;
}

bool CollisionSystem2D::PointHit(vec2 point, u32 * outColliderID)
{
	u32 colliderID;
	auto testArray = [&](Array& colliders)
	{
		for (int i = 0; i < colliders.count; i++)
		{
			Collider2D * collider = (Collider2D*)colliders.elements + i;
			if (PointColliding(collider, transforms->GetTransform(collider->transformIndex), point))
			{
				colliderID = collider->id;
				return true;
			}
		}
		return false;
	};

	if (testArray(dynamicColliders) || testArray(staticColliders))
	{
		*outColliderID = colliderID;
		return true;
	}

	return false;
}

/*


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


bool IsLinesColliding(vec2 startA, vec2 endA, vec2 startB, vec2 endB, vec2 * outIntersection)
{
	float uA = ((endB.x-startB.x)*(startA.y-startB.y) - (endB.y-startB.y)*(startA.x-startB.x)) /
			   ((endB.y-startB.y)*(endA.x-startA.x) - (endB.x-startB.x)*(endA.y-startA.y));
	float uB = ((endA.x-startA.x)*(startA.y-startB.y) - (endA.y-startA.y)*(startA.x-startB.x)) /
			   ((endB.y-startB.y)*(endA.x-startA.x) - (endB.x-startB.x)*(endA.y-startA.y));

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
		}
	}

	if (colliding && outPoint != NULL) { *outPoint = closestPoint; }

	return colliding;
}
*/
