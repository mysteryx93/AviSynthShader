sampler s0 : register(s0);
float4 out_size : register(c0);
float4 in_size : register(c1);
float2 BC : register(c2);

#define dst_size out_size.xy
#define dst_dx out_size.z
#define dst_dy out_size.w
#define dst_dxy out_size.zw

#define src_size in_size.xy
#define src_dx in_size.z
#define src_dy in_size.w
#define src_dxy in_size.zw

#define support_size 2
#define B BC.x
#define C BC.y

static const float2
	reduction_factor = max(1,src_size*dst_dxy),
	inv_reduction_factor = min(1,dst_size*src_dxy),
	xydia = 2*ceil(support_size*reduction_factor);

float cubic(float x)
{
	float ax = abs(x);

	if(ax < 1.)
	{
		return
			(
			((12. -  9. * B - 6. * C) * ax +
			(-18. + 12. * B + 6. * C)) * ax * ax +
			(  6. -  2. * B)
			) * .16666666666666666666666666666667;
	} else if(ax < 2.)
	{
		return
			(
			((    - B -  6. * C) * ax +
			(  6. * B + 30. * C)) * ax * ax +
			(-12. * B - 48. * C) * ax +
			(  8. * B + 24. * C)
			) * .16666666666666666666666666666667;
	}
	
	return 0.;
}

float4 main(float2 tex : TEXCOORD0) : COLOR0
{
	float2
		pos = tex + src_dxy * .5,
		f = frac(pos * src_size),
		start_pos = pos + (.5 - xydia/2 - f) * src_dxy;
	
	float3 OUT = 0;
	float ytaps_sum = 0;
	
	[loop] for(int i = xydia.y ; i-- ; )
	{
		float3 tmp = 0;
		float
			pos_y = start_pos.y+i*src_dy,
			xtaps_sum = 0,
			y_weight = (pos_y < 0 || pos_y > 1) ? 0 : cubic((1-f.y-xydia.y/2+i)*inv_reduction_factor.y);

		if(y_weight)
		{		
			[loop] for(int j = xydia.x ; j-- ; )
			{
				float
					pos_x = start_pos.x+j*src_dx,
					x_weight = (pos_x < 0 || pos_x > 1) ? 0 : cubic((1-f.x-xydia.x/2+j)*inv_reduction_factor.x);

				if(x_weight)
				{
					tmp += tex2Dlod(s0,float4(pos_x,pos_y,0,0)).rgb*x_weight;
					xtaps_sum += x_weight;
				}
			}
			
			if(xtaps_sum)
			{
				tmp /= xtaps_sum;
			}
			
			OUT += tmp*y_weight;		
			ytaps_sum += y_weight;
		}
	}

	if(ytaps_sum)
	{
		OUT /= ytaps_sum;
	}
	
	return float4(OUT,1);
}