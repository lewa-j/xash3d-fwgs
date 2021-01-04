
#include "vk_local.h"
#include "crtlib.h"

char		r_speeds_msg[MAX_SYSPATH];
ref_speeds_t	r_stats;	// r_speeds counters


qboolean R_SpeedsMessage( char *out, size_t size )
{
	if( gEngfuncs.drawFuncs->R_SpeedsMessage != NULL )
	{
		if( gEngfuncs.drawFuncs->R_SpeedsMessage( out, size ))
			return true;
		// otherwise pass to default handler
	}

	if( r_speeds->value <= 0 )
		return false;
	if( !out || !size ) return false;

	Q_strncpy( out, r_speeds_msg, size );

	return true;
}

void R_Speeds_Printf( const char *msg, ... )
{
	va_list	argptr;
	char	text[2048];

	va_start( argptr, msg );
	Q_vsprintf( text, msg, argptr );
	va_end( argptr );

	Q_strncat( r_speeds_msg, text, sizeof( r_speeds_msg ));
}

void GL_BackendStartFrame( void )
{
	r_speeds_msg[0] = '\0';
}

void GL_BackendEndFrame( void )
{
	mleaf_t	*curleaf;

	if( r_speeds->value <= 0 || !RI.drawWorld )
		return;

	if( !RI.viewleaf )
		curleaf = WORLDMODEL->leafs;
	else curleaf = RI.viewleaf;

	R_Speeds_Printf( "Renderer: ^1Engine^7\n\n" );

	switch( (int)r_speeds->value )
	{
	case 1:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i wpoly, %3i apoly\n%3i epoly, %3i spoly",
		r_stats.c_world_polys, r_stats.c_alias_polys, r_stats.c_studio_polys, r_stats.c_sprite_polys );
		break;		
	case 2:
		R_Speeds_Printf( "visible leafs:\n%3i leafs\ncurrent leaf %3i\n", r_stats.c_world_leafs, curleaf - WORLDMODEL->leafs );
		R_Speeds_Printf( "ReciusiveWorldNode: %3lf secs\nDrawTextureChains %lf\n", r_stats.t_world_node, r_stats.t_world_draw );
		break;
	case 3:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i alias models drawn\n%3i studio models drawn\n%3i sprites drawn",
		r_stats.c_alias_models_drawn, r_stats.c_studio_models_drawn, r_stats.c_sprite_models_drawn );
		break;
	case 4:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i static entities\n%3i normal entities\n%3i server entities",
		r_stats.c_client_ents, tr.draw_list->num_solid_entities + tr.draw_list->num_trans_entities - r_stats.c_client_ents, gEngfuncs.EngineGetParm( PARM_NUMENTITIES, 0 ));
		break;
	case 5:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i tempents\n%3i viewbeams\n%3i particles",
		r_stats.c_active_tents_count, r_stats.c_view_beams_count, r_stats.c_particle_count );
		break;
	}

	memset( &r_stats, 0, sizeof( r_stats ));
}



void GL_SetRenderMode( int mode )
{
	vkState.renderMode = mode;
	//Next drawing command will use this mode

	/*switch( mode )
	{
	case kRenderNormal:
	default:
		//no blend, no alpha test
		break;
	case kRenderTransColor:
	case kRenderTransTexture:
		//blend srcAlpha + oneMinusSrcAlpha, no alpha test
		break;
	case kRenderTransAlpha:
		//no blend, only alpha test
		break;
	case kRenderGlow:
	case kRenderTransAdd:
		//blend srcAlpha + one, no alpha test
		break;
	}*/
}


void R_ShowTextures( void )
{
	//STUB
}

