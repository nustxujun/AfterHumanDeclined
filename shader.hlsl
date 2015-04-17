//--------------------------------------------------------------------------------------
// File: Tutorial02.fx
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


struct VS_OUTPUT
{
	float4 pos :SV_POSITION;
	float4 color: COLOR0;
	float3 normal :TEXCOORD0;
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
VS_OUTPUT VS(float4 Pos : POSITION, float3 Norm: NORMAL) 
{
	VS_OUTPUT o;   
	o.pos = mul(Pos, local);
	o.pos = mul(o.pos, World);
	o.pos = mul(o.pos, View);
	o.pos = mul(o.pos, Projection);

	o.color = color;

	o.normal = mul(Norm, local); 
	o.normal = mul(o.normal, World);
	o.normal = normalize(o.normal);
    return o;
}




//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(VS_OUTPUT input) : SV_Target
{
	float4 finalColor = float4(0.3,0.3,0.3,0.3);
	finalColor += saturate(dot((float3)lightdir, input.normal)) ;
	finalColor.a = 1;
	return finalColor;
}

