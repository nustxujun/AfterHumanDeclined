//--------------------------------------------------------------------------------------
// File: Tutorial02.fx
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


struct VS_OUTPUT
{
	float4 pos :SV_POSITION;
	float4 color: COLOR0;
};

cbuffer cbNeverChanges : register(b0)
{
	matrix World;
	matrix View;
	matrix Projection;
	float4 lightdir;
};

cbuffer cbVariable: register(b1)
{
	matrix local;
	float4 color;
}

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VS(float4 Pos : POSITION) 
{
	VS_OUTPUT o;   
	o.pos = mul(Pos, local);
	o.pos = mul(o.pos, World);
	o.pos = mul(o.pos, View);
	o.pos = mul(o.pos, Projection);

	o.color = color;
    return o;
}




//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(VS_OUTPUT input) : SV_Target
{
	return input.color;   
}

