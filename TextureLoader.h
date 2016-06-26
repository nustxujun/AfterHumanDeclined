#ifndef _TextureLoader_H_
#define _TextureLoader_H_

#include <d3d11.h>
#include <assert.h>
#include <vector>


class TextureLoader
{
public :
	struct Data
	{
		std::vector<char*> data;
		int width = 0;
		int height = 0;

		Data(void* buff, int width, int height)
		{
			int size = width * height * 4;
			data.resize(size);
			memcpy(data.data(), buff, size);
			this->width = width;
			this->height = height;
		}

		Data()
		{
			width = height = 0;
		}

		bool isNull()
		{
			return data.empty();
		}

		Data(Data&) = delete;
		void operator=(Data& d)
		{
			data.swap(d.data);
			width = d.width;
			height = d.height;
		}
	};
public :
	static Data createTexture(const char* file);
};


#endif