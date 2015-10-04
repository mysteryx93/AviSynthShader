"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResDiff.cso" "src\SuperRes\Diff.hlsl"

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperRes.cso" "src\SuperRes\SuperResEx.hlsl"

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "SuperResFinal.cso" "src\SuperRes\SuperResEx.hlsl" /DFinalPass=1

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "GammaToLinear.cso" "src\Common\GammaToLinear.hlsl"

"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\fxc.exe" /T ps_3_0 /Fo "LinearToGamma.cso" "src\Common\LinearToGamma.hlsl"
