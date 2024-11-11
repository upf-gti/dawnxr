#include "dawnxr_internal.h"

#ifdef XR_USE_GRAPHICS_API_D3D12

#include <dawn/native/D3D12Backend.h>

#include <iostream>
#include <vector>

namespace dawnxr::internal {

const auto dawnSwapchainFormat = WGPUTextureFormat_BGRA8UnormSrgb;
const auto d3d12SwapchainFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

struct D3D12Session : Session {

	D3D12Session(XrSession session, const WGPUDevice& device) : Session(session, device) {}

	XrResult enumerateSwapchainFormats(std::vector<WGPUTextureFormat>& formats) override {

		formats.push_back(dawnSwapchainFormat);

		return XR_SUCCESS;
	}

	XrResult createSwapchain(const XrSwapchainCreateInfo* createInfo, std::vector<WGPUTextureView>& images,
							 XrSwapchain* swapchain) override {

		if (createInfo->type != XR_TYPE_SWAPCHAIN_CREATE_INFO) return XR_ERROR_HANDLE_INVALID;

		if (createInfo->format != (int64_t)dawnSwapchainFormat) return XR_ERROR_RUNTIME_FAILURE;

		auto d3d12Info = *createInfo;
		d3d12Info.format = d3d12SwapchainFormat;

		XR_TRY(xrCreateSwapchain(backendSession, &d3d12Info, swapchain));
		// TODO: Need to cleanup swapchain if any of the below fails

		uint32_t n;

		XR_TRY(xrEnumerateSwapchainImages(*swapchain, 0, &n, nullptr));

		std::vector<XrSwapchainImageD3D12KHR> d3d12Images(n, {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});

		XR_TRY(xrEnumerateSwapchainImages(*swapchain, n, &n, (XrSwapchainImageBaseHeader*)d3d12Images.data()));
		if (n != d3d12Images.size()) return XR_ERROR_RUNTIME_FAILURE;

        int usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;

        WGPUTextureDescriptor textureDesc = {};
        textureDesc.nextInChain = nullptr;
        textureDesc.label = { nullptr, 0u };
        textureDesc.usage = usage;
        textureDesc.dimension = WGPUTextureDimension_2D;
        textureDesc.size = WGPUExtent3D{ createInfo->width, createInfo->height, 1 };
        textureDesc.format = dawnSwapchainFormat;
        textureDesc.mipLevelCount = createInfo->mipCount;
        textureDesc.sampleCount = createInfo->sampleCount;
        textureDesc.viewFormatCount = 0;
        textureDesc.viewFormats = nullptr;

        WGPUTextureViewDescriptor textureViewDesc = {};
        textureViewDesc.nextInChain = nullptr;
        textureViewDesc.label = { nullptr, 0u };
        textureViewDesc.format = textureDesc.format;
        textureViewDesc.dimension = WGPUTextureViewDimension_2D;
        textureViewDesc.baseMipLevel = 0;
        textureViewDesc.mipLevelCount = 1;
        textureViewDesc.baseArrayLayer = 0;
        textureViewDesc.arrayLayerCount = 1;
        textureViewDesc.aspect = WGPUTextureAspect_All;

        for (auto& it : d3d12Images) {
            auto texture = dawn::native::d3d12::CreateSwapchainWGPUTexture(device, (WGPUTextureDescriptor*)&textureDesc, it.texture);
            images.push_back(wgpuTextureCreateView(texture, &textureViewDesc));
        }

		return XR_SUCCESS;
	}
};

} // namespace

namespace dawnxr::internal {

XrResult getD3D12GraphicsRequirements(XrInstance instance, XrSystemId systemId, GraphicsRequirementsDawn* requirements) {

	PFN_xrGetD3D12GraphicsRequirementsKHR xrGetD3D12GraphicsRequirementsKHR = nullptr;
	XR_TRY(xrGetInstanceProcAddr(instance, "xrGetD3D12GraphicsRequirementsKHR",
								 (PFN_xrVoidFunction*)(&xrGetD3D12GraphicsRequirementsKHR)));

	XrGraphicsRequirementsD3D12KHR d3d12Reqs{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR};
	XR_TRY(xrGetD3D12GraphicsRequirementsKHR(instance, systemId, &d3d12Reqs));

	std::cout << "### D3D12 graphics requirements minFeatureLevel: " << d3d12Reqs.minFeatureLevel << std::endl;
	std::cout << "### D3D12 graphics requirements adapterLuid: " << d3d12Reqs.adapterLuid.HighPart << " "
			  << d3d12Reqs.adapterLuid.LowPart << std::endl;

	return XR_SUCCESS;
}

XrResult createD3D12OpenXRConfig(XrInstance instance, XrSystemId systemId, void** config) {

    *config = nullptr;

	return XR_SUCCESS;
}

XrResult createD3D12Session(XrInstance instance, const XrSessionCreateInfo* createInfo, Session** session) {

	if (createInfo->type != XR_TYPE_SESSION_CREATE_INFO) return XR_ERROR_HANDLE_INVALID;

	auto dawnBinding = (GraphicsBindingDawn*)createInfo->next;
	if (dawnBinding->type != XR_TYPE_GRAPHICS_BINDING_DAWN_EXT) return XR_ERROR_HANDLE_INVALID;
	auto dawnDevice = dawnBinding->device;

	XrGraphicsBindingD3D12KHR d3d12Binding{XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};

	d3d12Binding.device = dawn::native::d3d12::GetD3D12Device(dawnDevice).Get();
	d3d12Binding.queue = dawn::native::d3d12::GetD3D12CommandQueue(dawnDevice).Get();

	//	auto luid = d3d12Binding.device->GetAdapterLuid();
	//	std::cout << "### D3D12 Device adapter LUID: " << luid.HighPart << " " << luid.LowPart << std::endl;

	XrSessionCreateInfo d3d12CreateInfo{XR_TYPE_SESSION_CREATE_INFO};
	d3d12CreateInfo.next = &d3d12Binding;
	d3d12CreateInfo.systemId = createInfo->systemId;

	XrSession backendSession;
	XR_TRY(xrCreateSession(instance, &d3d12CreateInfo, &backendSession));
	*session = new D3D12Session(backendSession, dawnDevice);

	return XR_SUCCESS;
}

} // namespace dawnxr::internal

#endif
