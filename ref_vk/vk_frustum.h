
#pragma once

#define FRUSTUM_LEFT	0
#define FRUSTUM_RIGHT	1
#define FRUSTUM_BOTTOM	2
#define FRUSTUM_TOP		3
#define FRUSTUM_FAR		4
#define FRUSTUM_NEAR	5
#define FRUSTUM_PLANES	6

typedef struct gl_frustum_s
{
	mplane_t		planes[FRUSTUM_PLANES];
	unsigned int 	clipFlags;
} gl_frustum_t;

void GL_FrustumInitProj( gl_frustum_t *out, float flZNear, float flZFar, float flFovX, float flFovY );
void GL_FrustumInitOrtho( gl_frustum_t *out, float xLeft, float xRight, float yTop, float yBottom, float flZNear, float flZFar );
