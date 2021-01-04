
#include "vk_local.h"

/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/
//STUB
void CL_RunLightStyles( void )
{
/*	int		i, k, flight, clight;
	float		l, lerpfrac, backlerp;
	float		frametime = (gpGlobals->time -   gpGlobals->oldtime);
	float		scale;
	lightstyle_t	*ls;

	if( !WORLDMODEL ) return;

	scale = r_lighting_modulate->value;

	// light animations
	// 'm' is normal light, 'a' is no light, 'z' is double bright
	for( i = 0; i < MAX_LIGHTSTYLES; i++ )
	{
		ls = gEngfuncs.GetLightStyle( i );
		if( !WORLDMODEL->lightdata )
		{
			tr.lightstylevalue[i] = 256 * 256;
			continue;
		}

		if( !ENGINE_GET_PARM( PARAM_GAMEPAUSED ) && frametime <= 0.1f )
			ls->time += frametime; // evaluate local time

		flight = (int)Q_floor( ls->time * 10 );
		clight = (int)Q_ceil( ls->time * 10 );
		lerpfrac = ( ls->time * 10 ) - flight;
		backlerp = 1.0f - lerpfrac;

		if( !ls->length )
		{
			tr.lightstylevalue[i] = 256 * scale;
			continue;
		}
		else if( ls->length == 1 )
		{
			// single length style so don't bother interpolating
			tr.lightstylevalue[i] = ls->map[0] * 22 * scale;
			continue;
		}
		else if( !ls->interp || !CVAR_TO_BOOL( cl_lightstyle_lerping ))
		{
			tr.lightstylevalue[i] = ls->map[flight%ls->length] * 22 * scale;
			continue;
		}

		// interpolate animating light
		// frame just gone
		k = ls->map[flight % ls->length];
		l = (float)( k * 22.0f ) * backlerp;

		// upcoming frame
		k = ls->map[clight % ls->length];
		l += (float)( k * 22.0f ) * lerpfrac;

		tr.lightstylevalue[i] = (int)l * scale;
	}*/
}