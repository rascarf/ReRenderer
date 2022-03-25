struct VertexShaderInput
{
	float3 position  : POSITION;
	float2 texcoord  : TEXCOORD;
};
struct PixelShaderInput
{
	float4 pixelPosition : SV_POSITION;
	float2 texcoord : TEXCOORD;
};

SamplerState defaultSampler : register(s0);

Texture2D DebugTexture :register(t0);

PixelShaderInput main_vs(VertexShaderInput vin)
{
    PixelShaderInput Vout;

    Vout.pixelPosition = float4(vin.position,1.0);
    Vout.texcoord = vin.texcoord;

    return Vout;
}

float4 main_ps(PixelShaderInput pin) : SV_Target
{
    return float4(DebugTexture.Sample(defaultSampler,pin.texcoord).rrr,1.0f);
    // return float4(0.5f,0.5f,0.5f,1.0f);
}