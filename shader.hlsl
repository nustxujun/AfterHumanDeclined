//--------------------------------------------------------------------------------------
// File: Tutorial02.fx
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


struct VS_OUTPUT
{
	float4 pos :SV_POSITION;
	float3 worldPos:TEXCOORD0;
	float3 normal :TEXCOORD1;
};

struct VS_INPUT
{
	float4 Pos : POSITION;
	float3 Norm : NORMAL;
};

cbuffer cbNeverChanges : register(b0)
{
	matrix World;
	matrix View;
	matrix Projection;
	float4 lightdir;
	float4 eye;
};

cbuffer cbVariable: register(b1)
{
	matrix local;
	float4 kd;
	float4 ks;
	float ns;
}


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VS(VS_INPUT input)
{
	VS_OUTPUT o;   
	o.pos = mul(input.Pos, local);
	o.pos = mul(o.pos, World);
	o.worldPos = (float3) o.pos;
	o.pos = mul(o.pos, View);
	o.pos = mul(o.pos, Projection);


	o.normal = mul(input.Norm, local);
	o.normal = mul(o.normal, World);
	o.normal = normalize(o.normal);
    return o;
}




//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(VS_OUTPUT input) : SV_Target
{
	float4 finalColor = 0;
	finalColor += saturate(dot((float3)lightdir, input.normal)) * kd;
	float3 V = normalize(eye - input.worldPos);
		float3 H = normalize((float3)lightdir + V);
		float spec = pow(max(dot(input.normal, H), 0), ns);
	float4 specolor = ks * spec;

		finalColor += saturate(specolor);

	finalColor = saturate(finalColor);
	finalColor.a = 1;
	return finalColor;
}

