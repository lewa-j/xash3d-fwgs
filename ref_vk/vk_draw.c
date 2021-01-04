
#include "vk_local.h"

/*
=============
R_GetSpriteParms

same as GetImageParms but used
for sprite models
=============
*/
void R_GetSpriteParms( int *frameWidth, int *frameHeight, int *numFrames, int currentFrame, const model_t *pSprite )
{
	mspriteframe_t	*pFrame;

	if( !pSprite || pSprite->type != mod_sprite ) return; // bad model ?
	pFrame = R_GetSpriteFrame( pSprite, currentFrame, 0.0f );

	if( frameWidth ) *frameWidth = pFrame->width;
	if( frameHeight ) *frameHeight = pFrame->height;
	if( numFrames ) *numFrames = pSprite->numframes;
}

int R_GetSpriteTexture( const model_t *m_pSpriteModel, int frame )
{
	if( !m_pSpriteModel || m_pSpriteModel->type != mod_sprite || !m_pSpriteModel->cache.data )
		return 0;

	return R_GetSpriteFrame( m_pSpriteModel, frame, 0.0f )->gl_texturenum;
}

void R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, int texnum )
{
	//STUB
	if(vk_current2D_draw >= 1023)
		return;
	if(currentVert >= 4*1024*1024)
		return;

	if(vk_current2D_draw == 0 && vk_draws[vk_current2D_draw].count == 0)
		vk_draws[vk_current2D_draw].tex = texnum;
	
	if(vk_draws[vk_current2D_draw].tex != texnum || vk_draws[vk_current2D_draw].renderMode != vkState.renderMode)
	{
		vk_current2D_draw++;
		vk_draws[vk_current2D_draw].tex = texnum;
		vk_draws[vk_current2D_draw].renderMode = vkState.renderMode;
		vk_draws[vk_current2D_draw].offset = currentVert;
		vk_draws[vk_current2D_draw].count = 0;
	}

	float x1 = (x/swapchain.extent.width)*2-1;
	float x2 = ((x+w)/swapchain.extent.width)*2-1;
	float y1 = (y/swapchain.extent.height)*2-1;
	float y2 = ((y+h)/swapchain.extent.height)*2-1;

	tempQuadVerts[currentVert++] = (testVert_t){{x1, y1, 0},{s1, t1}}; Vector4Copy( vkState.color, tempQuadVerts[currentVert-1].color );
	tempQuadVerts[currentVert++] = (testVert_t){{x1, y2, 0},{s1, t2}}; Vector4Copy( vkState.color, tempQuadVerts[currentVert-1].color );
	tempQuadVerts[currentVert++] = (testVert_t){{x2, y2, 0},{s2, t2}}; Vector4Copy( vkState.color, tempQuadVerts[currentVert-1].color );

	tempQuadVerts[currentVert++] = (testVert_t){{x1, y1, 0},{s1, t1}}; Vector4Copy( vkState.color, tempQuadVerts[currentVert-1].color );
	tempQuadVerts[currentVert++] = (testVert_t){{x2, y2, 0},{s2, t2}}; Vector4Copy( vkState.color, tempQuadVerts[currentVert-1].color );
	tempQuadVerts[currentVert++] = (testVert_t){{x2, y1, 0},{s2, t1}}; Vector4Copy( vkState.color, tempQuadVerts[currentVert-1].color );

	vk_draws[vk_current2D_draw].count += 6;
}


//STUB
void R_Set2DMode( qboolean enable )
{
	if( enable )
	{
		if( vkState.in2DMode )
			return;

		//Viewport ( 0, 0, gpGlobals->width, gpGlobals->height )
		//Ortho matrix ( 0, gpGlobals->width, gpGlobals->height, 0, -99999, 99999 )
		//No cull
		//No depth test/write
		//Alpha test on
		Vector4Set( vkState.color, 1.0f, 1.0f, 1.0f, 1.0f );

		vkState.in2DMode = true;
		RI.currententity = NULL;
		RI.currentmodel = NULL;
	}
	else
	{
		//Depth test/write on
		vkState.in2DMode = false;
		//Set matrices RI.projectionMatrix RI.worldviewMatrix
		//Cull front
	}
}

