#ifndef _AHD_H_
#define _AHD_H_

#include <d3d11.h>
#include <D3DX11.h>
#include <vector>
#include <string>
#include <xnamath.h>

namespace AHD
{
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
		static const DXGI_FORMAT OUTPUT_FORMAT = DXGI_FORMAT_R32_UINT;
		static const UINT SLOT = 1;
		static const size_t ELEM_SIZE = 4;
	};

	struct MeshWrapper
	{
		const void* vertices = nullptr;
		size_t vertexCount = 0;
		size_t vertexStride = 0;

		const void* indexes = nullptr;
		size_t indexCount = 0;
		size_t indexStride = 0;

		size_t posOffset = 0;


	};

	class Voxelizer
	{
	public:

		struct Result
		{
			std::vector<char> datas;
			int width, height, depth;
		};
	public :
		Voxelizer();
		Voxelizer(ID3D11Device* device, ID3D11DeviceContext* context);
		~Voxelizer();

		void setMesh(const MeshWrapper& mesh, float scale);
		void setVoxelSize(float v);
		void setEffectAndUAVParameters(Effect* effect, DXGI_FORMAT Format, UINT slot, size_t elemSize);

		void voxelize( int start, int count);
		void exportVoxels(Result& output);

		template<class T, class ... Args>
		T* createEffect(Args& ... args)
		{
			T* e = new T(args...);
			mEffects.push_back(e);
			e->init(mDevice);

			return e;
		}

	private:
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


		bool prepare();
		void cleanResource();

	private:
		struct 
		{
			int width = 0;
			int height = 0;
			int depth = 0;
			int maxLength = 0;
		}mSize;

		MeshWrapper mMesh;
		float mScale = 1.0f;
		float mVoxelSize = 1.0f;
		XMMATRIX mTranslation;
		XMMATRIX mProjection;
		std::vector<Effect*> mEffects;
		Effect* mCurrentEffect = nullptr;
		DefaultEffect mDefaultEffect;
		bool mNeedPrepare = true;

		Interface<ID3D11Device> mDevice;
		Interface<ID3D11DeviceContext> mContext;


		Interface<ID3D11Texture3D> mOutputTexture3D;
		Interface<ID3D11UnorderedAccessView> mOutputUAV;
		DXGI_FORMAT mUAVFormat = DXGI_FORMAT_UNKNOWN;
		size_t mUAVElementSize = 0;
		UINT mUAVSlot = 1;

		Interface<ID3D11Texture2D> mRenderTarget;
		Interface<ID3D11RenderTargetView> mRenderTargetView;

		Interface<ID3D11Buffer> mVertexBuffer;
		Interface<ID3D11Buffer> mIndexBuffer;
	};
}

#endif