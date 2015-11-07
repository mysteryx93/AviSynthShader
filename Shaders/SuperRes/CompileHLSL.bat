"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResDiff.cso" "src\SuperRes\Diff.hlsl"

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResDiffYUV.cso" "src\SuperRes\DiffYUV.hlsl"

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperRes.cso" "src\SuperRes\SuperResEx.hlsl"

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResFinal.cso" "src\SuperRes\SuperResEx.hlsl" /DFinalPass=1

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResFinalYUV.cso" "src\SuperRes\SuperResYUV.hlsl" /DFinalPass=1

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "YuvToLinear.cso" "src\Common\YuvToLinear.hlsl"

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "GammaToYuv.cso" "src\Common\GammaToYuv.hlsl"