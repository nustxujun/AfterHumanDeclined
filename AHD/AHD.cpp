#include "AHD.h"
#include "AHDUtils.h"
#include <vector>
#include <algorithm>
#include "AHDd3d11Helper.h"
#include <d3dcompiler.h>

#undef max
#undef min

using namespace AHD;

#pragma comment (lib,"d3d11.lib")
#pragma comment (lib,"d3dx11.lib")

#define EXCEPT(x) {throw std::exception(x);}

#define CHECK_RESULT(x, y) { if (FAILED(x)) EXCEPT(y); }
#define SAFE_RELEASE(x) {if(x) (x)->Release(); (x) = 0;}


typedef D3D11Helper Helper;

void DefaultEffect::init(ID3D11Device* device)
{
	{
		D3D11_INPUT_ELEMENT_DESC desc[] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };

		ID3DBlob* blob;
		CHECK_RESULT(Helper::compileShader(&blob, "voxelizer.hlsl", "vs", "vs_5_0", NULL), 
					 "fail to compile vertex shader, cant use gpu voxelizer");
		CHECK_RESULT(device->CreateInputLayout(desc, ARRAYSIZE(desc), blob->GetBufferPointer(), blob->GetBufferSize(), &mLayout), 
					 "fail to create mLayout,  cant use gpu voxelizer");
		CHECK_RESULT(device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mVertexShader), 
					 "fail to create vertex shader,  cant use gpu voxelizer");
		blob->Release();
	}


	{
		ID3DBlob* blob;
		CHECK_RESULT(Helper::compileShader(&blob, "voxelizer.hlsl", "ps", "ps_5_0", NULL), 
					 "fail to compile pixel shader,  cant use gpu voxelizer");
		CHECK_RESULT(device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mPixelShader), 
					 "fail to create pixel shader, cant use gpu voxelizer");

		blob->Release();
	}


	CHECK_RESULT(Helper::createBuffer(&mConstant, device, D3D11_BIND_CONSTANT_BUFFER, sizeof(XMMATRIX) * 3), 
				 "fail to create constant buffer,  cant use gpu voxelizer");

}

void DefaultEffect::prepare(ID3D11DeviceContext* context)
{
	context->VSSetShader(mVertexShader, NULL, 0);
	context->IASetInputLayout(mLayout);
	context->PSSetShader(mPixelShader, NULL, 0);
	context->VSSetConstantBuffers(0, 1, &mConstant);

}

void DefaultEffect::update(EffectParameter& paras)
{
	paras.context->UpdateSubresource(mConstant, 0, NULL, &paras.world, 0, 0);

}


void DefaultEffect::clean()
{
	mConstant->Release();
	mVertexShader->Release();
	mLayout->Release();
	mPixelShader->Release();
}

void VoxelResource::setVertex(const void* vertices, size_t vertexCount, size_t vertexStride, size_t posoffset )
{
	mVertexStride = vertexStride;
	mVertexCount = vertexCount;
	mPositionOffset = posoffset;

	//calculate the max size
	size_t buffersize = vertexCount * vertexStride;
	{
		const char* begin = (const char*)vertices;
		const char* end = begin + buffersize;
		for (; begin != end; begin += vertexStride)
		{
			Vector3 v = (*(const Vector3*)begin) ;
			mAABB.merge(v);
		}
	}

	mNeedCalSize = false;

	mVertexBuffer.release();
	CHECK_RESULT(Helper::createBuffer(&mVertexBuffer, mDevice, D3D11_BIND_VERTEX_BUFFER, vertexCount * vertexStride, vertices),
				 "fail to create vertex buffer,  cant use gpu voxelizer");


}

void VoxelResource::setVertexFromVoxelResource(VoxelResource* res)
{
	mAABB = res->mAABB;

	setVertex(res->mVertexBuffer, res->mVertexCount, res->mVertexStride, res->mPositionOffset);
}

void VoxelResource::setVertex(ID3D11Buffer* vertexBuffer, size_t vertexCount, size_t vertexStride, size_t posoffset )
{
	mVertexStride = vertexStride;
	mVertexCount = vertexCount;
	mPositionOffset = posoffset;

	vertexBuffer->AddRef();
	mVertexBuffer.release();
	mVertexBuffer = vertexBuffer;

	mNeedCalSize = true;
}

void VoxelResource::setIndex(const void* indexes, size_t indexCount, size_t indexStride)
{
	mIndexCount = indexCount;
	mIndexStride = indexStride;

	mIndexBuffer.release();
	CHECK_RESULT(Helper::createBuffer(&mIndexBuffer, mDevice, D3D11_BIND_INDEX_BUFFER, indexCount * indexStride, indexes),
				 "fail to create index buffer,  cant use gpu voxelizer");
}

void VoxelResource::setIndex(ID3D11Buffer* indexBuffer, size_t indexCount, size_t indexStride)
{
	mIndexCount = indexCount;
	mIndexStride = indexStride;

	indexBuffer->AddRef();
	mIndexBuffer.release();
	mIndexBuffer = indexBuffer;
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
	if (!mNeedCalSize)
		return;

	if (mVertexBuffer == nullptr)
		return;

	D3D11_BUFFER_DESC desc;
	mVertexBuffer->GetDesc(&desc);

	AABB aabb;
	auto calSize = [&aabb](ID3D11DeviceContext* context, ID3D11Buffer* buffer, size_t count , size_t stride, size_t offset)
	{
		D3D11_MAPPED_SUBRESOURCE mr;
		context->Map(buffer, 0, D3D11_MAP_READ, 0, &mr);

		size_t buffersize = count * stride;
		{
			const char* begin = (const char*)mr.pData;
			const char* end = begin + buffersize;
			for (; begin != end; begin += stride)
			{
				Vector3 v = (*(const Vector3*)(begin + offset));
				aabb.merge(v);
			}
		}

		context->Unmap(buffer, 0);
	};

	if ((desc.CPUAccessFlags & D3D11_CPU_ACCESS_READ) != 0)
	{
		calSize(context, mVertexBuffer, mVertexCount, mVertexStride, mPositionOffset);
	}
	else
	{
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.BindFlags = 0;
		desc.Usage = D3D11_USAGE_STAGING;
		ID3D11Buffer* tmp = 0;
		CHECK_RESULT(mDevice->CreateBuffer(&desc, nullptr, &tmp),
					 "fail to create temp buffer for reading.");
		calSize(context, tmp, mVertexCount, mVertexStride, mPositionOffset);
		tmp->Release();
	}
}





Voxelizer::Voxelizer()
{
	Helper::createDevice(&mDevice, &mContext);

	mDefaultEffect.init(mDevice);
	setEffectAndUAVParameters(&mDefaultEffect,
							  DefaultEffect::OUTPUT_FORMAT,
							  DefaultEffect::SLOT,
							  DefaultEffect::ELEM_SIZE);
}

Voxelizer::Voxelizer(ID3D11Device* device, ID3D11DeviceContext* context)
{
	mDevice = device;
	device->AddRef();
	mContext = context;
	context->AddRef();

	mDefaultEffect.init(mDevice);
	setEffectAndUAVParameters(&mDefaultEffect, 
							  DefaultEffect::OUTPUT_FORMAT, 
							  DefaultEffect::SLOT, 
							  DefaultEffect::ELEM_SIZE);
}

Voxelizer::~Voxelizer()
{
	cleanResource();

	for (auto i : mResources)
	{
		delete i;
	}

	mDefaultEffect.clean();
	for (auto i : mEffects)
	{
		i->clean();
		delete i;
	}

}

void Voxelizer::cleanResource()
{
	mOutputTexture3D.release();
	mOutputUAV.release();
	mRenderTarget.release();
	mRenderTargetView.release();

}


void Voxelizer::setVoxelSize(float v)
{
	mVoxelSize = v;
}

void Voxelizer::setScale(float v)
{
	mScale = v;
}

bool Voxelizer::prepare(VoxelResource* res, const AABB* aabb)
{
	if (res == nullptr)
		return false;
	mCurrentEffect->prepare(mContext);
	res->prepare(mContext);



	if (!aabb)
	{
		aabb = &res->mAABB;
	}


	float scale = mScale / mVoxelSize;
	Vector3 osize = aabb->getSize() * scale;
	osize += Vector3::UNIT_SCALE;
	float max = std::max(osize.x, std::max(osize.y, osize.z));
	Size size =
	{
		(size_t)std::ceil(osize.x),
		(size_t)std::ceil(osize.y),
		(size_t)std::ceil(osize.z),
		(size_t)std::ceil(max)
	};

	if (!mRenderTarget.isNull() && !mOutputTexture3D.isNull() && mSize == size)
		return true;
	mSize = size;

	cleanResource();

	//transfrom
	Vector3 min = aabb->getMin() * scale;
	mTranslation = XMMatrixTranspose(XMMatrixTranslation(-min.x, -min.y, -min.z)) *
		XMMatrixScaling(scale, scale, scale);
	mProjection = XMMatrixTranspose(XMMatrixOrthographicOffCenterLH(0, (float)mSize.maxLength, 0, (float)mSize.maxLength, 0, (float)mSize.maxLength));

	CHECK_RESULT(Helper::createUAVTexture3D(&mOutputTexture3D, &mOutputUAV, mDevice, mUAVFormat, mSize.width, mSize.height, mSize.depth),
				 "failed to create uav texture3D,  cant use gpu voxelizer");

	UINT initcolor[4] = { 0 };
	mContext->ClearUnorderedAccessViewUint(mOutputUAV, initcolor);

	CHECK_RESULT(Helper::createRenderTarget(&mRenderTarget, &mRenderTargetView, mDevice, mSize.maxLength, mSize.maxLength),
				 "failed to create rendertarget,  cant use gpu voxelizer");


	return true;
}

void Voxelizer::setEffectAndUAVParameters(Effect* effect, DXGI_FORMAT Format, UINT slot, size_t elemSize)
{

	if (slot == 0)
	{
		EXCEPT("uav slot 0 is almost used for rendertarget,cannot be 0.");
	}
	if (mUAVFormat != Format)
	{
		cleanResource();
	}

	mUAVSlot = slot;
	mCurrentEffect = effect;


	mUAVFormat = Format;

	mUAVElementSize = elemSize;
	
}


void Voxelizer::voxelize(VoxelResource* res, const AABB* range, size_t drawBegin , size_t drawCount)
{
	if (!prepare(res, range))
	{
		EXCEPT(" cant use gpu voxelizer");
	}
	//slot may be changed;
	mContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &mRenderTargetView, NULL, mUAVSlot, 1, &mOutputUAV, NULL);

	UINT stride = res->mVertexStride;
	UINT offset = 0;
	mContext->IASetVertexBuffers(0, 1, &res->mVertexBuffer, &stride, &offset);
	mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	int start = std::max((size_t)0, drawBegin);
	int count = std::min((size_t)res->mVertexCount, drawCount);

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

	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)mSize.maxLength;
	vp.Height = (FLOAT)mSize.maxLength;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	mContext->RSSetViewports(1, &vp);


	struct ViewPara
	{
		Vector3 eye;
		Vector3 at;
		Vector3 up;
	};


	EffectParameter parameters;
	parameters.device = mDevice;
	parameters.context = mContext;
	parameters.world = mTranslation;
	parameters.proj = mProjection;

	//we need to render 3 times from different views
	ViewPara views[] =
	{
		Vector3::ZERO, Vector3::UNIT_Z * (float)mSize.maxLength, Vector3::UNIT_Y,
		Vector3::UNIT_X * (float)mSize.maxLength, Vector3::ZERO, Vector3::UNIT_Y,
		Vector3::UNIT_Y * (float)mSize.maxLength, Vector3::ZERO, Vector3::UNIT_Z,
	};

	for (ViewPara& v : views)
	{
		XMVECTOR Eye = XMVectorSet(v.eye.x, v.eye.y, v.eye.z, 0.0f);
		XMVECTOR At = XMVectorSet(v.at.x, v.at.y, v.at.z, 0.0f);
		XMVECTOR Up = XMVectorSet(v.up.x, v.up.y, v.up.z, 0.0f);
		parameters.view = XMMatrixLookAtLH(Eye, At, Up);
		parameters.view = XMMatrixTranspose(parameters.view);

		mCurrentEffect->update(parameters);

		if (useIndex)
			mContext->DrawIndexed(count, start, 0);
		else
			mContext->Draw(count, start);

	}

}

void Voxelizer::exportVoxels(Result& result)
{
	result.width = mSize.width;
	result.height = mSize.height;
	result.depth = mSize.depth;

	Interface<ID3D11Texture3D> debug = NULL;
	D3D11_TEXTURE3D_DESC dsDesc;
	mOutputTexture3D->GetDesc(&dsDesc);
	dsDesc.BindFlags = 0;
	dsDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	dsDesc.MiscFlags = 0;
	dsDesc.Usage = D3D11_USAGE_STAGING;

	CHECK_RESULT(mDevice->CreateTexture3D(&dsDesc, NULL, &debug), 
				 "fail to create staging buffer, cant use gpu voxelizer");

	mContext->CopyResource(debug, mOutputTexture3D);
	D3D11_MAPPED_SUBRESOURCE mr;
	mContext->Map(debug, 0, D3D11_MAP_READ, 0, &mr);

	int stride = mUAVElementSize * mSize.width;
	result.datas.reserve(stride * mSize.height * mSize.depth);
	char* begin = result.datas.data();
	for (size_t z = 0; z < mSize.depth; ++z)
	{
		const char* depth = ((const char*)mr.pData + mr.DepthPitch * z);
		for (size_t y = 0; y < mSize.height; ++y)
		{
			memcpy(begin, depth + mr.RowPitch * y, stride);
			begin += stride;
		}
	}

	mContext->Unmap(debug, 0);

	UINT initcolor[4] = { 0 };
	mContext->ClearUnorderedAccessViewUint(mOutputUAV, initcolor);
}

VoxelResource* Voxelizer::createResource()
{
	VoxelResource* vr = new VoxelResource(mDevice);
	mResources.push_back(vr);
	return vr;
}