#include "AHD.h"
#include "AHDUtils.h"
#include <vector>
#include <algorithm>
#include "AHDd3d11Helper.h"
#include <d3dcompiler.h>
#include <functional>

#undef max
#undef min

using namespace AHD;

#pragma comment (lib,"d3d11.lib")
#pragma comment (lib,"d3dx11.lib")

#define EXCEPT(x) {throw std::exception(x);}

#define CHECK_RESULT(x, y) { if (FAILED(x)) EXCEPT(y); }
#define SAFE_RELEASE(x) {if(x) (x)->Release(); (x) = 0;}


typedef D3D11Helper Helper;

namespace AHD
{

}


void Effect::init(ID3D11Device* device, const std::map<Semantic, VertexDesc>& desc)
{
	auto end = desc.end();
	bool usingTexture = desc.find(S_TEXCOORD) != end;
	bool usingColor = desc.find(S_COLOR) != end;
	std::vector<D3D10_SHADER_MACRO> macros;
	if (usingTexture)
		macros.push_back({ "USINGTEXTURE", "0" });

	if (usingColor)
		macros.push_back({ "USINGCOLOR", "0" });

	if (!macros.empty())
		macros.push_back({ NULL, NULL });


	{
		std::vector<D3D11_INPUT_ELEMENT_DESC> desc;
		desc.push_back({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
		int offset = 12;
		if (usingColor)
		{
			desc.push_back({ "COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, offset, D3D11_INPUT_PER_VERTEX_DATA, 0 });
			offset += 4;
		}
		if (usingTexture)
		{
			desc.push_back({ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offset, D3D11_INPUT_PER_VERTEX_DATA, 0 });
		}

		ID3DBlob* blob;
		CHECK_RESULT(Helper::compileShader(&blob, "DefaultEffect.hlsl", "vs", "vs_5_0", macros.data()),
					 "fail to compile vertex shader, cant use gpu voxelizer");
		CHECK_RESULT(device->CreateInputLayout(desc.data(), desc.size(), blob->GetBufferPointer(), blob->GetBufferSize(), &mLayout),
					 "fail to create mLayout,  cant use gpu voxelizer");
		CHECK_RESULT(device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mVertexShader), 
					 "fail to create vertex shader,  cant use gpu voxelizer");
		blob->Release();
	}

	{
		ID3DBlob* blob;
		CHECK_RESULT(Helper::compileShader(&blob, "DefaultEffect.hlsl", "gs", "gs_5_0", macros.data()),
			"fail to compile geometry shader,  cant use gpu voxelizer");
		CHECK_RESULT(device->CreateGeometryShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mGeometryShader),
			"fail to create geometry shader, cant use gpu voxelizer");
	}

	{
		ID3DBlob* blob;
		CHECK_RESULT(Helper::compileShader(&blob, "DefaultEffect.hlsl", "ps", "ps_5_0", macros.data()),
					 "fail to compile pixel shader,  cant use gpu voxelizer");
		CHECK_RESULT(device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mPixelShader), 
					 "fail to create pixel shader, cant use gpu voxelizer");

		blob->Release();
	}


	CHECK_RESULT(Helper::createBuffer(&mConstant, device, D3D11_BIND_CONSTANT_BUFFER, sizeof(EffectParameter)),
				 "fail to create constant buffer,  cant use gpu voxelizer");

}

void Effect::prepare(ID3D11DeviceContext* context)
{
	context->VSSetShader(mVertexShader, NULL, 0);
	context->IASetInputLayout(mLayout);
	context->PSSetShader(mPixelShader, NULL, 0);
	context->GSSetShader(mGeometryShader, NULL, 0);

	context->VSSetConstantBuffers(0, 1, &mConstant);
	context->PSSetConstantBuffers(0, 1, &mConstant);
	context->GSSetConstantBuffers(0, 1, &mConstant);

}

void Effect::update(ID3D11DeviceContext* context, EffectParameter& paras)
{
	context->UpdateSubresource(mConstant, 0, NULL, &paras, 0, 0);
}


void Effect::clean()
{
	mConstant->Release();
	mVertexShader->Release();
	mLayout->Release();
	mPixelShader->Release();
	mGeometryShader->Release();
}

void VoxelResource::setVertex(const void* vertices, size_t vertexCount, size_t vertexStride, const VertexDesc* desc, size_t size)
{
	const VertexDesc* pos = nullptr;
	const VertexDesc* color = nullptr;
	const VertexDesc* uv = nullptr;
	for (size_t i = 0; i < size; ++i)
	{
		switch (desc[i].semantic)
		{
		case S_POSITION: pos = desc + i; break;
		case S_COLOR: color = desc + i; break;
		case S_TEXCOORD: uv = desc + i; break;
		default:
			EXCEPT("unexpected semantic")
			break;
		}
		mDesc[desc[i].semantic] = desc[i];
	}


	int newVertexStride = pos->size + (uv?uv->size:0) + (color?color->size:0);
	mVertexStride = newVertexStride;
	mVertexCount = vertexCount;

	mAABB.setNull();
	size_t buffersize = vertexCount * vertexStride;
	std::vector<char> buffer;
	buffer.reserve(buffersize);
	{

		const char* begin = (const char*)vertices;
		const char* end = begin + buffersize;
		char* wbegin = buffer.data();
		for (; begin != end; begin += vertexStride)
		{
			Vector3 v = (*(const Vector3*)(begin + pos->offset));
			mAABB.merge(v);
			memcpy(wbegin, begin + pos->offset, pos->size);
			wbegin += pos->size;
			if (color)
			{
				memcpy(wbegin, begin + color->offset, color->size);
				wbegin += color->size;
			}
			if (uv)
			{
				memcpy(wbegin, begin + uv->offset, uv->size);
				wbegin += uv->size;
			}

		}
	}

	CHECK_RESULT(Helper::createBuffer(&mVertexBuffer, mDevice, D3D11_BIND_VERTEX_BUFFER, buffersize, buffer.data()),
		"fail to create vertex buffer,  cant use gpu voxelizer");
}


void VoxelResource::setIndex(const void* indexes, size_t indexCount, size_t indexStride)
{
	size_t size = indexCount * indexStride;
	mIndexStride = indexStride;
	mIndexCount = indexCount;

	CHECK_RESULT(Helper::createBuffer(&mIndexBuffer, mDevice, D3D11_BIND_INDEX_BUFFER, size, indexes),
		"fail to create index buffer,  cant use gpu voxelizer");
}

void VoxelResource::setTexture(const std::string& name)
{
	mTexture = name;
}

VoxelResource::VoxelResource(ID3D11Device* device)
	:mDevice(device)
{

}

VoxelResource::~VoxelResource()
{

}

void VoxelResource::prepare(ID3D11DeviceContext* context)
{




}

Voxelizer::Voxelizer()
{
	Helper::createDevice(&mDevice, &mContext);

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

		CHECK_RESULT(mDevice->CreateRasterizerState(&desc, &rasterizerState),
			"fail to create rasterizer state,  cant use gpu voxelizer");
		mContext->RSSetState(rasterizerState);
	}
}

Voxelizer::~Voxelizer()
{
	for (auto& i : mResources)
	{
		delete i;
	}

	for (auto& i : mOutputs)
	{
		delete i;
	}

	for (auto i : mEffects)
	{
		i.second->clean();
		delete i.second;
	}

}

void Voxelizer::setSize(float voxelSize, float scale)
{
	mScale = scale / voxelSize;
}

Vector3 Voxelizer::prepare( size_t count, VoxelResource** res)
{
	if (res == nullptr)
		return Vector3::ZERO;

	AABB aabb;
	for (size_t i = 0; i < count; ++i)
	{
		res[i]->prepare(mContext);
		aabb.merge(res[i]->mAABB);
	}

	
	Vector3 osize = aabb.getSize();

	//transfrom
	Vector3 center = aabb.getCenter();
	mTranslation = XMMatrixTranspose(XMMatrixTranslation(-center.x, -center.y, -center.z));
	

	return aabb.getSize();
}


void Voxelizer::voxelize(VoxelOutput* output, size_t count, VoxelResource** res)
{
	Vector3 range;
	if ((range = prepare(count, res)) == Vector3::ZERO)
	{
		EXCEPT(" cant use gpu voxelizer");
	}

	auto mapBuffer = [this](std::function<void(void*)> cb, UAVObj& obj)
	{

		Interface<ID3D11Buffer> debug = NULL;
		D3D11_BUFFER_DESC dsDesc;
		obj.buffer->GetDesc(&dsDesc);
		dsDesc.BindFlags = 0;
		dsDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		dsDesc.MiscFlags = 0;
		dsDesc.Usage = D3D11_USAGE_STAGING;

		CHECK_RESULT(mDevice->CreateBuffer(&dsDesc, NULL, &debug),
			"fail to create staging buffer, cant use gpu voxelizer");

		mContext->CopyResource(debug, obj.buffer);
		D3D11_MAPPED_SUBRESOURCE mr;
		mContext->Map(debug, 0, D3D11_MAP_READ, 0, &mr);
		//memcpy(data, mr.pData, size);
		cb(mr.pData);
		mContext->Unmap(debug, 0);
	};


	auto render = [mapBuffer, count, range, &res, this](UAVObj& uav, std::function<void(void*)> cb, bool bcount)
	{
		mContext->OMSetRenderTargetsAndUnorderedAccessViews(
			0, 0, NULL, bcount ? 0 : 1, 1, &uav.uav, NULL);
		UINT initcolor[4] = { 0 };
		mContext->ClearUnorderedAccessViewUint(uav.uav, initcolor);
		for (size_t i = 0; i < count; ++i)
		{
			voxelizeImpl(res[i], range, bcount);
		}
		mapBuffer(cb, uav);

	};

	UAVObj counter;
	Helper::createUAVCounter(&counter.buffer, &counter.uav, mDevice);
	int numVoxels = 0;
	render(counter, [&numVoxels](void* data){numVoxels = *((int*)data); }, true);

	if (numVoxels)
	{
		UAVObj target;
		Helper::createUAVBuffer(&target.buffer, &target.uav, mDevice, sizeof(Voxel), numVoxels);
		render(target, std::function<void(void*)>([&output, numVoxels](void*data){output->output((Voxel*)data, numVoxels); }), false);
	}
	else
	{
		output->output(nullptr, 0);
	}
}


Effect* Voxelizer::getEffect(VoxelResource* res)
{
	auto end = res->mDesc.end();
	bool usingTexture = res->mDesc.find(S_TEXCOORD) != end;
	bool usingColor = res->mDesc.find(S_COLOR) != end;
	int hash = 1 + (usingColor ? 2 : 0) + (usingTexture ? 4 : 0);

	auto ret = mEffects.find(hash);
	if (ret == mEffects.end())
	{
		Effect* effect = new Effect();
		effect->init(mDevice, res->mDesc);
		mEffects[hash] = effect;
		return effect;
	}
	else
		return ret->second;
}


void Voxelizer::voxelizeImpl(VoxelResource* res, const Vector3& range, bool countOnly)
{


	UINT stride = res->mVertexStride;
	UINT offset = 0;
	mContext->IASetVertexBuffers(0, 1, &res->mVertexBuffer, &stride, &offset);
	mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	if (!res->mTexture.empty())
	{
		auto tex = mTextures.find(res->mTexture);

		mContext->PSSetShaderResources(0, 1, &tex->second.srv);
		mContext->PSSetSamplers(0, 1, &tex->second.sampler);
	}
	int start = 0;
	int count = res->mVertexCount;

	bool useIndex = res->mIndexBuffer != nullptr;
	if (useIndex)
	{
		DXGI_FORMAT format = DXGI_FORMAT_R16_UINT;
		switch (res->mIndexStride)
		{
		case 2: format = DXGI_FORMAT_R16_UINT; break;
		case 4: format = DXGI_FORMAT_R32_UINT; break;
		default:
			EXCEPT("unknown index format");
			break;
		}
		mContext->IASetIndexBuffer(res->mIndexBuffer, format, 0);

		count = res->mIndexCount;
	}

	Effect* effect = getEffect(res);
	effect->prepare(mContext);

	struct ViewPara
	{
		Vector3 eye;
		Vector3 dir;
		Vector3 up;
	};

	

	float length = std::max(range.x, std::max(range.y, range.z));

	EffectParameter parameters;
	parameters.bcount = countOnly;
	parameters.world = mTranslation;
	parameters.width = length* mScale;
	parameters.height = length * mScale;
	parameters.depth = length * mScale;



	const ViewPara views[] =
	{
		Vector3::ZERO, Vector3::UNIT_X, Vector3::UNIT_Y,
		Vector3::ZERO, Vector3::UNIT_Y, Vector3::UNIT_Z,
		Vector3::ZERO, Vector3::UNIT_Z, Vector3::UNIT_Y,
	};

	size_t arraysize = ARRAYSIZE(views);

	for (size_t i = 0; i < arraysize; ++i)
	{
		const ViewPara& v = views[i];
		const XMVECTOR Eye = XMVectorSet(v.eye.x, v.eye.y, v.eye.z, 0.0f);
		const XMVECTOR Dir = XMVectorSet(v.dir.x, v.dir.y, v.dir.z, 0.0f);
		const XMVECTOR Up = XMVectorSet(v.up.x, v.up.y, v.up.z, 0.0f);

		parameters.views[i] = XMMatrixTranspose(XMMatrixLookToLH(Eye, Dir, Up));

	}

	float half = length / 2;
	parameters.proj = XMMatrixTranspose(XMMatrixOrthographicOffCenterLH(
		-half, half, -half, half, -half, half));

	D3D11_VIEWPORT vp;
	vp.Width = length* mScale;
	vp.Height = length* mScale;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	mContext->RSSetViewports(1, &vp);

	effect->update(mContext,parameters);

	if (useIndex)
		mContext->DrawIndexed(count, start, 0);
	else
		mContext->Draw(count, start);

}


VoxelResource* Voxelizer::createResource()
{
	VoxelResource* vr = new VoxelResource(mDevice);
	mResources.push_back(vr);
	return vr;
}

void Voxelizer::addTexture(const std::string& name, size_t width, size_t height, void* data)
{
	if (hasTexture(name))
		return;

	auto& texture = mTextures[name];
	texture.width = width;
	texture.height = height;
	{
		D3D11_TEXTURE2D_DESC desc;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.Width = width;
		desc.Height = height;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.CPUAccessFlags = 0;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.MiscFlags = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;

		D3D11_SUBRESOURCE_DATA initdata;
		initdata.pSysMem = data;
		initdata.SysMemPitch = 4 * width;
		initdata.SysMemSlicePitch = 4 * width * height;
		CHECK_RESULT(mDevice->CreateTexture2D(&desc, &initdata, &texture.texture), "fail to create texture2d");
	}
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC desc;
			desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MostDetailedMip = 0;
			desc.Texture2D.MipLevels = -1;
			CHECK_RESULT(mDevice->CreateShaderResourceView(texture.texture, &desc, &texture.srv), "fail to creat srv");

			//texture->Release();

		}

		{
			D3D11_SAMPLER_DESC sampDesc;
			ZeroMemory(&sampDesc, sizeof(sampDesc));
			sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
			sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
			sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
			sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
			sampDesc.MinLOD = 0;
			sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
			sampDesc.MaxAnisotropy = 16;
			CHECK_RESULT(mDevice->CreateSamplerState(&sampDesc, &texture.sampler), "fail to creat sampler");
		}

}

bool Voxelizer::hasTexture(const std::string& name)
{
	return mTextures.find(name) != mTextures.end();
}
