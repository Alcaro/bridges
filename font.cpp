#include "arlib.h"
#include "font.h"

void font::init_from_image(const image& img)
{
	memset(characters, 0, sizeof(characters));
	memset(width, 0, sizeof(uint8_t)*32);
	
	int cw = img.width/16;
	int ch = img.height/6;
	
	for (int cy=0;cy<6;cy++)
	for (int cx=0;cx<16;cx++)
	{
		uint8_t charid = 32 + cy*16 + cx;
		
		uint8_t maxbits = 1;
		for (int y=0;y<ch;y++)
		{
			int ty = cy*ch + y;
			uint32_t* pixels = img.pixels32 + img.stride/sizeof(uint32_t)*ty;
			
			uint8_t bits = 0;
			for (int x=0;x<cw;x++)
			{
				int tx = cx*cw + x;
				bits |= !(pixels[tx]&1) << x;
			}
			characters[charid][y] = bits;
			maxbits |= bits;
		}
		
		width[charid] = ilog2(maxbits)+2; // +1 because maxbits=1 has width 1 but log2(1)=0, +1 because letter spacing
	}
	
	height = ch;
	scale = 1;
}

uint32_t font::measure(cstring text, float spacesize)
{
	float ret = 0;
	for (size_t i=0;i<text.length();i++)
	{
		if (text[i] == ' ') ret += spacesize;
		ret += width[(uint8_t)text[i]] * scale;
	}
	return ret;
}

uint32_t font::render(image& img, int32_t x, int32_t y, cstring text, float xspace, bool align) const
{
	uint32_t color;
	if (img.fmt == ifmt_0rgb8888) color = this->color & 0x00FFFFFF;
	else color = this->color | 0xFF000000;
	
	uint32_t maxwidth = 0;
	
	float xoff = 0;
	for (size_t i=0;i<text.length();i++)
	{
		uint8_t ch = text[i];
		int32_t xstart = x + (align ? ((int)xoff/this->scale)*this->scale : (int)xoff);
		
		if (ch == '\n')
		{
			y += this->height * this->scale;
			xoff = 0;
			continue;
		}
		else if (ch <= 0x1F)
		{
			this->fallback(img, *this, xstart, y, ch);
			//refresh the color, in case the fallback changed it
			if (img.fmt == ifmt_0rgb8888) color = this->color & 0x00FFFFFF;
			else color = this->color | 0xFF000000;
		}
		else
		{
			for (size_t yp=0;yp<8;yp++)
			{
				for (size_t yps=0;yps<this->scale;yps++)
				{
					uint32_t yt = y + yp*this->scale + yps;
					if (yt < 0 || yt >= img.height) continue;
					
					uint32_t* targetpx = img.pixels32 + yt*img.stride/sizeof(uint32_t);
					
					for (size_t xp=0;xp<8;xp++)
					{
						if (this->characters[ch][yp] & (1<<xp))
						{
							for (size_t xps=0;xps<this->scale;xps++)
							{
								int32_t xt = xstart + xp*this->scale + xps;
								if (xt < 0 || (uint32_t)xt >= img.width) continue;
								
								targetpx[xt] = color;
							}
						}
					}
				}
			}
		}
		
		xoff += this->width[ch] * this->scale;
		if (ch == ' ') xoff += xspace;
		
		//-scale to remove the inter-letter spacing
		maxwidth = max(maxwidth, xoff-this->scale);
	}
	
	return maxwidth;
}

void font::render_justified(image& img, int32_t x, int32_t y, uint32_t width1, uint32_t width2, cstring text, bool align) const
{
	size_t linestart = 0;
	size_t numspaces = 0;
	
	uint32_t linewidth = 0;
	for (size_t i=0;i<=text.length();i++)
	{
		if (i == text.length() || text[i] == '\n')
		{
			linewidth *= this->scale;
			
			float spacesize;
			if (linewidth >= width1 && linewidth <= width2) spacesize = ((width2 - linewidth) / (float)numspaces);
			else spacesize = 0;
			
			this->render(img, x, y, text.substr(linestart, i), spacesize, align);
			
			y += this->height * this->scale;
			linestart = i+1;
			linewidth = 0;
			numspaces = 0;
			continue;
		}
		
		if (text[i] == ' ') numspaces++;
		linewidth += this->width[(uint8_t)text[i]];
	}
}

void font::render_wrap(image& img, int32_t x, int32_t y, uint32_t width, cstring text) const
{
	size_t linestart = 0;
	size_t numspaces = 0;
	
	size_t lastspace = 0; // index to the string
	uint32_t widthlastspace = 0; // text width (pixels) from linestart to lastspace
	
	uint32_t linewidth = 0; // text width (pixels) from linestart to i
	
	for (size_t i=0;i<=text.length();i++)
	{
		if (i == text.length() || text[i] == '\n' || linewidth >= width)
		{
			float spacesize = 0;
			if (linewidth >= width)
			{
				i = lastspace;
				linewidth = widthlastspace;
				spacesize = ((width - linewidth) / (float)(numspaces-1));
			}
			
			this->render(img, x, y, text.substr(linestart, i), spacesize, true);
			
			y += this->height * this->scale;
			linestart = i+1;
			linewidth = 0;
			numspaces = 0;
			continue;
		}
		
		if (text[i] == ' ')
		{
			lastspace = i;
			widthlastspace = linewidth;
			numspaces++;
		}
		linewidth += this->width[(uint8_t)text[i]] * this->scale;
	}
}
