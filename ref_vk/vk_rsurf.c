
#include "vk_local.h"

static void BoundPoly( int numverts, float *verts, vec3_t mins, vec3_t maxs )
{
	int	i, j;
	float	*v;

	ClearBounds( mins, maxs );

	for( i = 0, v = verts; i < numverts; i++ )
	{
		for( j = 0; j < 3; j++, v++ )
		{
			if( *v < mins[j] ) mins[j] = *v;
			if( *v > maxs[j] ) maxs[j] = *v;
		}
	}
}

static void SubdividePolygon_r( msurface_t *warpface, int numverts, float *verts )
{
	vec3_t		front[SUBDIVIDE_SIZE], back[SUBDIVIDE_SIZE];
	mextrasurf_t	*warpinfo = warpface->info;
	float		dist[SUBDIVIDE_SIZE];
	float		m, frac, s, t, *v;
	int		i, j, k, f, b;
	float		sample_size;
	vec3_t		mins, maxs;
	glpoly_t		*poly;
	model_t *loadmodel = gEngfuncs.Mod_GetCurrentLoadingModel();

	if( numverts > ( SUBDIVIDE_SIZE - 4 ))
		gEngfuncs.Host_Error( "Mod_SubdividePolygon: too many vertexes on face ( %i )\n", numverts );

	sample_size = gEngfuncs.Mod_SampleSizeForFace( warpface );
	BoundPoly( numverts, verts, mins, maxs );

	for( i = 0; i < 3; i++ )
	{
		m = ( mins[i] + maxs[i] ) * 0.5f;
		m = SUBDIVIDE_SIZE * floor( m / SUBDIVIDE_SIZE + 0.5f );
		if( maxs[i] - m < 8 ) continue;
		if( m - mins[i] < 8 ) continue;

		// cut it
		v = verts + i;
		for( j = 0; j < numverts; j++, v += 3 )
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v -= i;
		VectorCopy( verts, v );

		f = b = 0;
		v = verts;
		for( j = 0; j < numverts; j++, v += 3 )
		{
			if( dist[j] >= 0 )
			{
				VectorCopy( v, front[f] );
				f++;
			}

			if( dist[j] <= 0 )
			{
				VectorCopy (v, back[b]);
				b++;
			}

			if( dist[j] == 0 || dist[j+1] == 0 )
				continue;

			if(( dist[j] > 0 ) != ( dist[j+1] > 0 ))
			{
				// clip point
				frac = dist[j] / ( dist[j] - dist[j+1] );
				for( k = 0; k < 3; k++ )
					front[f][k] = back[b][k] = v[k] + frac * (v[3+k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon_r( warpface, f, front[0] );
		SubdividePolygon_r( warpface, b, back[0] );
		return;
	}

	if( numverts != 4 )
		ClearBits( warpface->flags, SURF_DRAWTURB_QUADS );

	// add a point in the center to help keep warp valid
	poly = Mem_Calloc( loadmodel->mempool, sizeof( glpoly_t ) + (numverts - 4) * VERTEXSIZE * sizeof( float ));
	poly->next = warpface->polys;
	poly->flags = warpface->flags;
	warpface->polys = poly;
	poly->numverts = numverts;

	for( i = 0; i < numverts; i++, verts += 3 )
	{
		VectorCopy( verts, poly->verts[i] );

		if( FBitSet( warpface->flags, SURF_DRAWTURB ))
		{
			s = DotProduct( verts, warpface->texinfo->vecs[0] );
			t = DotProduct( verts, warpface->texinfo->vecs[1] );
		}
		else
		{
			s = DotProduct( verts, warpface->texinfo->vecs[0] ) + warpface->texinfo->vecs[0][3];
			t = DotProduct( verts, warpface->texinfo->vecs[1] ) + warpface->texinfo->vecs[1][3];
			s /= warpface->texinfo->texture->width;
			t /= warpface->texinfo->texture->height;
		}

		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		// for speed reasons
		if( !FBitSet( warpface->flags, SURF_DRAWTURB ))
		{
			// lightmap texture coordinates
			s = DotProduct( verts, warpinfo->lmvecs[0] ) + warpinfo->lmvecs[0][3];
			s -= warpinfo->lightmapmins[0];
			s += warpface->light_s * sample_size;
			s += sample_size * 0.5;
			s /= BLOCK_SIZE * sample_size; //fa->texinfo->texture->width;

			t = DotProduct( verts, warpinfo->lmvecs[1] ) + warpinfo->lmvecs[1][3];
			t -= warpinfo->lightmapmins[1];
			t += warpface->light_t * sample_size;
			t += sample_size * 0.5;
			t /= BLOCK_SIZE * sample_size; //fa->texinfo->texture->height;

			poly->verts[i][5] = s;
			poly->verts[i][6] = t;
		}
	}
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void GL_SubdivideSurface( msurface_t *fa )
{
	vec3_t	verts[SUBDIVIDE_SIZE];
	int	numverts;
	int	i, lindex;
	float	*vec;
	model_t *loadmodel = gEngfuncs.Mod_GetCurrentLoadingModel();

	// convert edges back to a normal polygon
	numverts = 0;
	for( i = 0; i < fa->numedges; i++ )
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if( lindex > 0 ) vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy( vec, verts[numverts] );
		numverts++;
	}

	SetBits( fa->flags, SURF_DRAWTURB_QUADS ); // predict state

	// do subdivide
	SubdividePolygon_r( fa, numverts, verts[0] );
}

void R_DrawWorld( void )
{
	double	start, end;

	// paranoia issues: when gl_renderer is "0" we need have something valid for currententity
	// to prevent crashing until HeadShield drawing.
	RI.currententity = gEngfuncs.GetEntityByIndex( 0 );
	RI.currentmodel = RI.currententity->model;

	if( !RI.drawWorld || RI.onlyClientDraw )
		return;

	VectorCopy( RI.cullorigin, tr.modelorg );
/*	memset( gl_lms.lightmap_surfaces, 0, sizeof( gl_lms.lightmap_surfaces ));
	memset( fullbright_surfaces, 0, sizeof( fullbright_surfaces ));
	memset( detail_surfaces, 0, sizeof( detail_surfaces ));

	gl_lms.dynamic_surfaces = NULL;
	pglDisable( GL_ALPHA_TEST );
	pglDisable( GL_BLEND );
	tr.blend = 1.0f;

	R_ClearSkyBox ();

	start = gEngfuncs.pfnTime();
	if( RI.drawOrtho )
		R_DrawWorldTopView( WORLDMODEL->nodes, RI.frustum.clipFlags );
	else R_RecursiveWorldNode( WORLDMODEL->nodes, RI.frustum.clipFlags );
	end = gEngfuncs.pfnTime();

	r_stats.t_world_node = end - start;

	start = gEngfuncs.pfnTime();
	R_DrawVBO( !CVAR_TO_BOOL(r_fullbright) && !!WORLDMODEL->lightdata, true );

	R_DrawTextureChains();

	if( !ENGINE_GET_PARM( PARM_DEV_OVERVIEW ))
	{
		DrawDecalsBatch();
		GL_ResetFogColor();
		R_BlendLightmaps();
		R_RenderFullbrights();
		R_RenderDetails();

		if( skychain )
			R_DrawSkyBox();
	}

	end = gEngfuncs.pfnTime();

	r_stats.t_world_draw = end - start;
	tr.num_draw_decals = 0;
	skychain = NULL;

	R_DrawTriangleOutlines ();

	R_DrawWorldHull();*/
}
