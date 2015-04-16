cbuffer ConstantBuffer : register(b0)
{
	matrix View;
	matrix Projection;
}

RWTexture3D<float4> outtest:register(u1);

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
	//output.Pos = mul(input.Pos, World);
	output.Pos = mul(input.Pos, View);
	output.Pos = mul(output.Pos, Projection);
	//output.Pos = input.Pos;
	output.rPos = input.Pos;
	return output;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 ps(PS_INPUT input) : SV_Target
{

	outtest[int3(input.rPos.x, input.rPos.y, input.rPos.z)] = float4(1,1,1,1);
	return float4(1, 1, 1, 1);
}


