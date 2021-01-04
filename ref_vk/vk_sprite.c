
#include "vk_local.h"
#include "sprite.h"

// it's a Valve default value for LoadMapSprite (probably must be power of two)
#define MAPSPRITE_SIZE	128

cvar_t		*r_sprite_lerping;
cvar_t		*r_sprite_lighting;

char		sprite_name[MAX_QPATH];
char		group_suffix[8];
static uint	r_texFlags = 0;
static int	sprite_version;

void R_SpriteInit( void )
{
	r_sprite_lerping = gEngfuncs.Cvar_Get( "r_sprite_lerping", "1", FCVAR_ARCHIVE, "enables sprite animation lerping" );
	r_sprite_lighting = gEngfuncs.Cvar_Get( "r_sprite_lighting", "1", FCVAR_ARCHIVE, "enables sprite lighting (blood etc)" );
}

/*
====================
R_SpriteLoadFrame

upload a single frame
====================
*/
static const dframetype_t *R_SpriteLoadFrame( model_t *mod, const void *pin, mspriteframe_t **ppframe, int num )
{
	const dspriteframe_t	pinframe;
	mspriteframe_t	*pspriteframe;
	int		gl_texturenum = 0;
	char		texname[128];
	int		bytes = 1;

	memcpy( &pinframe, pin, sizeof(dspriteframe_t));

	if( sprite_version == SPRITE_VERSION_32 )
		bytes = 4;

	// build uinque frame name
	if( FBitSet( mod->flags, MODEL_CLIENT )) // it's a HUD sprite
	{
		Q_snprintf( texname, sizeof( texname ), "#HUD/%s(%s:%i%i).spr", sprite_name, group_suffix, num / 10, num % 10 );
		gl_texturenum = GL_LoadTexture( texname, pin, pinframe.width * pinframe.height * bytes, r_texFlags );
	}
	else
	{
		Q_snprintf( texname, sizeof( texname ), "#%s(%s:%i%i).spr", sprite_name, group_suffix, num / 10, num % 10 );
		gl_texturenum = GL_LoadTexture( texname, pin, pinframe.width * pinframe.height * bytes, r_texFlags );
	}

	// setup frame description
	pspriteframe = Mem_Malloc( mod->mempool, sizeof( mspriteframe_t ));
	pspriteframe->width = pinframe.width;
	pspriteframe->height = pinframe.height;
	pspriteframe->up = pinframe.origin[1];
	pspriteframe->left = pinframe.origin[0];
	pspriteframe->down = pinframe.origin[1] - pinframe.height;
	pspriteframe->right = pinframe.width + pinframe.origin[0];
	pspriteframe->gl_texturenum = gl_texturenum;
	*ppframe = pspriteframe;

	return ( (const byte*)pin + sizeof(dspriteframe_t) + pinframe.width * pinframe.height * bytes );
}

/*
====================
R_SpriteLoadGroup

upload a group frames
====================
*/
static const dframetype_t *R_SpriteLoadGroup( model_t *mod, const void *pin, mspriteframe_t **ppframe, int framenum )
{
	const dspritegroup_t	*pingroup;
	mspritegroup_t	*pspritegroup;
	const dspriteinterval_t	*pin_intervals;
	float		*poutintervals;
	int		i, groupsize, numframes;
	const void		*ptemp;

	pingroup = (const dspritegroup_t *)pin;
	numframes = pingroup->numframes;

	groupsize = sizeof( mspritegroup_t ) + (numframes - 1) * sizeof( pspritegroup->frames[0] );
	pspritegroup = Mem_Calloc( mod->mempool, groupsize );
	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;
	pin_intervals = (const dspriteinterval_t *)(pingroup + 1);
	poutintervals = Mem_Calloc( mod->mempool, numframes * sizeof( float ));
	pspritegroup->intervals = poutintervals;

	for( i = 0; i < numframes; i++ )
	{
		*poutintervals = pin_intervals->interval;
		if( *poutintervals <= 0.0f )
			*poutintervals = 1.0f; // set error value
		poutintervals++;
		pin_intervals++;
	}

	ptemp = (const void *)pin_intervals;
	for( i = 0; i < numframes; i++ )
	{
		ptemp = R_SpriteLoadFrame( mod, ptemp, &pspritegroup->frames[i], framenum * 10 + i );
	}

	return (const dframetype_t *)ptemp;
}

/*
====================
Mod_LoadSpriteModel

load sprite model
====================
*/
void Mod_LoadSpriteModel( model_t *mod, const void *buffer, qboolean *loaded, uint texFlags )
{
	const dsprite_t		*pin;
	const short		*numi = NULL;
	const dframetype_t	*pframetype;
	msprite_t		*psprite;
	int		i;

	pin = buffer;
	psprite = mod->cache.data;

	if( pin->version == SPRITE_VERSION_Q1 || pin->version == SPRITE_VERSION_32 )
		numi = NULL;
	else if( pin->version == SPRITE_VERSION_HL )
		numi = (const short *)((const byte*)buffer + sizeof( dsprite_hl_t ));

	r_texFlags = texFlags;
	sprite_version = pin->version;
	Q_strncpy( sprite_name, mod->name, sizeof( sprite_name ));
	COM_StripExtension( sprite_name );

	if( numi == NULL )
	{
		rgbdata_t	*pal;

		pal = gEngfuncs.FS_LoadImage( "#id.pal", (byte *)&i, 768 );
		pframetype = (const dframetype_t *)((const byte*)buffer + sizeof( dsprite_q1_t )); // pinq1 + 1
		gEngfuncs.FS_FreeImage( pal ); // palette installed, no reason to keep this data
	}
	else if( *numi == 256 )
	{
		const byte	*src = (const byte *)(numi+1);
		rgbdata_t	*pal;

		// install palette
		switch( psprite->texFormat )
		{
		case SPR_INDEXALPHA:
			pal = gEngfuncs.FS_LoadImage( "#gradient.pal", src, 768 );
			break;
		case SPR_ALPHTEST:
			pal = gEngfuncs.FS_LoadImage( "#masked.pal", src, 768 );
			break;
		default:
			pal = gEngfuncs.FS_LoadImage( "#normal.pal", src, 768 );
			break;
		}

		pframetype = (const dframetype_t *)(src + 768);
		gEngfuncs.FS_FreeImage( pal ); // palette installed, no reason to keep this data
	}
	else
	{
		gEngfuncs.Con_DPrintf( S_ERROR "%s has wrong number of palette colors %i (should be 256)\n", mod->name, *numi );
		return;
	}

	if( mod->numframes < 1 )
		return;

	for( i = 0; i < mod->numframes; i++ )
	{
		frametype_t frametype = pframetype->type;
		psprite->frames[i].type = frametype;

		switch( frametype )
		{
		case FRAME_SINGLE:
			Q_strncpy( group_suffix, "frame", sizeof( group_suffix ));
			pframetype = R_SpriteLoadFrame( mod, pframetype + 1, &psprite->frames[i].frameptr, i );
			break;
		case FRAME_GROUP:
			Q_strncpy( group_suffix, "group", sizeof( group_suffix ));
			pframetype = R_SpriteLoadGroup( mod, pframetype + 1, &psprite->frames[i].frameptr, i );
			break;
		case FRAME_ANGLED:
			Q_strncpy( group_suffix, "angle", sizeof( group_suffix ));
			pframetype = R_SpriteLoadGroup( mod, pframetype + 1, &psprite->frames[i].frameptr, i );
			break;
		}
		if( pframetype == NULL ) break; // technically an error
	}

	if( loaded ) *loaded = true;	// done
}

/*
====================
Mod_LoadMapSprite

Loading a bitmap image as sprite with multiple frames
as pieces of input image
====================
*/
void Mod_LoadMapSprite( model_t *mod, const void *buffer, size_t size, qboolean *loaded )
{
	byte		*src, *dst;
	rgbdata_t		*pix, temp;
	char		texname[128];
	int		i, j, x, y, w, h;
	int		xl, yl, xh, yh;
	int		linedelta, numframes;
	mspriteframe_t	*pspriteframe;
	msprite_t		*psprite;

	if( loaded ) *loaded = false;
	Q_snprintf( texname, sizeof( texname ), "#%s", mod->name );
	gEngfuncs.Image_SetForceFlags( IL_OVERVIEW );
	pix = gEngfuncs.FS_LoadImage( texname, buffer, size );
	gEngfuncs.Image_ClearForceFlags();
	if( !pix ) return;	// bad image or something else

	mod->type = mod_sprite;
	r_texFlags = 0; // no custom flags for map sprites

	if( pix->width % MAPSPRITE_SIZE )
		w = pix->width - ( pix->width % MAPSPRITE_SIZE );
	else w = pix->width;

	if( pix->height % MAPSPRITE_SIZE )
		h = pix->height - ( pix->height % MAPSPRITE_SIZE );
	else h = pix->height;

	if( w < MAPSPRITE_SIZE ) w = MAPSPRITE_SIZE;
	if( h < MAPSPRITE_SIZE ) h = MAPSPRITE_SIZE;

	// resample image if needed
	gEngfuncs.Image_Process( &pix, w, h, IMAGE_FORCE_RGBA|IMAGE_RESAMPLE, 0.0f );

	w = h = MAPSPRITE_SIZE;

	// check range
	if( w > pix->width ) w = pix->width;
	if( h > pix->height ) h = pix->height;

	// determine how many frames we needs
	numframes = (pix->width * pix->height) / (w * h);
	mod->mempool = Mem_AllocPool( va( "^2%s^7", mod->name ));
	psprite = Mem_Calloc( mod->mempool, sizeof( msprite_t ) + ( numframes - 1 ) * sizeof( psprite->frames ));
	mod->cache.data = psprite;	// make link to extradata

	psprite->type = SPR_FWD_PARALLEL_ORIENTED;
	psprite->texFormat = SPR_ALPHTEST;
	psprite->numframes = mod->numframes = numframes;
	psprite->radius = sqrt(((w >> 1) * (w >> 1)) + ((h >> 1) * (h >> 1)));

	mod->mins[0] = mod->mins[1] = -w / 2;
	mod->maxs[0] = mod->maxs[1] = w / 2;
	mod->mins[2] = -h / 2;
	mod->maxs[2] = h / 2;

	// create a temporary pic
	memset( &temp, 0, sizeof( temp ));
	temp.width = w;
	temp.height = h;
	temp.type = pix->type;
	temp.flags = pix->flags;
	temp.size = w * h * gEngfuncs.Image_GetPFDesc(temp.type)->bpp;
	temp.buffer = Mem_Malloc( r_temppool, temp.size );
	temp.palette = NULL;

	// chop the image and upload into video memory
	for( i = xl = yl = 0; i < numframes; i++ )
	{
		xh = xl + w;
		yh = yl + h;

		src = pix->buffer + ( yl * pix->width + xl ) * 4;
		linedelta = ( pix->width - w ) * 4;
		dst = temp.buffer;

		// cut block from source
		for( y = yl; y < yh; y++ )
		{
			for( x = xl; x < xh; x++ )
				for( j = 0; j < 4; j++ )
					*dst++ = *src++;
			src += linedelta;
		}

		// build uinque frame name
		Q_snprintf( texname, sizeof( texname ), "#MAP/%s_%i%i.spr", mod->name, i / 10, i % 10 );

		psprite->frames[i].frameptr = Mem_Calloc( mod->mempool, sizeof( mspriteframe_t ));
		pspriteframe = psprite->frames[i].frameptr;
		pspriteframe->width = w;
		pspriteframe->height = h;
		pspriteframe->up = ( h >> 1 );
		pspriteframe->left = -( w >> 1 );
		pspriteframe->down = ( h >> 1 ) - h;
		pspriteframe->right = w + -( w >> 1 );
		pspriteframe->gl_texturenum = GL_LoadTextureFromBuffer( texname, &temp, TF_IMAGE, false );

		xl += w;
		if( xl >= pix->width )
		{
			xl = 0;
			yl += h;
		}
	}

	gEngfuncs.FS_FreeImage( pix );
	Mem_Free( temp.buffer );

	if( loaded ) *loaded = true;
}

/*
====================
Mod_UnloadSpriteModel

release sprite model and frames
====================
*/
void Mod_SpriteUnloadTextures( void *data )
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int		i, j;

	psprite = data;

	if( psprite )
	{
		// release all textures
		for( i = 0; i < psprite->numframes; i++ )
		{
			if( psprite->frames[i].type == SPR_SINGLE )
			{
				pspriteframe = psprite->frames[i].frameptr;
				GL_FreeTexture( pspriteframe->gl_texturenum );
			}
			else
			{
				pspritegroup = (mspritegroup_t *)psprite->frames[i].frameptr;

				for( j = 0; j < pspritegroup->numframes; j++ )
				{
					pspriteframe = pspritegroup->frames[i];
					GL_FreeTexture( pspriteframe->gl_texturenum );
				}
			}
		}
	}
}

/*
================
R_GetSpriteFrame

assume pModel is valid
================
*/
mspriteframe_t *R_GetSpriteFrame( const model_t *pModel, int frame, float yaw )
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe = NULL;
	float		*pintervals, fullinterval;
	int		i, numframes;
	float		targettime;

	Assert( pModel != NULL );
	psprite = pModel->cache.data;

	if( frame < 0 )
	{
		frame = 0;
	}
	else if( frame >= psprite->numframes )
	{
		if( frame > psprite->numframes )
			gEngfuncs.Con_Printf( S_WARN "R_GetSpriteFrame: no such frame %d (%s)\n", frame, pModel->name );
		frame = psprite->numframes - 1;
	}

	if( psprite->frames[frame].type == SPR_SINGLE )
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else if( psprite->frames[frame].type == SPR_GROUP )
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by zero
		targettime = gpGlobals->time - ((int)( gpGlobals->time / fullinterval )) * fullinterval;

		for( i = 0; i < (numframes - 1); i++ )
		{
			if( pintervals[i] > targettime )
				break;
		}
		pspriteframe = pspritegroup->frames[i];
	}
	else if( psprite->frames[frame].type == FRAME_ANGLED )
	{
		int	angleframe = (int)(Q_rint(( RI.viewangles[1] - yaw + 45.0f ) / 360 * 8) - 4) & 7;

		// e.g. doom-style sprite monsters
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pspriteframe = pspritegroup->frames[angleframe];
	}

	return pspriteframe;
}
