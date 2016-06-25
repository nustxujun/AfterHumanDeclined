cbuffer ConstantBuffer : register(b0)
{
	matrix World;
	matrix ViewX;
	matrix ViewY;
	matrix ViewZ;
	matrix Projection;

	float width;
	float height;
	float depth;
	int bcount;
}

RWStructuredBuffer<int> counter:register(u0);

struct Voxel
{
	int3 pos;
	int color;
};

RWStructuredBuffer<Voxel> voxels:register(u1);

struct VS_INPUT
{
	float4 pos : POSITION;
#ifdef USINGCOLOR
	unsigned int color : COLOR0;
#endif
#ifdef USINGTEXTURE
	float2 uv: TEXCOORD0;
#endif
};

struct GS_INPUT
{
	float4 pos: SV_POSITION;
#ifdef USINGCOLOR
	unsigned int color : COLOR0;
#endif
#ifdef USINGTEXTURE
	float2 uv : TEXCOORD0;
#endif	
};

struct PS_INPUT
{
	float4 pos : SV_POSITION;
	unsigned int axis : COLOR1;
#ifdef USINGCOLOR
	unsigned int color : COLOR0;
#endif
#ifdef USINGTEXTURE
	float2 uv: TEXCOORD0;
#endif
};



GS_INPUT vs(VS_INPUT input)
{
	GS_INPUT output;// = (PS_INPUT)0;
	output.pos = input.pos;
	//output.pos = mul(output.pos, View);
	//output.pos = mul(output.pos, Projection);

#ifdef USINGCOLOR
	output.color = input.color;
#endif
#ifdef USINGTEXTURE
	output.uv = input.uv;
#endif
	
	return output;
}

[maxvertexcount(3)]
void gs(triangle GS_INPUT input[3], inout TriangleStream<PS_INPUT> output)
{
	float3 normal = normalize(cross((input[1].pos - input[0].pos).xyz, (input[2].pos - input[1].pos).xyz));
		float X = abs(normal.x);
	float Y = abs(normal.y);
	float Z = abs(normal.z);
	matrix view = ViewY;
	unsigned int axis = 1;

	//if (X > Y && X > Z)
	//{
	//	axis = 0; 
	//	view = ViewX;
	//}
	//else if (Y > X && Y > Z)
	//{
	//	axis = 1;
	//	view = ViewY;
	//}
	//else
	//{
	//	axis = 2;
	//	view = ViewZ;
	//}

	for (int i = 0; i < 3; ++i)
	{
		PS_INPUT o;
		o.pos = mul(input[i].pos, World);
		o.pos = mul(o.pos, view);
		o.pos = mul(o.pos, Projection);
		o.axis = axis;
#ifdef USINGCOLOR
		o.color = input[i].color;
#endif
#ifdef USINGTEXTURE
		o.uv = input[i].uv;
#endif
		output.Append(o);
	}
	output.RestartStrip();
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
void ps(PS_INPUT input) 
{
	if (bcount != 0)
	{
		counter[0] = counter.IncrementCounter();
		discard;
	}

	Voxel v;
	v.color = 0xffffffff;
	if (input.axis == 0)
	{
		v.pos.x = input.pos.z * width;
		v.pos.y = height - input.pos.y;
		v.pos.z = input.pos.x;
		v.color = 0xff804040;
	}
	else if (input.axis == 1)
	{
		v.pos.x = input.pos.x;
		v.pos.y = input.pos.z * height;
		v.pos.z = depth - input.pos.y;
	}
	else
	{
		v.pos.x = input.pos.x;
		v.pos.y = height - input.pos.y;
		v.pos.z = input.pos.z * depth;
	}
	voxels[voxels.IncrementCounter()] = v;
}


