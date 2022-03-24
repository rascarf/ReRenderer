cbuffer ShadowCB : register(b0)
{
	float4x4 viewProjectionMatrix;
    float NearZ;
};

struct VertexShaderInput
{
	float3 position  : POSITION;
};

struct PixelShaderInput
{
	float4 pixelPosition : SV_POSITION;
};

PixelShaderInput main_vs(VertexShaderInput In)
{
    PixelShaderInput Vout;
    
    float4 PosW = mul(viewProjectionMatrix,float4(In.position,1.0f));

    Vout.pixelPosition = PosW;

    return Vout;
}

void main_ps(PixelShaderInput Pin)
{

}