cbuffer ConstantBuffer : register(b0)
{
	matrix World;
	matrix View;
	matrix Projection;

	float width;
	float height;
	float depth;
	unsigned int viewport;
}

RWTexture3D<unsigned int> voxels:register(u1);

struct VS_INPUT
{
	float4 Pos : POSITION;

};

struct PS_INPUT
{
	float4 Pos : SV_POSITION;
};


PS_INPUT vs(VS_INPUT input)
{
	PS_INPUT output;// = (PS_INPUT)0;
	output.Pos = mul(input.Pos, World);
	output.Pos = mul(output.Pos, View);
	output.Pos = mul(output.Pos, Projection);
	
	return output;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
void ps(PS_INPUT input) 
{
	int3 pos = 0;

	if (viewport == 0)
	{
		pos.x = input.Pos.z * width;
		pos.y = height - input.Pos.y;
		pos.z = input.Pos.x;
	}
	else if (viewport == 1)
	{
		pos.x = input.Pos.x;
		pos.y = input.Pos.z * height;
		pos.z = depth - input.Pos.y;
	}
	else
	{
		pos.x = input.Pos.x;
		pos.y = height - input.Pos.y;
		pos.z = input.Pos.z * depth;
	}
	voxels[pos] = 1;
}


