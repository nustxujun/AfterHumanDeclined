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
		virtual void init(ID3D11Device* device, ID3D11DeviceContext* context) = 0;
		virtual void prepare(EffectParameter& paras) = 0;
		virtual void clean() = 0;

		virtual int getElementSize()const = 0;
	};

	class DefaultEffect :public Effect
	{
		void init(ID3D11Device* device, ID3D11DeviceContext* context) ;
		void prepare(EffectParameter& paras) ;
		void clean();
		int getElementSize()const{ return 4; }

		ID3D11VertexShader* mVertexShader;
		ID3D11PixelShader* mPixelShader;
		ID3D11InputLayout* mLayout;
		ID3D11Buffer* mConstant;

	public :
		static const DXGI_FORMAT OUTPUT_FORMAT = DXGI_FORMAT_R32_UINT;
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
		void setEffect(Effect* effect, DXGI_FORMAT outputFormat);

		void voxelize(Result& output, int start, int count);

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
		Effect* mEffect = nullptr;
		DefaultEffect mDefaultEffect;

		Interface<ID3D11Device> mDevice;
		Interface<ID3D11DeviceContext> mContext;


		Interface<ID3D11Texture3D> mOutputTexture3D;
		Interface<ID3D11UnorderedAccessView> mOutputUAV;
		DXGI_FORMAT mFormat;

		Interface<ID3D11Texture2D> mRenderTarget;
		Interface<ID3D11RenderTargetView> mRenderTargetView;

		Interface<ID3D11Buffer> mVertexBuffer;
		Interface<ID3D11Buffer> mIndexBuffer;
	};
}

#endif