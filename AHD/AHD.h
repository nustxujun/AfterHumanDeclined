#ifndef _AHD_H_
#define _AHD_H_

#include <d3d11.h>
#include <D3DX11.h>
#include <vector>
#include <string>
#include <xnamath.h>
#include "AHDUtils.h"
#include <set>

namespace AHD
{


	template<class T>
	class Interface
	{
	public:
		T* pointer;

		Interface() :pointer(nullptr){}
		Interface(T* p) :pointer(p){}
		~Interface(){ release(); }

		T* operator ->(){ return pointer; }
		void operator = (T* p)
		{
			if (p == pointer) return;
			release();
			pointer = p;
		}
		T** operator &(){ return &pointer; }
		operator T*(){ return pointer; }
		void release(){ if (pointer) pointer->Release(); pointer = nullptr; }
		bool isNull()const{ return pointer == nullptr; }
	};


	struct EffectParameter
	{
		ID3D11Device* device;
		ID3D11DeviceContext* context;

		XMMATRIX world;
		XMMATRIX view;
		XMMATRIX proj;

	};

	class Effect
	{
	public :
		virtual void init(ID3D11Device* device) = 0;
		virtual void prepare(ID3D11DeviceContext* context) = 0;
		virtual void update(EffectParameter& paras) = 0;
		virtual void clean() = 0;

	};

	class DefaultEffect :public Effect
	{
	public:
		void init(ID3D11Device* device) ;
		void prepare(ID3D11DeviceContext* context);
		void update(EffectParameter& paras);
		void clean();

		ID3D11VertexShader* mVertexShader;
		ID3D11PixelShader* mPixelShader;
		ID3D11InputLayout* mLayout;
		ID3D11Buffer* mConstant;

	public :
		static const DXGI_FORMAT OUTPUT_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
		static const UINT SLOT = 1;
		static const size_t ELEM_SIZE = 4;
	};

	class VoxelResource
	{
		friend class Voxelizer;

	public :
		void setVertex(const void* vertices, size_t vertexCount, size_t vertexStride, size_t posoffset = 0);
		void setVertex(ID3D11Buffer* vertexBuffer, size_t vertexCount, size_t vertexStride, size_t posoffset = 0);
		void setVertexFromVoxelResource(VoxelResource* res);

		
		void setIndex(const void* indexes, size_t indexCount, size_t indexStride);
		void setIndex(ID3D11Buffer* indexBuffer, size_t indexCount, size_t indexStride);
		void removeIndexes();

		~VoxelResource();
	private:
		VoxelResource(ID3D11Device* device);
		void prepare(ID3D11DeviceContext* context);

	private:

		Interface<ID3D11Buffer> mVertexBuffer = nullptr;
		Interface<ID3D11Buffer> mIndexBuffer = nullptr;
		size_t mVertexCount = 0;
		size_t mVertexStride = 0;
		size_t mPositionOffset = 0;
		size_t mIndexCount = 0;
		size_t mIndexStride = 0;

		ID3D11Device* mDevice;

		AABB mAABB;
		bool mNeedCalSize = true;
	};

	class Voxelizer
	{
	public:
		struct Result
		{
			std::vector<char> datas;
			int width, height, depth;
			int elementSize;
		};

	public :
		Voxelizer();
		Voxelizer(ID3D11Device* device, ID3D11DeviceContext* context);
		~Voxelizer();

		void setScale(float scale);
		void setVoxelSize(float v);
		void setEffectAndUAVParameters(Effect* effect, DXGI_FORMAT Format, UINT slot, size_t elemSize);

		void voxelize(VoxelResource* res, const AABB* aabb = nullptr, size_t drawBegin = 0, size_t drawCount = ~0);
		void exportVoxels(Result& output);

		void addEffect(Effect* effect);
		void removeEffect(Effect* effect);

		VoxelResource* createResource();

	private:

		bool prepare(VoxelResource* res, const AABB* range);
		void cleanResource();

	private:
		struct Size
		{
			size_t width;
			size_t  height;
			size_t  depth;
			size_t  maxLength;

			bool operator==(const Size& size)const
			{
				return width == size.width &&
					height == size.height &&
					depth == size.depth &&
					maxLength == size.maxLength;
			}

			Size(size_t  w, size_t  h, size_t  d, size_t  m)
				:width(w), height(h), depth(d), maxLength(m)
			{
			}

			Size(size_t  w, size_t  h, size_t  d)
				:width(w), height(h), depth(d)
			{
				maxLength = max(w, max(h, d));
			}

			Size()
				:width(0), height(0), depth(0), maxLength(0)
			{

			}
		};

		Size mSize;

		VoxelResource* mCurrentResource;
		float mScale = 1.0f;
		float mVoxelSize = 1.0f;
		XMMATRIX mTranslation;
		XMMATRIX mProjection;
		std::set<Effect*> mEffects;
		Effect* mCurrentEffect = nullptr;
		DefaultEffect mDefaultEffect;

		Interface<ID3D11Device> mDevice;
		Interface<ID3D11DeviceContext>	 mContext;

		Interface<ID3D11Texture3D> mOutputTexture3D = nullptr;
		Interface<ID3D11UnorderedAccessView> mOutputUAV = nullptr;
		DXGI_FORMAT mUAVFormat = DXGI_FORMAT_UNKNOWN;
		size_t mUAVElementSize = 0;
		UINT mUAVSlot = 1;

		Interface<ID3D11Texture2D> mRenderTarget = nullptr;
		Interface<ID3D11RenderTargetView> mRenderTargetView = nullptr;

		std::vector<VoxelResource*> mResources;
	};
}

#endif