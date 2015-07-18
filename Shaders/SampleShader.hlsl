sampler2D TexSampler : register(S0);

float4 main(float2 uv : TEXCOORD) : COLOR
{
	float4 color = tex2D(TexSampler, uv);
	return float4(1 - color.rgb, color.a);
}