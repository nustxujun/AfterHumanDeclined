#include "AHDd3d11Helper.h"
#include <d3dcompiler.h>
using namespace AHD;

HRESULT D3D11Helper::createDevice(ID3D11Device** mDevice, ID3D11DeviceContext** mContext)
{
	D3D_FEATURE_LEVEL lvl;
	return D3D11CreateDevice(
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		D3D11_CREATE_DEVICE_SINGLETHREADED
#ifdef _DEBUG
		| D3D11_CREATE_DEVICE_DEBUG
#endif
		,
		NULL,
		0,
		D3D11_SDK_VERSION,
		mDevice,
		&lvl,
		mContext
		);
}

HRESULT D3D11Helper::createUAVBuffer(ID3D11Buffer** buffer, ID3D11UnorderedAccessView** uav, ID3D11Device* device, size_t elementSize, size_t elementCount)
{
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = elementCount * elementSize;
	bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS ;
	bd.CPUAccessFlags = 0;
	bd.StructureByteStride = elementSize;
	bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	HRESULT hr = device->CreateBuffer(&bd, NULL, buffer);
	if (FAILED(hr))
		return hr;

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
	uavDesc.Buffer.NumElements = elementCount;
	hr = device->CreateUnorderedAccessView(*buffer, &uavDesc, uav);

	if (FAILED(hr))
	{
		(*buffer)->Release();
		return hr;
	}
	return hr;
}
HRESULT D3D11Helper::createUAVCounter(ID3D11Buffer** buffer, ID3D11UnorderedAccessView** uav, ID3D11Device* device)
{
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(int);
	bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	bd.CPUAccessFlags = 0;
	bd.StructureByteStride = sizeof(int);
	bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	HRESULT hr = device->CreateBuffer(&bd, NULL, buffer);
	if (FAILED(hr))
		return hr;

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
	uavDesc.Buffer.NumElements = 1;
	hr = device->CreateUnorderedAccessView(*buffer, &uavDesc, uav);

	if (FAILED(hr))
	{
		(*buffer)->Release();
		return hr;
	}
	return hr;
}


HRESULT D3D11Helper::createUAVTexture3D(ID3D11Texture3D** buffer, ID3D11UnorderedAccessView** uav, ID3D11Device* mDevice, DXGI_FORMAT format, int width, int height, int depth)
{
	//DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT;

	D3D11_TEXTURE3D_DESC TextureData;
	ZeroMemory(&TextureData, sizeof(TextureData));
	TextureData.Height = height;
	TextureData.Width = width;
	TextureData.Format = format;
	TextureData.CPUAccessFlags = 0;
	TextureData.BindFlags =  D3D11_BIND_UNORDERED_ACCESS;
	TextureData.MipLevels = 1;
	TextureData.MiscFlags = 0;
	TextureData.Usage = D3D11_USAGE_DEFAULT;

	TextureData.Depth = depth;

	HRESULT hr = mDevice->CreateTexture3D(&TextureData, 0, buffer);
	if (hr != S_OK)
		return hr;
	D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
	desc.Format = format;
	desc.Texture3D.MipSlice = 0;
	desc.Texture3D.FirstWSlice = 0;
	desc.Texture3D.WSize = depth;

	hr = mDevice->CreateUnorderedAccessView(*buffer, &desc, uav);
	return hr;
}
HRESULT D3D11Helper::createRenderTarget(ID3D11Texture2D** target, ID3D11RenderTargetView** targetView, ID3D11Device* mDevice, int width, int height)
{
	D3D11_TEXTURE2D_DESC dsDesc;
	dsDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dsDesc.Width = width;
	dsDesc.Height = height;
	dsDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
	dsDesc.MipLevels = 1;
	dsDesc.ArraySize = 1;
	dsDesc.CPUAccessFlags = 0;
	dsDesc.SampleDesc.Count = 1;
	dsDesc.SampleDesc.Quality = 0;
	dsDesc.MiscFlags = 0;
	dsDesc.Usage = D3D11_USAGE_DEFAULT;
	HRESULT hr;
	if (FAILED(hr = mDevice->CreateTexture2D(&dsDesc, 0, target)))
		return hr;

	if (FAILED(hr = mDevice->CreateRenderTargetView(*target, 0, targetView)))
		return hr;

	return hr;
}

HRESULT D3D11Helper::createDepthStencil(ID3D11Texture2D** ds, ID3D11DepthStencilView** dsv, ID3D11Device* mDevice, int width, int height)
{
	D3D11_TEXTURE2D_DESC dsDesc;
	dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsDesc.Width = width;
	dsDesc.Height = height;
	dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	dsDesc.MipLevels = 1;
	dsDesc.ArraySize = 1;
	dsDesc.CPUAccessFlags = 0;
	dsDesc.SampleDesc.Count = 1;
	dsDesc.SampleDesc.Quality = 0;
	dsDesc.MiscFlags = 0;
	dsDesc.Usage = D3D11_USAGE_DEFAULT;
	HRESULT hr = mDevice->CreateTexture2D(&dsDesc, 0, ds);
	if (hr != S_OK)
		return hr;
	hr = mDevice->CreateDepthStencilView(*ds, 0, dsv);
	return hr;
}

HRESULT D3D11Helper::compileShader(ID3DBlob** out, const char* filename, const char* function, const char* profile, const D3D10_SHADER_MACRO* macros)
{
	ID3DBlob* ret = NULL;
	ID3DBlob* error = NULL;
	HRESULT hr = D3DX11CompileFromFileA(
		filename,
		macros,
		NULL,
		function,
		profile,
		D3DCOMPILE_ENABLE_STRICTNESS
#ifdef _DEBUG
		| D3DCOMPILE_DEBUG
#endif
		,
		0,
		NULL,
		&ret,
		&error,
		NULL);

	*out = ret;

	if (error)
	{
		OutputDebugStringA((char*)error->GetBufferPointer());
		error->Release();
	}

	return hr;

}

HRESULT D3D11Helper::createBuffer(ID3D11Buffer** buffer, ID3D11Device* mDevice, D3D11_BIND_FLAG flag, size_t size, const void* initdata)
{
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = size;
	bd.BindFlags = flag;
	bd.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = initdata;
	return mDevice->CreateBuffer(&bd, initdata ? &InitData : 0, buffer);
}
