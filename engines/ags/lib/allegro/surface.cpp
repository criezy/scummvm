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

#include "ags/lib/allegro/gfx.h"
#include "ags/lib/allegro/color.h"
#include "ags/lib/allegro/flood.h"
#include "ags/ags.h"
#include "ags/globals.h"
#include "common/textconsole.h"
#include "graphics/screen.h"

namespace AGS3 {

BITMAP::BITMAP(Graphics::ManagedSurface *owner) : _owner(owner),
		w(owner->w), h(owner->h), pitch(owner->pitch), format(owner->format),
		clip(true), ct(0), cl(0), cr(owner->w), cb(owner->h) {
	line.resize(h);
	for (uint y = 0; y < h; ++y)
		line[y] = (byte *)_owner->getBasePtr(0, y);
}

int BITMAP::getpixel(int x, int y) const {
	if (x < 0 || y < 0 || x >= w || y >= h)
		return -1;

	const byte *pixel = (const byte *)getBasePtr(x, y);
	if (format.bytesPerPixel == 1)
		return *pixel;
	else if (format.bytesPerPixel == 2)
		return *(const uint16 *)pixel;
	else
		return *(const uint32 *)pixel;
}

void BITMAP::makeOpaque() {
	if (format.aBits() == 0)
		return;
	assert(format.bytesPerPixel == 4);
	uint32 alphaMask = format.ARGBToColor(0xff, 0, 0, 0);

	unsigned char *pixels = getPixels();
	for (int y = 0 ; y < h ; ++y, pixels += pitch) {
		uint32 *data = (uint32*)pixels;
		for (int x = 0 ; x < w ; ++x, ++data)
			(*data) |= alphaMask;
	}
}

void BITMAP::circlefill(int x, int y, int radius, int color) {
	int cx = 0;
	int cy = radius;
	int df = 1 - radius;
	int d_e = 3;
	int d_se = -2 * radius + 5;

	do {
		_owner->hLine(x - cy, y - cx, x + cy, color);

		if (cx)
			_owner->hLine(x - cy, y + cx, x + cy, color);

		if (df < 0) {
			df += d_e;
			d_e += 2;
			d_se += 2;
		} else {
			if (cx != cy) {
				_owner->hLine(x - cx, y - cy, x + cx, color);

				if (cy)
					_owner->hLine(x - cx, y + cy, x + cx, color);
			}

			df += d_se;
			d_e += 2;
			d_se += 4;
			cy--;
		}

		cx++;

	} while (cx <= cy);
}

void BITMAP::floodfill(int x, int y, int color) {
	AGS3::floodfill(this, x, y, color);
}

const int SCALE_THRESHOLD = 0x100;
#define VGA_COLOR_TRANS(x) ((x) * 255 / 63)

void BITMAP::draw(const BITMAP *srcBitmap, const Common::Rect &srcRect,
		int dstX, int dstY, bool horizFlip, bool vertFlip,
		bool skipTrans, int srcAlpha, int tintRed, int tintGreen,
		int tintBlue) {
	assert(format.bytesPerPixel == 2 || format.bytesPerPixel == 4 ||
		(format.bytesPerPixel == 1 && srcBitmap->format.bytesPerPixel == 1));

	// Allegro disables draw when the clipping rect has negative width/height.
	// Common::Rect instead asserts, which we don't want.
	if (cr <= cl || cb <= ct)
		return;

	// Figure out the dest area that will be updated
	Common::Rect dstRect(dstX, dstY, dstX + srcRect.width(), dstY + srcRect.height());
	Common::Rect destRect = dstRect.findIntersectingRect(
		Common::Rect(cl, ct, cr, cb));
	if (destRect.isEmpty())
		// Area is entirely outside the clipping area, so nothing to draw
		return;

	// Get source and dest surface. Note that for the destination we create
	// a temporary sub-surface based on the allowed clipping area
	const Graphics::ManagedSurface &src = **srcBitmap;
	Graphics::ManagedSurface &dest = *_owner;
	Graphics::Surface destArea = dest.getSubArea(destRect);

	// Define scaling and other stuff used by the drawing loops
	const int xDir = horizFlip ? -1 : 1;
	bool useTint = (tintRed >= 0 && tintGreen >= 0 && tintBlue >= 0);
	bool sameFormat = (src.format == format);

	uint32 palette[PAL_SIZE];
	if (src.format.bytesPerPixel == 1 && format.bytesPerPixel != 1) {
		for (int i = 0; i < PAL_SIZE; ++i) {
			palette[i] = format.RGBToColor(VGA_COLOR_TRANS(_G(current_palette)[i].r), VGA_COLOR_TRANS(_G(current_palette)[i].g), VGA_COLOR_TRANS(_G(current_palette)[i].b));
		}
		sameFormat = true;
	}

	uint32 tintCol = 0;
	if (useTint)
		tintCol = format.RGBToColor(tintRed, tintGreen, tintBlue);

	uint32 transColor = 0, alphaMask = 0xff;
	if (skipTrans && src.format.bytesPerPixel != 1) {
		transColor = src.format.ARGBToColor(0, 255, 0, 255);
		alphaMask = src.format.ARGBToColor(255, 0, 0, 0);
		alphaMask = ~alphaMask;
	}

	int xStart = (dstRect.left < destRect.left) ? dstRect.left - destRect.left : 0;
	int yStart = (dstRect.top < destRect.top) ? dstRect.top - destRect.top : 0;

	for (int destY = yStart, yCtr = 0; yCtr < dstRect.height(); ++destY, ++yCtr) {
		if (destY < 0 || destY >= destArea.h)
			continue;
		byte *destP = (byte *)destArea.getBasePtr(0, destY);
		const byte *srcP = (const byte *)src.getBasePtr(
			horizFlip ? srcRect.right - 1 : srcRect.left,
			vertFlip ? srcRect.bottom - 1 - yCtr :
			srcRect.top + yCtr);

		// Loop through the pixels of the row
		for (int destX = xStart, xCtr = 0, xCtrBpp = 0; xCtr < dstRect.width(); ++destX, ++xCtr, xCtrBpp += src.format.bytesPerPixel) {
			if (destX < 0 || destX >= destArea.w)
				continue;

			const byte *srcVal = srcP + xDir * xCtrBpp;
			uint32 srcCol = getColor(srcVal, src.format.bytesPerPixel);

			// Check if this is a transparent color we should skip
			if (skipTrans && ((srcCol & alphaMask) == transColor))
				continue;

			byte *destVal = (byte *)&destP[destX * format.bytesPerPixel];

			// When blitting to the same format we can just copy the color
			if (format.bytesPerPixel == 1) {
				*destVal = srcCol;
				continue;
			} else if (src.format.bytesPerPixel == 1)
				srcCol = palette[srcCol];

			if (srcAlpha == -1) {
				// Blit pixels (no blending)
				if (!sameFormat) {
					byte r, g, b, a;
					src.format.colorToARGB(srcCol, a, r, g, b);
					srcCol = format.ARGBToColor(a, r, g, b);
				}
				if (format.bytesPerPixel == 4)
					*(uint32 *)destVal = srcCol;
				else
					*(uint16 *)destVal = srcCol;
				continue;
			}

			uint32 destCol;
			if (useTint) {
				if (!sameFormat) {
					byte r, g, b, a;
					src.format.colorToARGB(srcCol, a, r, g, b);
					destCol = format.ARGBToColor(a, r, g, b);
				} else
					destCol = srcCol;
				srcCol = tintCol;
			} else
				destCol = getColor(destVal, format.bytesPerPixel);

			if (sameFormat || useTint)
				blendPixel(srcCol, destCol, srcAlpha);
			else
				blendPixel(srcCol, destCol, srcAlpha, src.format);

			if (format.bytesPerPixel == 4)
				*(uint32 *)destVal = destCol;
			else
				*(uint16 *)destVal = destCol;
		}
	}
}

void BITMAP::stretchDraw(const BITMAP *srcBitmap, const Common::Rect &srcRect,
		const Common::Rect &dstRect, bool skipTrans, int srcAlpha) {
	assert(format.bytesPerPixel == 2 || format.bytesPerPixel == 4 ||
		(format.bytesPerPixel == 1 && srcBitmap->format.bytesPerPixel == 1));

	// Allegro disables draw when the clipping rect has negative width/height.
	// Common::Rect instead asserts, which we don't want.
	if (cr <= cl || cb <= ct)
		return;

	// Figure out the dest area that will be updated
	Common::Rect destRect = dstRect.findIntersectingRect(
		Common::Rect(cl, ct, cr, cb));
	if (destRect.isEmpty())
		// Area is entirely outside the clipping area, so nothing to draw
		return;

	// Get source and dest surface. Note that for the destination we create
	// a temporary sub-surface based on the allowed clipping area
	const Graphics::ManagedSurface &src = **srcBitmap;
	Graphics::ManagedSurface &dest = *_owner;
	Graphics::Surface destArea = dest.getSubArea(destRect);

	// Define scaling and other stuff used by the drawing loops
	const int scaleX = SCALE_THRESHOLD * srcRect.width() / dstRect.width();
	const int scaleY = SCALE_THRESHOLD * srcRect.height() / dstRect.height();
	bool sameFormat = (src.format == format);

	uint32 palette[PAL_SIZE];
	if (src.format.bytesPerPixel == 1 && format.bytesPerPixel != 1) {
		for (int i = 0; i < PAL_SIZE; ++i) {
			palette[i] = format.RGBToColor(VGA_COLOR_TRANS(_G(current_palette)[i].r), VGA_COLOR_TRANS(_G(current_palette)[i].g), VGA_COLOR_TRANS(_G(current_palette)[i].b));
		}
		sameFormat = true;
	}

	uint32 transColor = 0, alphaMask = 0xff;
	if (skipTrans && src.format.bytesPerPixel != 1) {
		transColor = src.format.ARGBToColor(0, 255, 0, 255);
		alphaMask = src.format.ARGBToColor(255, 0, 0, 0);
		alphaMask = ~alphaMask;
	}

	int xStart = (dstRect.left < destRect.left) ? dstRect.left - destRect.left : 0;
	int yStart = (dstRect.top < destRect.top) ? dstRect.top - destRect.top : 0;

	for (int destY = yStart, yCtr = 0, scaleYCtr = 0; yCtr < dstRect.height();
			++destY, ++yCtr, scaleYCtr += scaleY) {
		if (destY < 0 || destY >= destArea.h)
			continue;
		byte *destP = (byte *)destArea.getBasePtr(0, destY);
		const byte *srcP = (const byte *)src.getBasePtr(
			srcRect.left, srcRect.top + scaleYCtr / SCALE_THRESHOLD);

		// Loop through the pixels of the row
		for (int destX = xStart, xCtr = 0, scaleXCtr = 0; xCtr < dstRect.width();
				++destX, ++xCtr, scaleXCtr += scaleX) {
			if (destX < 0 || destX >= destArea.w)
				continue;

			const byte *srcVal = srcP + scaleXCtr / SCALE_THRESHOLD * src.format.bytesPerPixel;
			uint32 srcCol = getColor(srcVal, src.format.bytesPerPixel);

			// Check if this is a transparent color we should skip
			if (skipTrans && ((srcCol & alphaMask) == transColor))
				continue;

			byte *destVal = (byte *)&destP[destX * format.bytesPerPixel];

			// When blitting to the same format we can just copy the color
			if (format.bytesPerPixel == 1) {
				*destVal = srcCol;
				continue;
			} else if (src.format.bytesPerPixel == 1)
				srcCol = palette[srcCol];

			if (srcAlpha == -1) {
				// Blit pixels (no blending)
				if (!sameFormat) {
					byte r, g, b, a;
					src.format.colorToARGB(srcCol, a, r, g, b);
					srcCol = format.ARGBToColor(a, r, g, b);
				}
				if (format.bytesPerPixel == 4)
					*(uint32 *)destVal = srcCol;
				else
					*(uint16 *)destVal = srcCol;
				continue;
			}

			uint32 destCol = getColor(destVal, format.bytesPerPixel);
			if (sameFormat)
				blendPixel(srcCol, destCol, srcAlpha);
			else
				blendPixel(srcCol, destCol, srcAlpha, src.format);

			if (format.bytesPerPixel == 4)
				*(uint32 *)destVal = destCol;
			else
				*(uint16 *)destVal = destCol;
		}
	}
}

void BITMAP::blendPixel(uint32 srcCol, uint32 &destCol, uint32 alpha) const {
	switch(_G(_blender_mode)) {
	case kSourceAlphaBlender:
		blendSourceAlpha(srcCol, destCol, alpha);
		break;
	case kArgbToArgbBlender:
		blendArgbToArgb(srcCol, destCol, alpha);
		break;
	case kArgbToRgbBlender:
		blendArgbToRgb(srcCol, destCol, alpha);
		break;
	case kRgbToArgbBlender:
		blendRgbToArgb(srcCol, destCol, alpha);
		break;
	case kRgbToRgbBlender:
		blendRgbToRgb(srcCol, destCol, alpha);
		break;
	case kAlphaPreservedBlenderMode:
		blendPreserveAlpha(srcCol, destCol, alpha);
		break;
	case kOpaqueBlenderMode:
		blendOpaque(srcCol, destCol, alpha);
		break;
	case kAdditiveBlenderMode:
		blendAdditiveAlpha(srcCol, destCol, alpha);
		break;
	case kTintBlenderMode:
		blendTintSprite(srcCol, destCol, alpha, false);
		break;
	case kTintLightBlenderMode:
		blendTintSprite(srcCol, destCol, alpha, true);
		break;
	}
}

void BITMAP::blendPixel(uint32 srcCol, uint32 &destCol, uint32 alpha, const Graphics::PixelFormat &srcFormat) const {
	switch(_G(_blender_mode)) {
	case kSourceAlphaBlender:
		blendSourceAlpha(srcCol, destCol, alpha, srcFormat);
		break;
	case kArgbToArgbBlender:
		blendArgbToArgb(srcCol, destCol, alpha, srcFormat);
		break;
	case kArgbToRgbBlender:
		blendArgbToRgb(srcCol, destCol, alpha, srcFormat);
		break;
	case kRgbToArgbBlender:
		blendRgbToArgb(srcCol, destCol, alpha, srcFormat);
		break;
	case kRgbToRgbBlender:
		blendRgbToRgb(srcCol, destCol, alpha, srcFormat);
		break;
	case kAlphaPreservedBlenderMode:
		blendPreserveAlpha(srcCol, destCol, alpha, srcFormat);
		break;
	case kOpaqueBlenderMode:
		blendOpaque(srcCol, destCol, alpha, srcFormat);
		break;
	case kAdditiveBlenderMode:
		blendAdditiveAlpha(srcCol, destCol, alpha, srcFormat);
		break;
	case kTintBlenderMode:
		blendTintSprite(srcCol, destCol, alpha, srcFormat, false);
		break;
	case kTintLightBlenderMode:
		blendTintSprite(srcCol, destCol, alpha, srcFormat, true);
		break;
	}
}

void BITMAP::blendTintSprite(uint32 srcCol, uint32 &destCol, uint32 alpha, const Graphics::PixelFormat &srcFormat, bool light) const {
	// Used from draw_lit_sprite after set_blender_mode(kTintBlenderMode or kTintLightBlenderMode)
	// Original blender function: _myblender_color32 and _myblender_color32_light
	uint8 rSrc, gSrc, bSrc;
	srcFormat.colorToRGB(srcCol, rSrc, gSrc, bSrc);
	uint8 aDest, rDest, gDest, bDest;
	format.colorToARGB(destCol, aDest, rDest, gDest, bDest);
	float xh, xs, xv;
	float yh, ys, yv;
	int r, g, b;
	rgb_to_hsv(rSrc, gSrc, bSrc, &xh, &xs, &xv);
	rgb_to_hsv(rDest, gDest, bDest, &yh, &ys, &yv);
	if (light) {
		// adjust luminance
		yv -= (1.0 - ((float)alpha / 250.0));
		if (yv < 0.0)
			yv = 0.0;
	}
	hsv_to_rgb(xh, xs, yv, &r, &g, &b);
	rDest = static_cast<uint8>(r & 0xff);
	gDest = static_cast<uint8>(g & 0xff);
	bDest = static_cast<uint8>(b & 0xff);
	// Preserve value in aDest
	destCol = format.ARGBToColor(aDest, rDest, gDest, bDest);
}

/*-------------------------------------------------------------------*/

/**
 * Dervied screen surface
 */
class Screen : public Graphics::Screen, public BITMAP {
public:
	Screen() : Graphics::Screen(), BITMAP(this) {}
	Screen(int width, int height) : Graphics::Screen(width, height), BITMAP(this) {}
	Screen(int width, int height, const Graphics::PixelFormat &pixelFormat) :
		Graphics::Screen(width, height, pixelFormat), BITMAP(this) {}
	~Screen() override {}
};

/*-------------------------------------------------------------------*/

BITMAP *create_bitmap(int width, int height) {
	return create_bitmap_ex(get_color_depth(), width, height);
}

BITMAP *create_bitmap_ex(int color_depth, int width, int height) {
	Graphics::PixelFormat format;

	switch (color_depth) {
	case 8:
		format = Graphics::PixelFormat::createFormatCLUT8();
		break;
	case 16:
		format = Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0);
		break;
	case 32:
		format = Graphics::PixelFormat(4, 8, 8, 8, 8, 16, 8, 0, 24);
		break;
	default:
		error("Invalid color depth");
	}

	BITMAP *bitmap = new Surface(width, height, format);
	return bitmap;
}

BITMAP *create_sub_bitmap(BITMAP *parent, int x, int y, int width, int height) {
	Graphics::ManagedSurface &surf = **parent;
	return new Surface(surf, Common::Rect(x, y, x + width, y + height));
}

BITMAP *create_video_bitmap(int width, int height) {
	return new Screen(width, height);
}

BITMAP *create_system_bitmap(int width, int height) {
	return create_bitmap(width, height);
}

void destroy_bitmap(BITMAP *bitmap) {
	delete bitmap;
}

} // namespace AGS3
