/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef AGS_LIB_ALLEGRO_SURFACE_H
#define AGS_LIB_ALLEGRO_SURFACE_H

#include "graphics/managed_surface.h"
#include "ags/lib/allegro/base.h"
#include "common/array.h"

namespace AGS3 {

class BITMAP {
private:
	Graphics::ManagedSurface *_owner;
public:
	uint16 &w, &h, &pitch;
	Graphics::PixelFormat &format;
	bool clip;
	int ct, cb, cl, cr;
	Common::Array<byte *> line;
public:
	BITMAP(Graphics::ManagedSurface *owner);
	virtual ~BITMAP() {
	}

	Graphics::ManagedSurface &operator*() const {
		return *_owner;
	}
	Graphics::ManagedSurface &getSurface() {
		return *_owner;
	}
	const Graphics::ManagedSurface &getSurface() const {
		return *_owner;
	}

	unsigned char *getPixels() const {
		return (unsigned char *)_owner->getPixels();
	}

	unsigned char *getBasePtr(uint16 x, uint16 y) const {
		return (unsigned char *)_owner->getBasePtr(x, y);
	}

	uint getTransparentColor() const {
		// See allegro bitmap_mask_color
		// For paletted sprites this is 0.
		// For other color depths this is bright pink (RGB 255, 0, 255) with alpha set to 0.
		if (format.bytesPerPixel == 1)
			return 0;
		return format.ARGBToColor(0, 255, 0, 255);
	}

	int getpixel(int x, int y) const;

	void clear() {
		_owner->clear();
	}

	void makeOpaque();

	/**
	 * Draws a solid filled in circle
	 */
	void circlefill(int x, int y, int radius, int color);

	/**
	 * Fills an enclosed area starting at a given point
	 */
	void floodfill(int x, int y, int color);

	/**
	 * Draw a horizontal line
	 */
	void hLine(int x, int y, int x2, uint32 color) {
		_owner->hLine(x, y, x2, color);
	}

	/**
	 * Draw a vertical line.
	 */
	void vLine(int x, int y, int y2, uint32 color) {
		_owner->vLine(x, y, y2, color);
	}

	/**
	 * Draws the passed surface onto this one
	 */
	void draw(const BITMAP *srcBitmap, const Common::Rect &srcRect,
		int dstX, int dstY, bool horizFlip, bool vertFlip,
		bool skipTrans, int srcAlpha, int tintRed = -1, int tintGreen = -1,
		int tintBlue = -1);

	/**
	 * Stretches and draws the passed surface onto this one
	 */
	void stretchDraw(const BITMAP *srcBitmap, const Common::Rect &srcRect,
		const Common::Rect &destRect, bool skipTrans, int srcAlpha);

private:
	// True color blender functions
	// In Allegro all the blender functions are of the form
	// unsigned int blender_func(unsigned long x, unsigned long y, unsigned long n)
	// when x is the sprite color, y the destination color, and n an alpha value

	void blendPixel(uint32 srcCol, uint32 &destCol, uint32 alpha) const;
	void blendPixel(uint32 srcCol, uint32 &destCol, uint32 alpha, const Graphics::PixelFormat &srcFormat) const;

	// Original rgb blending logic
	// Used in kRgbToRgbBlender, kAlphaPreservedBlenderMode, kArgbToRgbBlender, kSourceAlphaBlender
	// src and dst colors are both RGB888
	// if (alpha)
	//     ++alpha;
	// uint32 res = ((src & 0xFF00FF) - (dst & 0xFF00FF)) * alpha / 256 + dst;
	// dst &= 0x00FF00;
	// src &= 0x00FF00;
	// uint32 g = (src - dst) * alpha / 256 + dst;
	// return (res & 0xFF00FF) | (g & 0x00FF00)

	// Original argb blending logic
	// Used in kArgbToArgbBlender, kRgbToArgbBlender
	// src and dst colors are both ARGB8888
	// ++src_alpha;
	// uint32 dst_alpha = geta32(dst);
	// if (dst_alpha)
	//     ++dst_alpha;
	// uint32 dst_g = (dst & 0x00FF00) * dst_alpha / 256;
	// dst = (dst & 0xFF00FF) * dst_alpha / 256;
	// dst_g = (((src & 0x00FF00) - (dst_g & 0x00FF00)) * src_alpha / 256 + dst_g) & 0x00FF00;
	// dst = (((src & 0xFF00FF) - (dst & 0xFF00FF)) * src_alpha / 256 + dst) & 0xFF00FF;
	// dst_alpha = 256 - (256 - src_alpha) * (256 - dst_alpha) / 256;
	// src_alpha = /* 256 * 256 == */ 0x10000 / dst_alpha;
	// dst_g = (dst_g * src_alpha / 256) & 0x00FF00;
	// dst = (dst * src_alpha / 256) & 0xFF00FF;
	// return dst | dst_g | (--dst_alpha << 24);

	// kRgbToRgbBlender
	inline void blendRgbToRgb(uint32 srcCol, uint32 &destCol, uint32 alpha, const Graphics::PixelFormat &srcFormat) const {
		// Default mode for set_trans_blender
		// Original doesn't set alpha (so it is 0)
		uint8 rSrc, gSrc, bSrc;
		srcFormat.colorToRGB(srcCol, rSrc, gSrc, bSrc);
		uint8 rDest, gDest, bDest;
		format.colorToRGB(destCol, rDest, gDest, bDest);
		double sAlpha = (double)(alpha & 0xff) / 255.0;
		rDest = static_cast<uint8>(rSrc * sAlpha + rDest * (1. - sAlpha));
		gDest = static_cast<uint8>(gSrc * sAlpha + gDest * (1. - sAlpha));
		bDest = static_cast<uint8>(bSrc * sAlpha + bDest * (1. - sAlpha));
		destCol = format.ARGBToColor(0, rDest, gDest, bDest);
	}

	inline void blendRgbToRgb(uint32 srcCol, uint32 &destCol, uint32 alpha) const {
		// We could maybe optimize this case for the RGB565 and ARGB8888 cases
		blendRgbToRgb(srcCol, destCol, alpha, format);
	}

	// kAlphaPreservedBlenderMode
	inline void blendPreserveAlpha(uint32 srcCol, uint32 &destCol, uint32 alpha, const Graphics::PixelFormat &srcFormat) const {
		// Original blender function: _myblender_alpha_trans24
		// Like blendRgbToRgb, but result as the same alpha as destCol
		uint8 rSrc, gSrc, bSrc;
		srcFormat.colorToRGB(srcCol, rSrc, gSrc, bSrc);
		uint8 aDest, rDest, gDest, bDest;
		format.colorToARGB(destCol, aDest, rDest, gDest, bDest);
		double sAlpha = (double)(alpha & 0xff) / 255.0;
		rDest = static_cast<uint8>(rSrc * sAlpha + rDest * (1. - sAlpha));
		gDest = static_cast<uint8>(gSrc * sAlpha + gDest * (1. - sAlpha));
		bDest = static_cast<uint8>(bSrc * sAlpha + bDest * (1. - sAlpha));
		destCol = format.ARGBToColor(aDest, rDest, gDest, bDest);
	}

	inline void blendPreserveAlpha(uint32 srcCol, uint32 &destCol, uint32 alpha) const {
		// We could maybe optimize this case for the RGB565 and ARGB8888 cases
		blendPreserveAlpha(srcCol, destCol, alpha, format);
	}

	// kArgbToArgbBlender
	inline void blendArgbToArgb(uint32 srcCol, uint32 &destCol, uint32 alpha, const Graphics::PixelFormat &srcFormat) const {
		// Original blender functions: _argb2argb_blender
		uint8 aSrc, rSrc, gSrc, bSrc;
		srcFormat.colorToARGB(srcCol, aSrc, rSrc, gSrc, bSrc);
		if (alpha != 0)
			aSrc = aSrc * ((alpha & 0xff) + 1) / 256;
		if (aSrc != 0) {
			uint8 aDest, rDest, gDest, bDest;
			format.colorToARGB(destCol, aDest, rDest, gDest, bDest);
			double sAlpha = (double)(aSrc & 0xff) / 255.0;
			double dAlpha = (double)aDest / 255.0;
				dAlpha *= (1.0 - sAlpha);
			rDest = static_cast<uint8>((rSrc * sAlpha + rDest * dAlpha) / (sAlpha + dAlpha));
			gDest = static_cast<uint8>((gSrc * sAlpha + gDest * dAlpha) / (sAlpha + dAlpha));
			bDest = static_cast<uint8>((bSrc * sAlpha + bDest * dAlpha) / (sAlpha + dAlpha));
			aDest = static_cast<uint8>(255. * (sAlpha + dAlpha));
			destCol = format.ARGBToColor(aDest, rDest, gDest, bDest);
		}
	}

	inline void blendArgbToArgb(uint32 srcCol, uint32 &destCol, uint32 alpha) const {
		// We could maybe optimize this case for the RGB565 and ARGB8888 cases
		blendArgbToArgb(srcCol, destCol, alpha, format);
	}

	// kRgbToArgbBlender
	inline void blendRgbToArgb(uint32 srcCol, uint32 &destCol, uint32 alpha, const Graphics::PixelFormat &srcFormat) const {
		// Original blender function: _rgb2argb_blenders
		uint8 rSrc, gSrc, bSrc;
		srcFormat.colorToRGB(srcCol, rSrc, gSrc, bSrc);
		if (alpha == 0 || alpha == 0xff)
			destCol = format.ARGBToColor(0xff, rSrc, gSrc, bSrc);
		else {
			uint8 aDest, rDest, gDest, bDest;
			format.colorToARGB(destCol, aDest, rDest, gDest, bDest);
			double sAlpha = (double)(alpha & 0xff) / 255.0;
			double dAlpha = (double)aDest / 255.0;
			dAlpha *= (1.0 - sAlpha);
			rDest = static_cast<uint8>((rSrc * sAlpha + rDest * dAlpha) / (sAlpha + dAlpha));
			gDest = static_cast<uint8>((gSrc * sAlpha + gDest * dAlpha) / (sAlpha + dAlpha));
			bDest = static_cast<uint8>((bSrc * sAlpha + bDest * dAlpha) / (sAlpha + dAlpha));
			aDest = static_cast<uint8>(255. * (sAlpha + dAlpha));
			destCol = format.ARGBToColor(aDest, rDest, gDest, bDest);
		}
	}

	inline void blendRgbToArgb(uint32 srcCol, uint32 &destCol, uint32 alpha) const {
		if (alpha == 0 || alpha == 0xff)
			destCol = srcCol | ((0xFF >> format.aLoss) << format.aShift);
		else
			blendRgbToArgb(srcCol, destCol, alpha, format);
	}

	// kArgbToRgbBlender
	inline void blendArgbToRgb(uint32 srcCol, uint32 &destCol, uint32 alpha, const Graphics::PixelFormat &srcFormat) const {
		// Original blender function: _argb2rgb_blender
		// // Original doesn't set alpha (so it is 0).
		// Original logic has uint32 src color as ARGB8888 and dst color as RGB888
		// uint32 src_alpha = src & oxFF000000;
		// if (alpha != 0)
		//  alpha = src_alpha * ((alpha & 0xff) + 1) / 256;
		// else
		//	alpha = src_alpha;
		// if (alpha)
		//     ++alpha;
		// uint32 res = ((src & 0xFF00FF) - (dst & 0xFF00FF)) * alpha / 256 + dst;
		// dst &= 0x00FF00;
		// src &= 0x00FF00;
		// uint32 g = (src - dst) * alpha / 256 + dst;
		// return (res & 0xFF00FF) | (g & 0x00FF00);
		uint8 aSrc, rSrc, gSrc, bSrc;
		srcFormat.colorToARGB(srcCol, aSrc, rSrc, gSrc, bSrc);
		if (alpha != 0)
			aSrc = aSrc * ((alpha & 0xff) + 1) / 256;
		uint8 rDest, gDest, bDest;
		format.colorToRGB(destCol, rDest, gDest, bDest);
		double sAlpha = (double)(aSrc & 0xff) / 255.0;
		rDest = static_cast<uint8>(rSrc * sAlpha + rDest * (1. - sAlpha));
		gDest = static_cast<uint8>(gSrc * sAlpha + gDest * (1. - sAlpha));
		bDest = static_cast<uint8>(bSrc * sAlpha + bDest * (1. - sAlpha));
		destCol = format.ARGBToColor(0, rDest, gDest, bDest);
	}

	inline void blendArgbToRgb(uint32 srcCol, uint32 &destCol, uint32 alpha) const {
		// We could maybe optimize this case for the RGB565 and ARGB8888 cases
		blendArgbToRgb(srcCol, destCol, alpha, format);
	}

	// kOpaqueBlenderMode
	inline void blendOpaque(uint32 srcCol, uint32 &destCol, uint32 alpha, const Graphics::PixelFormat &srcFormat) const {
		// Original blender function: _opaque_alpha_blender
		uint8 rSrc, gSrc, bSrc;
		srcFormat.colorToRGB(srcCol, rSrc, gSrc, bSrc);
		destCol = format.ARGBToColor(0xff, rSrc, gSrc, bSrc);
	}

	inline void blendOpaque(uint32 srcCol, uint32 &destCol, uint32 alpha) const {
		destCol = srcCol | ((0xFF >> format.aLoss) << format.aShift);
	}

	// kSourceAlphaBlender
	inline void blendSourceAlpha(uint32 srcCol, uint32 &destCol, uint32 /*alpha*/, const Graphics::PixelFormat &srcFormat) const {
		// Original blender function: _additive_alpha_copysrc_blender
		// Used after set_alpha_blender
		// Uses alpha from source. Original doesn't set alpha on result (so it is 0).
		uint8 aSrc, rSrc, gSrc, bSrc;
		srcFormat.colorToARGB(srcCol, aSrc, rSrc, gSrc, bSrc);
		uint8 rDest, gDest, bDest;
		format.colorToRGB(destCol, rDest, gDest, bDest);
		double sAlpha = (double)(aSrc & 0xff) / 255.0;
		rDest = static_cast<uint8>(rSrc * sAlpha + rDest * (1. - sAlpha));
		gDest = static_cast<uint8>(gSrc * sAlpha + gDest * (1. - sAlpha));
		bDest = static_cast<uint8>(bSrc * sAlpha + bDest * (1. - sAlpha));
		destCol = format.ARGBToColor(0, rDest, gDest, bDest);
	}

	inline void blendSourceAlpha(uint32 srcCol, uint32 &destCol, uint32 alpha) const {
		// We could maybe optimize this case for the RGB565 and ARGB8888 cases
		blendSourceAlpha(srcCol, destCol, alpha, format);
	}

	// kAdditiveBlenderMode
	inline void blendAdditiveAlpha(uint32 srcCol, uint32 &destCol, uint32 alpha, const Graphics::PixelFormat &srcFormat) const {
		// Original blender function: _additive_alpha_copysrc_blender
		uint8 aSrc, rSrc, gSrc, bSrc;
		srcFormat.colorToARGB(srcCol, aSrc, rSrc, gSrc, bSrc);
		if (aSrc != 0xff) {
			uint8 aDest = format.aBits() != 0 ? 0xff : Graphics::PixelFormat::expand(format.aBits(), destCol >> format.aShift);
			uint32 a = (uint32)aSrc + (uint32)aDest;
			if (a > 0xff)
				aSrc = 0xff;
			else
				aSrc = static_cast<uint8>(a);
			destCol = format.ARGBToColor(aSrc, rSrc, gSrc, bSrc);
		}
	}

	inline void blendAdditiveAlpha(uint32 srcCol, uint32 &destCol, uint32 alpha) const {
		// Original blender function: _additive_alpha_copysrc_blender
		if (format.aBits() == 0)
			destCol = srcCol;
		else {
			uint8 aSrc = Graphics::PixelFormat::expand(format.aBits(), srcCol >> format.aShift);
			if (aSrc == 0xff)
				destCol = srcCol;
			else {
				uint8 aDest = Graphics::PixelFormat::expand(format.aBits(), destCol >> format.aShift);
				uint32 a = (uint32)aSrc + (uint32)aDest;
				if (a > 0xff)
					aSrc = 0xff;
				else
					aSrc = static_cast<uint8>(a);
				destCol = srcCol | ((aSrc >> format.aLoss) << format.aShift);;
			}
		}
	}

	// kTintBlenderMode and kTintLightBlenderMode
	void blendTintSprite(uint32 srcCol, uint32 &destCol, uint32 alpha, const Graphics::PixelFormat &srcFormat, bool light) const;

	inline void blendTintSprite(uint32 srcCol, uint32 &destCol, uint32 alpha, bool light) const {
		blendTintSprite(srcCol, destCol, alpha, format, light);
	}


	inline uint32 getColor(const byte *data, byte bpp) const {
		switch (bpp) {
		case 1:
			return *data;
		case 2:
			return *(const uint16 *)data;
		case 4:
			return *(const uint32 *)data;
		default:
			error("Unsupported format in BITMAP::getColor");
		}
	}
};

/**
 * Derived surface class
 */
class Surface : public Graphics::ManagedSurface, public BITMAP {
public:
	Surface(int width, int height, const Graphics::PixelFormat &pixelFormat) :
			Graphics::ManagedSurface(width, height, pixelFormat), BITMAP(this) {
		// Allegro uses 255, 0, 255 RGB as the transparent color
		if (pixelFormat.bytesPerPixel == 2 || pixelFormat.bytesPerPixel == 4)
			setTransparentColor(pixelFormat.RGBToColor(255, 0, 255));
	}
	Surface(Graphics::ManagedSurface &surf, const Common::Rect &bounds) :
			Graphics::ManagedSurface(surf, bounds), BITMAP(this) {
		// Allegro uses 255, 0, 255 RGB as the transparent color
		if (surf.format.bytesPerPixel == 2 || surf.format.bytesPerPixel == 4)
			setTransparentColor(surf.format.RGBToColor(255, 0, 255));
	}
	~Surface() override {
	}
};

BITMAP *create_bitmap(int width, int height);
BITMAP *create_bitmap_ex(int color_depth, int width, int height);
BITMAP *create_sub_bitmap(BITMAP *parent, int x, int y, int width, int height);
BITMAP *create_video_bitmap(int width, int height);
BITMAP *create_system_bitmap(int width, int height);
void destroy_bitmap(BITMAP *bitmap);

} // namespace AGS3

#endif
