#ifndef _ObjReader_H_
#define _ObjReader_H_

#include "AHD.h"
#include <string>
#include <fstream>
#include <map>
class ObjReader
{
	struct Material
	{
		float ka[3];
		float kd[3];
		float ks[3];
		float ns;
		int illum;

		std::string mapKd;
	};

	struct Sub
	{
		int indexStart;
		int indexCount;

		std::string material;
	};

public:
	ObjReader(const std::string& file);
	~ObjReader();

	const void* getVertexBuffer(){ return mVertexBuffer; }
	const void* getIndexBuffer(){ return mIndexBuffer; }
	int getVertexCount(){ return mVertexCount; }
	int getIndexCount(){ return mIndexCount; }
	int getVertexStride(){return mVertexStride;}
	int getIndexStride(){ return mIndexStride; }
	D3D11_INPUT_ELEMENT_DESC* getDesc(){ return mDesc; }
	int getDescCount(){ return mDescCount; }

	int getSubCount(){ return mSubs.size(); }
	const Sub& getSub(int index){ return mSubs[index]; }

	Material* getMaterial(const std::string name);
private:
	bool parse(std::ifstream& file);
	bool parseMTL(std::ifstream& file);

private:
	void* mVertexBuffer = nullptr;
	int mVertexCount = 0;
	int mVertexStride = 0;

	void* mIndexBuffer = nullptr;
	int mIndexCount = 0;
	int mIndexStride = 0;

	D3D11_INPUT_ELEMENT_DESC* mDesc = nullptr;
	int mDescCount = 0;

	std::vector<Sub> mSubs;

	std::map<std::string, Material> mMaterials;
};


#endif