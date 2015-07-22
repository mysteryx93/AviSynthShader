#include <cstdint>
typedef unsigned int u32;
typedef unsigned short u16;

union FloatBits {
	float m_float;
	u32 m_int;
	u32 Mantissa() const { return m_int & 0x7fffff; }
	u32 Exponent() const { return (m_int >> 23) & 0xff; }
	u32 Sign() const { return (m_int >> 31) & 0x1; }
	void Mantissa(u32 mantissa) { m_int = ((m_int & ~0x7fffffu) | mantissa); }
	void Exponent(u32 exponent) { m_int = ((m_int & ~0x7f800000u) | (exponent << 23)); }
	void Sign(u32 sign) { m_int = ((m_int & ~0x80000000u) | (sign << 31)); }
	void MantissaExponentSign(u32 mantissa, u32 exponent, u32 sign)
	{
		m_int = (mantissa | (exponent << 23) | (sign << 31));
	}
};
union HalfBits {
	u16 m_half;
	u32 Mantissa() const { return m_half & 0x3ff; }
	u32 Exponent() const { return (m_half >> 10) & 0x1f; }
	u32 Sign() const { return (m_half >> 15) & 0x1; }
	void Mantissa(u32 mantissa) { m_half = (u16)((m_half & ~0x3ff) | mantissa); }
	void Exponent(u32 exponent) { m_half = (u16)((m_half & ~0x7c00) | (exponent << 10)); }
	void Sign(u32 sign) { m_half = (u16)((m_half & ~0x8000) | (sign << 15)); }
	void MantissaExponentSign(u32 mantissa, u32 exponent, u32 sign)
	{
		m_half = (u16)(mantissa | (exponent << 10) | (sign << 15));
	}
};
struct half {
	half() {}
	half(const half& h) : bits(h.bits) {}
	explicit half(float f) { SetFromFloat(f); }
	void SetFromFloat(float input)
	{
		FloatBits f;
		f.m_float = input;

		u32 newSign = f.Sign();
		u32 newMantissa = 0;
		u32 newExponent = 0;
		u32 mantissa = f.Mantissa();
		u32 exponent = f.Exponent();

		if (exponent == 0xff)// Handle special cases
		{
			newExponent = 31;
			if (mantissa != 0)// NaN
				newMantissa = 1;
		}
		else if (exponent != 0)// Regular number
		{
			int unbiasedExponent = exponent - 127;
			if (unbiasedExponent < -14)// This maps to a denorm
			{
				u32 e = (u32)(-14 - unbiasedExponent); // 2 ^ -exponent
				if (e < 10)
					newMantissa = (0x800000 | mantissa) >> (13 + e);
				else if (e == 10)
					newMantissa = 1;
			}
			else if (unbiasedExponent > 15) {
				newExponent = 31; // Map this value to infinity
			}
			else {
				newMantissa = mantissa >> 13;
				newExponent = unbiasedExponent + 15;
			}
		}

		HalfBits h;
		h.MantissaExponentSign(newMantissa, newExponent, newSign);
		bits = h.m_half;
	}
	u16 bits;
};