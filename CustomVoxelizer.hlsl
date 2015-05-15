cbuffer ConstantBuffer : register(b0)
{
	matrix World;
	matrix View;
	matrix Projection;
	float4 diffuse;
	float4 ambient;
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
	float4 rPos: COLOR1;
	float2 Texcoord: TEXCOORD0;
};


PS_INPUT vs(VS_INPUT input)
{
	PS_INPUT output;// = (PS_INPUT)0;
	output.Pos = mul(input.Pos, World);
	output.rPos = output.Pos;
	output.Pos = mul(output.Pos, View);
	output.Pos = mul(output.Pos, Projection);
	
	output.Texcoord = input.Texcoord;
	return output;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 ps(PS_INPUT input) : SV_Target
{
	voxels[int3(input.rPos.x, input.rPos.y, input.rPos.z)] = saturate(
#ifdef HAS_TEXTURE
	texDiffuse.Sample(texsampler, input.Texcoord) *
#endif
		(diffuse + ambient));
	return float4(1, 1, 1, 1);
}


