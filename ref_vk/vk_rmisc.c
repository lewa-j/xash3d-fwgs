
#include "vk_local.h"

//STUB
void R_NewMap( void )
{
	R_ClearDecals(); // clear all level decals


	vkState.isFogEnabled = false;
	tr.skytexturenum = -1;
	tr.max_recursion = 0;

	if( gEngfuncs.drawFuncs->R_NewMap != NULL )
		gEngfuncs.drawFuncs->R_NewMap();
}
