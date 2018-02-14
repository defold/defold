/*
* Copyright (c) 2006-2009 Erin Catto http://www.box2d.org
*
* This software is provided 'as-is', without any express or implied
* warranty.  In no event will the authors be held liable for any damages
* arising from the use of this software.
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
*/

#include <Box2D/Dynamics/b2Fixture.h>
#include <Box2D/Dynamics/Contacts/b2Contact.h>
#include <Box2D/Dynamics/b2World.h>
#include <Box2D/Collision/Shapes/b2CircleShape.h>
#include <Box2D/Collision/Shapes/b2EdgeShape.h>
#include <Box2D/Collision/Shapes/b2PolygonShape.h>
#include <Box2D/Collision/Shapes/b2ChainShape.h>
#include <Box2D/Collision/b2BroadPhase.h>
#include <Box2D/Collision/b2Collision.h>
#include <Box2D/Common/b2BlockAllocator.h>

b2Fixture::b2Fixture()
{
	m_userData = NULL;
	m_body = NULL;
	m_next = NULL;
	m_proxies = NULL;
	m_proxyCount = 0;
	m_shape = NULL;
	m_density = 0.0f;
	m_filters = &m_singleFilter;
}

void b2Fixture::Create(b2BlockAllocator* allocator, b2Body* body, const b2FixtureDef* def)
{
	m_userData = def->userData;
	m_friction = def->friction;
	m_restitution = def->restitution;

	m_body = body;
	m_next = NULL;

	// Defold modification. Set first filter as default
	m_filters[0] = def->filter;

	m_isSensor = def->isSensor;

    // Defold mod: Don't take ownership of shape
    //m_shape = def->shape->Clone(allocator);
    m_shape = (b2Shape*)def->shape;

	// Reserve proxy space
	int32 childCount = m_shape->GetChildCount();
	m_proxies = (b2FixtureProxy*)allocator->Allocate(childCount * sizeof(b2FixtureProxy));
    // Defold modification. Allocate filters per child-shape
	if (m_shape->m_filterPerChild)
	{
	    m_filters = (b2Filter*)allocator->Allocate(childCount * sizeof(b2Filter));
	}
	for (int32 i = 0; i < childCount; ++i)
	{
		m_proxies[i].fixture = NULL;
		m_proxies[i].proxyId = b2BroadPhase::e_nullProxy;
	    // Defold modification. Set filter per child shape
	    if (m_shape->m_filterPerChild)
	    {
	        m_filters[i] = def->filter;
	    }
	}
	m_proxyCount = 0;

	m_density = def->density;
}

void b2Fixture::Destroy(b2BlockAllocator* allocator)
{
	// The proxies must be destroyed before calling this.
	b2Assert(m_proxyCount == 0);

	// Free the proxy array.
	int32 childCount = m_shape->GetChildCount();
	allocator->Free(m_proxies, childCount * sizeof(b2FixtureProxy));
	m_proxies = NULL;
	if (m_shape->m_filterPerChild)
	{
	    allocator->Free(m_filters, childCount * sizeof(b2Filter));
	}

    // Defold mod: Don't take ownership of shape
//	// Free the child shape.
//	switch (m_shape->m_type)
//	{
//	case b2Shape::e_circle:
//		{
//			b2CircleShape* s = (b2CircleShape*)m_shape;
//			s->~b2CircleShape();
//			allocator->Free(s, sizeof(b2CircleShape));
//		}
//		break;
//
//	case b2Shape::e_edge:
//		{
//			b2EdgeShape* s = (b2EdgeShape*)m_shape;
//			s->~b2EdgeShape();
//			allocator->Free(s, sizeof(b2EdgeShape));
//		}
//		break;
//
//	case b2Shape::e_polygon:
//		{
//			b2PolygonShape* s = (b2PolygonShape*)m_shape;
//			s->~b2PolygonShape();
//			allocator->Free(s, sizeof(b2PolygonShape));
//		}
//		break;
//
//	case b2Shape::e_chain:
//		{
//			b2ChainShape* s = (b2ChainShape*)m_shape;
//			s->~b2ChainShape();
//			allocator->Free(s, sizeof(b2ChainShape));
//		}
//		break;
//
//	default:
//		b2Assert(false);
//		break;
//	}

	m_shape = NULL;
}

void b2Fixture::CreateProxies(b2BroadPhase* broadPhase, const b2Transform& xf)
{
	b2Assert(m_proxyCount == 0);

	// Create proxies in the broad-phase.
	m_proxyCount = m_shape->GetChildCount();

	for (int32 i = 0; i < m_proxyCount; ++i)
	{
		b2FixtureProxy* proxy = m_proxies + i;
		m_shape->ComputeAABB(&proxy->aabb, xf, i);
		proxy->proxyId = broadPhase->CreateProxy(proxy->aabb, proxy);
		proxy->fixture = this;
		proxy->childIndex = i;
	}
}

void b2Fixture::DestroyProxies(b2BroadPhase* broadPhase)
{
	// Destroy proxies in the broad-phase.
	for (int32 i = 0; i < m_proxyCount; ++i)
	{
		b2FixtureProxy* proxy = m_proxies + i;
		broadPhase->DestroyProxy(proxy->proxyId);
		proxy->proxyId = b2BroadPhase::e_nullProxy;
	}

	m_proxyCount = 0;
}

void b2Fixture::Synchronize(b2BroadPhase* broadPhase, const b2Transform& transform1, const b2Transform& transform2)
{
	if (m_proxyCount == 0)
	{
		return;
	}

	for (int32 i = 0; i < m_proxyCount; ++i)
	{
		b2FixtureProxy* proxy = m_proxies + i;

		// Compute an AABB that covers the swept shape (may miss some rotation effect).
		b2AABB aabb1, aabb2;
		m_shape->ComputeAABB(&aabb1, transform1, proxy->childIndex);
		m_shape->ComputeAABB(&aabb2, transform2, proxy->childIndex);

		proxy->aabb.Combine(aabb1, aabb2);

		b2Vec2 displacement = transform2.p - transform1.p;

		broadPhase->MoveProxy(proxy->proxyId, proxy->aabb, displacement);
	}
}

void b2Fixture::SynchronizeSingle(b2BroadPhase* broadPhase, int32 index, const b2Transform& transform1, const b2Transform& transform2)
{
    b2Assert(index < m_proxyCount);

    b2FixtureProxy* proxy = m_proxies + index;

    b2AABB aabb1, aabb2;
    m_shape->ComputeAABB(&aabb1, transform1, proxy->childIndex);
    m_shape->ComputeAABB(&aabb2, transform2, proxy->childIndex);

    proxy->aabb.Combine(aabb1, aabb2);

    b2Vec2 displacement = transform2.p - transform1.p;

    broadPhase->MoveProxy(proxy->proxyId, proxy->aabb, displacement);
}

void b2Fixture::SetFilterData(const b2Filter& filter, int32 index)
{
    // Defold modifications. Added index
    m_filters[index * m_shape->m_filterPerChild] = filter;

    // Defold modifications. Skip re-filtering of grid-shapes
    // in order to avoid O(n^2) memory allocations
    // see b2BroadPhase::BufferMove
    // We don't support changing filters at run-time so the
    // fix will hopefully have no impact
    if (GetType() != b2Shape::e_grid)
    {
        Refilter();
    }
}

void b2Fixture::Refilter()
{
	if (m_body == NULL)
	{
		return;
	}

	// Flag associated contacts for filtering.
	b2ContactEdge* edge = m_body->GetContactList();
	while (edge)
	{
		b2Contact* contact = edge->contact;
		b2Fixture* fixtureA = contact->GetFixtureA();
		b2Fixture* fixtureB = contact->GetFixtureB();
		if (fixtureA == this || fixtureB == this)
		{
			contact->FlagForFiltering();
		}

		edge = edge->next;
	}

	b2World* world = m_body->GetWorld();

	if (world == NULL)
	{
		return;
	}

	// Touch each proxy so that new pairs may be created
	b2BroadPhase* broadPhase = &world->m_contactManager.m_broadPhase;
	for (int32 i = 0; i < m_proxyCount; ++i)
	{
		broadPhase->TouchProxy(m_proxies[i].proxyId);
	}
}

void b2Fixture::SetSensor(bool sensor)
{
	if (sensor != m_isSensor)
	{
		m_body->SetAwake(true);
		m_isSensor = sensor;
	}
}

void b2Fixture::Dump(int32 bodyIndex)
{
	b2Log("    b2FixtureDef fd;\n");
	b2Log("    fd.friction = %.15lef;\n", m_friction);
	b2Log("    fd.restitution = %.15lef;\n", m_restitution);
	b2Log("    fd.density = %.15lef;\n", m_density);
	b2Log("    fd.isSensor = bool(%d);\n", m_isSensor);
	if (m_shape->m_filterPerChild)
	{
	    int32 childCount = m_shape->GetChildCount();
	    for (int32 i = 0; i < childCount; ++i)
	    {
	        b2Log("    fd.filter[%d].categoryBits = uint16(%d);\n", i, m_filters[i].categoryBits);
	        b2Log("    fd.filter[%d].maskBits = uint16(%d);\n", i, m_filters[i].maskBits);
	        b2Log("    fd.filter[%d].groupIndex = int16(%d);\n", i, m_filters[i].groupIndex);
	    }
	}
	else
	{
	    b2Log("    fd.filter.categoryBits = uint16(%d);\n", m_filters[0].categoryBits);
	    b2Log("    fd.filter.maskBits = uint16(%d);\n", m_filters[0].maskBits);
	    b2Log("    fd.filter.groupIndex = int16(%d);\n", m_filters[0].groupIndex);
	}

	switch (m_shape->m_type)
	{
	case b2Shape::e_circle:
		{
			b2CircleShape* s = (b2CircleShape*)m_shape;
			b2Log("    b2CircleShape shape;\n");
			b2Log("    shape.m_radius = %.15lef;\n", s->m_radius);
			b2Log("    shape.m_p.Set(%.15lef, %.15lef);\n", s->m_p.x, s->m_p.y);
		}
		break;

	case b2Shape::e_edge:
		{
			b2EdgeShape* s = (b2EdgeShape*)m_shape;
			b2Log("    b2EdgeShape shape;\n");
			b2Log("    shape.m_radius = %.15lef;\n", s->m_radius);
			b2Log("    shape.m_vertex0.Set(%.15lef, %.15lef);\n", s->m_vertex0.x, s->m_vertex0.y);
			b2Log("    shape.m_vertex1.Set(%.15lef, %.15lef);\n", s->m_vertex1.x, s->m_vertex1.y);
			b2Log("    shape.m_vertex2.Set(%.15lef, %.15lef);\n", s->m_vertex2.x, s->m_vertex2.y);
			b2Log("    shape.m_vertex3.Set(%.15lef, %.15lef);\n", s->m_vertex3.x, s->m_vertex3.y);
			b2Log("    shape.m_hasVertex0 = bool(%d);\n", s->m_hasVertex0);
			b2Log("    shape.m_hasVertex3 = bool(%d);\n", s->m_hasVertex3);
		}
		break;

	case b2Shape::e_polygon:
		{
			b2PolygonShape* s = (b2PolygonShape*)m_shape;
			b2Log("    b2PolygonShape shape;\n");
			b2Log("    b2Vec2 vs[%d];\n", b2_maxPolygonVertices);
			for (int32 i = 0; i < s->m_vertexCount; ++i)
			{
				b2Log("    vs[%d].Set(%.15lef, %.15lef);\n", i, s->m_vertices[i].x, s->m_vertices[i].y);
			}
			b2Log("    shape.Set(vs, %d);\n", s->m_vertexCount);
		}
		break;

	case b2Shape::e_chain:
		{
			b2ChainShape* s = (b2ChainShape*)m_shape;
			b2Log("    b2ChainShape shape;\n");
			b2Log("    b2Vec2 vs[%d];\n", s->m_count);
			for (int32 i = 0; i < s->m_count; ++i)
			{
				b2Log("    vs[%d].Set(%.15lef, %.15lef);\n", i, s->m_vertices[i].x, s->m_vertices[i].y);
			}
			b2Log("    shape.CreateChain(vs, %d);\n", s->m_count);
			b2Log("    shape.m_prevVertex.Set(%.15lef, %.15lef);\n", s->m_prevVertex.x, s->m_prevVertex.y);
			b2Log("    shape.m_nextVertex.Set(%.15lef, %.15lef);\n", s->m_nextVertex.x, s->m_nextVertex.y);
			b2Log("    shape.m_hasPrevVertex = bool(%d);\n", s->m_hasPrevVertex);
			b2Log("    shape.m_hasNextVertex = bool(%d);\n", s->m_hasNextVertex);
		}
		break;

	default:
		return;
	}

	b2Log("\n");
	b2Log("    fd.shape = &shape;\n");
	b2Log("\n");
	b2Log("    bodies[%d]->CreateFixture(&fd);\n", bodyIndex);
}


// DEFOLD modifications
int32 b2Fixture::GetNumProxies() const
{
	return m_proxyCount;
}

const b2FixtureProxy* b2Fixture::GetProxy(int32 index) const
{
	return &m_proxies[index];
}
