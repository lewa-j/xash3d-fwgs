
#include "vk_local.h"

#define DECAL_OVERLAP_DISTANCE	2

decal_t	gDecalPool[MAX_RENDER_DECALS];
static int	gDecalCount;


void R_ClearDecals( void )
{
	memset( gDecalPool, 0, sizeof( gDecalPool ));
	gDecalCount = 0;
}

/*
=============================================================

  DECALS SERIALIZATION

=============================================================
*/
static qboolean R_DecalUnProject( decal_t *pdecal, decallist_t *entry )
{
	if( !pdecal || !( pdecal->psurface ))
		return false;

	VectorCopy( pdecal->position, entry->position );
	entry->entityIndex = pdecal->entityIndex;

	// Grab surface plane equation
	if( pdecal->psurface->flags & SURF_PLANEBACK )
		VectorNegate( pdecal->psurface->plane->normal, entry->impactPlaneNormal );
	else VectorCopy( pdecal->psurface->plane->normal, entry->impactPlaneNormal );

	return true;
}

static int DecalListAdd( decallist_t *pList, int count )
{
	vec3_t		tmp;
	decallist_t	*pdecal;
	int		i;

	pdecal = pList + count;

	for( i = 0; i < count; i++ )
	{
		if( !Q_strcmp( pdecal->name, pList[i].name ) &&  pdecal->entityIndex == pList[i].entityIndex )
		{
			VectorSubtract( pdecal->position, pList[i].position, tmp );	// Merge

			if( VectorLength( tmp ) < DECAL_OVERLAP_DISTANCE )
				return count;
		}
	}

	// this is a new decal
	return count + 1;
}

static int DecalDepthCompare( const void *a, const void *b )
{
	const decallist_t	*elem1, *elem2;

	elem1 = (const decallist_t *)a;
	elem2 = (const decallist_t *)b;

	if( elem1->depth > elem2->depth )
		return 1;
	if( elem1->depth < elem2->depth )
		return -1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Called by CSaveRestore::SaveClientState
// Input  : *pList -
// Output : int
//-----------------------------------------------------------------------------
int R_CreateDecalList( decallist_t *pList )
{
	int	total = 0;
	int	i, depth;

	if( WORLDMODEL )
	{
		for( i = 0; i < MAX_RENDER_DECALS; i++ )
		{
			decal_t	*decal = &gDecalPool[i];
			decal_t	*pdecals;

			// decal is in use and is not a custom decal
			if( decal->psurface == NULL || FBitSet( decal->flags, FDECAL_DONTSAVE ))
				 continue;

			// compute depth
			depth = 0;
			pdecals = decal->psurface->pdecals;

			while( pdecals && pdecals != decal )
			{
				depth++;
				pdecals = pdecals->pnext;
			}

			pList[total].depth = depth;
			pList[total].flags = decal->flags;
			pList[total].scale = decal->scale;

			R_DecalUnProject( decal, &pList[total] );
			COM_FileBase( R_GetTexture( decal->texture )->name, pList[total].name );

			// check to see if the decal should be added
			total = DecalListAdd( pList, total );
		}

		if( gEngfuncs.drawFuncs->R_CreateStudioDecalList )
		{
			total += gEngfuncs.drawFuncs->R_CreateStudioDecalList( pList, total );
		}
	}

	// sort the decals lowest depth first, so they can be re-applied in order
	qsort( pList, total, sizeof( decallist_t ), DecalDepthCompare );

	return total;
}
