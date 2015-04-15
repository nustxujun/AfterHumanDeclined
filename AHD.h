#ifndef _AHD_H_
#define _AHD_H_

#include <d3d11.h>
#include <D3DX11.h>

namespace AHD
{
	class Voxelizer
	{
	public:
		struct Parameter
		{
			const void* vertices;
			size_t vertexCount;
			size_t vertexStride;
			size_t positionElemOffset;
			
			const void* indexes;
			size_t indexCount;
			size_t indexStride;
			
			float voxelSize;
			float meshScale;

		};

		struct Result
		{
			typedef unsigned int ARGB;
			ARGB* voxels = nullptr;
			int width, height, depth;

			ARGB getColor(int x, int y, int z)
			{
				return getColor(x + y * width + z * width * height);
			}

			ARGB getColor(int index)
			{
				return voxels[index];
			}

			~Result()
			{
				if (voxels != nullptr)
				delete voxels;
			}

			
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