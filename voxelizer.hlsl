cbuffer ConstantBuffer : register(b0)
{
	matrix World;
	matrix View;
	matrix Projection;
}

RWTexture3D<float4> voxels:register(u1);//rendertarget is using u0

struct VS_INPUT
{
	float4 Pos : POSITION;

#ifdef HAS_TEXCOORD
	float2 texcoord: TEXCOORD0;
#endif
};

struct PS_INPUT
{
	float4 Pos : SV_POSITION;
	float4 rPos: COLOR0;

#ifdef HAS_TEXCOORD
	float2 texcoord: TEXCOORD0;
#endif
};


PS_INPUT vs(VS_INPUT input)
{
	PS_INPUT output;// = (PS_INPUT)0;
	output.Pos = mul(input.Pos, World);
	output.rPos = output.Pos;
	output.Pos = mul(output.Pos, View);
	output.Pos = mul(output.Pos, Projection);
	
#ifdef HAS_TEXCOORD
	output.texcoord = input.texcoord;
#endif
	
	return output;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 ps(PS_INPUT input) : SV_Target
{
#ifdef HAS_TEXCOORD
	voxels[int3(input.rPos.x, input.rPos.y, input.rPos.z)] = float4(input.texcoord.x, input.texcoord.y, 1, 1);
#else
	voxels[int3(input.rPos.x, input.rPos.y, input.rPos.z)] = float4(0, 0, 1, 1);
#endif
	return float4(1, 1, 1, 1);
}


