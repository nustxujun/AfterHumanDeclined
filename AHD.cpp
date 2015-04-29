#include "AHD.h"
#include "AHDUtils.h"
#include <vector>
#include <algorithm>
#include "AHDd3d11Helper.h"
#include <d3dcompiler.h>

#undef max

using namespace AHD;

#pragma comment (lib,"d3d11.lib")
#pragma comment (lib,"d3dx11.lib")

#define EXCEPT(x) {throw std::exception(x);}

#define CHECK_RESULT(x, y) { if (FAILED(x)) EXCEPT(y); }

typedef D3D11Helper Helper;

void DefaultEffect::init(ID3D11Device* device, ID3D11DeviceContext* context)
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

	context->VSSetShader(mVertexShader, NULL, 0);
	context->IASetInputLayout(mLayout);

	{
		ID3DBlob* blob;
		CHECK_RESULT(Helper::compileShader(&blob, "voxelizer.hlsl", "ps", "ps_5_0", NULL), 
					 "fail to compile pixel shader,  cant use gpu voxelizer");
		CHECK_RESULT(device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mPixelShader), 
					 "fail to create pixel shader, cant use gpu voxelizer");

		blob->Release();
	}
	context->PSSetShader(mPixelShader, NULL, 0);


	CHECK_RESULT(Helper::createBuffer(&mConstant, device, D3D11_BIND_CONSTANT_BUFFER, sizeof(XMMATRIX) * 3), 
				 "fail to create constant buffer,  cant use gpu voxelizer");
	context->VSSetConstantBuffers(0, 1, &mConstant);

}

void DefaultEffect::prepare(EffectParameter& paras)
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

Voxelizer::Voxelizer()
{
	Helper::createDevice(&mDevice, &mContext);

}

Voxelizer::Voxelizer(ID3D11Device* device, ID3D11DeviceContext* context)
{
	mDevice = device;
	device->AddRef();
	mContext = context;
	context->AddRef();
}

Voxelizer::~Voxelizer()
{
	cleanResource();

	mEffect->clean();

	mContext.release();
	mDevice.release();
}

void Voxelizer::cleanResource()
{
	mOutputTexture3D.release();
	mOutputUAV.release();
	mRenderTarget.release();
	mRenderTargetView.release();
	mVertexBuffer.release();
	mIndexBuffer.release();
}

void Voxelizer::setMesh(const MeshWrapper& mesh, float scale)
{
	mMesh = mesh;
	mScale = scale;

	prepare();
}

void Voxelizer::setVoxelSize(float v)
{
	mVoxelSize = v;
}

void Voxelizer::setEffect(Effect* effect, DXGI_FORMAT format)
{
	if (mEffect != nullptr)
	{
		mEffect->clean();
	}

	mEffect = effect;
	effect->init(mDevice, mContext);
	mFormat = format;
}

bool Voxelizer::prepare()
{
	if (mDevice.isNull())
		return false;

	cleanResource();

	if (mEffect == nullptr)
		setEffect(&mDefaultEffect, DefaultEffect::OUTPUT_FORMAT);

	AABB aabb;

	//calculate the max size
	size_t buffersize = mMesh.vertexCount * mMesh.vertexStride;
	{
		const char* begin = (const char*)mMesh.vertices;
		const char* end = begin + buffersize;
		for (; begin != end; begin += mMesh.vertexStride)
		{
			Vector3 v = (*(const Vector3*)begin) * mScale;
			aabb.merge(v);
		}
	}


	Vector3 size = aabb.getSize();
	Vector3 tran = -aabb.getMin();

	aabb.setExtents(Vector3::ZERO, size);
	size = aabb.getSize() / mVoxelSize;
	size += Vector3::UNIT_SCALE;
	mSize.width = (int)std::floor(size.x);
	mSize.height = (int)std::floor(size.y);
	mSize.depth = (int)std::floor(size.z);

	mSize.maxLength = std::max(mSize.width, std::max(mSize.height, mSize.depth));

	//transfrom
	mTranslation = XMMatrixTranspose(XMMatrixTranslation(tran.x, tran.y, tran.z)) *
		XMMatrixScaling(mScale, mScale, mScale);
	mProjection = XMMatrixTranspose(XMMatrixOrthographicOffCenterLH(0, (float)mSize.maxLength, 0, (float)mSize.maxLength, 0, (float)mSize.maxLength));

	CHECK_RESULT(Helper::createUAVTexture3D(&mOutputTexture3D, &mOutputUAV, mDevice, mFormat, mSize.width, mSize.height, mSize.depth),
				 "failed to create uav texture3D,  cant use gpu voxelizer");

	CHECK_RESULT(Helper::createRenderTarget(&mRenderTarget, &mRenderTargetView, mDevice, mSize.maxLength, mSize.maxLength),
				 "failed to create rendertarget,  cant use gpu voxelizer");
	mContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &mRenderTargetView, NULL, 1, 1, &mOutputUAV, NULL);

	CHECK_RESULT(Helper::createBuffer(&mVertexBuffer, mDevice, D3D11_BIND_VERTEX_BUFFER, mMesh.vertexCount * mMesh.vertexStride, mMesh.vertices), 
				 "fail to create vertex buffer,  cant use gpu voxelizer");
	UINT stride = mMesh.vertexStride;
	UINT offset = 0;
	mContext->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);
	mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	bool useIndex = mMesh.indexCount != 0 && mMesh.indexes != nullptr && mMesh.indexStride != 0;
	if (useIndex)
	{
		CHECK_RESULT(Helper::createBuffer(&mIndexBuffer, mDevice, D3D11_BIND_INDEX_BUFFER, mMesh.indexCount * mMesh.indexStride, mMesh.indexes), "fail to create index buffer,  cant use gpu voxelizer");

		DXGI_FORMAT format = DXGI_FORMAT_R16_UINT;
		switch (mMesh.indexStride)
		{
		case 2: format = DXGI_FORMAT_R16_UINT; break;
		case 4: format = DXGI_FORMAT_R32_UINT; break;

		default:
			EXCEPT("unknown index format");
			break;
		}
		mContext->IASetIndexBuffer(mIndexBuffer, format, 0);
	}


	return true;
}


void Voxelizer::voxelize(Result& result, int start, int count)
{

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

	bool useIndex = !mIndexBuffer.isNull();

	EffectParameter parameters;
	parameters.device = mDevice;
	parameters.context = mContext;
	parameters.world = mTranslation;
	parameters.proj = mProjection;

	{
		//uav is our real mRenderTarget
		UINT initcolor[4] = { 0 };
		mContext->ClearUnorderedAccessViewUint(mOutputUAV, initcolor);

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

			mEffect->prepare(parameters);

			if (useIndex)
				mContext->DrawIndexed(count, start, 0);
			else
				mContext->Draw(count, start);

			Interface<ID3D11Texture3D> debug = NULL;
			D3D11_TEXTURE3D_DESC dsDesc;
			mOutputTexture3D->GetDesc(&dsDesc);
			dsDesc.BindFlags = 0;
			dsDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			dsDesc.MiscFlags = 0;
			dsDesc.Usage = D3D11_USAGE_STAGING;

			CHECK_RESULT(mDevice->CreateTexture3D(&dsDesc, NULL, &debug), "fail to create staging buffer, cant use gpu voxelizer");

			mContext->CopyResource(debug, mOutputTexture3D);
			D3D11_MAPPED_SUBRESOURCE mr;
			mContext->Map(debug, 0, D3D11_MAP_READ, 0, &mr);

			int stride = mEffect->getElementSize() * mSize.width;
			result.datas.reserve(stride * mSize.height * mSize.depth );
			char* begin = result.datas.data();
			for (int z = 0; z < mSize.depth; ++z)
			{
				const char* depth = ((const char*)mr.pData + mr.DepthPitch * z);
				for (int y = 0; y < mSize.height; ++y)
				{
					memcpy(begin, depth + mr.RowPitch * y, stride);
					begin += stride;
				}
			}

			mContext->Unmap(debug, 0);
		}

	}


	result.width = mSize.width;
	result.height = mSize.height;
	result.depth = mSize.depth;
	
}
