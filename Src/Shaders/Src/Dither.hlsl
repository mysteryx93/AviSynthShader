sampler s0 : register(s0);
uniform sampler s1 : register(s1); // the Bayer Matrix texture
float4 p0 :  register(c0);
float2 p1 :  register(c1);
uniform float4 MatrixSize : register(c2); // width, height, 1/width, 1/height

#define width  (p0[0])
#define height (p0[1])
#define px (p1[0])
#define py (p1[1])

float Bayer(float2 uv)
{
    uv = uv * MatrixSize.zw;
    float val = dot(tex2D(s1, uv).bg, float2(256.0 * 255.0, 255.0));
    val = val * MatrixSize.z * MatrixSize.w;
    return val;
}

// -- Main code --
float4 main(float2 tex : TEXCOORD0) : COLOR {
    float4 c0 = tex2D(s0, tex);
    c0.xyz += ((Bayer(tex) - 128.0) / 256.0 / 255.0);
    return c0;
}