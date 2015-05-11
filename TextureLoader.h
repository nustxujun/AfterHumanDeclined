#ifndef _TextureLoader_H_
#define _TextureLoader_H_

#include <d3d11.h>
#include <assert.h>



class TextureLoader
{
public :
	static ID3D11ShaderResourceView* createTexture(ID3D11Device* device, const char* file);
};


#endif