
#include "vk_local.h"
#include "r_studioint.h"


#define MAX_LOCALLIGHTS	4

typedef struct
{
	char		name[MAX_OSPATH];
	char		modelname[MAX_OSPATH];
	model_t		*model;
} player_model_t;

typedef struct sortedmesh_s
{
	mstudiomesh_t	*mesh;
	int		flags;			// face flags
} sortedmesh_t;

typedef struct
{
	double		time;
	double		frametime;
	int		framecount;		// studio framecount
	qboolean		interpolate;
	int		rendermode;
	float		blend;			// blend value

	// bones
	matrix3x4		rotationmatrix;
	matrix3x4		bonestransform[MAXSTUDIOBONES];
	matrix3x4		lighttransform[MAXSTUDIOBONES];

	// boneweighting stuff
	matrix3x4		worldtransform[MAXSTUDIOBONES];

	// cached bones
	matrix3x4		cached_bonestransform[MAXSTUDIOBONES];
	matrix3x4		cached_lighttransform[MAXSTUDIOBONES];
	char		cached_bonenames[MAXSTUDIOBONES][32];
	int		cached_numbones;		// number of bones in cache

	sortedmesh_t	meshes[MAXSTUDIOMESHES];	// sorted meshes
	vec3_t		verts[MAXSTUDIOVERTS];
	vec3_t		norms[MAXSTUDIOVERTS];

	// lighting state
	float		ambientlight;
	float		shadelight;
	vec3_t		lightvec;			// averaging light direction
	vec3_t		lightspot;		// shadow spot
	vec3_t		lightcolor;		// averaging lightcolor
	vec3_t		blightvec[MAXSTUDIOBONES];	// bone light vecs
	vec3_t		lightvalues[MAXSTUDIOVERTS];	// precomputed lightvalues per each shared vertex of submodel

	// chrome stuff
	vec3_t		chrome_origin;
	vec2_t		chrome[MAXSTUDIOVERTS];	// texture coords for surface normals
	vec3_t		chromeright[MAXSTUDIOBONES];	// chrome vector "right" in bone reference frames
	vec3_t		chromeup[MAXSTUDIOBONES];	// chrome vector "up" in bone reference frames
	int		chromeage[MAXSTUDIOBONES];	// last time chrome vectors were updated

	// glowshell stuff
	int		normaltable[MAXSTUDIOVERTS];	// glowshell uses this

	// elights cache
	int		numlocallights;
	int		lightage[MAXSTUDIOBONES];
	dlight_t		*locallight[MAX_LOCALLIGHTS];
	color24		locallightcolor[MAX_LOCALLIGHTS];
	vec4_t		lightpos[MAXSTUDIOVERTS][MAX_LOCALLIGHTS];
	vec3_t		lightbonepos[MAXSTUDIOBONES][MAX_LOCALLIGHTS];
	float		locallightR2[MAX_LOCALLIGHTS];

	// playermodels
	player_model_t  player_models[MAX_CLIENTS];

	// drawelements renderer
	vec3_t			arrayverts[MAXSTUDIOVERTS];
	vec2_t			arraycoord[MAXSTUDIOVERTS];
	uint16_t	arrayelems[MAXSTUDIOVERTS*6];
	uint8_t			arraycolor[MAXSTUDIOVERTS][4];
	uint			numverts;
	uint			numelems;
} studio_draw_state_t;


static cvar_t			*r_drawviewmodel;
cvar_t			*cl_righthand = NULL;
static cvar_t			*cl_himodels;

static r_studio_interface_t	*pStudioDraw;
static studio_draw_state_t	g_studio;		// global studio state


/*
====================
R_StudioInit

====================
*/
void R_StudioInit( void )
{
	cl_himodels = gEngfuncs.Cvar_Get( "cl_himodels", "1", FCVAR_ARCHIVE, "draw high-resolution player models in multiplayer" );
	//r_studio_sort_textures = gEngfuncs.Cvar_Get( "r_studio_sort_textures", "0", FCVAR_ARCHIVE, "change draw order for additive meshes" );
	r_drawviewmodel = gEngfuncs.Cvar_Get( "r_drawviewmodel", "1", 0, "draw firstperson weapon model" );
	//r_studio_drawelements = gEngfuncs.Cvar_Get( "r_studio_drawelements", "1", FCVAR_ARCHIVE, "use glDrawElements for studiomodels" );

	Matrix3x4_LoadIdentity( g_studio.rotationmatrix );
	//r_glowshellfreq = gEngfuncs.Cvar_Get( "r_glowshellfreq", "2.2", 0, "glowing shell frequency update" );

	g_studio.interpolate = true;
	g_studio.framecount = 0;
}


/*
===============
R_GetChromeSprite

===============
*/
static model_t *R_GetChromeSprite( void )
{
	return gEngfuncs.GetDefaultSprite( REF_CHROME_SPRITE );
}

/*
===============
pfnGetModelCounters

===============
*/
static void pfnGetModelCounters( int **s, int **a )
{
	*s = &g_studio.framecount;
	*a = &r_stats.c_studio_models_drawn;
}

/*
===============
pfnGetAliasScale

===============
*/
static void pfnGetAliasScale( float *x, float *y )
{
	if( x ) *x = 1.0f;
	if( y ) *y = 1.0f;
}

/*
===============
pfnStudioGetBoneTransform

===============
*/
static float ****pfnStudioGetBoneTransform( void )
{
	return (float ****)g_studio.bonestransform;
}

/*
===============
pfnStudioGetLightTransform

===============
*/
static float ****pfnStudioGetLightTransform( void )
{
	return (float ****)g_studio.lighttransform;
}

/*
===============
pfnStudioGetAliasTransform

===============
*/
static float ***pfnStudioGetAliasTransform( void )
{
	return NULL;
}

/*
===============
pfnStudioGetRotationMatrix

===============
*/
static float ***pfnStudioGetRotationMatrix( void )
{
	return (float ***)g_studio.rotationmatrix;
}

float R_StudioEstimateFrame( cl_entity_t *e, mstudioseqdesc_t *pseqdesc )
{
	double	dfdt, f;

	if( g_studio.interpolate )
	{
		if( g_studio.time < e->curstate.animtime ) dfdt = 0.0;
		else dfdt = (g_studio.time - e->curstate.animtime) * e->curstate.framerate * pseqdesc->fps;
	}
	else dfdt = 0;

	if( pseqdesc->numframes <= 1 ) f = 0.0;
	else f = (e->curstate.frame * (pseqdesc->numframes - 1)) / 256.0;
 
	f += dfdt;

	if( pseqdesc->flags & STUDIO_LOOPING ) 
	{
		if( pseqdesc->numframes > 1 )
			f -= (int)(f / (pseqdesc->numframes - 1)) *  (pseqdesc->numframes - 1);
		if( f < 0 ) f += (pseqdesc->numframes - 1);
	}
	else 
	{
		if( f >= pseqdesc->numframes - 1.001 )
			f = pseqdesc->numframes - 1.001;
		if( f < 0.0 )  f = 0.0;
	}
	return f;
}

static model_t *R_StudioSetupPlayerModel( int index )
{
	player_info_t	*info = gEngfuncs.pfnPlayerInfo( index );
	player_model_t	*state;

	state = &g_studio.player_models[index];

	// g-cont: force for "dev-mode", non-local games and menu preview
	if(( gpGlobals->developer || !gEngfuncs.EngineGetParm( PARM_LOCAL_GAME, 0 ) || !RI.drawWorld ) && info->model[0] )
	{
		if( Q_strcmp( state->name, info->model ))
		{
			Q_strncpy( state->name, info->model, sizeof( state->name ));
			state->name[sizeof( state->name ) - 1] = 0;

			Q_snprintf( state->modelname, sizeof( state->modelname ), "models/player/%s/%s.mdl", info->model, info->model );

			if( gEngfuncs.FS_FileExists( state->modelname, false ))
				state->model = gEngfuncs.Mod_ForName( state->modelname, false, true );
			else state->model = NULL;

			if( !state->model )
				state->model = RI.currententity->model;
		}
	}
	else
	{
		if( state->model != RI.currententity->model )
			state->model = RI.currententity->model;
		state->name[0] = 0;
	}

	return state->model;
}

/*
================
R_GetEntityRenderMode

check for texture flags
================
*/
int R_GetEntityRenderMode( cl_entity_t *ent )
{
	int		i, opaque, trans;
	mstudiotexture_t	*ptexture;
	cl_entity_t	*oldent;
	model_t		*model;
	studiohdr_t	*phdr;

	oldent = RI.currententity;
	RI.currententity = ent;

	if( ent->player ) // check it for real playermodel
		model = R_StudioSetupPlayerModel( ent->curstate.number - 1 );
	else model = ent->model;

	RI.currententity = oldent;

	if(( phdr = gEngfuncs.Mod_Extradata( mod_studio, model )) == NULL )
	{
		if( ent->curstate.rendermode == kRenderNormal )
		{
			// forcing to choose right sorting type
			if(( model && model->type == mod_brush ) && FBitSet( model->flags, MODEL_TRANSPARENT ))
				return kRenderTransAlpha;
		}
		return ent->curstate.rendermode;
	}
	ptexture = (mstudiotexture_t *)((byte *)phdr + phdr->textureindex);

	for( opaque = trans = i = 0; i < phdr->numtextures; i++, ptexture++ )
	{
		// ignore chrome & additive it's just a specular-like effect
		if( FBitSet( ptexture->flags, STUDIO_NF_ADDITIVE ) && !FBitSet( ptexture->flags, STUDIO_NF_CHROME ))
			trans++;
		else opaque++;
	}

	// if model is more additive than opaque
	if( trans > opaque )
		return kRenderTransAdd;
	return ent->curstate.rendermode;
}

/*
=================
R_RunViewmodelEvents
=================
*/
void R_RunViewmodelEvents( void )
{
	int	i;
	vec3_t simorg;

	if( r_drawviewmodel->value == 0 )
		return;

	if( gEngfuncs.EngineGetParm( PARM_THIRDPERSON, 0 ))
		return;

	// ignore in thirdperson, camera view or client is died
	if( FBitSet( RI.params, RP_ENVVIEW ) != 0 || gEngfuncs.EngineGetParm( PARM_LOCAL_HEALTH, 0 ) <= 0 || !CL_IsViewEntityLocalPlayer())
		return;

	RI.currententity = gEngfuncs.GetViewModel();

	if( !RI.currententity->model || RI.currententity->model->type != mod_studio )
		return;

	//R_StudioSetupTimings();

	gEngfuncs.GetPredictedOrigin( simorg );
	for( i = 0; i < 4; i++ )
		VectorCopy( simorg, RI.currententity->attachment[i] );
	RI.currentmodel = RI.currententity->model;

	//R_StudioDrawModelInternal( RI.currententity, STUDIO_EVENTS );
}

/*
====================
R_StudioLoadTexture

load model texture with unique name
====================
*/
static void R_StudioLoadTexture( model_t *mod, studiohdr_t *phdr, mstudiotexture_t *ptexture )
{
	size_t		size;
	int		flags = 0;
	char		texname[128], name[128], mdlname[128];
	texture_t		*tx = NULL;

	if( ptexture->flags & STUDIO_NF_NORMALMAP )
		flags |= (TF_NORMALMAP);

	// store some textures for remapping
	if( !Q_strnicmp( ptexture->name, "DM_Base", 7 ) || !Q_strnicmp( ptexture->name, "remap", 5 ))
	{
		int	i, size;
		char	val[6];
		byte	*pixels;

		i = mod->numtextures;
		mod->textures = (texture_t **)Mem_Realloc( mod->mempool, mod->textures, ( i + 1 ) * sizeof( texture_t* ));
		size = ptexture->width * ptexture->height + 768;
		tx = Mem_Calloc( mod->mempool, sizeof( *tx ) + size );
		mod->textures[i] = tx;

		// store ranges into anim_min, anim_max etc
		if( !Q_strnicmp( ptexture->name, "DM_Base", 7 ))
		{
			Q_strncpy( tx->name, "DM_Base", sizeof( tx->name ));
			tx->anim_min = PLATE_HUE_START; // topcolor start
			tx->anim_max = PLATE_HUE_END; // topcolor end
			// bottomcolor start always equal is (topcolor end + 1)
			tx->anim_total = SUIT_HUE_END;// bottomcolor end
		}
		else
		{
			Q_strncpy( tx->name, "DM_User", sizeof( tx->name )); // custom remapped
			Q_strncpy( val, ptexture->name + 7, 4 );
			tx->anim_min = bound( 0, Q_atoi( val ), 255 ); // topcolor start
			Q_strncpy( val, ptexture->name + 11, 4 );
			tx->anim_max = bound( 0, Q_atoi( val ), 255 ); // topcolor end
			// bottomcolor start always equal is (topcolor end + 1)
			Q_strncpy( val, ptexture->name + 15, 4 );
			tx->anim_total = bound( 0, Q_atoi( val ), 255 ); // bottomcolor end
		}

		tx->width = ptexture->width;
		tx->height = ptexture->height;

		// the pixels immediately follow the structures
		pixels = (byte *)phdr + ptexture->index;
		memcpy( tx+1, pixels, size );

		ptexture->flags |= STUDIO_NF_COLORMAP;	// yes, this is colormap image
		flags |= TF_FORCE_COLOR;

		mod->numtextures++;	// done
	}

	Q_strncpy( mdlname, mod->name, sizeof( mdlname ));
	COM_FileBase( ptexture->name, name );
	COM_StripExtension( mdlname );

	if( FBitSet( ptexture->flags, STUDIO_NF_NOMIPS ))
		SetBits( flags, TF_NOMIPMAP );

	// NOTE: replace index with pointer to start of imagebuffer, ImageLib expected it
	//ptexture->index = (int)((byte *)phdr) + ptexture->index;
	gEngfuncs.Image_SetMDLPointer((byte *)phdr + ptexture->index);
	size = sizeof( mstudiotexture_t ) + ptexture->width * ptexture->height + 768;

	if( FBitSet( gEngfuncs.EngineGetParm( PARM_FEATURES, 0 ), ENGINE_IMPROVED_LINETRACE ) && FBitSet( ptexture->flags, STUDIO_NF_MASKED ))
		flags |= TF_KEEP_SOURCE; // Paranoia2 texture alpha-tracing

	// build the texname
	Q_snprintf( texname, sizeof( texname ), "#%s/%s.mdl", mdlname, name );
	ptexture->index = GL_LoadTexture( texname, (byte *)ptexture, size, flags );

	if( !ptexture->index )
	{
		ptexture->index = tr.defaultTexture;
	}
	else if( tx )
	{
		// duplicate texnum for easy acess
		tx->gl_texturenum = ptexture->index;
	}
}

void Mod_StudioLoadTextures( model_t *mod, void *data )
{
	studiohdr_t	*phdr = (studiohdr_t *)data;
	mstudiotexture_t	*ptexture;
	int		i;

	if( !phdr )
		return;

	ptexture = (mstudiotexture_t *)(((byte *)phdr) + phdr->textureindex);
	if( phdr->textureindex > 0 && phdr->numtextures <= MAXSTUDIOSKINS )
	{
		for( i = 0; i < phdr->numtextures; i++ )
			R_StudioLoadTexture( mod, phdr, &ptexture[i] );
	}
}

void Mod_StudioUnloadTextures( void *data )
{
	studiohdr_t	*phdr = (studiohdr_t *)data;
	mstudiotexture_t	*ptexture;
	int		i;

	if( !phdr )
		return;

	ptexture = (mstudiotexture_t *)(((byte *)phdr) + phdr->textureindex);

	// release all textures
	for( i = 0; i < phdr->numtextures; i++ )
	{
		if( ptexture[i].index == tr.defaultTexture )
			continue;
		GL_FreeTexture( ptexture[i].index );
	}
}

static model_t *pfnModelHandle( int modelindex )
{
	return gEngfuncs.pfnGetModelByIndex( modelindex );
}

static void *pfnMod_CacheCheck( struct cache_user_s *c )
{
	return gEngfuncs.Mod_CacheCheck( c );
}

static void *pfnMod_StudioExtradata( model_t *mod )
{
	return gEngfuncs.Mod_Extradata( mod_studio, mod );
}

static void pfnMod_LoadCacheFile( const char *path, struct cache_user_s *cu )
{
	gEngfuncs.Mod_LoadCacheFile( path, cu );
}

static cvar_t *pfnGetCvarPointer( const char *name )
{
	return (cvar_t*)gEngfuncs.pfnGetCvarPointer( name, 0 );
}

static void *pfnMod_Calloc( int number, size_t size )
{
	return gEngfuncs.Mod_Calloc( number, size );
}

static engine_studio_api_t gStudioAPI =
{
	pfnMod_Calloc,
	pfnMod_CacheCheck,
	pfnMod_LoadCacheFile,
	0,//pfnMod_ForName,
	pfnMod_StudioExtradata,
	pfnModelHandle,
	0,//pfnGetCurrentEntity,
	0,//pfnPlayerInfo,
	0,//R_StudioGetPlayerState,
	0,//pfnGetViewEntity,
	0,//pfnGetEngineTimes,
	pfnGetCvarPointer,
	0,//pfnGetViewInfo,
	R_GetChromeSprite,
	pfnGetModelCounters,
	pfnGetAliasScale,
	pfnStudioGetBoneTransform,
	pfnStudioGetLightTransform,
	pfnStudioGetAliasTransform,
	pfnStudioGetRotationMatrix,
	0,//R_StudioSetupModel,
	0,//R_StudioCheckBBox,
	0,//R_StudioDynamicLight,
	0,//R_StudioEntityLight,
	0,//R_StudioSetupLighting,
	0,//R_StudioDrawPoints,
	0,//R_StudioDrawHulls,
	0,//R_StudioDrawAbsBBox,
	0,//R_StudioDrawBones,
	0,//(void*)R_StudioSetupSkin,
	0,//R_StudioSetRemapColors,
	R_StudioSetupPlayerModel,/*
	R_StudioClientEvents,
	R_StudioGetForceFaceFlags,
	R_StudioSetForceFaceFlags,
	(void*)R_StudioSetHeader,
	R_StudioSetRenderModel,
	R_StudioSetupRenderer,
	R_StudioRestoreRenderer,
	R_StudioSetChromeOrigin,
	pfnIsHardware,
	GL_StudioDrawShadow,
	GL_StudioSetRenderMode,
	R_StudioSetRenderamt,
	R_StudioSetCullState,
	R_StudioRenderShadow,*/
};

static r_studio_interface_t gStudioDraw =
{
	STUDIO_INTERFACE_VERSION,
//	R_StudioDrawModel,
//	R_StudioDrawPlayer,
};

/*
===============
CL_InitStudioAPI

Initialize client studio
===============
*/
void CL_InitStudioAPI( void )
{
	pStudioDraw = &gStudioDraw;

	// trying to grab them from client.dll
	cl_righthand = gEngfuncs.pfnGetCvarPointer( "cl_righthand", 0 );

	if( cl_righthand == NULL )
		cl_righthand = gEngfuncs.Cvar_Get( "cl_righthand", "0", FCVAR_ARCHIVE, "flip viewmodel (left to right)" );

	// Xash will be used internal StudioModelRenderer
	if( gEngfuncs.pfnGetStudioModelInterface( STUDIO_INTERFACE_VERSION, &pStudioDraw, &gStudioAPI ))
		return;

	// NOTE: we always return true even if game interface was not correct
	// because we need Draw our StudioModels
	// just restore pointer to builtin function
	pStudioDraw = &gStudioDraw;
}
