# AviSynth Shader v1.0

This plugin allows running HLSL pixel shaders within AviSynth. This gives access to various HLSL filters that haven't been programmed in AviSynth.

## Syntax:

#### ConvertToFloat(input, convertYuv, precision)

Converts a YV12, YV24 or RGB32 clip into a wider frame containing half-float data. Clips must be converted in such a way before running any shader.

16-bit-per-channel half-float data isn't natively supported by AviSynth. It is stored in a RGB32 container with a Width that is twice larger. When using Clip.Width, you must divine by 2 to get the accurate width.

Arguments:

convertYuv: True to convert RGB data to YUV on the CPU. If false, YUV data will be copied as-is and can then be converted with a shader. Default=true unless source is RGB32.

precision: 1 to convert into 8-bit-per-channel, 2 to convert into 16-bit-per-channel. Default=2

#### ConvertFromFloat(input, format, convertYuv, precision)

Convert a half-float clip into a standard YV12, YV24 or RGB32 clip.

Arguments:

format: The video format to convert to. Valid formats are YV12, YV24 and RGB32. Default=YV12.

convertYuv: True to convert YUV data to RGB on the CPU. If false, you already ran a shader to convert to YUV and the data will be copied as-is. Default=true unless destination is RGB32.

precision: 1 to convert from 8-bit-per-channel, 2 to convert from 16-bit-per-channel. Default=2

#### Shader(input, path, entryPoint, shaderModel, precision, param1, param2, param3, param4, param5, param6, param7, param8, param9, clip1, clip2, clip3, clip4, width, height)

Runs a HLSL pixel shader on specified clip. You can either run a compiled .cso file or compile a .hlsl file.

Arguments:

input: The first input clip.

path: The path to the HLSL pixel shader file to run.

entryPoint: If compiling HLSL source code, specified the code entry point.

shaderModel: If compiling HLSL source code, specified the shader model. Usually PS_2_0 or PS_3_0

precision: 1 if input clip is 8-bit-per-channel, 2 if input clip is 16-bit-per-channel. Default=2

param1-param9: Sets each of the shader parameters.
Ex: "float4 p0 : register(c0);" will be set with Param1="p0=1,1,1,1f"

The first part is the shader parameter name, followed by '=' and the value, ending with 'f'(float), 'i'(int) or 'b'(bool).
If setting float, you can set a vector or 2, 3 or 4 elements by separating the values with ','.
The order of the parameters is not important. Note that if a parameter is not being used, it may be discarded during compilation and setting its value will fail.

clip1-clip4: Sets additional input clips. Input sets 's0' within the shader, while clip1-clip4 set 's1' to 's4'. The order is important.

width, height: The size of the output texture. Default = same as input texture.

#### SuperRes(input, passes, strength, softness, hqdownscaling, upscalecommand, folder)

In Shaders\SuperRes\SuperRes.avsi. Thanks to Shiandow for writing this great code!

Enhances upscaling quality.

Arguments:

passes: How many SuperRes passes to run. Default=1.

strength: How agressively we want to run SuperRes, between 0 and 1. Default=1.

softness: How much smoothness we want to add, between 0 and 1. Default=0.

hqdownscaling: True to downscale using Bicubic, false to downscale using Bilinear.

upscalecommand: An upscaling command that must contain offset-correction. Ex: """nnedi3_rpow2(2, cshift="Spline16Resize")"""

folder: The folder containing .cso files, ending with '/'. Leave empty to use default folder.


Shiandow provides many other HLSL shaders available here that can be integrated into AviSynth.

https://github.com/zachsaw/MPDN_Extensions/tree/master/Extensions/RenderScripts
