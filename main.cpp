#include "stdafx.h"
#include "AHD.h"
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dcompiler.h>
#include <xnamath.h>

#pragma comment (lib,"d3d11.lib")
#pragma comment (lib,"d3dx11.lib")

using namespace AHD;

HWND window;
ID3D11Device*           device = NULL;
ID3D11DeviceContext*    context = NULL;
IDXGISwapChain*         swapChain = NULL;
ID3D11RenderTargetView* renderTargetView = NULL;
ID3D11VertexShader*     vertexShader = NULL;
ID3D11PixelShader*      pixelShader = NULL;
ID3D11InputLayout*      vertexLayout = NULL;
ID3D11Buffer*           vertexBuffer = NULL;
ID3D11Buffer*           indexBuffer = NULL;
ID3D11Buffer*			constantBuffer = NULL;
ID3D11Buffer*			variableBuffer = NULL;
Voxelizer::Result		voxels;

struct ConstantBuffer
{
	XMMATRIX world;
	XMMATRIX view;
	XMMATRIX proj;
	XMFLOAT4 lightdir;
} constants;

struct VariableBuffer
{
	XMMATRIX local;
	XMFLOAT4 color;
}variables;

struct SimpleVertex
{
	XMFLOAT3 Pos;
	XMFLOAT3 Normal;
};

enum Button
{
	MouseLeft,
	MouseRight,


	B_COUNT
};

bool mButtonState[B_COUNT] = {false};


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
		case WM_PAINT:
			hdc = BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		case WM_LBUTTONDOWN: mButtonState[MouseLeft] = true; break;
		case WM_LBUTTONUP: mButtonState[MouseLeft] = false; break;
		case WM_MOUSEWHEEL:
		{
			union
			{
				int para;
				struct
				{
					short b;
					short roll;
				};
			}wheelpara = { wParam };

			float scale = 1.0f;
			if (wheelpara.roll > 0)
				scale *= wheelpara.roll / 100.0f ;
			else
				scale /= -wheelpara.roll / 100.0f;

			XMMATRIX s = XMMatrixScaling(scale, scale, scale);
			constants.world = s * constants.world;

		}
			break;
		case WM_MOUSEMOVE:
		{
			union
			{
				int para;
				struct
				{
					short x;
					short y;
				};
			}mousepos = { lParam };

			static short lastX = mousepos.x;
			static short lastY = mousepos.y;

			if (mButtonState[MouseLeft])
			{
				XMMATRIX rot = XMMatrixRotationRollPitchYaw((mousepos.y - lastY) / 100.0f, (mousepos.x - lastX) / 100.0f, 0);
				constants.world = rot * constants.world;

			}

			lastX = mousepos.x;
			lastY = mousepos.y;
		}
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

HRESULT initWindow(HINSTANCE hInstance, int nCmdShow)
{
	// Register class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"voxelizerDemo";
	wcex.hIconSm = NULL;
	if (!RegisterClassEx(&wcex))
		return E_FAIL;

	// Create window
	RECT rc = { 0, 0, 640, 480 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	window = CreateWindow(L"voxelizerDemo", L"voxelizerDemo",
						  WS_OVERLAPPEDWINDOW,
						  CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance,
						  NULL);
	if (!window)
		return E_FAIL;

	ShowWindow(window, nCmdShow);

	return S_OK;
}

HRESULT compileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pErrorBlob;
	hr = D3DX11CompileFromFile(szFileName, NULL, NULL, szEntryPoint, szShaderModel,
							   dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL);
	if (FAILED(hr))
	{
		if (pErrorBlob != NULL)
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
		if (pErrorBlob) pErrorBlob->Release();
		return hr;
	}
	if (pErrorBlob) pErrorBlob->Release();

	return S_OK;
}

HRESULT createBuffer(ID3D11Buffer** buffer, D3D11_BIND_FLAG flag, size_t size, const void* initdata)
{
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = size;
	bd.BindFlags = flag;
	bd.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = initdata;
	return device->CreateBuffer(&bd, initdata ? &InitData : 0, buffer);
}

HRESULT initDevice()
{
	HRESULT hr = S_OK;

	RECT rc;
	GetClientRect(window, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = window;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		D3D_FEATURE_LEVEL level;
		hr = D3D11CreateDeviceAndSwapChain(NULL, driverTypes[driverTypeIndex], NULL, createDeviceFlags, featureLevels, numFeatureLevels,
										   D3D11_SDK_VERSION, &sd, &swapChain, &device, &level, &context);
		if (SUCCEEDED(hr))
			break;
	}
	if (FAILED(hr))
		return hr;

	// Create a render target view
	ID3D11Texture2D* pBackBuffer = NULL;
	hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	if (FAILED(hr))
		return hr;

	hr = device->CreateRenderTargetView(pBackBuffer, NULL, &renderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
		return hr;

	context->OMSetRenderTargets(1, &renderTargetView, NULL);

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	context->RSSetViewports(1, &vp);

	// Compile the vertex shader
	ID3DBlob* pVSBlob = NULL;
	hr = compileShaderFromFile(L"shader.hlsl", "VS", "vs_5_0", &pVSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL,
				   L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the vertex shader
	hr = device->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &vertexShader);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}

	// Define the input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = ARRAYSIZE(layout);

	// Create the input layout
	hr = device->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
										 pVSBlob->GetBufferSize(), &vertexLayout);
	pVSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Set the input layout
	context->IASetInputLayout(vertexLayout);

	// Compile the pixel shader
	ID3DBlob* pPSBlob = NULL;
	hr = compileShaderFromFile(L"shader.hlsl", "PS", "ps_5_0", &pPSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL,
				   L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the pixel shader
	hr = device->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &pixelShader);
	pPSBlob->Release();
	if (FAILED(hr))
		return hr;



	// Set primitive topology
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	XMVECTOR Eye = XMVectorSet(0.0, 0, -10.0, 0.0f);
	XMVECTOR At = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	constants.world = XMMatrixIdentity();
	constants.view = XMMatrixLookAtLH(Eye, At, Up);
	constants.view = XMMatrixTranspose(constants.view);
	constants.proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, width / (FLOAT)height, 0.01f, 1000.0f);
	constants.proj = XMMatrixTranspose(constants.proj);



	hr = createBuffer(&constantBuffer, D3D11_BIND_CONSTANT_BUFFER, sizeof(constants), &constants);
	if (FAILED(hr))
		return hr;


	hr = createBuffer(&variableBuffer, D3D11_BIND_CONSTANT_BUFFER, sizeof(variables), &variables);
	if (FAILED(hr))
		return hr;

	context->VSSetConstantBuffers(0, 1, &constantBuffer);
	context->VSSetConstantBuffers(1, 1, &variableBuffer);

	// Create vertex buffer
	SimpleVertex vertices[] =
	{
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },

		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },

		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },

		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },

		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },

		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
	};



	hr = createBuffer(&vertexBuffer, D3D11_BIND_VERTEX_BUFFER, sizeof(SimpleVertex) * 24, vertices);
	if (FAILED(hr))
		return hr;

	// Set vertex buffer
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);


	WORD indices[] =
	{
		3, 1, 0,
		2, 1, 3,

		6, 4, 5,
		7, 4, 6,

		11, 9, 8,
		10, 9, 11,

		14, 12, 13,
		15, 12, 14,

		19, 17, 16,
		18, 17, 19,

		22, 20, 21,
		23, 20, 22
	};

	hr = createBuffer(&indexBuffer, D3D11_BIND_INDEX_BUFFER, sizeof(WORD) * 36, indices);
	if (FAILED(hr))
		return hr;

	context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R16_UINT, 0);

	return S_OK;
}

#include <vector>

HRESULT initGeometry()
{
	Voxelizer v;
	Voxelizer::Parameter para;
	

	FILE* f;
	fopen_s(&f, "teapot.mesh", "rb");
	//FILE* f = fopen("../Reasource/Model/tiger.mesh", "rb");
	struct MeshInfo
	{
		int vertexSize;
		int vertexCount;
		int indexCount;
	};
	MeshInfo info;
	fread(&info, sizeof(MeshInfo), 1, f);

	std::vector<char*> verts;
	verts.resize(info.vertexCount * info.vertexSize);
	std::vector<short> indexs;
	indexs.resize(info.indexCount);

	//Ð´ÈëÊý¾Ý
	fread(&verts[0], info.vertexSize * info.vertexCount, 1, f);
	fread(&indexs[0], 2 * info.indexCount, 1, f);
	fclose(f);

	para.vertexCount = info.vertexCount;
	para.vertexStride = info.vertexSize;
	para.vertices = &verts[0];
	para.indexCount = info.indexCount;
	para.indexStride = 2;
	para.indexes = &indexs[0];
	para.voxelSize = 1.0f;
	para.meshScale = 25.0f;

	v.voxelize(voxels, para);


	return S_OK;


}

void cleanDevice()
{
#define SAFE_RELEASE(x) if (x) (x)->Release();
	SAFE_RELEASE(variableBuffer);
	SAFE_RELEASE(constantBuffer);
	SAFE_RELEASE(vertexBuffer);
	SAFE_RELEASE(indexBuffer);
	SAFE_RELEASE(vertexLayout);
	SAFE_RELEASE(pixelShader);
	SAFE_RELEASE(vertexShader);
	SAFE_RELEASE(renderTargetView);
	SAFE_RELEASE(swapChain);
	SAFE_RELEASE(context);
	SAFE_RELEASE(device);

}

void render()
{
	// Clear the back buffer 
	float ClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f }; // red,green,blue,alpha
	context->ClearRenderTargetView(renderTargetView, ClearColor);

	context->UpdateSubresource(constantBuffer, 0, NULL, &constants, 0, 0);


	// Render a triangle
	context->VSSetShader(vertexShader, NULL, 0);
	context->PSSetShader(pixelShader, NULL, 0);
	
	int count = voxels.width * voxels.height * voxels.depth;
	for (int z = 0; z < voxels.depth; ++z)
	{
		for (int y = 0; y < voxels.height; ++y)
		{
			for (int x = 0; x < voxels.width; ++x)
			{
				Voxelizer::Result::ARGB c = voxels.getColor(x, y, z);
				if (c == 0)
					continue;
				//variables.color = c;
				variables.local = XMMatrixTranspose(XMMatrixTranslation(x * 2, y * 2, z * 2));
				context->UpdateSubresource(variableBuffer, 0, NULL, &variables, 0, 0);
				context->DrawIndexed(36, 0, 0);

			}
		}
	}




	swapChain->Present(0, 0);
}

int _tmain(int argc, _TCHAR* argv[])
{
	if (FAILED(initWindow(0, SW_SHOW)))
		return 0;
	if (FAILED(initDevice()) ||
		FAILED(initGeometry()))
	{
		cleanDevice();
		return 0;
	}
	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			render();
		}
	}

	cleanDevice();
	
	return 0;
}

