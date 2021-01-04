
#include "vk_local.h"

VkShaderModule VK_CreateShader(const char *name)
{
	char *data = 0;
	fs_offset_t size = 0;
	data = gEngfuncs.COM_LoadFile(name, &size, false);
	if(!data){
		gEngfuncs.Host_Error("VK_CreateShader: can't read file %s\n", name);
		return 0;
	}

	VkShaderModuleCreateInfo smCreateInfo = {0};
	smCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	smCreateInfo.codeSize = size;
	smCreateInfo.pCode = (const uint32_t*)data;

	VkShaderModule sh;
	VkResult r = vkCreateShaderModule(device, &smCreateInfo, 0, &sh);

	Mem_Free( data );
	if(r){
		gEngfuncs.Host_Error("vkCreateShaderModule return %X\n",r);
		return VK_NULL_HANDLE;
	}

	return sh;
}

VkPipelineLayout VK_CreatePipelineLayout(VkDescriptorSetLayout *descSetLayout, VkPushConstantRange *pushConst)
{
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	if(descSetLayout){
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = descSetLayout;
	}
	if(pushConst){
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = pushConst;
	}

	VkPipelineLayout out = VK_NULL_HANDLE;
	VkResult r = vkCreatePipelineLayout(device, &pipelineLayoutInfo, 0, &out);
	if(r){
		gEngfuncs.Host_Error("vkCreatePipelineLayout return %X\n",r);
		return VK_NULL_HANDLE;
	}
	return out;
}

VK_GraphicsPipeline VK_CreateGraphicsPipeline(VkShaderModule vert, VkShaderModule frag, VK_VertexInputDesc *vertInputDesc, VkPrimitiveTopology topology, VkDescriptorSetLayout descSetLayout, VkPipelineLayout layout, VkRenderPass rPass, VkExtent2D extent, uint32_t renderMode)
{
	VK_GraphicsPipeline gp = {0};
	gp.shaderVert = vert;
	gp.shaderFrag = frag;
	gp.pipelineLayout = layout;
	gp.descriptorSetLayout = descSetLayout;
	
	VkPipelineShaderStageCreateInfo shaderStageInfos[2] = {0};
	shaderStageInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStageInfos[0].module = gp.shaderVert;
	shaderStageInfos[0].pName = "main";

	shaderStageInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStageInfos[1].module = gp.shaderFrag;
	shaderStageInfos[1].pName = "main";
	//
	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {0};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = vertInputDesc->bindingCount;
	vertexInputInfo.pVertexBindingDescriptions = vertInputDesc->bindings;
	vertexInputInfo.vertexAttributeDescriptionCount = vertInputDesc->attribCount;
	vertexInputInfo.pVertexAttributeDescriptions = vertInputDesc->attribs;
	//
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {0};
	inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyInfo.topology = topology;
	inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
	//
	VkPipelineTessellationStateCreateInfo *tessStateInfo = NULL;
	//
	VkViewport viewport = {0,0, (float)extent.width, (float)extent.height, 0,1};
	VkRect2D scissor = {{0,0}, extent};

	VkPipelineViewportStateCreateInfo viewportStateInfo = {0};
	viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.viewportCount = 1;
	viewportStateInfo.pViewports = &viewport;
	viewportStateInfo.scissorCount = 1;
	viewportStateInfo.pScissors = &scissor;
	//
	VkPipelineRasterizationStateCreateInfo rasterizationInfo = {0};
	rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationInfo.depthClampEnable = VK_FALSE;
	rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;//depthmap rendering?
	rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationInfo.depthBiasEnable = VK_FALSE;
	rasterizationInfo.lineWidth = 1.0f;
	//
	VkPipelineMultisampleStateCreateInfo multisampleInfo = {0};
	multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleInfo.sampleShadingEnable = VK_FALSE;
	//
	VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {0};
	depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilInfo.depthTestEnable = false;//true;
	depthStencilInfo.depthWriteEnable = false;//true;
	depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	//
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {0};
	if(renderMode==kRenderTransTexture)
	{
		colorBlendAttachment.blendEnable = true;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		//colorBlendAttachment.srcAlphaBlendFactor
		//colorBlendAttachment.dstAlphaBlendFactor
		//colorBlendAttachment.alphaBlendOp
	}
	else if(renderMode == kRenderTransAdd)
	{
		colorBlendAttachment.blendEnable = true;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	}
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendInfo={0};
	colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendInfo.attachmentCount = 1;//more then 1 for mrt
	colorBlendInfo.pAttachments = &colorBlendAttachment;
	//
	VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamicStateInfo={0};
	dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount = 2;
	dynamicStateInfo.pDynamicStates = dynStates;

	VkGraphicsPipelineCreateInfo graphicsPiplineInfo = {0};
	graphicsPiplineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPiplineInfo.stageCount = 2;
	graphicsPiplineInfo.pStages = shaderStageInfos;
	graphicsPiplineInfo.pVertexInputState = &vertexInputInfo;
	graphicsPiplineInfo.pInputAssemblyState = &inputAssemblyInfo;
	graphicsPiplineInfo.pTessellationState = tessStateInfo;
	graphicsPiplineInfo.pViewportState = &viewportStateInfo;
	graphicsPiplineInfo.pRasterizationState = &rasterizationInfo;
	graphicsPiplineInfo.pMultisampleState = &multisampleInfo;
	graphicsPiplineInfo.pDepthStencilState = &depthStencilInfo;
	graphicsPiplineInfo.pColorBlendState = &colorBlendInfo;
	graphicsPiplineInfo.pDynamicState = &dynamicStateInfo;
	graphicsPiplineInfo.layout = layout;
	graphicsPiplineInfo.renderPass = rPass;
	graphicsPiplineInfo.subpass = 0;
	//graphicsPiplineInfo.basePipelineHandle
	//graphicsPiplineInfo.basePipelineIndex
	
	VkResult r = vkCreateGraphicsPipelines(device, pipelineCache, 1, &graphicsPiplineInfo, NULL, &gp.pipeline);
	if(r){
		gEngfuncs.Host_Error("vkCreateGraphicsPipelines return %X\n",r);
		//return 0;
	}

	return gp;
}