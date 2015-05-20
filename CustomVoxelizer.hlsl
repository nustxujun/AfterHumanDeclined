cbuffer ConstantBuffer : register(b0)
{
	matrix World;
	matrix View;
	matrix Projection;
	float4 diffuse;
	float4 ambient;

	float width;
	float height;
	float depth;
	unsigned int viewport;
}

RWTexture3D<float4> voxels:register(u1);//rendertarget is using u0
SamplerState texsampler : register(s0);
Texture2D texDiffuse : register(t0);

struct VS_INPUT
{
	float4 Pos : POSITION;
	float2 Texcoord: TEXCOORD0;
};

struct PS_INPUT
{
	float4 Pos : SV_POSITION;
	float2 Texcoord: TEXCOORD0;
};


PS_INPUT vs(VS_INPUT input)
{
	PS_INPUT output;// = (PS_INPUT)0;
	output.Pos = mul(input.Pos, World);
	output.Pos = mul(output.Pos, View);
	output.Pos = mul(output.Pos, Projection);
	
	output.Texcoord = input.Texcoord;
	return output;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
void ps(PS_INPUT input) 
{
	int3 pos = 0;

	if(viewport == 0)
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

	voxels[pos] = saturate(
	//voxels[int3(input.rPos.x, input.rPos.y, input.rPos.z)] = saturate(
#ifdef HAS_TEXTURE
	texDiffuse.Sample(texsampler, input.Texcoord) *
#endif
		(diffuse + ambient));
	
}


