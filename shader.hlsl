//--------------------------------------------------------------------------------------
// File: Tutorial02.fx
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


struct VS_OUTPUT
{
	float4 pos :SV_POSITION;
};

cbuffer cbNeverChanges : register(b0)
{
	matrix World;
	matrix View;
	matrix Projection;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VS(float4 Pos : POSITION) 
{
	VS_OUTPUT o;   
	o.pos = mul(Pos, World);
	o.pos = mul(o.pos, View);
	o.pos = mul(o.pos, Projection);
    return o;
}




//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(VS_OUTPUT input) : SV_Target
{
	

	return float4(1,1, 0.0f, 1.0f);    // Yellow, with Alpha = 1
}

