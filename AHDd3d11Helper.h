#ifndef _AHDd3d11Helper_H_
#define _AHDd3d11Helper_H_

#include <d3d11.h>
#include <D3DX11.h>

namespace AHD
{
	class D3D11Helper
	{
	public :
		static HRESULT createDevice(ID3D11Device** device, ID3D11DeviceContext** context);
		static HRESULT createUAVTexture3D(ID3D11Texture3D** buffer, ID3D11UnorderedAccessView** uav, ID3D11Device* device, DXGI_FORMAT format, int width, int height, int depth);
		static HRESULT createRenderTarget(ID3D11Texture2D** target, ID3D11RenderTargetView** targetView, ID3D11Device* device, int width, int height);
		static HRESULT createDepthStencil(ID3D11Texture2D** ds, ID3D11DepthStencilView** dsv, ID3D11Device* device, int width, int height);

		static HRESULT compileShader(ID3DBlob** out, const char* filename, const char* function, const char* profile, const D3D10_SHADER_MACRO* macros);
		static HRESULT createBuffer(ID3D11Buffer** buffer, ID3D11Device* device, D3D11_BIND_FLAG flag, size_t size, const void* initdata = 0);

	};
}

#endif