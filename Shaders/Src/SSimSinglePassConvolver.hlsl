// -- Define horizontal convolver --
#define EntryPoint ScaleH
#define axis 0
#include "./SSimConvolver.hlsl"

// -- Define vertical convolver -- 
#define EntryPoint main
#define Get(pos) ScaleH(pos)
#define axis 1
#include "./SSimConvolver.hlsl"