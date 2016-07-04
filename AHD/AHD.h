#ifndef _AHD_H_
#define _AHD_H_

#include <d3d11.h>
#include <D3DX11.h>
#include <vector>
#include <string>
#include <xnamath.h>
#include "AHDUtils.h"
#include <set>
#include <map>

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
		XMMATRIX world;
		XMMATRIX views[3];
		XMMATRIX proj;

		float width;
		float height;
		float depth;
		int bcount;
	};

	enum Semantic
	{
		S_POSITION,
		S_COLOR,
		S_TEXCOORD,

		S_NUM
	};

	struct VertexDesc
	{
		Semantic semantic;
		size_t offset;
		size_t size;
	};

	class Effect
	{
	public:
		void init(ID3D11Device* device,const std::map<Semantic,VertexDesc>& desc);
		void prepare(ID3D11DeviceContext* context);
		void update(ID3D11DeviceContext* context, EffectParameter& paras);
		void clean();

		ID3D11GeometryShader* mGeometryShader;
		ID3D11VertexShader* mVertexShader;
		ID3D11PixelShader* mPixelShader;
		ID3D11InputLayout* mLayout;
		ID3D11Buffer* mConstant;

	public:
		static const DXGI_FORMAT OUTPUT_FORMAT = DXGI_FORMAT_R32_UINT;
		static const UINT SLOT = 1;
		static const size_t ELEM_SIZE = 4;
	};


	class VoxelResource
	{
		friend class Voxelizer;
	public :
		void setVertex(const void* vertices, size_t vertexCount, size_t vertexStride, const VertexDesc* desc, size_t size);
		void setIndex(const void* indexes, size_t indexCount, size_t indexStride);
		void setTexture(size_t width, size_t height, void* data);

		~VoxelResource();

	private:
		VoxelResource(ID3D11Device* device);
		void prepare(ID3D11DeviceContext* context);

	private:

		Interface<ID3D11Buffer> mVertexBuffer = nullptr;
		Interface<ID3D11Buffer> mIndexBuffer = nullptr;
		Interface<ID3D11Texture2D> mTexture = nullptr;
		Interface<ID3D11ShaderResourceView> mTextureSRV = nullptr;
		Interface<ID3D11SamplerState> mSampler = nullptr;
		std::vector<char> mVertexData;
		std::vector<char> mIndexData;
		std::vector<int> mTextureData;
		size_t mVertexStride;
		size_t mIndexStride;
		size_t mVertexCount;
		size_t mIndexCount;
		size_t mTexWidth, mTexHeight;

		ID3D11Device* mDevice;

		AABB mAABB;

		std::map<Semantic, VertexDesc> mDesc;
	};

	struct Voxel
	{
		int pos[3];
		int color[4];
	};

	class VoxelOutput
	{
	public:
		virtual void output(Voxel* voxels, size_t size) = 0;
	};

	class Voxelizer
	{

		struct UAVObj
		{
			Interface<ID3D11Buffer> buffer;
			Interface<ID3D11UnorderedAccessView> uav;
			size_t size;
		};
	public :
		Voxelizer();
		~Voxelizer();

		void setSize(float voxelSize, float scale);

		void voxelize(VoxelOutput* output, size_t resourceNum, VoxelResource** res);

		void addEffect(Effect* effect);
		void removeEffect(Effect* effect);

		VoxelResource* createResource();

	private:
		void voxelizeImpl(VoxelResource* res, const Vector3& range, bool countOnly);
		Vector3 prepare( size_t resourceNum, VoxelResource** res);
		Effect* getEffect(VoxelResource* res);
		void mapBuffer(void* data, size_t size, UAVObj& obj);
	private:


		VoxelResource* mCurrentResource;
		float mScale = 1.0f;
		Vector3 mSize;

		XMMATRIX mTranslation;
		XMMATRIX mProjection;
		std::map<int, Effect*> mEffects;

		Interface<ID3D11Device> mDevice;
		Interface<ID3D11DeviceContext>	 mContext;

		std::vector<VoxelResource*> mResources;
		std::vector<VoxelOutput*> mOutputs;

	};
}

#endif