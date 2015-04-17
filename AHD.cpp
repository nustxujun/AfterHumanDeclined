#include "AHD.h"
#include "AHDUtils.h"
#include <vector>
#include <algorithm>
#include <d3dcompiler.h>
#include <xnamath.h>

#undef max

using namespace AHD;

#pragma comment (lib,"d3d11.lib")
#pragma comment (lib,"d3dx11.lib")

#define EXCEPT(x) {throw std::exception(x);}

#define CHECK_RESULT(x, y) { if (FAILED(x)) EXCEPT(y); }

HRESULT Voxelizer::createDevice(ID3D11Device** device, ID3D11DeviceContext** context)
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
		device,
		&lvl,
		context
		);
}

HRESULT Voxelizer::createUAV(ID3D11Texture3D** buffer, ID3D11UnorderedAccessView** uav, ID3D11Device* device, int width, int height, int depth)
{
	DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT;

	D3D11_TEXTURE3D_DESC TextureData;
	ZeroMemory(&TextureData, sizeof(TextureData));
	TextureData.Height = height;
	TextureData.Width = width;
	TextureData.Format = format;
	TextureData.CPUAccessFlags = 0;
	TextureData.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS;
	TextureData.MipLevels = 1;
	TextureData.MiscFlags = 0;
	TextureData.Usage = D3D11_USAGE_DEFAULT;

	TextureData.Depth = depth;

	HRESULT hr = device->CreateTexture3D(&TextureData, 0, buffer);
	if (hr != S_OK)
		return hr;
	D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
	desc.Format = format;
	desc.Texture3D.MipSlice = 0;
	desc.Texture3D.FirstWSlice = 0;
	desc.Texture3D.WSize = depth;

	hr = device->CreateUnorderedAccessView(*buffer, &desc, uav);
	return hr;
}
HRESULT Voxelizer::createRenderTarget(ID3D11Texture2D** target, ID3D11RenderTargetView** targetView, ID3D11Device* device, int width, int height)
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
	if (FAILED(hr = device->CreateTexture2D(&dsDesc, 0, target)))
		return hr;

	if (FAILED(hr = device->CreateRenderTargetView(*target, 0, targetView)))
		return hr;

	return hr;
}

HRESULT Voxelizer::createDepthStencil(ID3D11Texture2D** ds, ID3D11DepthStencilView** dsv, ID3D11Device* device, int width, int height)
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
	HRESULT hr = device->CreateTexture2D(&dsDesc, 0, ds);
	if (hr != S_OK)
		return hr;
	hr = device->CreateDepthStencilView(*ds, 0, dsv);
	return hr;
}

HRESULT Voxelizer::compileShader(ID3DBlob** out, const char* filename, const char* function, const char* profile)
{
	ID3DBlob* ret = NULL;
	ID3DBlob* error = NULL;
	HRESULT hr = D3DX11CompileFromFileA(
		filename,
		NULL,
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

HRESULT Voxelizer::createBuffer(ID3D11Buffer** buffer, ID3D11Device* device, D3D11_BIND_FLAG flag, size_t size, const void* initdata)
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
	return device->CreateBuffer(&bd, initdata ? &InitData : 0, buffer);
}

void Voxelizer::voxelize(Result& result, const Parameter& para)
{
	AABB aabb;

	//calculate the max size
	size_t buffersize = para.vertexCount * para.vertexStride;
	{
		const char* begin = (const char*)para.vertices;
		const char* end = begin + buffersize;
		for (; begin != end; begin += para.vertexStride)
		{
			Vector3 v = (*(const Vector3*)begin) * para.meshScale;
			aabb.merge(v);
		}
	}


	Vector3 size = aabb.getSize();
	Vector3 tran = -aabb.getMin();

	aabb.setExtents(Vector3::ZERO, size);
	size = aabb.getSize() / para.voxelSize;
	size += Vector3::UNIT_SCALE;
	int width = std::floor(size.x);
	int height = std::floor(size.y);
	int depth = std::floor(size.z);

	result.width = width;
	result.height = height;
	result.depth = depth;

	int length = std::max(width, std::max(height, depth));

	Interface<ID3D11Device> device;
	Interface<ID3D11DeviceContext> context;

	HRESULT hr = S_OK;
	CHECK_RESULT(createDevice(&device, &context), "fail to create device, cant use gpu voxelizer");

	Interface<ID3D11Texture3D> output;
	Interface<ID3D11UnorderedAccessView> outputUAV;
	hr = createUAV(&output, &outputUAV, device, width, height, depth);


	Interface<ID3D11Texture2D> rendertarget;
	Interface<ID3D11RenderTargetView> rendertargetview;
	hr = createRenderTarget(&rendertarget, &rendertargetview, device, length, length);

	context->OMSetRenderTargetsAndUnorderedAccessViews(1, &rendertargetview, NULL, 1, 1, &outputUAV, NULL);

	Interface<ID3D11InputLayout> layout;
	Interface<ID3D11VertexShader> vertexShader;
	Interface<ID3D11PixelShader> pixelShader;
	{
		Interface<ID3DBlob> blob;
		CHECK_RESULT(compileShader(&blob, "voxelizer.hlsl", "vs", "vs_5_0"), "fail to compile vertex shader, cant use gpu voxelizer");
		CHECK_RESULT(device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &vertexShader), "fail to create vertex shader,  cant use gpu voxelizer");
		D3D11_INPUT_ELEMENT_DESC desc[] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };
		CHECK_RESULT(device->CreateInputLayout(desc, 1, blob->GetBufferPointer(), blob->GetBufferSize(), &layout), "fail to create layout,  cant use gpu voxelizer");

		context->VSSetShader(vertexShader, NULL, 0);
		context->IASetInputLayout(layout);

	}
	{
		Interface<ID3DBlob> blob;
		CHECK_RESULT(compileShader(&blob, "voxelizer.hlsl", "ps", "ps_5_0"), "fail to compile pixel shader,  cant use gpu voxelizer");
		CHECK_RESULT(device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &pixelShader), "fail to create pixel shader, cant use gpu voxelizer");

		context->PSSetShader(pixelShader, NULL, 0);
	}
	Interface<ID3D11Buffer> vertexBuffer;
	CHECK_RESULT(createBuffer(&vertexBuffer, device, D3D11_BIND_VERTEX_BUFFER, para.vertexCount * para.vertexStride, para.vertices), "fail to create vertex buffer,  cant use gpu voxelizer");
	UINT stride = para.vertexStride;
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	Interface<ID3D11Buffer> indexBuffer;
	bool useIndex = para.indexCount != 0 && para.indexes != nullptr && para.indexStride != 0;
	if (useIndex)
	{
		CHECK_RESULT(createBuffer(&indexBuffer, device, D3D11_BIND_INDEX_BUFFER, para.indexCount * para.indexStride, para.indexes), "fail to create index buffer,  cant use gpu voxelizer");

		DXGI_FORMAT format = DXGI_FORMAT_R16_UINT;
		switch (para.indexStride)
		{
			case 2: format = DXGI_FORMAT_R16_UINT; break;
			case 4: format = DXGI_FORMAT_R32_UINT; break;

			default:
				EXCEPT("unknown index format");
				break;
		}
		context->IASetIndexBuffer(indexBuffer, format, 0);
	}

	Interface<ID3D11Buffer> constantBuffer;
	struct ConstantBuffer
	{
		XMMATRIX world;
		XMMATRIX view;
		XMMATRIX proj;
	} parameters;
	// all the vertices pos.xyz will move above zero
	// the render range is (length, length, length)
	parameters.world = XMMatrixTranspose(XMMatrixTranslation(tran.x, tran.y, tran.z)) *
		XMMatrixScaling(para.meshScale, para.meshScale, para.meshScale);
	XMVECTOR Eye = XMVectorSet(0.0, 0, -0.0, 0.0f);
	XMVECTOR At = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	parameters.view = XMMatrixLookAtLH(Eye, At, Up);
	parameters.view = XMMatrixTranspose(parameters.view);
	parameters.proj = XMMatrixOrthographicOffCenterLH(0, length, 0, length, 0, length);
	parameters.proj = XMMatrixTranspose(parameters.proj);



	CHECK_RESULT(createBuffer(&constantBuffer, device, D3D11_BIND_CONSTANT_BUFFER, sizeof(parameters), &parameters), "fail to create constant buffer,  cant use gpu voxelizer");
	context->VSSetConstantBuffers(0, 1, &constantBuffer);


	//no need to cull
	Interface<ID3D11RasterizerState> rasterizerState;
	{
		D3D11_RASTERIZER_DESC desc;
		desc.FillMode = D3D11_FILL_SOLID;
		desc.CullMode = D3D11_CULL_NONE;
		desc.FrontCounterClockwise = false;
		desc.DepthBias = 0;
		desc.DepthBiasClamp = 0;
		desc.SlopeScaledDepthBias = 0;
		desc.DepthClipEnable = true;
		desc.ScissorEnable = false;
		desc.MultisampleEnable = false;
		desc.AntialiasedLineEnable = false;

		CHECK_RESULT(device->CreateRasterizerState(&desc, &rasterizerState), "fail to create rasterizer state,  cant use gpu voxelizer");
		context->RSSetState(rasterizerState);
	}



	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)length;
	vp.Height = (FLOAT)length;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	context->RSSetViewports(1, &vp);


	struct ViewPara
	{
		Vector3 eye;
		Vector3 at;
		Vector3 up;
	};


	//we need to render 3 times from different views
	ViewPara views[] =
	{
		Vector3::ZERO, Vector3::UNIT_Z * length, Vector3::UNIT_Y,
		Vector3::UNIT_X * length, Vector3::ZERO, Vector3::UNIT_Y,
		Vector3::UNIT_Y * length, Vector3::ZERO, Vector3::UNIT_Z,
	};

	for (ViewPara& v : views)
	{
		XMVECTOR Eye = XMVectorSet(v.eye.x, v.eye.y, v.eye.z, 0.0f);
		XMVECTOR At = XMVectorSet(v.at.x, v.at.y, v.at.z, 0.0f);
		XMVECTOR Up = XMVectorSet(v.up.x, v.up.y, v.up.z, 0.0f);
		parameters.view = XMMatrixLookAtLH(Eye, At, Up);
		parameters.view = XMMatrixTranspose(parameters.view);
		context->UpdateSubresource(constantBuffer, 0, NULL, &parameters, 0, 0);


		if (useIndex)
			context->DrawIndexed(para.indexCount, 0, 0);
		else
			context->Draw(para.vertexCount, 0);

#ifdef  _DEBUG
		{
			Interface<ID3D11Texture2D> debug;
			D3D11_TEXTURE2D_DESC desc;
			rendertarget->GetDesc(&desc);
			desc.BindFlags = 0;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			desc.MiscFlags = 0;
			desc.Usage = D3D11_USAGE_STAGING;

			device->CreateTexture2D(&desc, NULL, &debug);
			context->CopyResource(debug, rendertarget);

			D3D11_MAPPED_SUBRESOURCE mr;
			context->Map(debug, 0, D3D11_MAP_READ, 0, &mr);
			context->Unmap(debug, 0);
		}
#endif


		Interface<ID3D11Texture3D> debug = NULL;
		D3D11_TEXTURE3D_DESC dsDesc;
		output->GetDesc(&dsDesc);
		dsDesc.BindFlags = 0;
		dsDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		dsDesc.MiscFlags = 0;
		dsDesc.Usage = D3D11_USAGE_STAGING;

		CHECK_RESULT(device->CreateTexture3D(&dsDesc, NULL, &debug), "fail to create staging buffer, cant use gpu voxelizer");

		context->CopyResource(debug, output);
		D3D11_MAPPED_SUBRESOURCE mr;
		context->Map(debug, 0, D3D11_MAP_READ, 0, &mr);

		for (int z = 0; z < depth; ++z)
		{
			const char* depth = ((const char*)mr.pData + mr.DepthPitch * z);
			for (int y = 0; y < height; ++y)
			{
				const int* begin = (const int*)(depth + mr.RowPitch * y);
				for (int x = 0; x < width; ++x)
				{
					if (*begin != 0)
					{
						Voxel v = { x, y, z, begin[0], begin[1] };
						result.voxels.push_back(v);
					}

					begin += 4;
				}
			}
		}

		context->Unmap(debug, 0);
	}

	
}
