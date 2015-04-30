cbuffer ConstantBuffer : register(b0)
{
	matrix World;
	matrix View;
	matrix Projection;
}

RWTexture3D<unsigned int> voxels:register(u1);//rendertarget is using u0

struct VS_INPUT
{
	float4 Pos : POSITION;

};

struct PS_INPUT
{
	float4 Pos : SV_POSITION;
	float4 rPos: COLOR0;

};


PS_INPUT vs(VS_INPUT input)
{
	PS_INPUT output;// = (PS_INPUT)0;
	output.Pos = mul(input.Pos, World);
	output.rPos = output.Pos;
	output.Pos = mul(output.Pos, View);
	output.Pos = mul(output.Pos, Projection);
	
	return output;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 ps(PS_INPUT input) : SV_Target
{
	voxels[int3(input.rPos.x, input.rPos.y, input.rPos.z)] = 1;

	return float4(1, 1, 1, 1);
}


