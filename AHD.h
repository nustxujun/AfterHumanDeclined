#ifndef _AHD_H_
#define _AHD_H_

#include <d3d11.h>
#include <D3DX11.h>
#include <vector>

namespace AHD
{
	struct MeshWrapper
	{
		const void* vertices = nullptr;
		size_t vertexCount = 0;
		size_t vertexStride = 0;

		enum SEMANTIC
		{
			POSITION,
			TEXCOORD,

			NUM
		};

		size_t elementsOffset[NUM];

		const void* indexes = nullptr;
		size_t indexCount = 0;
		size_t indexStride = 0;


	};

	class Voxelizer
	{
	public:
		struct Parameter
		{
			MeshWrapper mesh;

			float voxelSize = 1.0f;
			float meshScale = 1.0f;
		};

		struct Voxel
		{
			struct
			{
				int x;
				int y;
				int z;
			} pos;

			struct
			{
				float u;
				float v;
			}texcoord;

		};

		struct Result
		{
			std::vector<Voxel> voxels;
			int width, height, depth;
		};
	public :
		void voxelize(Result& output, const Parameter& p);


		template<class T>
		class Interface
		{
		public:
			T* pointer;

			Interface() :pointer(0){}
			Interface(T* p) :pointer(p){}
			~Interface(){ if (pointer) pointer->Release(); }

			T* operator ->(){ return pointer; }
			T* operator = (T* p){ pointer = p; }
			T** operator &(){ return &pointer; }
			operator T*(){ return pointer; }

		};

		HRESULT createDevice(ID3D11Device** device, ID3D11DeviceContext** context);
		HRESULT createUAV(ID3D11Texture3D** buffer, ID3D11UnorderedAccessView** uav, ID3D11Device* device, int width, int height, int depth);
		HRESULT createRenderTarget(ID3D11Texture2D** target, ID3D11RenderTargetView** targetView, ID3D11Device* device, int width, int height);
		HRESULT createDepthStencil(ID3D11Texture2D** ds, ID3D11DepthStencilView** dsv, ID3D11Device* device, int width, int height);

		HRESULT compileShader(ID3DBlob** out, const char* filename, const char* function, const char* profile);
		HRESULT createBuffer(ID3D11Buffer** buffer, ID3D11Device* device, D3D11_BIND_FLAG flag, size_t size, const void* initdata = 0);

	};
}

#endif