# AviSynth Shader v1.3

<a href="https://github.com/mysteryx93/AviSynthShader/releases">Download here >></a>

This plugin allows running HLSL pixel shaders within AviSynth. This gives access to various HLSL filters that haven't been programmed in AviSynth.

This implementation allows running several shaders in a row. Shader() returns a clip containing commands, and each subsequent to Shader() adds a line to that command chain clip. You must call ExecuteShader() to execute the chain of commands. You must also call ConvertToShader() before and ConvertFromShader() at the end.

The following example will run Diff1 and Diff2 on the clip before returning a Merge of both results. (these shader names are fictive, you have to use real shaders!)

    ConvertToShader()
	Input
    Shader("Diff1.cso", Output=2)
    Shader("Diff2.cso", Output=3)
    Shader("Merge.cso", Clip1=2, Clip2=3, Output=1)
    ShaderExecute(last, Input, PrecisionIn=1, Precision=3, PrecisionOut=1)
    ConvertFromShader()

## Syntax:

#### ConvertToShader(Input, Precision)

Converts a YV12, YV24 or RGB32 clip into a wider frame containing UINT16 or half-float data. Clips must be converted in such a way before running any shader.

16-bit-per-channel half-float data isn't natively supported by AviSynth. It is stored in a RGB32 container with a Width that is twice larger. When using Clip.Width, you must divine by 2 to get the accurate width.

Arguments:

Precision: 1 to convert into BYTE, 2 to convert into UINT16, 3 to convert into half-float. Default=1 (conversion can be done on the GPU)

#### ConvertFromShader(Input, Format, Precision)

Convert a half-float clip into a standard YV12, YV24 or RGB32 clip.

Arguments:

Format: The video format to convert to. Valid formats are YV12, YV24 and RGB32. Default=YV12.

Precision: 1 to convert into BYTE, 2 to convert into UINT16, 3 to convert into half-float. Default=1

#### Shader(Input, Path, EntryPoint, ShaderModel, Param1-Param9, Clip1-Clip9, Output, Width, Height)

Runs a HLSL pixel shader on specified clip. You can either run a compiled .cso file or compile a .hlsl file.

Arguments:

Input: The first input clip.

Path: The path to the HLSL pixel shader file to run.

EntryPoint: If compiling HLSL source code, specified the code entry point.

ShaderModel: If compiling HLSL source code, specified the shader model. Usually PS_2_0 or PS_3_0

Param0-Param8: Sets each of the shader parameters.
Ex: "float4 p4 : register(c4);" will be set with Param4="1,1,1,1f"

End each value with 'f'(float), 'i'(int) or 'b'(bool) to specify its type.
Param0 corresponds to c0, Param1 corresponds to c1, etc.
If setting float or int, you can set a vector or 2, 3 or 4 elements by separating the values with ','.

Clip1-Clip9: The index of the clips to set into this shader. Input clips are defined when calling ExecuteShader. Clip1 sets 's0' within the shader, while clip2-clip9 set 's1' to 's8'. The order is important.
Default for clip1 is 1, for clip2-clip9 is 0 which means no source clip.

Output: The clip index where to write the output of this shader, between 1 and 9. Default is 1 which means it will be the output of ExecuteShader. If set to another value, you can use it as the input of another shader. The last shader in the chain must have output=1.

Width, Height: The size of the output texture. Default = same as input texture.

#### ExecuteShader(cmd, Clip1-Clip9, Precision)

Executes the chain of commands on specified input clips.

Arguments:

cmd: A clip containing the commands returned by calling Shader.

Clip1-Clip9: The clips on which to run the shaders.

Precision: 1 to execute with 8-bit precision, 2 to execute with 16-bit precision, 3 to execute with half-float precision. Default=2

PrecisionIn: 1 if input clips are BYTE, 2 if input clips are UINT16, 3 if input clips are half-float. Default=1

PrecisionOut: 1 to get an output clip with BYTE, 2 for UINT16, 3 for half-float. Default=1


#### SuperRes(Input, Passes, Strength, Softness, Upscalecommand, SrcMatrix601, PrecisionIn)

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


#### Super-xBR(Input, EdgeStrength, Sharpness, ThirdPass, Convert)

In Shaders\Super-xBR\super-xbr.avsi. Thanks to Shiandow for writing this great code!

Doubles the size of the image. Produces a sharp result, but with severe ringing.

Supported video formats: YV12, YV24, RGB24 and RGB32.

Arguments:

EdgeStrength: Value between 0 and 5 specifying the strength. Default=1.

Sharpness: Value between 0 and 1.5 specifying the weight. Default=1.

ThirdPass: Whether to run a 3rd pass. Default=true.

PrecisionIn: 0 to call ConvertToShader and ConvertFromShader within the shader. 1-3 if the source is already converted. Default=0.


#### ColorMatrix601to709(input)

In Shaders\ColorMatrix\ColorMatrix.avsi

Converts color matrix from Rec.601 to Rec.709 with 16 bit depth to avoid banding. Source can be YV12, YV24, RGB24 or RGB32.


#### ColorMatrix709to601(input)

Converts color matrix from Rec.709 to Rec.601 with 16 bit depth to avoid banding. Source can be YV12, YV24, RGB24 or RGB32.



Shiandow provides many other HLSL shaders available here that can be integrated into AviSynth.

https://github.com/zachsaw/MPDN_Extensions/tree/master/Extensions/RenderScripts
