#include <windows.h>
#include <d3d11.h>
#include <VersionHelpers.h>
#include <dxgi1_2.h>
#include "get-graphics-offsets.h"

typedef HRESULT(WINAPI *d3d11create_t)(
	IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software,
	UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels,
	UINT SDKVersion, const DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
	IDXGISwapChain **ppSwapChain, ID3D11Device **ppDevice,
	D3D_FEATURE_LEVEL *pFeatureLevel,
	ID3D11DeviceContext **ppImmediateContext);

typedef HRESULT(WINAPI *create_fac_t)(IID *id, void **);

struct dxgi_info {
	HMODULE module;
	HMODULE d3d11_module;
	HWND hwnd;
	IDXGISwapChain *swap;
	ID3D11Device *device;
	ID3D11DeviceContext *context;
};

static const IID dxgiFactory2 = {0x50c83a1c,
				 0xe072,
				 0x4c48,
				 {0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6,
				  0xd0}};

static inline bool dxgi_init(dxgi_info &info)
{
	d3d11create_t create;
	create_fac_t create_factory;
	IDXGIFactory1 *factory;
	IDXGIAdapter1 *adapter;
	HRESULT hr;

	info.hwnd = CreateWindowExA(0, DUMMY_WNDCLASS,
				    "d3d11 get-offset window", WS_POPUP, 0, 0,
				    2, 2, nullptr, nullptr,
				    GetModuleHandleA(nullptr), nullptr);
	if (!info.hwnd) {
		return false;
	}

	info.module = LoadLibraryA("dxgi.dll");
	if (!info.module) {
		return false;
	}

	create_factory =
		(create_fac_t)GetProcAddress(info.module, "CreateDXGIFactory1");

	info.d3d11_module = LoadLibraryA("d3d11.dll");
	if (!info.d3d11_module) {
		return false;
	}

	create = (d3d11create_t)GetProcAddress(info.d3d11_module,
					       "D3D11CreateDeviceAndSwapChain");
	if (!create) {
		return false;
	}

	IID factory_iid = IsWindows8OrGreater() ? dxgiFactory2
						: __uuidof(IDXGIFactory1);

	hr = create_factory(&factory_iid, (void **)&factory);
	if (FAILED(hr)) {
		return false;
	}

	hr = factory->EnumAdapters1(0, &adapter);
	factory->Release();
	if (FAILED(hr)) {
		return false;
	}

	DXGI_SWAP_CHAIN_DESC desc = {};
	desc.BufferCount = 2;
	desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.BufferDesc.Width = 2;
	desc.BufferDesc.Height = 2;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.OutputWindow = info.hwnd;
	desc.SampleDesc.Count = 1;
	desc.Windowed = true;
	
	D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};

	const bool debug = false;//device offset will be incorrect for debug device 
	UINT flags = debug ? D3D11_CREATE_DEVICE_DEBUG : 0;

	hr = create(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
		    flags, levels,
		    sizeof levels / sizeof levels[0], D3D11_SDK_VERSION, &desc,
		    &info.swap, &info.device,
		    nullptr, &info.context);

	adapter->Release();
	if (FAILED(hr)) {
		return false;
	}

	return true;
}

static inline void dxgi_free(dxgi_info &info)
{
	if (info.context)
		info.context->Release();
	if (info.device)
		info.device->Release();
	if (info.swap)
		info.swap->Release();
	if (info.hwnd)
		DestroyWindow(info.hwnd);
}

void get_dxgi_offsets(struct dxgi_offsets *offsets,
		      struct dxgi_offsets2 *offsets2,
                      struct d3d11_offsets *offsets3)
{
	dxgi_info info = {};
	bool success = dxgi_init(info);
	HRESULT hr;

	if (success) {
		offsets->present = vtable_offset(info.module, info.swap, 8);
		offsets->resize = vtable_offset(info.module, info.swap, 13);
		offsets3->create_texture2d = vtable_offset(info.d3d11_module, info.device, 0);
		offsets3->om_set_render_targets = vtable_offset(info.d3d11_module, info.context, 7 + 26);
		
		IDXGISwapChain1 *swap1;
		hr = info.swap->QueryInterface(__uuidof(IDXGISwapChain1),
					       (void **)&swap1);
		if (SUCCEEDED(hr)) {
			offsets->present1 =
				vtable_offset(info.module, swap1, 22);
			swap1->Release();
		}

		offsets2->release = vtable_offset(info.module, info.swap, 2);
	}

	dxgi_free(info);
}
