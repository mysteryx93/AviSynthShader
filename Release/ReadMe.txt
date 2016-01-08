AviSynthShader v1.3.4 (January 7th 2016)
by Etienne Charland

Provides a bridge between AviSynth and HLSL pixel shaders for high bit depth processing on the GPU.
http://forum.doom9.org/showthread.php?t=172698

Ex: NNEDI3+SuperRes
SuperRes(2, .42, 0, """nnedi3_rpow2(2, nns=4, cshift="Spline16Resize", threads=2)""")

Supported video formats: YV12, YV24, RGB24 and RGB32.



#### function SuperResXBR(Input, Passes, Strength, Softness, xbrEdgeStrength, xbrSharpness, MatrixIn, MatrixOut, FormatOut, Convert, ConvertYuv, lsb_in, lsb_out, WidthOut, HeightOut, b, c)

Enhances upscaling quality, combining Super-xBR and SuperRes to run in the same command chain, reducing memory transfers and increasing performance.

Arguments are the same as SuperRes and Super-xBR

WidthOut, HeightOut: Allows downscaling output with Bicubic before reading back from GPU

b, c: b and c parameters of Bicubic resize. Default is b=0, c=.75


#### SuperRes(Input, Passes, Strength, Softness, UpscaleCommand, MatrixIn, MatrixOut, FormatOut, Convert, ConvertYuv, lsb_in, lsb_upscale, lsb_out)

Enhances upscaling quality.

Arguments:

Passes: How many SuperRes passes to run. Default=1.

Strength: How agressively we want to run SuperRes, between 0 and 1. Default=1.

Softness: How much smoothness we want to add, between 0 and 1. Default=0.

UpscaleCommand: An upscaling command that must contain offset-correction. Ex: """nnedi3_rpow2(2, cshift="Spline16Resize")"""

MatrixIn/MatrixOut: The input and output color matrix (601 or 709). This can be used for color matrix conversion. Default="709" for both

FormatOut: The output format. Default = same as input.

Convert: Whether to call ConvertToShader and ConvertFromShader within the shader. Default=true

ConvertYuv: Whether do YUV-RGB color conversion. Default=true unless Convert=true and source is RGB

lsb_in, lsb_upscale, lsb_out: Whether the input, result of UpscaleCommand and output are to be converted to/from DitherTools' Stack16 format. Default=false


#### Super-xBR(Input, EdgeStrength, Sharpness, ThirdPass, FormatOut, Convert, lsb_in, lsb_out)

Doubles the size of the image. Produces a sharp result, but with severe ringing.

Arguments:

EdgeStrength: Value between 0 and 5 specifying the strength. Default=1.

Sharpness: Value between 0 and 1.5 specifying the weight. Default=1.

ThirdPass: Whether to run a 3rd pass. Default=true.

FormatOut: The output format. Default = same as input.

Convert: Whether to call ConvertToShader and ConvertFromShader within the shader. Default=true

lsb_in, lsb_out: Whether the input and output are to be converted to/from DitherTools' Stack16 format. Default=false


#### ColorMatrixShader(input, MatrixIn, MatrixOut, FormatOut)

Converts the color matrix with 16 bit depth to avoid banding. Source can be YV12, YV24, RGB24 or RGB32.

Arguments:

MatrixIn/MatrixOut: The input and output color matrix (601 or 709). Default="709" for both

FormatOut: The output format. Default = same as input.



Shaders are written by Shiandow and are available here
https://github.com/zachsaw/MPDN_Extensions/

The code was integrated into AviSynth by Etienne Charland.