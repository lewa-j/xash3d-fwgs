
#include "vk_local.h"

void _TriColor4ub( byte r, byte g, byte b, byte a )
{
	Vector4Set( vkState.color, r/255.0f, g/255.0f, b/255.0f, a/255.0f );
}
