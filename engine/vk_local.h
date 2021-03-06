
#pragma once
#include <stdint.h>

typedef struct VkInstance_T* VkInstance;
#ifdef XASH_64BIT
typedef struct VkSurfaceKHR_T *VkSurfaceKHR;
#else
typedef uint64_t VkSurfaceKHR;
#endif

#if defined(_WIN32)
    #define XVKAPI_PTR __stdcall
#elif defined(__ANDROID__) && defined(__ARM_ARCH) && __ARM_ARCH >= 7 && defined(__ARM_32BIT_STATE)
    #define XVKAPI_PTR __attribute__((pcs("aapcs-vfp")))
#else
	#define XVKAPI_PTR
#endif
typedef void (XVKAPI_PTR *PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction (XVKAPI_PTR *PFN_vkGetInstanceProcAddr)(VkInstance instance, const char* pName);
