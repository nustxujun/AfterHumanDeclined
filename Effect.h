#ifndef _Effect_H_
#define _Effect_H_

#include <d3d11.h>
#include "AHD.h"
#include "AHDD3D11Helper.h"
#include "TextureLoader.h"

class SponzaEffect : public AHD::Effect
{
public:
	struct Constant
	{
		XMMATRIX world;
		XMMATRIX view;
		XMMATRIX proj;
		float diffuse[4];
		float ambient[4];

		float length;
	}mConstant;

	enum
	{
		NORMAL,
		HAS_TEXTURE,
		NUM
	};


	void addTexture(const std::string& file)
	{
		if (!file.empty())
		{
			mTextureMap[file] = nullptr;
		}
	}

	void setTexture(int slot, const std::string& tex)
	{
		mCurTex = tex;
	}

	void init(ID3D11Device* dev)
	{

		{
			D3D11_INPUT_ELEMENT_DESC desc[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};

			ID3DBlob* blob;
			AHD::D3D11Helper::compileShader(&blob, "CustomVoxelizer.hlsl", "vs", "vs_5_0", NULL);
			dev->CreateInputLayout(desc, ARRAYSIZE(desc), blob->GetBufferPointer(), blob->GetBufferSize(), &mLayout);
			dev->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mVertexShader);
			blob->Release();
		}

		D3D_SHADER_MACRO macros[] = { { "HAS_TEXTURE", "1" }, { NULL, NULL } };
		D3D_SHADER_MACRO* macroref[] = { NULL, macros };
		for (int i = NORMAL; i < NUM; ++i)
		{
			ID3DBlob* blob;
			AHD::D3D11Helper::compileShader(&blob, "CustomVoxelizer.hlsl", "ps", "ps_5_0", macroref[i]);
			dev->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mPixelShader[i]);

			blob->Release();
		}


		AHD::D3D11Helper::createBuffer(&mConstantBuffer, dev, D3D11_BIND_CONSTANT_BUFFER, sizeof(mConstant));

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
		dev->CreateSamplerState(&sampDesc, &mSampler);

		for (auto& i : mTextureMap)
		{
			i.second = TextureLoader::createTexture(dev, i.first.c_str());
		}

	}
	void prepare(ID3D11DeviceContext* cont)
	{
		cont->VSSetShader(mVertexShader, NULL, 0);
		cont->IASetInputLayout(mLayout);
		cont->VSSetConstantBuffers(0, 1, &mConstantBuffer);
		cont->PSSetConstantBuffers(0, 1, &mConstantBuffer);

		ID3D11ShaderResourceView* texture = nullptr;
		auto ret = mTextureMap.find(mCurTex);
		if (ret != mTextureMap.end())
		{
			texture = ret->second;
			cont->PSSetShader(mPixelShader[HAS_TEXTURE], NULL, 0);
		}
		else
			cont->PSSetShader(mPixelShader[NORMAL], NULL, 0);

		cont->PSSetShaderResources(0, 1, &texture);

		cont->PSSetSamplers(0, 1, &mSampler);

	}

	void update(AHD::EffectParameter& paras)
	{
		memcpy(&mConstant.world, &paras.world, sizeof(XMMATRIX)* 3);
		mConstant.length = paras.length;
		paras.context->UpdateSubresource(mConstantBuffer, 0, NULL, &mConstant, 0, 0);
	}

	void clean()
	{
		auto endi = mTextureMap.end();
		for (auto i = mTextureMap.begin(); i != endi; ++i)
		{
			if (i->second)
				i->second->Release();
		}
		mConstantBuffer->Release();
		mVertexShader->Release();
		mLayout->Release();
		for (auto i : mPixelShader)
		{
			i->Release();
		}
		mSampler->Release();
	}
	int getElementSize()const{ return 4; }

	ID3D11VertexShader* mVertexShader;
	ID3D11PixelShader* mPixelShader[NUM];
	ID3D11InputLayout* mLayout;
	ID3D11Buffer* mConstantBuffer;
	ID3D11SamplerState*		mSampler;
	std::map<std::string, ID3D11ShaderResourceView*> mTextureMap;
	std::string mCurTex;

};

#endif