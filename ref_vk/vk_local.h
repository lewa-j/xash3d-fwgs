
#pragma once
#include "port.h"
#include "xash3d_types.h"
#include "cvardef.h"
#include "const.h"
#include "com_model.h"
#include "cl_entity.h"
#include "render_api.h"
#include "protocol.h"
#include "dlight.h"
#include "vk_frustum.h"
#include "ref_api.h"
#include "xash3d_mathlib.h"
#include "ref_params.h"
#include "enginefeatures.h"
#include "com_strings.h"
#include "pm_movevars.h"
#include "crtlib.h"
#include <stdint.h>

#define VK_NO_PROTOTYPES 1
#include "vulkan/vulkan.h"

#define VK_FUNCTION( fun ) extern PFN_##fun fun;
#define VK_GLOBAL_FUNCTION( fun ) extern PFN_##fun fun;
#define VK_EXT_FUNCTION( fun ) extern PFN_##fun fun;
#include "vk_funcs.h"

#define ASSERT(x) if(!( x )) gEngfuncs.Host_Error( "assert failed at %s:%i\n", __FILE__, __LINE__ )
#define Assert(x) if(!( x )) gEngfuncs.Host_Error( "assert failed at %s:%i\n", __FILE__, __LINE__ )

#define WORLDMODEL (gEngfuncs.pfnGetModelByIndex( 1 ))

extern byte	*r_temppool;

#define BLOCK_SIZE		tr.block_size	// lightmap blocksize

#define MAX_TEXTURES	4096
#define MAX_LIGHTMAPS	256
#define SUBDIVIDE_SIZE	64
#define MAX_DECAL_SURFS	4096
#define MAX_DRAW_STACK	2		// normal view and menu view

// refparams
#define RP_NONE		0
#define RP_ENVVIEW		BIT( 0 )	// used for cubemapshot
#define RP_OLDVIEWLEAF	BIT( 1 )
#define RP_CLIPPLANE	BIT( 2 )

#define CL_IsViewEntityLocalPlayer() ( gEngfuncs.EngineGetParm( PARM_VIEWENT_INDEX, 0 ) == gEngfuncs.EngineGetParm( PARM_PLAYER_INDEX, 0 ) )

typedef struct
{
	VkPhysicalDevice handle;
	VkPhysicalDeviceProperties props;
	VkPhysicalDeviceFeatures features;
	VkPhysicalDeviceMemoryProperties memProps;
	uint32_t queueFamilyCount;
	VkQueueFamilyProperties *queueProps;
	
	uint32_t queueFamilyIndex;
} VK_PhysicalDevice;

typedef struct {
	VkSurfaceKHR handle;
	VkSurfaceCapabilitiesKHR capabilities;
	uint32_t presentModesCount;
	VkPresentModeKHR *presentModes;
	uint32_t formatsCount;
	VkSurfaceFormatKHR *formats;
}VK_Surface;

typedef struct 
{
	VkSwapchainKHR handle;
	VkExtent2D extent;
	VkSurfaceFormatKHR surfaceFormat;
	VkPresentModeKHR presentMode;
	uint32_t imageCount;
	VkImage *images;
	VkImageView *imageViews;
}VK_Swapchain;

typedef struct
{
	VkBuffer handle;
	VkDeviceMemory memory;
	VkDeviceSize size;
} VK_Buffer;

typedef struct
{
	uint32_t bindingCount;
	VkVertexInputBindingDescription bindings[4];//16
	uint32_t attribCount;
	VkVertexInputAttributeDescription attribs[16];
} VK_VertexInputDesc;

typedef struct
{
	VkShaderModule shaderVert;
	VkShaderModule shaderFrag;
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
} VK_GraphicsPipeline;

extern VkInstance instance;
extern VK_PhysicalDevice physicalDevice;
extern VkDevice device;
extern VkQueue renderQueue;
extern VK_Surface surface;
extern VK_Swapchain swapchain;
extern VkCommandPool commandPool;
extern VkCommandBuffer renderCommandBuffer;
extern VkPipelineCache pipelineCache;

typedef struct vktexture_s
{
	char		name[256];	// game path, including extension (can be store image programs)
	word		srcWidth;		// keep unscaled sizes
	word		srcHeight;
	word		width;		// upload width\height
	word		height;
	word		depth;		// texture depth or count of layers for 2D_ARRAY
	byte		numMips;		// mipmap count

	VkFormat format;
	VkImage image;
	VkDeviceMemory memory;//FIXME
	VkImageView imageView;
	//VkSampler here?
	VkDescriptorSet descriptor;
	texFlags_t	flags;

	rgba_t		fogParams;	// some water textures
					// contain info about underwater fog
	rgbdata_t		*original;	// keep original image

	// debug info
	size_t		size;		// upload size for debug targets

	// detail textures stuff
	float		xscale;
	float		yscale;

	int		servercount;
	uint		hashValue;
	struct vktexture_s	*nextHash;
} vk_texture_t;

typedef struct
{
	int		params;		// rendering parameters

	qboolean		drawWorld;	// ignore world for drawing PlayerModel
	qboolean		isSkyVisible;	// sky is visible
	qboolean		onlyClientDraw;	// disabled by client request
	qboolean		drawOrtho;	// draw world as orthogonal projection	

	float		fov_x, fov_y;	// current view fov

	cl_entity_t	*currententity;
	model_t		*currentmodel;
	cl_entity_t	*currentbeam;	// same as above but for beams

	int		viewport[4];
	gl_frustum_t	frustum;

	mleaf_t		*viewleaf;
	mleaf_t		*oldviewleaf;
	vec3_t		pvsorigin;
	vec3_t		vieworg;		// locked vieworigin
	vec3_t		viewangles;
	vec3_t		vforward;
	vec3_t		vright;
	vec3_t		vup;

	vec3_t		cullorigin;
	vec3_t		cull_vforward;
	vec3_t		cull_vright;
	vec3_t		cull_vup;

	float		farClip;

	qboolean		fogCustom;
	qboolean		fogEnabled;
	qboolean		fogSkybox;
	vec4_t		fogColor;
	float		fogDensity;
	float		fogStart;
	float		fogEnd;
	int		cached_contents;	// in water
	int		cached_waterlevel;	// was in water

	float		skyMins[2][6];
	float		skyMaxs[2][6];

	matrix4x4		objectMatrix;		// currententity matrix
	matrix4x4		worldviewMatrix;		// modelview for world
	matrix4x4		modelviewMatrix;		// worldviewMatrix * objectMatrix

	matrix4x4		projectionMatrix;
	matrix4x4		worldviewProjectionMatrix;	// worldviewMatrix * projectionMatrix
	byte		visbytes[(MAX_MAP_LEAFS+7)/8];// actual PVS for current frame

	float		viewplanedist;
	mplane_t		clipPlane;
} ref_instance_t;

typedef struct
{
	cl_entity_t	*solid_entities[MAX_VISIBLE_PACKET];	// opaque moving or alpha brushes
	cl_entity_t	*trans_entities[MAX_VISIBLE_PACKET];	// translucent brushes
	cl_entity_t	*beam_entities[MAX_VISIBLE_PACKET];
	uint		num_solid_entities;
	uint		num_trans_entities;
	uint		num_beam_entities;
} draw_list_t;

typedef struct
{
	int		defaultTexture;   	// use for bad textures
	int		particleTexture;
	int		whiteTexture;
	int		grayTexture;
	int		blackTexture;
	int		solidskyTexture;	// quake1 solid-sky layer
	int		alphaskyTexture;	// quake1 alpha-sky layer
	int		lightmapTextures[MAX_LIGHTMAPS];
	int		dlightTexture;	// custom dlight texture
	int		skyboxTextures[6];	// skybox sides
	int		cinTexture;      	// cinematic texture

	int		skytexturenum;	// this not a gl_texturenum!
	int		skyboxbasenum;	// start with 5800

	// entity lists
	draw_list_t	draw_stack[MAX_DRAW_STACK];
	int		draw_stack_pos;
	draw_list_t	*draw_list;

	msurface_t	*draw_decals[MAX_DECAL_SURFS];
	int		num_draw_decals;
         
	// OpenGL matrix states
	qboolean		modelviewIdentity;

	int		visframecount;	// PVS frame
	int		dlightframecount;	// dynamic light frame
	int		realframecount;	// not including viewpasses
	int		framecount;

	qboolean		ignore_lightgamma;
	qboolean		fCustomRendering;
	qboolean		fResetVis;
	qboolean		fFlipViewModel;

	// tree visualization stuff
	int		recursion_level;
	int		max_recursion;

	int		lightstylevalue[MAX_LIGHTSTYLES];	// value 0 - 65536
	int		block_size;			// lightmap blocksize

	double		frametime;	// special frametime for multipass rendering (will set to 0 on a nextview)
	float		blend;		// global blend value

	// cull info
	vec3_t		modelorg;		// relative to viewpoint

	qboolean fCustomSkybox;
} gl_globals_t;

typedef struct
{
	uint		c_world_polys;
	uint		c_studio_polys;
	uint		c_sprite_polys;
	uint		c_alias_polys;
	uint		c_world_leafs;

	uint		c_view_beams_count;
	uint		c_active_tents_count;
	uint		c_alias_models_drawn;
	uint		c_studio_models_drawn;
	uint		c_sprite_models_drawn;
	uint		c_particle_count;

	uint		c_client_ents;	// entities that moved to client
	double		t_world_node;
	double		t_world_draw;
} ref_speeds_t;

typedef struct
{
	int width, height;
	int		activeTMU;
	uint32_t	isFogEnabled;
	int		faceCull;
	qboolean		stencilEnabled;
	qboolean		in2DMode;
	
	vec4_t color;
	uint32_t renderMode;
} vkstate_t;

extern ref_speeds_t		r_stats;
extern ref_instance_t	RI;
extern gl_globals_t	tr;
extern vkstate_t vkState;

// gl_backend.c
void GL_BackendStartFrame( void );
void GL_BackendEndFrame( void );

//vk_decals.c
void R_ClearDecals( void );
int R_CreateDecalList( decallist_t *pList );

// vk_draw.c
void R_GetSpriteParms( int *frameWidth, int *frameHeight, int *numFrames, int curFrame, const struct model_s *pSprite );
int R_GetSpriteTexture( const model_t *m_pSpriteModel, int frame );
void R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, int texnum );
void R_Set2DMode( qboolean enable );

//vk_texture.c
vk_texture_t *R_GetTexture( uint32_t texnum );
int GL_LoadTextureFromBuffer( const char *name, rgbdata_t *pic, texFlags_t flags, qboolean update );
void GL_ProcessTexture( int texnum, float gamma, int topColor, int bottomColor );
int GL_FindTexture( const char *name );
void GL_FreeTexture( uint32_t texnum );
void R_InitImages( void );
void R_ShutdownImages( void );
VkImage VK_CreateImage(VkDeviceMemory *memory, VkExtent2D size, uint32_t mipCount, uint32_t layersCount, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memFlags);
VkImageView VK_CreateImageView(VkImage img, VkImageViewType type, VkFormat format, VkImageAspectFlags aspect, uint32_t numMipLevels, uint32_t flags );
VkSampler VK_CreateSampler();

// vk_rlight.c
void CL_RunLightStyles( void );

// vk_rmain.c
void R_ClearScene( void );
void R_RenderScene( void );
void R_AllowFog( qboolean allowed );
void R_PushScene( void );
void R_PopScene( void );
int CL_FxBlend( cl_entity_t *e );

//vk_rmisc.c
void R_NewMap( void );

//vk_rsurf.c
void R_DrawWorld( void );
void GL_SubdivideSurface( msurface_t *fa );

//vk_sprite.c
void R_SpriteInit( void );
void Mod_LoadSpriteModel( model_t *mod, const void *buffer, qboolean *loaded, uint texFlags );
void Mod_LoadMapSprite( struct model_s *mod, const void *buffer, size_t size, qboolean *loaded );
void Mod_SpriteUnloadTextures( void *data );
mspriteframe_t *R_GetSpriteFrame( const model_t *pModel, int frame, float yaw );

// vk_studio.c
void R_StudioInit( void );
int R_GetEntityRenderMode( cl_entity_t *ent );
float R_StudioEstimateFrame( cl_entity_t *e, mstudioseqdesc_t *pseqdesc );
void CL_InitStudioAPI( void );
void R_RunViewmodelEvents( void );
void Mod_StudioLoadTextures( model_t *mod, void *data );
void Mod_StudioUnloadTextures( void *data );

//vulkan_main.c
void VK_Present();
int VK_FindMemoryType( uint32_t typeFilter, VkMemoryPropertyFlags properties );
VkCommandBuffer VK_BeginSingleTimeCommands();
int VK_EndSingleTimeCommands( VkCommandBuffer comBuf );
VK_Buffer VK_CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memFlags);

//vk_pipeline.c
VkShaderModule VK_CreateShader( const char *name );
VkPipelineLayout VK_CreatePipelineLayout( VkDescriptorSetLayout *descSetLayout, VkPushConstantRange *pushConst );
VK_GraphicsPipeline VK_CreateGraphicsPipeline( VkShaderModule vert, VkShaderModule frag, VK_VertexInputDesc *vertInputDesc, VkPrimitiveTopology topology, VkDescriptorSetLayout descSetLayout, VkPipelineLayout layout, VkRenderPass rPass, VkExtent2D extent, uint32_t renderMode );

typedef struct
{
	vec3_t pos;
	vec2_t uv;
	vec4_t color;
} testVert_t;
extern testVert_t *tempQuadVerts;
extern uint32_t currentVert;
typedef struct
{
	uint32_t offset;
	uint32_t count;
	uint32_t tex;
	uint32_t renderMode;
} VK_Draw2D_t;
extern VK_Draw2D_t vk_draws[1024];
extern uint32_t vk_current2D_draw;

//
// vk_vgui.c
//
void VGUI_DrawInit( void );
void VGUI_DrawShutdown( void );
void VGUI_SetupDrawingText( int *pColor );
void VGUI_SetupDrawingRect( int *pColor );
void VGUI_SetupDrawingImage( int *pColor );
void VGUI_BindTexture( int id );
void VGUI_EnableTexture( qboolean enable );
void VGUI_CreateTexture( int id, int width, int height );
void VGUI_UploadTexture( int id, const char *buffer, int width, int height );
void VGUI_UploadTextureBlock( int id, int drawX, int drawY, const byte *rgba, int blockWidth, int blockHeight );
void VGUI_DrawQuad( const vpoint_t *ul, const vpoint_t *lr );
void VGUI_GetTextureSizes( int *width, int *height );
int VGUI_GenerateTexture( void );

//
// renderer exports
//
qboolean R_Init( void );
void R_Shutdown( void );
void GL_InitExtensions( void );
void GL_ClearExtensions( void );
int GL_LoadTexture( const char *name, const byte *buf, size_t size, int flags );
void R_BeginFrame( qboolean clearScene );
void R_RenderFrame( const struct ref_viewpass_s *vp );
void R_EndFrame( void );
qboolean R_SpeedsMessage( char *out, size_t size );
qboolean R_AddEntity( struct cl_entity_s *pRefEntity, int entityType );
void R_ShowTextures( void );
void R_ShowTree( void );
void GL_SetRenderMode( int mode );
void R_NewMap( void );

//triapi
void _TriColor4ub( byte r, byte g, byte b, byte a );

extern ref_api_t      gEngfuncs;
extern ref_globals_t *gpGlobals;

extern cvar_t	*gl_nosort;

extern cvar_t	*r_speeds;
extern cvar_t	*r_norefresh;
extern cvar_t	*r_drawentities;
extern cvar_t	*r_lockfrustum;

#define Mem_Malloc( pool, size ) gEngfuncs._Mem_Alloc( pool, size, false, __FILE__, __LINE__ )
#define Mem_Calloc( pool, size ) gEngfuncs._Mem_Alloc( pool, size, true, __FILE__, __LINE__ )
#define Mem_Realloc( pool, ptr, size ) gEngfuncs._Mem_Realloc( pool, ptr, size, true, __FILE__, __LINE__ )
#define Mem_Free( mem ) gEngfuncs._Mem_Free( mem, __FILE__, __LINE__ )
#define Mem_AllocPool( name ) gEngfuncs._Mem_AllocPool( name, __FILE__, __LINE__ )
#define Mem_FreePool( pool ) gEngfuncs._Mem_FreePool( pool, __FILE__, __LINE__ )
#define Mem_EmptyPool( pool ) gEngfuncs._Mem_EmptyPool( pool, __FILE__, __LINE__ )
