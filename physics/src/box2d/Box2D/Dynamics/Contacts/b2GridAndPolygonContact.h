#ifndef B2_GRID_AND_POLYGON_CONTACT_H
#define B2_GRID_AND_POLYGON_CONTACT_H

#include <Box2D/Dynamics/Contacts/b2Contact.h>

class b2BlockAllocator;

class b2GridAndPolygonContact : public b2Contact
{
public:
	static b2Contact* Create(b2Fixture* fixtureA, int32 indexA,
	                         b2Fixture* fixtureB, int32 indexB, b2BlockAllocator* allocator);
	static void Destroy(b2Contact* contact, b2BlockAllocator* allocator);

	b2GridAndPolygonContact(b2Fixture* fixtureA, int32 indexA, b2Fixture* fixtureB);
	~b2GridAndPolygonContact() {}

	void Evaluate(b2Manifold* manifold, const b2Transform& xfA, const b2Transform& xfB);
};

#endif
