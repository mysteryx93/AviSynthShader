AviSynthShader by Etienne Charland
Provides a bridge between AviSynth and HLSL pixel shaders for high bit depth processing on the GPU.

Ex: NNEDI3+SuperRes
SuperRes(2, .42, 0, """nnedi3_rpow2(2, nns=4, cshift="Spline16Resize", threads=2)""")

Ex: Super-xBR+SuperRes
ConvertToShader(Precision=2)
SuperRes(2, 1, 0, """SuperXBR(PrecisionIn=2)""", PrecisionIn=2)
ConvertFromShader("YV12", Precision=2)



SuperRes(Input, Passes, Strength, Softness, Upscalecommand, SrcMatrix601, PrecisionIn)

In Shaders\SuperRes\SuperRes.avsi. Thanks to Shiandow for writing this great code!

Enhances upscaling quality.

Supported video formats: YV12, YV24, RGB24 and RGB32.

Arguments:

Passes: How many SuperRes passes to run. Default=1.

Strength: How agressively we want to run SuperRes, between 0 and 1. Default=1.

Softness: How much smoothness we want to add, between 0 and 1. Default=0.

Upscalecommand: An upscaling command that must contain offset-correction. Ex: """nnedi3_rpow2(2, cshift="Spline16Resize")"""

SrcMatrix601: If true, the color matrix will be changed from Rec.601 to Rec.709 while running SuperRes. This avoids having to apply ColorMatrix separately. Default=false.

PrecisionIn: 0 to call ConvertToShader and ConvertFromShader within the shader. 1-3 if the source is already converted. Default=0.




Super-xBR(Input, EdgeStrength, Sharpness, ThirdPass, Convert)

In Shaders\Super-xBR\super-xbr.avsi. Thanks to Shiandow for writing this great code!

Doubles the size of the image. Produces a sharp result, but with severe ringing.

Supported video formats: YV12, YV24, RGB24 and RGB32.

Arguments:

EdgeStrength: Value between 0 and 5 specifying the strength. Default=1.

Sharpness: Value between 0 and 1.5 specifying the weight. Default=1.

ThirdPass: Whether to run a 3rd pass. Default=true.

PrecisionIn: 0 to call ConvertToShader and ConvertFromShader within the shader. 1-3 if the source is already converted. Default=0.




ColorMatrix601to709(input)

In Shaders\ColorMatrix\ColorMatrix.avsi

Converts color matrix from Rec.601 to Rec.709 with 16 bit depth to avoid banding. Source can be YV12, YV24, RGB24 or RGB32.




ColorMatrix709to601(input)

Converts color matrix from Rec.709 to Rec.601 with 16 bit depth to avoid banding. Source can be YV12, YV24, RGB24 or RGB32.




Shaders are written by Shiandow and are available here
https://github.com/zachsaw/MPDN_Extensions/

The code was integrated into AviSynth by Etienne Charland.