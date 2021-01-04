
#include "vk_local.h"

void GL_FrustumSetPlane( gl_frustum_t *out, int side, const vec3_t vecNormal, float flDist )
{
	Assert( side >= 0 && side < FRUSTUM_PLANES );

	out->planes[side].type = PlaneTypeForNormal( vecNormal );
	out->planes[side].signbits = SignbitsForPlane( vecNormal );
	VectorCopy( vecNormal, out->planes[side].normal );
	out->planes[side].dist = flDist;

	SetBits( out->clipFlags, BIT( side ));
}

void GL_FrustumInitProj( gl_frustum_t *out, float flZNear, float flZFar, float flFovX, float flFovY )
{
	float	xs, xc;
	vec3_t	farpoint, nearpoint;
	vec3_t	normal, iforward;

	// horizontal fov used for left and right planes
	SinCos( DEG2RAD( flFovX ) * 0.5f, &xs, &xc );

	// setup left plane
	VectorMAM( xs, RI.cull_vforward, -xc, RI.cull_vright, normal );
	GL_FrustumSetPlane( out, FRUSTUM_LEFT, normal, DotProduct( RI.cullorigin, normal ));

	// setup right plane
	VectorMAM( xs, RI.cull_vforward, xc, RI.cull_vright, normal );
	GL_FrustumSetPlane( out, FRUSTUM_RIGHT, normal, DotProduct( RI.cullorigin, normal ));

	// vertical fov used for top and bottom planes
	SinCos( DEG2RAD( flFovY ) * 0.5f, &xs, &xc );
	VectorNegate( RI.cull_vforward, iforward );

	// setup bottom plane
	VectorMAM( xs, RI.cull_vforward, -xc, RI.cull_vup, normal );
	GL_FrustumSetPlane( out, FRUSTUM_BOTTOM, normal, DotProduct( RI.cullorigin, normal ));

	// setup top plane
	VectorMAM( xs, RI.cull_vforward, xc, RI.cull_vup, normal );
	GL_FrustumSetPlane( out, FRUSTUM_TOP, normal, DotProduct( RI.cullorigin, normal ));

	// setup far plane
	VectorMA( RI.cullorigin, flZFar, RI.cull_vforward, farpoint );
	GL_FrustumSetPlane( out, FRUSTUM_FAR, iforward, DotProduct( iforward, farpoint ));

	// no need to setup backplane for general view.
	if( flZNear == 0.0f ) return;

	// setup near plane
	VectorMA( RI.cullorigin, flZNear, RI.cull_vforward, nearpoint );
	GL_FrustumSetPlane( out, FRUSTUM_NEAR, RI.cull_vforward, DotProduct( RI.cull_vforward, nearpoint ));
}

void GL_FrustumInitOrtho( gl_frustum_t *out, float xLeft, float xRight, float yTop, float yBottom, float flZNear, float flZFar )
{
	vec3_t	iforward, iright, iup;

	// setup the near and far planes
	float orgOffset = DotProduct( RI.cullorigin, RI.cull_vforward );
	VectorNegate( RI.cull_vforward, iforward );

	// because quake ortho is inverted and far and near should be swaped
	GL_FrustumSetPlane( out, FRUSTUM_FAR, iforward, -flZNear - orgOffset );
	GL_FrustumSetPlane( out, FRUSTUM_NEAR, RI.cull_vforward, flZFar + orgOffset );

	// setup left and right planes
	orgOffset = DotProduct( RI.cullorigin, RI.cull_vright );
	VectorNegate( RI.cull_vright, iright );

	GL_FrustumSetPlane( out, FRUSTUM_LEFT, RI.cull_vright, xLeft + orgOffset );
	GL_FrustumSetPlane( out, FRUSTUM_RIGHT, iright, -xRight - orgOffset );

	// setup top and buttom planes
	orgOffset = DotProduct( RI.cullorigin, RI.cull_vup );
	VectorNegate( RI.cull_vup, iup );

	GL_FrustumSetPlane( out, FRUSTUM_TOP, RI.cull_vup, yTop + orgOffset );
	GL_FrustumSetPlane( out, FRUSTUM_BOTTOM, iup, -yBottom - orgOffset );
}
