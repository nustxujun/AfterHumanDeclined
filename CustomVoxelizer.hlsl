cbuffer ConstantBuffer : register(b0)
{
	matrix World;
	matrix View;
	matrix Projection;
}

RWTexture3D<float4> voxels:register(u1);//rendertarget is using u0
SamplerState samLinear : register(s0);
Texture2D texDiffuse : register(t0);

struct VS_INPUT
{
	float4 Pos : POSITION;
	float4 Color: COLOR0;
	float2 Texcoord: TEXCOORD0;
};

struct PS_INPUT
{
	float4 Pos : SV_POSITION;
	float4 rPos: COLOR1;
	float4 Color: COLOR0;
	float2 Texcoord: TEXCOORD0;
};


PS_INPUT vs(VS_INPUT input)
{
	PS_INPUT output;// = (PS_INPUT)0;
	output.Pos = mul(input.Pos, World);
	output.rPos = output.Pos;
	output.Pos = mul(output.Pos, View);
	output.Pos = mul(output.Pos, Projection);
	
	output.Color = input.Color;
	output.Texcoord = input.Texcoord;
	return output;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 ps(PS_INPUT input) : SV_Target
{
	input.Color.a = 1;
	voxels[int3(input.rPos.x, input.rPos.y, input.rPos.z)] = 
#ifdef HAS_TEXTURE
		texDiffuse.Sample(samLinear, input.Texcoord) *
#endif
		input.Color;

	return float4(1, 1, 1, 1);
}


