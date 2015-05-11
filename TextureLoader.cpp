#include "TextureLoader.h"
#include "FreeImage.h"


#pragma comment (lib,"d3d11.lib")
#pragma comment (lib,"3Party\\FreeImage.lib")



ID3D11ShaderResourceView* TextureLoader::createTexture(ID3D11Device* device, const char* file)
{
	FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
	FIBITMAP *dib(0);
	BYTE* bits(0);
	unsigned int width(0), height(0);

	fif = FreeImage_GetFileType(file, 0);
	//if still unknown, try to guess the file format from the file extension
	if (fif == FIF_UNKNOWN)
		fif = FreeImage_GetFIFFromFilename(file);
	//if still unkown, return failure
	if (fif == FIF_UNKNOWN)
		assert(0);

	//check that the plugin has reading capabilities and load the file
	if (FreeImage_FIFSupportsReading(fif))
		dib = FreeImage_Load(fif, file);
	//if the image failed to load, return failure
	if (dib == NULL)
		assert(0);

	//retrieve the image data
	bits = FreeImage_GetBits(dib);
	//get the image width and height
	width = FreeImage_GetWidth(dib);
	height = FreeImage_GetHeight(dib);
	//if this somehow one of these failed (they shouldn't), return failure
	if ((bits == 0) || (width == 0) || (height == 0))
		assert(0);

	FREE_IMAGE_TYPE imageType = FreeImage_GetImageType(dib);

	switch (imageType)
	{
		case FIT_BITMAP:
		{
			FIBITMAP *newdib = FreeImage_ConvertTo32Bits(dib);
			FreeImage_Unload(dib);
			dib = newdib;
			bits = FreeImage_GetBits(dib);

		}
			break;
		default:
			//unsupported
			assert(0);
	}

	ID3D11Texture2D* texture;
	{
		D3D11_TEXTURE2D_DESC desc;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.Width = width;
		desc.Height = height;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.CPUAccessFlags = 0;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.MiscFlags = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;

		D3D11_SUBRESOURCE_DATA initdata;
		initdata.pSysMem = bits;
		initdata.SysMemPitch = 4 * width;
		initdata.SysMemSlicePitch = 4 * width * height;

		if (FAILED(device->CreateTexture2D(&desc, &initdata, &texture)))
			return nullptr;
	}
	ID3D11ShaderResourceView* resource;
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC desc;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MostDetailedMip = 0;
		desc.Texture2D.MipLevels = -1;
		if (FAILED(device->CreateShaderResourceView(texture, &desc, &resource)))
		{
			texture->Release();
			return nullptr;
		}
		texture->Release();
	}

	//Free FreeImage's copy of the data
	FreeImage_Unload(dib);
	return resource;
}