
#include "vk_local.h"

ref_api_t      gEngfuncs;
ref_globals_t *gpGlobals;

//STUB
static void R_ClearScreen( void )
{
	//pglClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	//pglClear( GL_COLOR_BUFFER_BIT );
}

//STUB vk_alias.c
void Mod_LoadAliasModel( model_t *mod, const void *buffer, qboolean *loaded )
{}
//STUB
void Mod_AliasUnloadTextures( void *data )
{}

void Mod_BrushUnloadTextures( model_t *mod )
{
	int i;

	for( i = 0; i < mod->numtextures; i++ )
	{
		texture_t *tx = mod->textures[i];
		if( !tx || tx->gl_texturenum == tr.defaultTexture )
			continue;       // free slot

		GL_FreeTexture( tx->gl_texturenum );    // main texture
		GL_FreeTexture( tx->fb_texturenum );    // luma texture
	}
}

void Mod_UnloadTextures( model_t *mod )
{
	Assert( mod != NULL );

	switch( mod->type )
	{
	case mod_studio:
		Mod_StudioUnloadTextures( mod->cache.data );
		break;
	case mod_alias:
		Mod_AliasUnloadTextures( mod->cache.data );
		break;
	case mod_brush:
		Mod_BrushUnloadTextures( mod );
		break;
	case mod_sprite:
		Mod_SpriteUnloadTextures( mod->cache.data );
		break;
	default: gEngfuncs.Host_Error( "Mod_UnloadModel: unsupported type %d\n", mod->type );
	}
}

//STUB
qboolean Mod_ProcessRenderData( model_t *mod, qboolean create, const byte *buf )
{
	qboolean loaded = true;

	if( create )
	{
		switch( mod->type )
		{
			case mod_studio:
				// Mod_LoadStudioModel( mod, buf, loaded );
				break;
			case mod_sprite:
				Mod_LoadSpriteModel( mod, buf, &loaded, mod->numtexinfo );
				break;
			case mod_alias:
				Mod_LoadAliasModel( mod, buf, &loaded );
				break;
			case mod_brush:
				// Mod_LoadBrushModel( mod, buf, loaded );
				break;

			default: gEngfuncs.Host_Error( "Mod_ProcessRenderData: unsupported type %d\n", mod->type );
		}
	}

	if( loaded && gEngfuncs.drawFuncs->Mod_ProcessUserData )
		gEngfuncs.drawFuncs->Mod_ProcessUserData( mod, create, buf );

	if( !create )
		Mod_UnloadTextures( mod );

	return loaded;
}
//STUB
static int GL_RefGetParm( int parm, int arg )
{
	vk_texture_t *vkt;
	
	switch( parm )
	{
	case PARM_TEX_WIDTH:
		vkt = R_GetTexture( arg );
		return vkt->width;
	case PARM_TEX_HEIGHT:
		vkt = R_GetTexture( arg );
		return vkt->height;
	case PARM_TEX_SRC_WIDTH:
		vkt = R_GetTexture( arg );
		return vkt->srcWidth;
	case PARM_TEX_SRC_HEIGHT:
		vkt = R_GetTexture( arg );
		return vkt->srcHeight;
	//PARM_TEX_GLFORMAT
	//PARM_TEX_ENCODE
	case PARM_TEX_MIPCOUNT:
		vkt = R_GetTexture( arg );
		return vkt->numMips;
	case PARM_TEX_DEPTH:
		vkt = R_GetTexture( arg );
		return vkt->depth;
	//PARM_TEX_SKYBOX
	//PARM_TEX_SKYTEXNUM
	//PARM_TEX_LIGHTMAP
	case PARM_WIDESCREEN:
		return gpGlobals->wideScreen;
	case PARM_FULLSCREEN:
		return gpGlobals->fullScreen;
	case PARM_SCREEN_WIDTH:
		return gpGlobals->width;
	case PARM_SCREEN_HEIGHT:
		return gpGlobals->height;
	//PARM_TEX_TARGET
	//PARM_TEX_TEXNUM
	case PARM_TEX_FLAGS:
		vkt = R_GetTexture( arg );
		return vkt->flags;
	//PARM_ACTIVE_TMU
	case PARM_LIGHTSTYLEVALUE:
		arg = bound( 0, arg, MAX_LIGHTSTYLES - 1 );
		return tr.lightstylevalue[arg];
	//PARM_MAX_IMAGE_UNITS
	//PARM_REBUILD_GAMMA
	//PARM_SURF_SAMPLESIZE
	//PARM_GL_CONTEXT_TYPE
	//PARM_GLES_WRAPPER
	//PARM_STENCIL_ACTIVE
	case PARM_SKY_SPHERE:
		return gEngfuncs.EngineGetParm( parm, arg ) && !tr.fCustomSkybox;
	default:
		return gEngfuncs.EngineGetParm( parm, arg );
	}
	
	return 0;
}

void R_ProcessEntData( qboolean allocate )
{
	if( !allocate )
	{
		tr.draw_list->num_solid_entities = 0;
		tr.draw_list->num_trans_entities = 0;
		tr.draw_list->num_beam_entities = 0;
	}

	if( gEngfuncs.drawFuncs->R_ProcessEntData )
		gEngfuncs.drawFuncs->R_ProcessEntData( allocate );
}

qboolean R_SetDisplayTransform( ref_screen_rotation_t rotate, int offset_x, int offset_y, float scale_x, float scale_y )
{
	qboolean ret = true;
	if( rotate > 0 )
	{
		gEngfuncs.Con_Printf("rotation transform not supported\n");
		ret = false;
	}

	if( offset_x || offset_y )
	{
		gEngfuncs.Con_Printf("offset transform not supported\n");
		ret = false;
	}

	if( scale_x != 1.0f || scale_y != 1.0f )
	{
		gEngfuncs.Con_Printf("scale transform not supported\n");
		ret = false;
	}

	return ret;
}

static const char *R_GetConfigName( void )
{
	return "vulkan";
}

ref_interface_t gReffuncs =
{
	R_Init,
	R_Shutdown,
	R_GetConfigName,
	R_SetDisplayTransform,

	0,//GL_SetupAttributes,
	GL_InitExtensions,
	GL_ClearExtensions,

	R_BeginFrame,
	R_RenderScene,
	R_EndFrame,
	R_PushScene,
	R_PopScene,
	GL_BackendStartFrame,
	GL_BackendEndFrame,

	R_ClearScreen,
	R_AllowFog,
	GL_SetRenderMode,

	R_AddEntity,
	0,//CL_AddCustomBeam,
	R_ProcessEntData,

	R_ShowTextures,

	0,//R_GetTextureOriginalBuffer,
	0,//GL_LoadTextureFromBuffer,
	GL_ProcessTexture,
	0,//R_SetupSky,

	R_Set2DMode,
	0,//R_DrawStretchRaw,
	R_DrawStretchPic,
	0,//R_DrawTileClear,
	0,//CL_FillRGBA,
	0,//CL_FillRGBABlend,

	0,//VID_ScreenShot,
	0,//VID_CubemapShot,

	0,//R_LightPoint,

	0,//R_DecalShoot,
	0,//R_DecalRemoveAll,
	R_CreateDecalList,
	0,//R_ClearAllDecals,

	R_StudioEstimateFrame,
	0,//R_StudioLerpMovement,
	CL_InitStudioAPI,

	0,//R_InitSkyClouds,
	GL_SubdivideSurface,
	CL_RunLightStyles,

	R_GetSpriteParms,
	R_GetSpriteTexture,

	Mod_LoadMapSprite,
	Mod_ProcessRenderData,
	Mod_StudioLoadTextures,

	0,//CL_DrawParticles,
	0,//CL_DrawTracers,
	0,//CL_DrawBeams,
	0,//R_BeamCull,

	GL_RefGetParm,
	0,//R_GetDetailScaleForTexture,
	0,//R_GetExtraParmsForTexture,
	0,//R_GetFrameTime,

	0,//R_SetCurrentEntity,
	0,//R_SetCurrentModel,

	GL_FindTexture,
	0,//GL_TextureName,
	0,//GL_TextureData,
	GL_LoadTexture,
	0,//GL_CreateTexture,
	0,//GL_LoadTextureArray,
	0,//GL_CreateTextureArray,
	GL_FreeTexture,

	0,//DrawSingleDecal,
	0,//R_DecalSetupVerts,
	0,//R_EntityRemoveDecals,

	0,//R_UploadStretchRaw,

	0,//GL_Bind,
	0,//GL_SelectTexture,
	0,//GL_LoadTexMatrixExt,
	0,//GL_LoadIdentityTexMatrix,
	0,//GL_CleanUpTextureUnits,
	0,//GL_TexGen,
	0,//GL_TextureTarget,
	0,//GL_SetTexCoordArrayMode,
	0,//GL_UpdateTexSize,
	NULL,
	NULL,

	0,//CL_DrawParticlesExternal,
	0,//R_LightVec,
	0,//R_StudioGetTexture,

	R_RenderFrame,
	0,//Mod_SetOrthoBounds,
	R_SpeedsMessage,
	0,//Mod_GetCurrentVis,
	R_NewMap,
	R_ClearScene,
	0,//R_GetProcAddress

	0,//TriRenderMode,
	0,//TriBegin,
	0,//TriEnd,
	0,//_TriColor4f,
	_TriColor4ub,
	0,//TriTexCoord2f,
	0,//TriVertex3fv,
	0,//TriVertex3f,
	0,//TriWorldToScreen,
	0,//TriFog,
	0,//R_ScreenToWorld,
	0,//TriGetMatrix,
	0,//TriFogParams,
	0,//TriCullFace,

	VGUI_DrawInit,
	VGUI_DrawShutdown,
	VGUI_SetupDrawingText,
	VGUI_SetupDrawingRect,
	VGUI_SetupDrawingImage,
	VGUI_BindTexture,
	VGUI_EnableTexture,
	VGUI_CreateTexture,
	VGUI_UploadTexture,
	VGUI_UploadTextureBlock,
	VGUI_DrawQuad,
	VGUI_GetTextureSizes,
	VGUI_GenerateTexture,
};

int EXPORT GetRefAPI( int version, ref_interface_t *funcs, ref_api_t *engfuncs, ref_globals_t *globals )
{
	if( version != REF_API_VERSION )
		return 0;

	// fill in our callbacks
	memcpy( funcs, &gReffuncs, sizeof( ref_interface_t ));
	memcpy( &gEngfuncs, engfuncs, sizeof( ref_api_t ));
	gpGlobals = globals;

	return REF_API_VERSION;
}

void EXPORT GetRefHumanReadableName( char *out, size_t size )
{
	Q_strncpy( out, "Vulkan", size );
}
