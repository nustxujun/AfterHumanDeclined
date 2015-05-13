#include <Windows.h>
#include "AHD.h"
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dcompiler.h>
#include <xnamath.h>
#include <iostream>
#include "AHDd3d11Helper.h"

#include <vector>
#include <list>
#include <deque>
#include "tiny_obj_loader.h"
#include "AHDUtils.h"
#include "ring.h"
#include "TextureLoader.h"

#pragma comment (lib,"d3d11.lib")
#pragma comment (lib,"d3dx11.lib")

using namespace AHD;
using namespace tinyobj;


float scale = 0.08;
const char* modelname = "sponza.obj";



HWND window;
ID3D11Device*           device = NULL;
ID3D11DeviceContext*    context = NULL;
IDXGISwapChain*         swapChain = NULL;
ID3D11RenderTargetView* renderTargetView = NULL;
ID3D11VertexShader*     vertexShader = NULL;
ID3D11PixelShader*      pixelShader = NULL;
ID3D11InputLayout*      vertexLayout = NULL;
ID3D11Buffer*			constantBuffer = NULL;
ID3D11Buffer*			variableBuffer = NULL;
ID3D11Texture2D*        depthStencil = NULL;
ID3D11DepthStencilView* depthStencilView = NULL;
ID3D11RasterizerState* rasterizerState = NULL;

ID3D11Buffer*		optimizedVertices = NULL;
ID3D11Buffer*		optimizedIndexes = NULL;
size_t drawCount = 0;

Voxelizer::Result		voxels;
std::vector<shape_t> shapes;
std::vector<material_t> materials;

struct Material
{
	XMFLOAT4 kd ;
	XMFLOAT4 ks ;
	float ns = 0;
};

//std::vector<Material> materials;



struct ConstantBuffer
{
	XMMATRIX world;
	XMMATRIX view;
	XMMATRIX proj;
	XMFLOAT4 lightdir;
	XMFLOAT4 eye;
} constants;

struct VariableBuffer
{
	XMMATRIX local;
	XMFLOAT4 kd;
	XMFLOAT4 ks;
	float ns;
}variables;

struct Camera
{
	XMVECTOR pos;
	XMVECTOR dir;
	XMVECTOR up;

	XMMATRIX getMatrix()
	{
		XMMATRIX mat = XMMatrixLookToLH(pos, dir, up);
		return XMMatrixTranspose(mat);
	}
}camera;

struct Target
{
	XMFLOAT3 pos;
	XMFLOAT3 rot;
}target;

class SponzaEffect : public Effect
{
public:
	struct Constant
	{
		float world[16];
		float view[16];
		float proj[16];
		float diffuse[4];
		float ambient[4];
	}mConstant;

	enum
	{
		NORMAL,
		HAS_TEXTURE,
		NUM
	};


	void addTexture(const std::string& file)
	{
		if (!file.empty() )
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
			D3D11Helper::compileShader(&blob, "CustomVoxelizer.hlsl", "vs", "vs_5_0", NULL);
			dev->CreateInputLayout(desc, ARRAYSIZE(desc), blob->GetBufferPointer(), blob->GetBufferSize(), &mLayout);
			dev->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mVertexShader);
			blob->Release();
		}

		D3D_SHADER_MACRO macros[] = { { "HAS_TEXTURE", "1" }, {NULL,NULL} };
		D3D_SHADER_MACRO* macroref[] = { NULL, macros };
		for (int i = NORMAL; i < NUM; ++i)
		{
			ID3DBlob* blob;
			D3D11Helper::compileShader(&blob, "CustomVoxelizer.hlsl", "ps", "ps_5_0", macroref[i]);
			dev->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mPixelShader[i]);

			blob->Release();
		}


		D3D11Helper::createBuffer(&mConstantBuffer, dev, D3D11_BIND_CONSTANT_BUFFER, sizeof(mConstant));

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

	void update(EffectParameter& paras)
	{
		memcpy(mConstant.world, &paras.world, sizeof(XMMATRIX) * 3);

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
	ID3D11SamplerState*		mSampler ;
	std::map<std::string, ID3D11ShaderResourceView*> mTextureMap;
	std::string mCurTex;

};

class EffectProxy : public Effect
{
public:
	SponzaEffect* effect;

	struct
	{
		float diffuse[4];
		float ambient[4];
	}constant;

	std::string texture;

	void init(ID3D11Device* device)
	{}
	void prepare(ID3D11DeviceContext* context)
	{
		memcpy(effect->mConstant.diffuse, &constant, sizeof(float) * 8);
		effect->setTexture(0, texture);
		effect->prepare(context);
	}
	void update(EffectParameter& paras)
	{
		effect->update(paras);
	}
	void clean()
	{

	}
};


enum Button
{
	MouseLeft,
	MouseRight,
	MouseMid,

	B_COUNT
};

bool mButtonState[B_COUNT] = {false};

enum Key
{
	K_W = 87,
	K_A = 65,
	K_S = 83,
	K_D = 68,
	K_1 = 49,
	K_2 = 50,
};


void voxelize(float s = 1.0);

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
		case WM_KEYDOWN:
		{
			XMVECTOR left = XMVector3Cross(camera.up, camera.dir);
			switch (wParam)
			{
			case K_W: camera.pos += camera.dir; break;
			case K_A: camera.pos += -left; break;
			case K_S: camera.pos += -camera.dir; break;
			case K_D: camera.pos += left; break;
			case K_1: scale = (scale > 2) ? scale + 1 : scale + 0.1; voxelize(scale); break;
			case K_2: scale = (scale > 2) ? scale - 1 : scale - 0.1; voxelize(scale); break;
			}
		}
			break;
		case WM_KEYUP:
			break;
		case WM_LBUTTONDOWN: mButtonState[MouseLeft] = true; break;
		case WM_LBUTTONUP: mButtonState[MouseLeft] = false; break;
		case WM_RBUTTONDOWN: mButtonState[MouseRight] = true; break;
		case WM_RBUTTONUP: mButtonState[MouseRight] = false; break;
		case WM_MBUTTONDOWN: mButtonState[MouseMid] = true; break;
		case WM_MBUTTONUP: mButtonState[MouseMid] = false; break;
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

			camera.pos += camera.dir * wheelpara.roll / 10;
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

				target.rot.x += (mousepos.y - lastY) / 100.0f;
				target.rot.y += (mousepos.x - lastX) / 100.0f;
			}
			else if (mButtonState[MouseRight])
			{
				XMVECTOR left = XMVector3Cross(camera.up, camera.dir);
				XMMATRIX rot1 = XMMatrixRotationAxis(camera.up, (mousepos.x - lastX) / 100.0f);
				XMMATRIX rot2 = XMMatrixRotationAxis(left, (mousepos.y - lastY) / 100.0f);

				camera.dir = XMVector4Transform(camera.dir, rot2 * rot1);
			}
			else if (mButtonState[MouseMid])
			{
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

	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = width;
	descDepth.Height = height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = device->CreateTexture2D(&descDepth, NULL, &depthStencil);
	if (FAILED(hr))
		return hr;

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = device->CreateDepthStencilView(depthStencil, &descDSV, &depthStencilView);
	if (FAILED(hr))
		return hr;

	context->OMSetRenderTargets(1, &renderTargetView, depthStencilView);

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
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },

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

	target.rot = XMFLOAT3(0, 0, 0);
	target.pos = XMFLOAT3(0, 0, 0);
	camera.pos = XMVectorSet(0.0f, 0.0f, -10.0f, 0.0f);
	camera.dir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	camera.up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	constants.view = camera.getMatrix();
	constants.world = XMMatrixIdentity();
	constants.proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, width / (FLOAT)height, 0.01f, 1000.0f);
	constants.proj = XMMatrixTranspose(constants.proj);
	constants.lightdir = XMFLOAT4(0, 0, -1, 0);


	hr = createBuffer(&constantBuffer, D3D11_BIND_CONSTANT_BUFFER, sizeof(constants), &constants);
	if (FAILED(hr))
		return hr;


	hr = createBuffer(&variableBuffer, D3D11_BIND_CONSTANT_BUFFER, sizeof(variables), &variables);
	if (FAILED(hr))
		return hr;

	context->VSSetConstantBuffers(0, 1, &constantBuffer);
	context->VSSetConstantBuffers(1, 1, &variableBuffer);
	context->PSSetConstantBuffers(0, 1, &constantBuffer);
	context->PSSetConstantBuffers(1, 1, &variableBuffer);




	D3D11_RASTERIZER_DESC desc;
	desc.FillMode = D3D11_FILL_SOLID;
	desc.CullMode = D3D11_CULL_FRONT;
	desc.FrontCounterClockwise = false;
	desc.DepthBias = 0;
	desc.DepthBiasClamp = 0;
	desc.SlopeScaledDepthBias = 0;
	desc.DepthClipEnable = true;
	desc.ScissorEnable = false;
	desc.MultisampleEnable = false;
	desc.AntialiasedLineEnable = false;

	device->CreateRasterizerState(&desc, &rasterizerState);
	context->RSSetState(rasterizerState);
	return S_OK;
}


HRESULT initGeometry()
{
	std::cout << "Loading ...";
	long timer = GetTickCount();

	LoadObj(shapes, materials, modelname);

	std::cout << (GetTickCount() - timer) << " ms" << std::endl;

	voxelize(scale);

	camera.pos = XMVectorSet(0.0f, 0.0f, -voxels.width * 2, 0.0f);
	return S_OK;

}

void optimizeVoxels()
{
	enum FaceType
	{
		P_X = 1,
		P_Y = 1 << 1,
		P_Z = 1 << 2,
		N_X = 1 << 3,
		N_Y = 1 << 4,
		N_Z = 1 << 5,
	};

	struct Pos
	{
		int x, y, z;
	};

	enum Check
	{
		C_NONE = 0,
		C_INSERT,
	};

	struct Block
	{
		Pos pos;
		int face;
		int color;
		Check check;
	};



	const char* data = voxels.datas.data();
	int width = voxels.width;
	int height = voxels.height;
	int depth = voxels.depth;
	int count = width * height * depth;

	std::vector<Block> blocks(count);

	auto getVoxel = [&data, width, height, depth](int x, int y, int z)->int*
	{
		if (x < width && y < height && z < depth)
			return (int*)data + x + y * width + z * height * width;
		return nullptr;
	};

	auto checkBlock = [](Block& b1, Block& b2, FaceType face)
	{
		if (b1.color == b2.color )
			return;

		if (b1.color == 0)
		{
			switch (face)
			{
			case P_X: b2.face |= N_X; break;
			case P_Y: b2.face |= N_Y; break;
			case P_Z: b2.face |= N_Z; break;
			default:
				assert(0);
			}
		}
		else
			b1.face |= face;
	};

	auto getIndex = [width, height](int x, int y, int z)
	{
		return x + y * width + z * height * width;
	};

	struct Vertex
	{
		Vector3 p;
		Vector3 n;
		int color;
	};

	struct Face
	{
		Vertex verts[4];
		void setup(const Pos& offset, int color)
		{
			for (auto& i : verts)
			{
				i.p.x += offset.x;
				i.p.y += offset.y;
				i.p.z += offset.z;
				i.color = color;
			}
		}


	};
	const Face cube[] =
	{
		//P_X
		{ 
			Vector3(1, 0, 0), Vector3::UNIT_X,0,
			Vector3(1, 0, 1), Vector3::UNIT_X, 0,
			Vector3(1, 1, 1), Vector3::UNIT_X, 0,
			Vector3(1, 1, 0), Vector3::UNIT_X, 0
		},
		//P_Y
		{ 
			Vector3(0, 1, 0), Vector3::UNIT_Y, 0,
			Vector3(1, 1, 0), Vector3::UNIT_Y, 0,
			Vector3(1, 1, 1), Vector3::UNIT_Y, 0,
			Vector3(0, 1, 1), Vector3::UNIT_Y ,0
		},
		//P_Z
		{
			Vector3(0, 0, 1), Vector3::UNIT_Z, 0,
			Vector3(0, 1, 1), Vector3::UNIT_Z, 0,
			Vector3(1, 1, 1), Vector3::UNIT_Z, 0,
			Vector3(1, 0, 1), Vector3::UNIT_Z, 0
		},
		//N_X
		{ 
			Vector3(0, 0, 0), Vector3::NEGATIVE_UNIT_X, 0,
			Vector3(0, 1, 0), Vector3::NEGATIVE_UNIT_X, 0,
			Vector3(0, 1, 1), Vector3::NEGATIVE_UNIT_X, 0,
			Vector3(0, 0, 1), Vector3::NEGATIVE_UNIT_X, 0
		},
		//N_Y
		{ 
			Vector3(0, 0, 0), Vector3::NEGATIVE_UNIT_Y, 0,
			Vector3(0, 0, 1), Vector3::NEGATIVE_UNIT_Y, 0,
			Vector3(1, 0, 1), Vector3::NEGATIVE_UNIT_Y, 0,
			Vector3(1, 0, 0), Vector3::NEGATIVE_UNIT_Y, 0
		},
		//N_Z
		{
			Vector3(0, 0, 0), Vector3::NEGATIVE_UNIT_Z, 0,
			Vector3(1, 0, 0), Vector3::NEGATIVE_UNIT_Z, 0,
			Vector3(1, 1, 0), Vector3::NEGATIVE_UNIT_Z, 0,
			Vector3(0, 1, 0), Vector3::NEGATIVE_UNIT_Z, 0
		},

	};

	

	ring<Block*> testQueue;
	blocks[0] = { { 0, 0, 0 }, *getVoxel(0, 0, 0) ? N_X | N_Y | N_Z : 0, *getVoxel(0, 0, 0), C_INSERT };
	testQueue.push_back(&blocks[0]);

	std::vector<Face> faces;
	std::vector<size_t> indexes;

	while (!testQueue.empty())
	{
		Block& b = **testQueue.begin();
		const struct
		{
			Pos p;
			FaceType f;
		}pos[3] =
		{
			{ b.pos.x + 1, b.pos.y, b.pos.z, P_X },
			{ b.pos.x, b.pos.y + 1, b.pos.z, P_Y },
			{ b.pos.x, b.pos.y, b.pos.z + 1, P_Z }
		};

		for (int i = 0; i < 3; ++i)
		{
			const Pos& p = pos[i].p;
			int * color = getVoxel(p.x, p.y, p.z);
			if (color == nullptr)
				continue;
			Block& next = blocks[getIndex(p.x, p.y, p.z)];
			if (next.check != C_INSERT)
			{
				testQueue.push_back(&next);
			}
			next = { p, next.face | (p.x ? 0 : N_X) | (p.y ? 0 : N_Y) | (p.z ? 0 : N_Z), *color, C_INSERT };
			checkBlock(b, next, pos[i].f);

		}
		if (b.color != 0)
		{
			for (int i = 0; i < 6; ++i)
			{
				int mask = 1 << i;
				if ((mask & b.face) == 0)
					continue;

				size_t indexBegin = faces.size() * 4;
				const size_t indexSample[]=
				{
					0,1,2,
					0,2,3,
				};
				for (auto i : indexSample)
				{
					indexes.push_back(indexBegin + i);
				}

				Face face = cube[i];
				//face.color = b.color;
				face.setup(b.pos, b.color);
				faces.push_back(face);


			}
		}

		testQueue.pop_front();
	}

	if (optimizedVertices)
		optimizedVertices->Release();
	HRESULT hr = createBuffer(&optimizedVertices, D3D11_BIND_VERTEX_BUFFER, faces.size() * sizeof(Face), faces.data());
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, &optimizedVertices, &stride, &offset);

	if (optimizedIndexes)
		optimizedIndexes->Release();
	hr = createBuffer(&optimizedIndexes, D3D11_BIND_INDEX_BUFFER, sizeof(size_t)* indexes.size(), indexes.data());
	drawCount = indexes.size();
	context->IASetIndexBuffer(optimizedIndexes, DXGI_FORMAT_R32_UINT, 0);

}

void voxelize(float s)
{
	Voxelizer v;
	v.setScale(s);

	std::cout << "Voxelizing...";
	long timer = GetTickCount();

	SponzaEffect sponzaEffect;
	std::vector<EffectProxy> effects(shapes.size());
	

	std::vector<VoxelResource*> subs;
	AABB aabb;
	std::vector<char> buffer;
	for (int i = 0; i < shapes.size();++i)
	{
		std::string texDiff, texAmb;
		auto& matidvec = shapes[i].mesh.material_ids;
		if (!matidvec.empty())
		{
			auto& mat = materials[matidvec[0]];

			memcpy(effects[i].constant.diffuse, mat.diffuse, sizeof(float) * 3);
			memcpy(effects[i].constant.ambient, mat.ambient, sizeof(float) * 3);
			effects[i].constant.diffuse[3] = 1;
			effects[i].constant.ambient[3] = 1;
			texDiff = mat.diffuse_texname;
			sponzaEffect.addTexture(texDiff);
			effects[i].texture = texDiff;
			effects[i].effect = &sponzaEffect;
		}

		size_t size = shapes[i].mesh.positions.size() / 3;
		const size_t stride = 12 + 8;
		buffer.reserve(size * stride);
		char* data = buffer.data();
		char* begin = data;
		for (size_t j = 0; j < size; ++j)
		{
			memcpy(begin, &shapes[i].mesh.positions[j * 3], 12);
			begin += 12;
			memcpy(begin, &shapes[i].mesh.texcoords[j * 2], 8);
			begin += 8;
		}


		auto res = v.createResource();
		res->setVertex(data, size, stride);
		res->setIndex(shapes[i].mesh.indices.data(), shapes[i].mesh.indices.size(), 4);
		res->setEffect(&effects[i]);

		subs.push_back(res);


	}

	buffer.swap(std::vector<char>());

	v.setUAVParameters( DXGI_FORMAT_R8G8B8A8_UNORM, 1, 4);
	v.addEffect(&sponzaEffect);


	v.voxelize(voxels,subs.size(),subs.data());


	v.removeEffect(&sponzaEffect);

	std::cout << (GetTickCount() - timer) << " ms" << std::endl;

	std::cout << "Optimizing...";
	timer = GetTickCount();
	optimizeVoxels();
	std::cout << (GetTickCount() - timer) << " ms" << std::endl;

	target.pos = XMFLOAT3(-voxels.width / 2, -voxels.height / 2, -voxels.depth / 2);
}



void cleanDevice()
{
#define SAFE_RELEASE(x) if (x) (x)->Release();

	SAFE_RELEASE(optimizedVertices);
	SAFE_RELEASE(optimizedIndexes);
	SAFE_RELEASE(rasterizerState);
	SAFE_RELEASE(depthStencil);
	SAFE_RELEASE(depthStencilView);
	SAFE_RELEASE(variableBuffer);
	SAFE_RELEASE(constantBuffer);
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
	context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	constants.view = camera.getMatrix();
	XMStoreFloat4(&constants.eye, camera.pos);
	constants.world = XMMatrixRotationRollPitchYaw(target.rot.x, target.rot.y, 0) *XMMatrixTranspose(XMMatrixTranslation(target.pos.x, target.pos.y, target.pos.z));


	context->UpdateSubresource(constantBuffer, 0, NULL, &constants, 0, 0);


	context->VSSetShader(vertexShader, NULL, 0);
	context->PSSetShader(pixelShader, NULL, 0);


	variables.local = XMMatrixIdentity();
	//variables.kd = XMFLOAT4(content[0] / 255., content[1] / 255., content[2] / 255., content[3] / 255.);
	variables.kd = XMFLOAT4(0.5, 0.5, 0.5, 1);
	variables.ks = XMFLOAT4(0, 0, 0, 0);
	variables.ns = 0;
	context->UpdateSubresource(variableBuffer, 0, NULL, &variables, 0, 0);
	context->DrawIndexed(drawCount, 0, 0);
	swapChain->Present(0, 0);
}

int main()
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

