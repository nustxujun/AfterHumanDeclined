#include "ObjReader.h"
#include <hash_map>
#include <sstream>

using namespace std;

ObjReader::ObjReader(const std::string& file)
{
	ifstream f(file);
	if (!!f )
		parse(f);
}

ObjReader::~ObjReader()
{
#define SAFE_DELETE(x) if (x) delete (x);

	SAFE_DELETE(mVertexBuffer);
	SAFE_DELETE(mIndexBuffer);
	SAFE_DELETE(mDesc);

}

bool ObjReader::parse(ifstream& f)
{
	struct float3
	{
		float x, y, z;
	};

	struct float2
	{
		float x, y;
	};

	struct Vertex
	{
		size_t t;
		size_t n;
	};



	std::vector<float3> poses;
	std::vector<float2> texcds;
	std::vector<float3> norms;
	std::hash_map<size_t, Vertex> vertices;
	std::vector<unsigned short> indexes;

	bool usingTexcoord = false;
	bool usingNormal = false;

	while (!!f)
	{
		char key[256];
		f >> key;
		if (key[0] == 'v' && key[1] == 0)
		{
			float3 p;
			f >> p.x >> p.y >> p.z;
			poses.push_back(p);
		}
		else if (strncmp(key, "vt", 256) == 0)
		{
			usingTexcoord = true;
			float2 t;
			f >> t.x >> t.y;
			texcds.push_back(t);
		}
		else if (strncmp(key, "vn", 256) == 0)
		{
			usingNormal = true;
			float3 n;
			f >> n.x >> n.y >> n.z;
			norms.push_back(n);
		}
		else if (key[0] == 'f' && key[1] == 0)
		{
			string line;
			getline(f, line);

			stringstream ss;
			ss << line;

			int index[4] = { -1,-1,-1,-1 };
			for (int vi = 0; vi < 4; ++vi)
			{

				ss >> index[vi];
				if (!ss)
					break;
				if (index[vi] > 0)
					index[vi] -= 1;
				else
				{
					index[vi] = poses.size() + index[vi];
				}

				Vertex v = { 0 };
				if ('/' == ss.peek())
				{
					ss.ignore();

					if ('/' != ss.peek())
					{
						ss >> v.t;
						v.t -= 1;
					}

					if ('/' == ss.peek())
					{
						ss.ignore();
						ss >> v.n;

						v.n -= 1;
					}
				}

				vertices.insert(make_pair(index[vi], v));
			}

			for (int i = 0; i < 3; ++i)
				indexes.push_back(index[i]);

			if (index[3] != -1)
			{
				indexes.push_back(index[0]);
				indexes.push_back(index[2]);
				indexes.push_back(index[3]);
			}

			continue;
		}
		else if (key[0] == 'g' && key[1] == 0)
		{

		}
		else if (strncmp(key, "usemtl", 256) == 0)
		{
			if (!mSubs.empty())
			{
				Sub& s = mSubs.back();
				s.indexCount = indexes.size() - s.indexStart;
			}
			Sub s;
			f >> s.material;
			s.indexStart = indexes.size();
			mSubs.push_back(s);
		}
		else if (strncmp(key, "mtllib", 256) == 0)
		{
			string mtl;
			f >> mtl;

			ifstream mtlf(mtl);
			if (!!mtlf)
				parseMTL(mtlf);
		}

		f.ignore(1024, '\n');
	}

	Sub& s = mSubs.back();
	s.indexCount = indexes.size() - s.indexStart;

	mVertexStride = sizeof(float3)+
		(usingTexcoord ? sizeof(float2) : 0) + 
		(usingNormal ? sizeof(float3) : 0);
	mVertexBuffer = new char[mVertexStride * poses.size()];

	mVertexCount = poses.size();
	char* begin = (char*)mVertexBuffer;
	for (size_t i = 0; i < mVertexCount; ++i)
	{
		memcpy(begin, &poses[i], sizeof(float3));
		begin += sizeof(float3);
		Vertex& v = vertices[i];

		if (usingTexcoord)
		{
			memcpy(begin, &texcds[v.t], sizeof(float2));
			begin += sizeof(float2);
		}
		if (usingNormal)
		{
			memcpy(begin, &norms[v.n], sizeof(float3));
			begin += sizeof(float3);
		}
	}


	mIndexCount = indexes.size();
	if (!indexes.empty())
	{
		mIndexBuffer = new unsigned short[indexes.size()];
		memcpy(mIndexBuffer, &indexes[0], sizeof(unsigned short)* indexes.size());

		mIndexStride = sizeof(unsigned short);
	}

	size_t desccount = 1 + usingTexcoord + usingNormal;
	mDesc = new D3D11_INPUT_ELEMENT_DESC[desccount];
	mDescCount = desccount;

	mDesc[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };
	int index = 1;
	int offset = 12;
	if (usingTexcoord)
	{
		mDesc[index++] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offset, D3D11_INPUT_PER_VERTEX_DATA, 0 };
		offset += 8;
	}
	if (usingNormal)
	{
		mDesc[index++] = { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offset, D3D11_INPUT_PER_VERTEX_DATA, 0 };
	}

	return true;
}

bool ObjReader::parseMTL(std::ifstream& file)
{
	Material* mat = 0;
	while (!!file)
	{
		string key;
		file >> key;
		if (key == "newmtl")
		{
			string name;
			file >> name;
			mat = &mMaterials.insert(make_pair(name, Material())).first->second;
		}
		else if (key == "map_Kd")
		{
			file >> mat->mapKd;
		}
		else if (key == "Kd")
		{
			file >> mat->kd[0] >> mat->kd[1] >> mat->kd[2];
		}
		else if (key == "Ks")
		{
			file >> mat->ks[0] >> mat->ks[1] >> mat->ks[2];
		}
		else if (key == "Ns")
		{
			file >> mat->ns;
		}

		file.ignore(1024, '\n');
	}
	return true;
}

ObjReader::Material* ObjReader::getMaterial(const std::string name)
{
	auto ret = mMaterials.find(name);
	if (ret == mMaterials.end())
		return nullptr;
	return &ret->second;
}

