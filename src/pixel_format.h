/////////////////////////////////////////////////////////////////////////////
// This file is part of EasyRPG Player.
//
// EasyRPG Player is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// EasyRPG Player is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
/////////////////////////////////////////////////////////////////////////////

#ifndef _EASYRPG_PIXEL_FORMAT_H_
#define _EASYRPG_PIXEL_FORMAT_H_

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <cstdlib>
#include <algorithm>
#include "system.h"

////////////////////////////////////////////////////////////
/// enums
////////////////////////////////////////////////////////////

namespace PF {
	enum AlphaType { NoAlpha, ColorKey, Alpha };
	enum OpacityType { Opaque, Binary, Variable };
	enum { ONE = 255 };
	enum { StaticMasks = false, DynamicMasks = true };
	enum { StaticAlpha = false, DynamicAlpha = true };
	enum { NotAligned = false, IsAligned = true };
}

////////////////////////////////////////////////////////////
/// Component struct
////////////////////////////////////////////////////////////
struct Component {
	uint8 bits;
	uint8 shift;
	uint8 byte;
	uint32 mask;

	static inline int count_bits(uint32 mask) {
		int count = 0;
		if ((mask & 0xFFFF0000) != 0)
			count += 16, mask >>= 16;
		if ((mask & 0xFF00) != 0)
			count += 8, mask >>= 8;
		if ((mask & 0xF0) != 0)
			count += 4, mask >>= 4;
		if ((mask & 0xC) != 0)
			count += 2, mask >>= 2;
		if ((mask & 0x2) != 0)
			count += 1, mask >>= 1;
		if ((mask & 0x1) != 0)
			count++;
		return count;
	}

	inline void convert_mask() {
		int bit_count = count_bits(mask);
		uint32 mask_ex = (1U << bit_count) - 1;
		uint32 mask_lo = mask_ex - mask;
		shift = (uint8)count_bits(mask_lo);
		bits = (uint8)(bit_count - shift);
		byte = shift / 8;
	}

	inline bool operator==(const Component& c) {
		return mask == c.mask;
	}

	inline bool operator!=(const Component& c) {
		return mask != c.mask;
	}

	Component() {}

	Component(unsigned int bits, unsigned int shift) :
		bits((uint8)bits),
		shift((uint8)shift),
		byte((uint8)(shift / 8)),
		mask(((1 << bits)-1) << shift) {}

	Component(uint32 mask) :
		mask(mask) { convert_mask(); }
};

////////////////////////////////////////////////////////////
/// DynamicFormat struct
////////////////////////////////////////////////////////////
class DynamicFormat {
public:
	int bits;
	Component r, g, b, a;
	PF::AlphaType alpha_type;

	DynamicFormat() {}

	DynamicFormat(int bits,
				  int rb, int rs,
				  int gb, int gs,
				  int bb, int bs,
				  int ab, int as,
				  PF::AlphaType alpha_type) :
		bits(bits),
		r(rb, rs), g(gb, gs), b(bb, bs), a(ab, as),
		alpha_type(alpha_type) {}

	DynamicFormat(int bits,
				  uint32 rmask,
				  uint32 gmask,
				  uint32 bmask,
				  uint32 amask,
				  PF::AlphaType alpha_type) :
		bits(bits),
		r(rmask), g(gmask), b(bmask), a(amask),
		alpha_type(alpha_type) {}

	DynamicFormat(const DynamicFormat& ref) :
		bits(ref.bits),
		r(ref.r), g(ref.g), b(ref.b), a(ref.a),
		alpha_type(ref.alpha_type) {}

	void Set(int _bits,
			 int rb, int rs,
			 int gb, int gs,
			 int bb, int bs,
			 int ab, int as,
			 PF::AlphaType _alpha_type) {
		bits = _bits;
		r = Component(rb, rs);
		g = Component(gb, gs);
		b = Component(bb, bs);
		a = Component(ab, as);
		alpha_type = _alpha_type;
	}

	void Set(int _bits,
			 uint32 rmask,
			 uint32 gmask,
			 uint32 bmask,
			 uint32 amask,
			 PF::AlphaType _alpha_type) {
		bits = _bits;
		r = Component(rmask);
		g = Component(gmask);
		b = Component(bmask);
		a = Component(amask);
		alpha_type = _alpha_type;
	}

	inline int code(bool shifts) const {
		int x = (int) alpha_type | ((bits - 1) << 2);
		if (!shifts)
			return x;
		return x |
			(r.shift <<  7) |
			(g.shift << 12) |
			(b.shift << 17);
	}

	inline bool operator==(const DynamicFormat& f) {
		return r ==  f.r && g == f.g && b == f.b && a == f.a && alpha_type == f.alpha_type;
	}

	inline bool operator!=(const DynamicFormat& f) {
		return r !=  f.r || g != f.g || b != f.b || a != f.a || alpha_type != f.alpha_type;
	}
};

////////////////////////////////////////////////////////////
/// Bits traits
////////////////////////////////////////////////////////////

template <class TPF, int bits>
struct bits_traits {
};

template <class TPF>
struct bits_traits<TPF, 16> {
	static inline uint32 get_uint32(const uint8* p) {
		return (uint32)*(const uint16*)p;
	}
	static inline void set_uint32(uint8* p, uint32 pix) {
		*(uint16*)p = (uint16) pix;
	}
	static inline void copy_pixel(uint8* dst, const uint8* src) {
		*(uint16*)dst = *(const uint16*)src;
	}
	static inline void set_pixels(uint8* dst, const uint8* src, int n) {
		uint16 pixel = (uint16) get_uint32(src);
		uint16* dst_pix = (uint16*) dst;
		std::fill(dst_pix, dst_pix + n, pixel);
	}
};

template <class TPF>
struct bits_traits<TPF, 24> {
	static inline uint32 get_uint32(const uint8* p) {
		return
			((uint32)(p[TPF::endian(2)]) << 16) |
			((uint32)(p[TPF::endian(1)]) <<  8) |
			((uint32)(p[TPF::endian(0)]) <<  0);
	}
	static inline void set_uint32(uint8* p, uint32 pix) {
		p[TPF::endian(0)] = (pix >>  0) & 0xFF;
		p[TPF::endian(1)] = (pix >>  8) & 0xFF;
		p[TPF::endian(2)] = (pix >> 16) & 0xFF;
	}
	static inline void copy_pixel(uint8* dst, const uint8* src) {
		memcpy(dst, src, 3);
	}
	static inline void set_pixels(uint8* dst, const uint8* src, int n) {
		for (int i = 0; i < n; i++)
			copy_pixel(dst + i * 3, src);
	}
};

template <class TPF>
struct bits_traits<TPF, 32> {
	static inline uint32 get_uint32(const uint8* p) {
		return *(const uint32*) p;
	}
	static inline void set_uint32(uint8* p, uint32 pix) {
		*(uint32*)p = pix;
	}
	static inline void copy_pixel(uint8* dst, const uint8* src) {
		*(uint32*)dst = *(const uint32*)src;
	}
	static inline void set_pixels(uint8* dst, const uint8* src, int n) {
		uint32 pixel = get_uint32(src);
		uint32* dst_pix = (uint32*) dst;
		std::fill(dst_pix, dst_pix + n, pixel);
	}
};

////////////////////////////////////////////////////////////
/// alpha_type_traits
////////////////////////////////////////////////////////////

// general case
template<class TPF, bool dynamic_alpha, PF::AlphaType alpha_type>
struct alpha_type_traits {};

template<class TPF, PF::AlphaType _alpha_type>
struct alpha_type_traits<TPF, PF::StaticAlpha, _alpha_type> {
	static inline PF::AlphaType alpha_type(const TPF* pf) {
		return (PF::AlphaType) _alpha_type;
	}
};

template<class TPF, PF::AlphaType _alpha_type>
struct alpha_type_traits<TPF, PF::DynamicAlpha, _alpha_type> {
	static inline PF::AlphaType alpha_type(const TPF* pf) {
		return pf->format.alpha_type;
	}
};


////////////////////////////////////////////////////////////
/// opacity_type_traits
////////////////////////////////////////////////////////////

// general case
template<class TPF, bool dynamic_alpha, PF::OpacityType opacity_type>
struct opacity_type_traits {};

template<class TPF, PF::OpacityType _opacity_type>
struct opacity_type_traits<TPF, PF::StaticAlpha, _opacity_type> {
	static inline PF::OpacityType opacity_type(const TPF* pf) {
		return _opacity_type;
	}
};

template<class TPF, PF::OpacityType _opacity_type>
struct opacity_type_traits<TPF, PF::DynamicAlpha, _opacity_type> {
	static inline PF::OpacityType opacity_type(const TPF* pf) {
		return pf->format.opacity_type;
	}
};

////////////////////////////////////////////////////////////
/// Alpha traits
////////////////////////////////////////////////////////////

// general case
template<class TPF,
		 bool aligned,
		 bool dynamic_alpha,
		 int alpha_type>
struct alpha_traits {
	static inline uint8 get_alpha(const TPF* pf, const uint8* p) {
		uint8 r, g, b, a;
		pf->get_rgba(p, r, g, b, a);
		return a;
	}
	static inline void set_alpha(const TPF* pf, uint8* p, uint8 alpha) {
		uint8 r, g, b, a;
		pf->get_rgba(p, r, g, b, a);
		pf->set_rgba(p, r, g, b, alpha);
	}
};

// dynamic, colorkey
template <class TPF, bool aligned>
struct alpha_traits<TPF, aligned, PF::DynamicAlpha, PF::ColorKey> {
	static inline uint8 get_alpha(const TPF* pf, const uint8* p) {
		uint32 pix = pf->get_uint32(p);
		return (pf->has_alpha() && pix == pf->colorkey) ? 0 : 255;
	}
	static inline void set_alpha(const TPF* pf, uint8* p, uint8 alpha) {
		if (pf->has_alpha() && alpha == 0)
			pf->set_uint32(p, pf->colorkey);
	}
};

// colorkey
template <class TPF, bool aligned>
struct alpha_traits<TPF, aligned, PF::StaticAlpha, PF::ColorKey> {
	static inline uint8 get_alpha(const TPF* pf, const uint8* p) {
		uint32 pix = pf->get_uint32(p);
		return (pix == pf->colorkey) ? 0 : 255;
	}
	static inline void set_alpha(const TPF* pf, uint8* p, uint8 alpha) {
		if (alpha == 0)
			pf->set_uint32(p, pf->colorkey);
	}
};

// no alpha or colorkey
template<class TPF, bool aligned>
struct alpha_traits<TPF, aligned, PF::StaticAlpha, PF::NoAlpha> {
	static inline uint8 get_alpha(const TPF* pf, const uint8* p) {
		return 255;
	}
	static inline void set_alpha(const TPF* pf, uint8* p, uint8 alpha) {
	}
};

// aligned, with alpha
template<class TPF, bool dynamic_alpha>
struct alpha_traits<TPF, PF::IsAligned, dynamic_alpha, PF::Alpha> {
	static inline uint8 get_alpha(const TPF* pf, const uint8* p) {
		return pf->has_alpha() ? p[pf->a_byte()] : 255;
	}
	static inline void set_alpha(const TPF* pf, uint8* p, uint8 alpha) {
		if (pf->has_alpha())
			p[pf->a_byte()] = alpha;
	}
};

////////////////////////////////////////////////////////////
/// RGBA traits
////////////////////////////////////////////////////////////

template<class TPF, bool aligned, bool dynamic_alpha, int alpha>
struct rgba_traits {
};

// aligned, has alpha
template<class TPF, bool dynamic_alpha>
struct rgba_traits<TPF, PF::IsAligned, dynamic_alpha, PF::Alpha> {
	static inline void get_rgba(const TPF* pf, const uint8* p, uint8& r, uint8& g, uint8& b, uint8& a) {
		r = p[pf->r_byte()];
		g = p[pf->g_byte()];
		b = p[pf->b_byte()];
		a = pf->has_alpha() ? p[pf->a_byte()] : 255;
	}

	static inline void set_rgba(const TPF* pf, uint8* p, const uint8& r, const uint8& g, const uint8& b, const uint8& a) {
		p[pf->r_byte()] = r;
		p[pf->g_byte()] = g;
		p[pf->b_byte()] = b;
		if (pf->has_alpha())
			p[pf->a_byte()] = a;
	}
};

// aligned, has colorkey
template<class TPF, bool dynamic_alpha>
struct rgba_traits<TPF, PF::IsAligned, dynamic_alpha, PF::ColorKey> {
	static inline void get_rgba(const TPF* pf, const uint8* p, uint8& r, uint8& g, uint8& b, uint8& a) {
		r = p[pf->r_byte()];
		g = p[pf->g_byte()];
		b = p[pf->b_byte()];
		a = (pf->has_alpha() && pf->get_uint32(p) == pf->colorkey) ? 0 : 255;
	}

	static inline void set_rgba(const TPF* pf, uint8* p, const uint8& r, const uint8& g, const uint8& b, const uint8& a) {
		if (pf->has_alpha() && a == 0)
			pf->set_uint32(p, pf->colorkey);
		else {
			p[pf->r_byte()] = r;
			p[pf->g_byte()] = g;
			p[pf->b_byte()] = b;
		}
	}
};

// aligned, no alpha or colorkey
template<class TPF>
struct rgba_traits<TPF, PF::IsAligned, PF::StaticAlpha, PF::NoAlpha> {
	static inline void get_rgba(const TPF* pf, const uint8* p, uint8& r, uint8& g, uint8& b, uint8& a) {
		r = p[pf->r_byte()];
		g = p[pf->g_byte()];
		b = p[pf->b_byte()];
		a = 255;
	}

	static inline void set_rgba(const TPF* pf, uint8* p, const uint8& r, const uint8& g, const uint8& b, const uint8& a) {
		p[pf->r_byte()] = r;
		p[pf->g_byte()] = g;
		p[pf->b_byte()] = b;
	}
};

// unaligned, has alpha (dynamic)
template<class TPF, bool dynamic_alpha>
struct rgba_traits<TPF, PF::NotAligned, dynamic_alpha, PF::Alpha> {
	static inline void get_rgba(const TPF* pf, const uint8* p, uint8& r, uint8& g, uint8& b, uint8& a) {
		const uint32 pix = pf->get_uint32(p);
		pf->uint32_to_rgba(pix, r, g, b, a);
		if (!pf->has_alpha())
			a = 255;
	}
	static inline void set_rgba(const TPF* pf, uint8* p, const uint8& r, const uint8& g, const uint8& b, const uint8& a) {
		const uint32 pix = pf->rgba_to_uint32(r, g, b, a);
		pf->set_uint32(p, pix);
	}
};

// unaligned, has colorkey (dynamic)
template<class TPF, bool dynamic_alpha>
struct rgba_traits<TPF, PF::NotAligned, dynamic_alpha, PF::ColorKey> {
	static inline void get_rgba(const TPF* pf, const uint8* p, uint8& r, uint8& g, uint8& b, uint8& a) {
		const uint32 pix = pf->get_uint32(p);
		if (pf->has_alpha() && pix == pf->colorkey)
			a = 0;
		pf->uint32_to_rgba(pix, r, g, b, a);
	}

	static inline void set_rgba(const TPF* pf, uint8* p, const uint8& r, const uint8& g, const uint8& b, const uint8& a) {
		const uint32 pix = (pf->has_alpha() && a == 0)
			? pf->colorkey
			: pf->rgba_to_uint32(r, g, b, a);
		pf->set_uint32(p, pix);
	}
};

// unaligned, no alpha or colorkey
template<class TPF>
struct rgba_traits<TPF, PF::NotAligned, PF::StaticAlpha, PF::NoAlpha> {
	static inline void get_rgba(const TPF* pf, const uint8* p, uint8& r, uint8& g, uint8& b, uint8& a) {
		const uint32 pix = pf->get_uint32(p);
		pf->uint32_to_rgba(pix, r, g, b, a);
		a = 255;
	}
	static inline void set_rgba(const TPF* pf, uint8* p, const uint8& r, const uint8& g, const uint8& b, const uint8& a) {
		const uint32 pix = pf->rgba_to_uint32(r, g, b, a);
		pf->set_uint32(p, pix);
	}
};

////////////////////////////////////////////////////////////
/// Mask traits
////////////////////////////////////////////////////////////

template<class TPF, bool dynamic, int _bits, int _shift>
struct mask_traits {
};

template<class TPF, int _bits, int _shift>
struct mask_traits<TPF, PF::StaticMasks, _bits, _shift> {
	static const int _byte = _shift / 8;
	static const int _mask = ((1 << _bits)-1) << _shift;
	static inline int bits(const Component& c) { return _bits; }
	static inline int shift(const Component& c) { return _shift; }
	static inline int byte(const Component& c) { return _byte; }
	static inline int mask(const Component& c) { return _mask; }
};

template<class TPF, int _bits, int _shift>
struct mask_traits<TPF, PF::DynamicMasks, _bits, _shift> {
	static inline int bits(const Component& c) { return c.bits; }
	static inline int shift(const Component& c) { return c.shift; }
	static inline int byte(const Component& c) { return c.byte; }
	static inline int mask(const Component& c) { return c.mask; }
};

////////////////////////////////////////////////////////////
/// Dynamic traits
////////////////////////////////////////////////////////////

template <bool DYNAMIC_MASKS, bool DYNAMIC_ALPHA,
		  int BITS, int RB, int RS, int GB, int GS, int BB, int BS, int AB, int AS, int ALPHA>
struct dynamic_traits_t {
	DynamicFormat format;
	dynamic_traits_t(const DynamicFormat &format) : format(format) {}
	void set_format(const DynamicFormat &_format) { format = _format; }
};

template <int BITS, int RB, int RS, int GB, int GS, int BB, int BS, int AB, int AS, int ALPHA>
struct dynamic_traits_t<false, false, BITS, RB, RS, GB, GS, BB, BS, AB, AS, ALPHA> {
	static const DynamicFormat format;
	dynamic_traits_t(const DynamicFormat &format) {}
	void set_format(const DynamicFormat &_format) const {}
};

template <int BITS, int RB, int RS, int GB, int GS, int BB, int BS, int AB, int AS, int ALPHA>
const DynamicFormat dynamic_traits_t<false, false, BITS, RB, RS, GB, GS, BB, BS, AB, AS, ALPHA>::format(BITS, RB, RS, GB, GS, BB, BS, AB, AS, (PF::AlphaType) ALPHA);

////////////////////////////////////////////////////////////
/// PixelFormat abstract base class
////////////////////////////////////////////////////////////

class PixelFormat {
public:
	PixelFormat() : colorkey(0) {}

	virtual bool Match(const DynamicFormat& ref) const = 0;
	virtual int Bits() const = 0;
	virtual bool HasAlpha() const = 0;
	virtual const DynamicFormat& Format() const  = 0;

	void SetColorKey(int _colorkey) {
		colorkey = _colorkey;
	}

	uint32 colorkey;
};

////////////////////////////////////////////////////////////
/// PixelFormatT template
////////////////////////////////////////////////////////////

template <int BITS,
		  bool DYNAMIC_MASKS, bool DYNAMIC_ALPHA, int ALPHA, bool ALIGNED,
		  int RB, int RS, int GB, int GS, int BB, int BS, int AB, int AS>
class PixelFormatT : public PixelFormat {
public:
	typedef PixelFormatT<BITS,DYNAMIC_MASKS,DYNAMIC_ALPHA,ALPHA,ALIGNED,RB,RS,GB,GS,BB,BS,AB,AS> my_type;

	static const int bits = BITS;
	static const int bytes = (BITS + 7) / 8;

	static const bool dynamic_masks = DYNAMIC_MASKS;
	static const bool dynamic_alpha = DYNAMIC_ALPHA;
	static const PF::AlphaType alpha = (PF::AlphaType) ALPHA;
	static const PF::OpacityType opacity =
		ALPHA == PF::NoAlpha ? PF::Opaque :
		(ALPHA == PF::ColorKey || AB == 1) ? PF::Binary :
		PF::Variable;

	static const bool aligned = ALIGNED;

	static const int ONE = 255;
	// static const int ONE = 256; // faster but less accurate

	typedef bits_traits<my_type, bits> bits_traits_type;
	typedef dynamic_traits_t<DYNAMIC_MASKS,DYNAMIC_ALPHA,BITS,RB,RS,GB,GS,BB,BS,AB,AS,ALPHA> dynamic_traits_type;
	typedef alpha_type_traits<my_type, dynamic_alpha, alpha> alpha_type_traits_type;
	typedef opacity_type_traits<my_type, dynamic_alpha, opacity> opacity_type_traits_type;
	typedef alpha_traits<my_type, aligned, dynamic_alpha, alpha> alpha_traits_type;
	typedef rgba_traits<my_type, aligned, dynamic_alpha, alpha> rgba_traits_type;
	typedef mask_traits<my_type, dynamic_masks, RB, RS> mask_r_traits_type;
	typedef mask_traits<my_type, dynamic_masks, GB, GS> mask_g_traits_type;
	typedef mask_traits<my_type, dynamic_masks, BB, BS> mask_b_traits_type;
	typedef mask_traits<my_type, dynamic_masks, AB, AS> mask_a_traits_type;
	dynamic_traits_type dynamic_traits;

	PixelFormatT() : dynamic_traits(DynamicFormat(BITS, RB, RS, GB, GS, BB, BS, AB, AS, (PF::AlphaType) ALPHA)) {}
	PixelFormatT(const DynamicFormat& format) : dynamic_traits(format) {}

	static inline int endian(int byte) {
#ifndef USE_BIG_ENDIAN
		return byte;
#else
		return bytes - 1 - byte;
#endif
	}

	inline void uint32_to_rgba(uint32 pix, uint8& r, uint8& g, uint8& b, uint8& a) const {
		r = (uint8)(((pix >> r_shift()) & ((1 << r_bits()) - 1)) << (8 - r_bits()));
		g = (uint8)(((pix >> g_shift()) & ((1 << g_bits()) - 1)) << (8 - g_bits()));
		b = (uint8)(((pix >> b_shift()) & ((1 << b_bits()) - 1)) << (8 - b_bits()));
		a = (uint8)(((pix >> a_shift()) & ((1 << a_bits()) - 1)) << (8 - a_bits()));
	}

	inline uint32 rgba_to_uint32(const uint8& r, const uint8& g, const uint8& b, const uint8& a) const {
		return
			(((uint32)r >> (8 - r_bits())) << r_shift()) | 
			(((uint32)g >> (8 - g_bits())) << g_shift()) | 
			(((uint32)b >> (8 - b_bits())) << b_shift()) | 
			(((uint32)a >> (8 - a_bits())) << a_shift());
	}

	inline const DynamicFormat& format() const {
		return dynamic_traits.format;
	}

	inline int r_byte() const {		return endian(mask_r_traits_type::byte(format().r));	}
	inline int g_byte() const {		return endian(mask_g_traits_type::byte(format().g));	}
	inline int b_byte() const {		return endian(mask_b_traits_type::byte(format().b));	}
	inline int a_byte() const {		return endian(mask_a_traits_type::byte(format().a));	}

	inline uint32 r_mask() const {	return mask_r_traits_type::mask(format().r);	}
	inline uint32 g_mask() const {	return mask_g_traits_type::mask(format().g);	}
	inline uint32 b_mask() const {	return mask_b_traits_type::mask(format().b);	}
	inline uint32 a_mask() const {	return mask_a_traits_type::mask(format().a);	}

	inline int r_bits() const {		return mask_r_traits_type::bits(format().r);	}
	inline int g_bits() const {		return mask_g_traits_type::bits(format().g);	}
	inline int b_bits() const {		return mask_b_traits_type::bits(format().b);	}
	inline int a_bits() const {		return mask_a_traits_type::bits(format().a);	}

	inline int r_shift() const {		return mask_r_traits_type::shift(format().r);	}
	inline int g_shift() const {		return mask_g_traits_type::shift(format().g);	}
	inline int b_shift() const {		return mask_b_traits_type::shift(format().b);	}
	inline int a_shift() const {		return mask_a_traits_type::shift(format().a);	}

	inline bool alpha_type() const {
		return alpha_type_traits_type::alpha_type(this);
	}

	inline bool opacity_type() const {
		return opacity_type_traits_type::opacity_type(this);
	}

	inline bool has_alpha() const {
		return alpha_type() != PF::NoAlpha;
	}

	inline uint32 get_uint32(const uint8* p) const {
		return bits_traits_type::get_uint32(p);
	}

	inline void set_uint32(uint8* p, uint32 pix) const {
		bits_traits_type::set_uint32(p, pix);
	}

	inline void copy_pixel(uint8* dst, const uint8* src) const {
		bits_traits_type::copy_pixel(dst, src);
	}

	inline void copy_pixels(uint8* dst, const uint8* src, int n) const {
		memcpy(dst, src, n * bytes);
	}

	inline void set_pixels(uint8* dst, const uint8* src, int n) const {
		bits_traits_type::set_pixels(dst, src, n);
	}

	inline uint8 opaque() const {
		return (a_bits() > 0) ? (uint8) (0xFF << (8 - a_bits())) : (uint8) 255;
	}

	inline uint8 get_alpha(const uint8* p) const {
		return alpha_traits_type::get_alpha(this, p);
	}

	inline void set_alpha(uint8* p, uint8 alpha) const {
		alpha_traits_type::set_alpha(this, p, alpha);
	}

	inline void get_rgba(const uint8* p, uint8& r, uint8& g, uint8& b, uint8& a) const {
		rgba_traits_type::get_rgba(this, p, r, g, b, a);
	}

	inline void set_rgba(uint8* p, const uint8& r, const uint8& g, const uint8& b, const uint8& a) const {
		rgba_traits_type::set_rgba(this, p, r, g, b, a);
	}

	bool Match(const DynamicFormat& ref) const {
		return
			bits == ref.bits &&
			(dynamic_alpha || alpha_type() == ref.alpha_type) &&
			(dynamic_masks || (
				 r_mask() == ref.r.mask &&
				 g_mask() == ref.g.mask &&
				 b_mask() == ref.b.mask &&
				 (a_mask() == ref.a.mask || alpha_type() != PF::Alpha)));
	}

	int Bits() const {
		return bits;
	}

	const DynamicFormat& Format() const {
		return dynamic_traits.format;
	}

	void SetFormat(const DynamicFormat& format) {
		dynamic_traits.set_format(format);
	}

	bool HasAlpha() const {
		return has_alpha();
	}
};

#ifndef USE_BIG_ENDIAN
typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::Alpha,PF::IsAligned,8,16,8,8,8,0,8,24> format_B8G8R8A8_a;
typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::Alpha,PF::IsAligned,8,0,8,8,8,16,8,24> format_R8G8B8A8_a;
typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::Alpha,PF::IsAligned,8,24,8,16,8,8,8,0> format_A8B8G8R8_a;
typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::Alpha,PF::IsAligned,8,8,8,16,8,24,8,0> format_A8R8G8B8_a;

typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::ColorKey,PF::IsAligned,8,16,8,8,8,0,8,24> format_B8G8R8A8_k;
typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::ColorKey,PF::IsAligned,8,0,8,8,8,16,8,24> format_R8G8B8A8_k;
typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::ColorKey,PF::IsAligned,8,24,8,16,8,8,8,0> format_A8B8G8R8_k;
typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::ColorKey,PF::IsAligned,8,8,8,16,8,24,8,0> format_A8R8G8B8_k;

typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::NoAlpha,PF::IsAligned,8,16,8,8,8,0,8,24> format_B8G8R8A8_n;
typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::NoAlpha,PF::IsAligned,8,0,8,8,8,16,8,24> format_R8G8B8A8_n;
typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::NoAlpha,PF::IsAligned,8,24,8,16,8,8,8,0> format_A8B8G8R8_n;
typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::NoAlpha,PF::IsAligned,8,8,8,16,8,24,8,0> format_A8R8G8B8_n;
#else
typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::Alpha,PF::IsAligned,8,8,8,16,8,24,8,0> format_B8G8R8A8_a;
typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::Alpha,PF::IsAligned,8,24,8,16,8,8,8,0> format_R8G8B8A8_a;
typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::Alpha,PF::IsAligned,8,0,8,8,8,16,8,24> format_A8B8G8R8_a;
typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::Alpha,PF::IsAligned,8,16,8,8,8,0,8,24> format_A8R8G8B8_a;

typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::ColorKey,PF::IsAligned,8,8,8,16,8,24,8,0> format_B8G8R8A8_k;
typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::ColorKey,PF::IsAligned,8,24,8,16,8,8,8,0> format_R8G8B8A8_k;
typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::ColorKey,PF::IsAligned,8,0,8,8,8,16,8,24> format_A8B8G8R8_k;
typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::ColorKey,PF::IsAligned,8,16,8,8,8,0,8,24> format_A8R8G8B8_k;

typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::NoAlpha,PF::IsAligned,8,8,8,16,8,24,8,0> format_B8G8R8A8_n;
typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::NoAlpha,PF::IsAligned,8,24,8,16,8,8,8,0> format_R8G8B8A8_n;
typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::NoAlpha,PF::IsAligned,8,0,8,8,8,16,8,24> format_A8B8G8R8_n;
typedef PixelFormatT<32,PF::StaticMasks,PF::StaticAlpha,PF::NoAlpha,PF::IsAligned,8,16,8,8,8,0,8,24> format_A8R8G8B8_n;
#endif

typedef PixelFormatT<32,PF::DynamicMasks,PF::StaticAlpha,PF::Alpha,PF::IsAligned,0,0,0,0,0,0,0,0> format_dynamic_32_a;
typedef PixelFormatT<32,PF::DynamicMasks,PF::StaticAlpha,PF::ColorKey,PF::IsAligned,0,0,0,0,0,0,0,0> format_dynamic_32_k;
typedef PixelFormatT<32,PF::DynamicMasks,PF::StaticAlpha,PF::NoAlpha,PF::IsAligned,0,0,0,0,0,0,0,0> format_dynamic_32_n;
typedef PixelFormatT<32,PF::DynamicMasks,PF::DynamicAlpha,PF::NoAlpha,PF::IsAligned,0,0,0,0,0,0,0,0> format_dynamic_32_d;

typedef PixelFormatT<24,PF::DynamicMasks,PF::StaticAlpha,PF::ColorKey,PF::IsAligned,0,0,0,0,0,0,0,0> format_dynamic_24_k;
typedef PixelFormatT<24,PF::DynamicMasks,PF::StaticAlpha,PF::NoAlpha,PF::IsAligned,0,0,0,0,0,0,0,0> format_dynamic_24_n;
typedef PixelFormatT<24,PF::DynamicMasks,PF::DynamicAlpha,PF::NoAlpha,PF::IsAligned,0,0,0,0,0,0,0,0> format_dynamic_24_d;

typedef PixelFormatT<16,PF::DynamicMasks,PF::StaticAlpha,PF::Alpha,PF::NotAligned,0,0,0,0,0,0,0,0> format_dynamic_16_a;
typedef PixelFormatT<16,PF::DynamicMasks,PF::StaticAlpha,PF::ColorKey,PF::NotAligned,0,0,0,0,0,0,0,0> format_dynamic_16_k;
typedef PixelFormatT<16,PF::DynamicMasks,PF::StaticAlpha,PF::NoAlpha,PF::NotAligned,0,0,0,0,0,0,0,0> format_dynamic_16_n;
typedef PixelFormatT<16,PF::DynamicMasks,PF::DynamicAlpha,PF::NoAlpha,PF::NotAligned,0,0,0,0,0,0,0,0> format_dynamic_16_d;

////////////////////////////////////////////////////////////

#endif
