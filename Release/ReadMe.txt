AviSynthShader by Etienne Charland
Provides a bridge between AviSynth and HLSL pixel shaders for high bit depth processing on the GPU.



SuperRes(input, passes, strength, softness, upscalecommand, srcMatrix601, convert)

Enhances upscaling quality.

Supported video formats: YV12, YV24, RGB24 and RGB32.

Arguments:

passes: How many SuperRes passes to run. Default=1.

strength: How agressively we want to run SuperRes, between 0 and 1. Default=1.

softness: How much smoothness we want to add, between 0 and 1. Default=0.

upscalecommand: An upscaling command that must contain offset-correction. Ex: """nnedi3_rpow2(2, cshift="Spline16Resize")"""

srcMatrix601: If true, the color matrix will be changed from Rec.601 to Rec.709 while running SuperRes. This avoids having to apply ColorMatrix separately. Default=false.

convert: Whether to call ConvertToFloat and ConvertFromFloat() within the shader. Set to false if input is already converted. Default=true.





Super-xBR(input, edgeStrength, weight, thirdPass, convert)

Doubles the size of the image. Produces a sharp result, but with severe ringing.

Supported video formats: YV12, YV24, RGB24 and RGB32.

Arguments:

edgeStrength: Value between 0 and 5 specifying the strength. Default=1.

weight: Value between 0 and 1.5 specifying the weight. Default=1.

thirdPass: Whether to run a 3rd pass. Default=true.

convert: Whether to call ConvertToFloat and ConvertFromFloat() within the shader. Set to false if input is already converted. Default=true.




ColorMatrix601to709(input)

Converts color matrix from Rec.601 to Rec.709 with 16 bit depth to avoid banding. Source can be YV12, YV24, RGB24 or RGB32.




ColorMatrix709to601(input)

Converts color matrix from Rec.709 to Rec.601 with 16 bit depth to avoid banding. Source can be YV12, YV24, RGB24 or RGB32.





Shaders are written by Shiandow and are available here
https://github.com/zachsaw/MPDN_Extensions/

The code was integrated into AviSynth by Etienne Charland.