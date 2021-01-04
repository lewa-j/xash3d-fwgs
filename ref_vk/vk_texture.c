
#include "vk_local.h"
#include "crclib.h"

#define TEXTURES_HASH_SIZE	(MAX_TEXTURES >> 2)

static vk_texture_t		vk_textures[MAX_TEXTURES];
static vk_texture_t*	vk_texturesHashTable[TEXTURES_HASH_SIZE];
static uint		vk_numTextures;

VkImage VK_CreateImage(VkDeviceMemory *memory, VkExtent2D size, uint32_t mipCount, uint32_t layersCount, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memFlags)
{
	VkImageCreateInfo imageInfo = {0};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.flags = layersCount >= 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = format;
	//imageInfo.extent = {width,height,1};
	imageInfo.extent.width = size.width;
	imageInfo.extent.height = size.height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = mipCount;
	imageInfo.arrayLayers = layersCount;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = tiling;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	//imageInfo.queueFamilyIndexCount = 1;
	//imageInfo.pQueueFamilyIndices = &selectedQueueFamily;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;//VK_IMAGE_LAYOUT_PREINITIALIZED

	VkImage img = VK_NULL_HANDLE;
	VkResult r = vkCreateImage(device, &imageInfo, NULL, &img);
	if(r){
		gEngfuncs.Host_Error("vkCreateImage return %X\n",r);
		return VK_NULL_HANDLE;
	}

	VkMemoryRequirements memReqs = {0};
	vkGetImageMemoryRequirements(device, img, &memReqs);
	gEngfuncs.Con_Printf("image %X: memory requirements: size %d, alignment %d, memory type bits %d\n", (int)img, (int)memReqs.size, (int)memReqs.alignment, memReqs.memoryTypeBits);

	VkMemoryAllocateInfo allocInfo = {0};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = VK_FindMemoryType(memReqs.memoryTypeBits, memFlags);

	r = vkAllocateMemory(device, &allocInfo, NULL, memory);
	if(r){
		gEngfuncs.Host_Error("vkAllocateMemory return %X\n",r);
		return VK_NULL_HANDLE;
	}

	r = vkBindImageMemory(device, img, *memory, /*memoryOffset*/0);
	if(r){
		gEngfuncs.Host_Error("vkBindImageMemory return %X\n",r);
		return VK_NULL_HANDLE;
	}

	return img;
}

VkImageView VK_CreateImageView(VkImage img, VkImageViewType type, VkFormat format, VkImageAspectFlags aspect, uint32_t numMipLevels, uint32_t flags)
{
	VkImageViewCreateInfo ivCreateInfo = {0};
	ivCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ivCreateInfo.image = img;
	ivCreateInfo.viewType = type;
	ivCreateInfo.format = format;
	ivCreateInfo.components = (VkComponentMapping){0, 0, 0, (flags & IMAGE_HAS_ALPHA) ? 0 : VK_COMPONENT_SWIZZLE_ONE};
	ivCreateInfo.subresourceRange = (VkImageSubresourceRange){aspect,0,numMipLevels,0,VK_REMAINING_ARRAY_LAYERS};

	VkImageView imageView = VK_NULL_HANDLE;
	VkResult r = vkCreateImageView(device, &ivCreateInfo, 0, &imageView);
	if(r){
		gEngfuncs.Host_Error("vkCreateImageView return %X\n",r);
		return VK_NULL_HANDLE;
	}

	return imageView;
}

VkSampler VK_CreateSampler()
{
	VkSamplerCreateInfo samplerInfo = {0};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_NEAREST;//VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_NEAREST;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.mipLodBias = 0;
	samplerInfo.anisotropyEnable = false;
	samplerInfo.maxAnisotropy = 0;
	samplerInfo.minLod = 0;
	samplerInfo.maxLod = 0;

	VkSampler sampler = VK_NULL_HANDLE;
	VkResult r = vkCreateSampler(device, &samplerInfo, NULL, &sampler);
	if(r){
		gEngfuncs.Host_Error("vkCreateSampler return %X\n",r);
		return VK_NULL_HANDLE;
	}

	return sampler;
}

qboolean VK_FormatSupport( VkFormat format )
{
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties( physicalDevice.handle, format, &formatProperties );
	if( !(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) )
	{
		gEngfuncs.Con_Printf( "Vulkan: %X format is not supported for a sampled image.", format );
		return false;
	}
	return true;
}

vk_texture_t *R_GetTexture( uint32_t texnum )
{
	ASSERT( texnum >= 0 && texnum < MAX_TEXTURES );
	return &vk_textures[texnum];
}

static int GL_CalcTextureSamples( int flags )
{
	if( FBitSet( flags, IMAGE_HAS_COLOR ))
		return FBitSet( flags, IMAGE_HAS_ALPHA ) ? 4 : 3;
	return FBitSet( flags, IMAGE_HAS_ALPHA ) ? 2 : 1;
}

static size_t GL_CalcImageSize( pixformat_t format, int width, int height, int depth )
{
	size_t	size = 0;

	// check the depth error
	depth = Q_max( 1, depth );

	switch( format )
	{
	case PF_LUMINANCE:
		size = width * height * depth;
		break;
	case PF_RGB_24:
	case PF_BGR_24:
		size = width * height * depth * 3;
		break;
	case PF_BGRA_32:
	case PF_RGBA_32:
		size = width * height * depth * 4;
		break;
	case PF_DXT1:
		size = (((width + 3) >> 2) * ((height + 3) >> 2) * 8) * depth;
		break;
	case PF_DXT3:
	case PF_DXT5:
	case PF_ATI2:
		size = (((width + 3) >> 2) * ((height + 3) >> 2) * 16) * depth;
		break;
	deafult:
		size = width * height * depth * 4;
	}

	return size;
}

static void VK_SetTextureFormat( vk_texture_t *tex, pixformat_t format, int channelMask )
{
	qboolean	haveColor = ( channelMask & IMAGE_HAS_COLOR );
	qboolean	haveAlpha = ( channelMask & IMAGE_HAS_ALPHA );

	Assert( tex != NULL );

	if( ImageDXT( format ))
	{
		switch( format )
		{
		case PF_DXT1: tex->format = VK_FORMAT_BC1_RGB_UNORM_BLOCK; break;	// never use DXT1 with 1-bit alpha
		case PF_DXT3: tex->format = VK_FORMAT_BC2_UNORM_BLOCK; break;
		case PF_DXT5: tex->format = VK_FORMAT_BC3_UNORM_BLOCK; break;
		case PF_ATI2: tex->format = VK_FORMAT_BC5_UNORM_BLOCK; break; //RGTC
		default: break;
		}
		return;
	}
	else if( FBitSet( tex->flags, TF_DEPTHMAP ))
	{
		if( FBitSet( tex->flags, TF_ARB_16BIT ))
			tex->format = VK_FORMAT_D16_UNORM;
		else if( FBitSet( tex->flags, TF_ARB_FLOAT ) && VK_FormatSupport( VK_FORMAT_D32_SFLOAT ))
			tex->format = VK_FORMAT_D32_SFLOAT;
		else tex->format = VK_FORMAT_D24_UNORM_S8_UINT;
	}
	else if( FBitSet( tex->flags, TF_ARB_FLOAT|TF_ARB_16BIT ))
	{
		if( haveColor )
		{
			if( FBitSet( tex->flags, TF_ARB_16BIT ))
				tex->format = VK_FORMAT_R16G16B16A16_SFLOAT;
			else tex->format = VK_FORMAT_R32G32B32A32_SFLOAT;
		}
		else if( haveAlpha )
		{
			if( FBitSet( tex->flags, TF_ARB_16BIT ) )
				tex->format = VK_FORMAT_R16G16_SFLOAT;
			else tex->format = VK_FORMAT_R32G32_SFLOAT;
		}
		else
		{
			if( FBitSet( tex->flags, TF_ARB_16BIT ) )
				tex->format = VK_FORMAT_R16_SFLOAT;
			else tex->format = VK_FORMAT_R32_SFLOAT;
		}
	}
	else
	{
#if 0
		// NOTE: not all the types will be compressed
		int	bits = 32;//glw_state.desktopBitsPixel;

		switch( GL_CalcTextureSamples( channelMask ))
		{
		case 1: 
			//if( FBitSet( tex->flags, TF_ALPHACONTRAST ))
				tex->format = VK_FORMAT_R8_UNORM;
			//else tex->format = GL_LUMINANCE8;
			break;
		case 2: tex->format = VK_FORMAT_R8G8_UNORM; break;
		case 3:
			switch( bits )
			{
			case 16: tex->format = VK_FORMAT_R5G6B5_UNORM_PACK16; break;
			case 32: tex->format = VK_FORMAT_R8G8B8A8_UNORM; break;
			}
			break;	
		case 4:
		default:
			switch( bits )
			{
			case 16: tex->format = VK_FORMAT_R4G4B4A4_UNORM_PACK16; break;
			case 32: tex->format = VK_FORMAT_R8G8B8A8_UNORM; break;
			}
			break;
		}
#endif
		switch(format)
		{
		case PF_LUMINANCE:
			tex->format = VK_FORMAT_R8_UNORM;
			break;
		case PF_RGB_24:
			tex->format = VK_FORMAT_R8G8B8_UNORM;
			break;
		case PF_BGR_24:
			tex->format = VK_FORMAT_B8G8R8_UNORM;
			break;
		case PF_BGRA_32:
			tex->format = VK_FORMAT_B8G8R8A8_UNORM;
			break;
		case PF_RGBA_32:
			tex->format = VK_FORMAT_R8G8B8A8_UNORM;
			break;
		default:
			tex->format = VK_FORMAT_R8G8B8A8_UNORM;
		}
	}
}

void VK_PipelineBarrier(VkCommandBuffer cmdBuff, VkImage img, VkImageAspectFlags aspect, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, 
		VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
{
	VkImageMemoryBarrier barrier = {0};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = dstAccessMask;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = img;
	barrier.subresourceRange = (VkImageSubresourceRange){aspect,0,VK_REMAINING_MIP_LEVELS,0,VK_REMAINING_ARRAY_LAYERS};

	vkCmdPipelineBarrier(cmdBuff, srcStageMask, dstStageMask, 0,0,NULL,0,NULL,1, &barrier);
}

static qboolean VK_UploadTexture( vk_texture_t *tex, rgbdata_t *pic )
{
	byte		*buf, *data;
	size_t		texsize, size;
	uint		width, height;
	uint		i, j, numSides;
	uint		offset = 0;
	qboolean		normalMap;
	const byte	*bufend;
	
	//from GL_SetTextureTarget
	pic->depth = Q_max( 1, pic->depth );
	tex->numMips = 0; // begin counting

	// correct mip count
	pic->numMips = Q_max( 1, pic->numMips );
	
	//from GL_SetTextureDimensions
	tex->srcWidth = pic->width;
	tex->srcHeight = pic->height;
	
	tex->width = Q_max( 1, pic->width );
	tex->height = Q_max( 1, pic->height );
	tex->depth = Q_max( 1, pic->depth );
	
	VK_SetTextureFormat( tex, pic->type, pic->flags );
	
	tex->fogParams[0] = pic->fogParams[0];
	tex->fogParams[1] = pic->fogParams[1];
	tex->fogParams[2] = pic->fogParams[2];
	tex->fogParams[3] = pic->fogParams[3];

	gEngfuncs.Con_Reportf( "VK_UploadTexture: %s %dx%d\n", tex->name, pic->width, pic->height );

	buf = pic->buffer;
	bufend = pic->buffer + pic->size; // total image size include all the layers, cube sides, mipmaps
	offset = GL_CalcImageSize( pic->type, pic->width, pic->height, pic->depth );
	//texsize = GL_CalcTextureSize( tex->format, tex->width, tex->height, tex->depth );
	normalMap = FBitSet( tex->flags, TF_NORMALMAP ) ? true : false;
	numSides = FBitSet( pic->flags, IMAGE_CUBEMAP ) ? 6 : 1;

	//Cleanup
	if(tex->imageView){
		vkDestroyImageView(device, tex->imageView, NULL);
		tex->imageView = VK_NULL_HANDLE;
		vkDestroyImage(device, tex->image, NULL);
		tex->image = VK_NULL_HANDLE;
		vkFreeMemory(device, tex->memory, NULL);
		tex->memory = VK_NULL_HANDLE;
		tex->descriptor = VK_NULL_HANDLE;
	}

	//Upload
	tex->image = VK_CreateImage(&tex->memory, (VkExtent2D){tex->width, tex->height}, pic->numMips, numSides, tex->format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	tex->imageView = VK_CreateImageView(tex->image, FBitSet( pic->flags, IMAGE_CUBEMAP ) ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D, tex->format, VK_IMAGE_ASPECT_COLOR_BIT, pic->numMips, pic->flags);

	if(buf){
		VK_Buffer stagingBuffer = VK_CreateBuffer(pic->size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void *mapped = 0;
		vkMapMemory(device, stagingBuffer.memory, 0, pic->size, 0, &mapped);
		memcpy(mapped, buf, pic->size);
		vkUnmapMemory(device, stagingBuffer.memory);

		VkCommandBuffer comBuf = VK_BeginSingleTimeCommands();

		VK_PipelineBarrier(comBuf, tex->image, VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT, 
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		width = Q_max( 1, tex->width );
		height = Q_max( 1, tex->height );
		uint32_t dataOffset = 0;
		VkBufferImageCopy regions[16] = {0};
		regions[0].bufferOffset = dataOffset;
		regions[0].imageSubresource = (VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,numSides};
		regions[0].imageExtent = (VkExtent3D){width,height,1};

		for(uint32_t j=0; j<pic->numMips; j++)
		{
			width = Q_max( 1, ( tex->width >> j ));
			height = Q_max( 1, ( tex->height >> j ));

			regions[j].bufferOffset = dataOffset;
			regions[j].imageSubresource = (VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,j,0,numSides};
			regions[j].imageExtent = (VkExtent3D){width,height,1};
			dataOffset += GL_CalcImageSize( pic->type, width, height, tex->depth );
			//texsize = GL_CalcTextureSize( tex->format, width, height, tex->depth );
			texsize = GL_CalcImageSize( pic->type, width, height, tex->depth );
			tex->size += texsize;
			tex->numMips++;
		}
		vkCmdCopyBufferToImage(comBuf, stagingBuffer.handle, tex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, pic->numMips, regions);

		//
		
		VK_PipelineBarrier(comBuf, tex->image, VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, 
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		
		VK_EndSingleTimeCommands(comBuf);

		vkDestroyBuffer(device, stagingBuffer.handle, NULL);
		vkFreeMemory(device, stagingBuffer.memory, NULL);

	}
	
	SetBits( tex->flags, TF_IMG_UPLOADED ); // done
	
	return true;
}

//TODO
void VK_CreateTexSampler( vk_texture_t *tex )
{
	//FBitSet( tex->flags, TF_NEAREST )
	//FBitSet( tex->flags, TF_NOMIPMAP )
	//tex->numMips <= 1 && ( IsLightMap( tex ) && gl_lightmap_nearest->value )
	//gl_texture_nearest->value 
	// !FBitSet( tex->flags, TF_ALPHACONTRAST ) no anisotropy
	//gl_texture_lodbias->value
	//FBitSet( tex->flags, TF_BORDER )
	//FBitSet( tex->flags, TF_CLAMP )
}

static void GL_ProcessImage( vk_texture_t *tex, rgbdata_t *pic )
{
	float	emboss_scale = 0.0f;
	uint	img_flags = 0; 

	// force upload texture as RGB or RGBA (detail textures requires this)
	if( tex->flags & TF_FORCE_COLOR ) pic->flags |= IMAGE_HAS_COLOR;
	if( pic->flags & IMAGE_HAS_ALPHA ) tex->flags |= TF_HAS_ALPHA;

//	tex->encode = pic->encode; // share encode method

	if( ImageDXT( pic->type ))
	{
		if( !pic->numMips )
			tex->flags |= TF_NOMIPMAP; // disable mipmapping by user request

		// clear all the unsupported flags
		tex->flags &= ~TF_KEEP_SOURCE;
	}
	else
	{
		// copy flag about luma pixels
		if( pic->flags & IMAGE_HAS_LUMA )
			tex->flags |= TF_HAS_LUMA;

		if( pic->flags & IMAGE_QUAKEPAL )
			tex->flags |= TF_QUAKEPAL;

		// create luma texture from quake texture
		if( tex->flags & TF_MAKELUMA )
		{
			img_flags |= IMAGE_MAKE_LUMA;
			tex->flags &= ~TF_MAKELUMA;
		}

		if( tex->flags & TF_ALLOW_EMBOSS )
		{
			img_flags |= IMAGE_EMBOSS;
			tex->flags &= ~TF_ALLOW_EMBOSS;
		}

		if( !FBitSet( tex->flags, TF_IMG_UPLOADED ) && FBitSet( tex->flags, TF_KEEP_SOURCE ))
			tex->original = gEngfuncs.FS_CopyImage( pic ); // because current pic will be expanded to rgba

		// we need to expand image into RGBA buffer
		if( pic->type == PF_INDEXED_24 || pic->type == PF_INDEXED_32 )
			img_flags |= IMAGE_FORCE_RGBA;

		// dedicated server doesn't register this variable
//		if( gl_emboss_scale != NULL )
//			emboss_scale = gl_emboss_scale->value;

		// processing image before uploading (force to rgba, make luma etc)
		if( pic->buffer ) gEngfuncs.Image_Process( &pic, 0, 0, img_flags, emboss_scale );

		if( FBitSet( tex->flags, TF_LUMINANCE ))
			ClearBits( pic->flags, IMAGE_HAS_COLOR );
	}
}

qboolean GL_CheckTexName( const char *name )
{
	if( !COM_CheckString( name ))
		return false;

	// because multi-layered textures can exceed name string
	if( Q_strlen( name ) >= sizeof( vk_textures->name ))
	{
		gEngfuncs.Con_Printf( S_ERROR "LoadTexture: too long name %s (%d)\n", name, Q_strlen( name ));
		return false;
	}

	return true;
}

static vk_texture_t *GL_TextureForName( const char *name )
{
	vk_texture_t	*tex;
	uint		hash;

	// find the texture in array
	hash = COM_HashKey( name, TEXTURES_HASH_SIZE );

	for( tex = vk_texturesHashTable[hash]; tex != NULL; tex = tex->nextHash )
	{
		if( !Q_stricmp( tex->name, name ))
			return tex;
	}

	return NULL;
}

static vk_texture_t *GL_AllocTexture( const char *name, texFlags_t flags )
{
	vk_texture_t	*tex;
	uint		i;

	// find a free texture_t slot
	for( i = 0, tex = vk_textures; i < vk_numTextures; i++, tex++ )
		if( !tex->name[0] ) break;

	if( i == vk_numTextures )
	{
		if( vk_numTextures == MAX_TEXTURES )
			gEngfuncs.Host_Error( "GL_AllocTexture: MAX_TEXTURES limit exceeds\n" );
		vk_numTextures++;
	}

	tex = &vk_textures[i];

	// copy initial params
	Q_strncpy( tex->name, name, sizeof( tex->name ));
	//TODO
	/*if( FBitSet( flags, TF_SKYSIDE ))
		tex->texnum = tr.skyboxbasenum++;
	else tex->texnum = i; // texnum is used for fast acess into vk_textures array too*/
	tex->flags = flags;

	// add to hash table
	tex->hashValue = COM_HashKey( name, TEXTURES_HASH_SIZE );
	tex->nextHash = vk_texturesHashTable[tex->hashValue];
	vk_texturesHashTable[tex->hashValue] = tex;

	return tex;
}

static void VK_DeleteTexture( vk_texture_t *tex )
{
	vk_texture_t	**prev;
	vk_texture_t	*cur;

	// already freed?
	if( !tex->image ) return;

	// debug
	if( !tex->name[0] )
	{
		gEngfuncs.Con_Printf( S_ERROR "VK_DeleteTexture: trying to free unnamed texture with image %p\n", tex->image );
		return;
	}

	// remove from hash table
	prev = &vk_texturesHashTable[tex->hashValue];

	while( 1 )
	{
		cur = *prev;
		if( !cur ) break;

		if( cur == tex )
		{
			*prev = cur->nextHash;
			break;
		}
		prev = &cur->nextHash;
	}

	// release source
	if( tex->original )
		gEngfuncs.FS_FreeImage( tex->original );

	if( tex->imageView )
		vkDestroyImageView(device, tex->imageView, NULL);
	if( tex->image )	
		vkDestroyImage(device, tex->image, NULL);
	if( tex->memory )
		vkFreeMemory(device, tex->memory, NULL);
	
	memset( tex, 0, sizeof( *tex ));
}

int GL_LoadTexture( const char *name, const byte *buf, size_t size, int flags )
{
	vk_texture_t	*tex;
	rgbdata_t		*pic;
	uint		picFlags = 0;

	if( !GL_CheckTexName( name ))
		return 0;

		// see if already loaded
	if(( tex = GL_TextureForName( name )))
		return (tex - vk_textures);

	if( FBitSet( flags, TF_NOFLIP_TGA ))
		SetBits( picFlags, IL_DONTFLIP_TGA );

	if( FBitSet( flags, TF_KEEP_SOURCE ) && !FBitSet( flags, TF_EXPAND_SOURCE ))
		SetBits( picFlags, IL_KEEP_8BIT );

	// set some image flags
	gEngfuncs.Image_SetForceFlags( picFlags );

	pic = gEngfuncs.FS_LoadImage( name, buf, size );
	if( !pic ) return 0; // couldn't loading image

	// allocate the new one
	tex = GL_AllocTexture( name, flags );
	GL_ProcessImage( tex, pic );

	if( !VK_UploadTexture( tex, pic ))
	{
		memset( tex, 0, sizeof( vk_texture_t ));
		gEngfuncs.FS_FreeImage( pic ); // release source texture
		return 0;
	}

	VK_CreateTexSampler( tex );// texture filter, wrap etc
	gEngfuncs.FS_FreeImage( pic ); // release source texture

	// NOTE: always return texnum as index in array or engine will stop work !!!
	return tex - vk_textures;
}

int GL_LoadTextureFromBuffer( const char *name, rgbdata_t *pic, texFlags_t flags, qboolean update )
{
	vk_texture_t	*tex;

	if( !GL_CheckTexName( name ))
		return 0;

	// see if already loaded
	if(( tex = GL_TextureForName( name )) && !update )
		return (tex - vk_textures);

	// couldn't loading image
	if( !pic ) return 0;

	if( update )
	{
		if( tex == NULL )
			gEngfuncs.Host_Error( "GL_LoadTextureFromBuffer: couldn't find texture %s for update\n", name );
		SetBits( tex->flags, flags );
	}
	else
	{
		// allocate the new one
		tex = GL_AllocTexture( name, flags );
	}

	GL_ProcessImage( tex, pic );

	if( !VK_UploadTexture( tex, pic ))
	{
		memset( tex, 0, sizeof( vk_texture_t ));
		return 0;
	}

	VK_CreateTexSampler( tex ); //texture filter, wrap etc
	return (tex - vk_textures);
}

int GL_FindTexture( const char *name )
{
	vk_texture_t	*tex;

	if( !GL_CheckTexName( name ))
		return 0;

	// see if already loaded
	if(( tex = GL_TextureForName( name )))
		return (tex - vk_textures);

	return 0;
}

void GL_FreeTexture( uint32_t texnum )
{
	// number 0 it's already freed
	if( texnum <= 0 ) return;

	VK_DeleteTexture( &vk_textures[texnum] );
}

void GL_ProcessTexture( int texnum, float gamma, int topColor, int bottomColor )
{
	vk_texture_t	*image;
	rgbdata_t		*pic;
	int		flags = 0;

	if( texnum <= 0 || texnum >= MAX_TEXTURES )
		return; // missed image
	image = &vk_textures[texnum];

	// select mode
	if( gamma != -1.0f )
	{
		flags = IMAGE_LIGHTGAMMA;
	}
	else if( topColor != -1 && bottomColor != -1 )
	{
		flags = IMAGE_REMAP;
	}
	else
	{
		gEngfuncs.Con_Printf( S_ERROR "GL_ProcessTexture: bad operation for %s\n", image->name );
		return;
	}

	if( !image->original )
	{
		gEngfuncs.Con_Printf( S_ERROR "GL_ProcessTexture: no input data for %s\n", image->name );
		return;
	}

	if( ImageDXT( image->original->type ))
	{
		gEngfuncs.Con_Printf( S_ERROR "GL_ProcessTexture: can't process compressed texture %s\n", image->name );
		return;
	}

	// all the operations makes over the image copy not an original
	pic = gEngfuncs.FS_CopyImage( image->original );
	gEngfuncs.Image_Process( &pic, topColor, bottomColor, flags, 0.0f );

	VK_UploadTexture( image, pic );
	VK_CreateTexSampler( image );// texture filter, wrap etc

	gEngfuncs.FS_FreeImage( pic );
}

/*
==============================================================================

INTERNAL TEXTURES

==============================================================================
*/

static rgbdata_t *GL_FakeImage( int width, int height, int depth, int flags )
{
	static byte	data2D[1024]; // 16x16x4
	static rgbdata_t	r_image;

	// also use this for bad textures, but without alpha
	r_image.width = Q_max( 1, width );
	r_image.height = Q_max( 1, height );
	r_image.depth = Q_max( 1, depth );
	r_image.flags = flags;
	r_image.type = PF_RGBA_32;
	r_image.size = r_image.width * r_image.height * r_image.depth * 4;
	r_image.buffer = (r_image.size > sizeof( data2D )) ? NULL : data2D;
	r_image.palette = NULL;
	r_image.numMips = 1;
	r_image.encode = 0;

	if( FBitSet( r_image.flags, IMAGE_CUBEMAP ))
		r_image.size *= 6;
	memset( data2D, 0xFF, sizeof( data2D ));

	return &r_image;
}

static void GL_CreateInternalTextures( void )
{
	int	dx2, dy, d;
	int	x, y;
	rgbdata_t	*pic;

	// emo-texture from quake1
	pic = GL_FakeImage( 16, 16, 1, IMAGE_HAS_COLOR );

	for( y = 0; y < 16; y++ )
	{
		for( x = 0; x < 16; x++ )
		{
			if(( y < 8 ) ^ ( x < 8 ))
				((uint *)pic->buffer)[y*16+x] = 0xFFFF00FF;
			else ((uint *)pic->buffer)[y*16+x] = 0xFF000000;
		}
	}

	tr.defaultTexture = GL_LoadTextureFromBuffer( REF_DEFAULT_TEXTURE, pic, TF_COLORMAP, false );

	// particle texture from quake1
	pic = GL_FakeImage( 16, 16, 1, IMAGE_HAS_COLOR|IMAGE_HAS_ALPHA );

	for( x = 0; x < 16; x++ )
	{
		dx2 = x - 8;
		dx2 = dx2 * dx2;

		for( y = 0; y < 16; y++ )
		{
			dy = y - 8;
			d = 255 - 35 * sqrt( dx2 + dy * dy );
			pic->buffer[( y * 16 + x ) * 4 + 3] = bound( 0, d, 255 );
		}
	}

	tr.particleTexture = GL_LoadTextureFromBuffer( "*particle", pic, TF_CLAMP, false );

	// white texture
	pic = GL_FakeImage( 4, 4, 1, IMAGE_HAS_COLOR );
	for( x = 0; x < 16; x++ )
		((uint *)pic->buffer)[x] = 0xFFFFFFFF;
	tr.whiteTexture = GL_LoadTextureFromBuffer( REF_WHITE_TEXTURE, pic, TF_COLORMAP, false );

	// gray texture
	pic = GL_FakeImage( 4, 4, 1, IMAGE_HAS_COLOR );
	for( x = 0; x < 16; x++ )
		((uint *)pic->buffer)[x] = 0xFF7F7F7F;
	tr.grayTexture = GL_LoadTextureFromBuffer( REF_GRAY_TEXTURE, pic, TF_COLORMAP, false );

	// black texture
	pic = GL_FakeImage( 4, 4, 1, IMAGE_HAS_COLOR );
	for( x = 0; x < 16; x++ )
		((uint *)pic->buffer)[x] = 0xFF000000;
	tr.blackTexture = GL_LoadTextureFromBuffer( REF_BLACK_TEXTURE, pic, TF_COLORMAP, false );

	// cinematic dummy
	pic = GL_FakeImage( 640, 100, 1, IMAGE_HAS_COLOR );
	tr.cinTexture = GL_LoadTextureFromBuffer( "*cintexture", pic, TF_NOMIPMAP|TF_CLAMP, false );
}

void R_InitImages( void )
{
	memset( vk_textures, 0, sizeof( vk_textures ));
	memset( vk_texturesHashTable, 0, sizeof( vk_texturesHashTable ));
	vk_numTextures = 0;

	// create unused 0-entry
	Q_strncpy( vk_textures->name, "*unused*", sizeof( vk_textures->name ));
	vk_textures->hashValue = COM_HashKey( vk_textures->name, TEXTURES_HASH_SIZE );
	vk_textures->nextHash = vk_texturesHashTable[vk_textures->hashValue];
	vk_texturesHashTable[vk_textures->hashValue] = vk_textures;
	vk_numTextures = 1;

	// validate cvars
	//R_SetTextureParameters();
	GL_CreateInternalTextures();

	//gEngfuncs.Cmd_AddCommand( "texturelist", R_TextureList_f, "display loaded textures list" );
}

void R_ShutdownImages( void )
{
	vk_texture_t	*tex;
	int		i;

	//gEngfuncs.Cmd_RemoveCommand( "texturelist" );
	//GL_CleanupAllTextureUnits();

	for( i = 0, tex = vk_textures; i < vk_numTextures; i++, tex++ )
		VK_DeleteTexture( tex );

	memset( tr.lightmapTextures, 0, sizeof( tr.lightmapTextures ));
	memset( vk_texturesHashTable, 0, sizeof( vk_texturesHashTable ));
	memset( vk_textures, 0, sizeof( vk_textures ));
	vk_numTextures = 0;
}
