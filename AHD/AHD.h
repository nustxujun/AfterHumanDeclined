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
		ID3D11Device* device;
		ID3D11DeviceContext* context;

		XMMATRIX world;
		XMMATRIX view;
		XMMATRIX proj;

		float width;
		float height;
		float depth;
		size_t viewport;// from 0 to 2 
	};

	enum Semantic
	{
		S_POSITION,
		S_COLOR,
		S_TEXCOORD
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
		void update(EffectParameter& paras);
		void clean();

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
		void removeIndexes();

		~VoxelResource();

	private:
		VoxelResource(ID3D11Device* device);
		void prepare(ID3D11DeviceContext* context);

	private:

		Interface<ID3D11Buffer> mVertexBuffer = nullptr;
		Interface<ID3D11Buffer> mIndexBuffer = nullptr;
		std::vector<char> mVertexData;
		std::vector<char> mIndexData;
		size_t mVertexStride;
		size_t mIndexStride;
		size_t mVertexCount;
		size_t mIndexCount;

		ID3D11Device* mDevice;

		AABB mAABB;

		std::map<Semantic, VertexDesc> mDesc;
	};

	struct VoxelData
	{
		std::vector<char> datas;
		int width = 0;
		int height = 0;
		int depth = 0;

	
	};

	class VoxelOutput
	{
	public:
		void addUAVBuffer(size_t slot, size_t elementSize, size_t elementCount = ~0);
		void addUAVTexture3D(size_t slot, DXGI_FORMAT format, size_t elementSize);
		void removeUAV(size_t slot);

		void exportData(VoxelData& data, size_t slot);

		VoxelOutput(ID3D11Device* device, ID3D11DeviceContext* context);

		void prepare( int width, int height, int depth);

	private:
		int mWidth;
		int mHeight;
		int mDepth;

		struct UAVParameter
		{
			size_t slot;
			DXGI_FORMAT format;
			size_t elementSize;
			bool isTexture;
			size_t elementCount;
		};
		struct UAV
		{
			UAVParameter para;
			Interface<ID3D11Texture3D> texture;
			Interface<ID3D11Buffer> buffer;
			Interface<ID3D11UnorderedAccessView> uav;
		};

		std::map<size_t, UAV> mUAVs;
		ID3D11Device* mDevice;
		ID3D11DeviceContext* mContext;
	};

	class Voxelizer
	{
	public :
		Voxelizer();
		Voxelizer(ID3D11Device* device, ID3D11DeviceContext* context);
		~Voxelizer();

		void setScale(float scale);
		void setVoxelSize(float v);
		

		void voxelize(VoxelOutput* output, size_t resourceNum, VoxelResource** res);

		void addEffect(Effect* effect);
		void removeEffect(Effect* effect);

		VoxelResource* createResource();
		VoxelOutput* createOutput();

	private:
		void voxelizeImpl(VoxelResource* res, const Vector3& range);
		Vector3 prepare(VoxelOutput* output, size_t resourceNum, VoxelResource** res);
		void cleanResource();
		Effect* getEffect(VoxelResource* res);
	private:


		VoxelResource* mCurrentResource;
		float mScale = 1.0f;
		float mVoxelSize = 1.0f;
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