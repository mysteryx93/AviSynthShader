sampler s0 : register(s0);
float4 p0 :  register(c0);
float2 p1 :  register(c1);

// -- Main code --
float4 main(float2 tex : TEXCOORD0) : COLOR {
    float4 c0 = tex2D(s0, tex);

    return c0;
}