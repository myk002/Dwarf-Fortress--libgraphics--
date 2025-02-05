#include "enabler.h"
#include "init.h"
#include "resize++.h"
//#include "ttf_manager.hpp"

#include <iostream>
using namespace std;

#ifndef FULL_RELEASE_VERSION
extern bool cinematic_mode;
extern int32_t cinematic_shift_x;
extern int32_t cinematic_shift_y;
extern int32_t cinematic_shift_dx;
extern int32_t cinematic_shift_dy;
extern int32_t cinematic_shift_velx;
extern int32_t cinematic_shift_vely;
extern int32_t cinematic_start_scrollx;
extern int32_t cinematic_start_scrolly;
#endif

#ifndef FULL_RELEASE_VERSION
#define DEBUG_CONTROLS
#endif

void report_error(const char*, const char*);

class renderer_2d_base : public renderer {
protected:
  SDL_Surface *screen;
  map<texture_fullid, SDL_Surface*> tile_cache;
  int dispx, dispy, dimx, dimy;
  // We may shrink or enlarge dispx/dispy in response to zoom requests. dispx/y_z are the
  // size we actually display tiles at.
  int dispx_z, dispy_z;
  // Viewport origin
  int origin_x, origin_y;

	bool use_viewport_zoom;
	int32_t viewport_zoom_factor;

	virtual void set_viewport_zoom_factor(int32_t nfactor){viewport_zoom_factor=nfactor;}

  SDL_Surface *tile_cache_lookup(texture_fullid &id) {

    map<texture_fullid, SDL_Surface*>::iterator it = tile_cache.find(id);
    if (it != tile_cache.end()) {
      return it->second;
    } else {
      // Create the colorized texture
      SDL_Surface *tex   = enabler.textures.get_texture_data(id.texpos);
	  if(tex==NULL)return NULL;
      SDL_Surface *color;
	  //***************************** BLIT BACKGROUND going to alpha
	  /*
      color = SDL_CreateRGBSurface(SDL_SWSURFACE,
                                   tex->w, tex->h,
                                   tex->format->BitsPerPixel,
                                   tex->format->Rmask,
                                   tex->format->Gmask,
                                   tex->format->Bmask,
                                   0);
								   */
      color = SDL_CreateRGBSurface(SDL_SWSURFACE,
                                   tex->w, tex->h,
                                   32,
                                   tex->format->Rmask,
                                   tex->format->Gmask,
                                   tex->format->Bmask,
                                   tex->format->Amask);
      if (!color) {
        MessageBox (NULL, "Unable to create texture!", "Fatal error", MB_OK | MB_ICONEXCLAMATION);
        abort();
      }
      
      // Fill it
      Uint32 color_fgi = SDL_MapRGB(color->format, (Uint8)(id.r*255), (Uint8)(id.g*255), (Uint8)(id.b*255));
      Uint8 *color_fg = (Uint8*) &color_fgi;
      Uint32 color_bgi = SDL_MapRGB(color->format, (Uint8)(id.br*255), (Uint8)(id.bg*255), (Uint8)(id.bb*255));
      Uint8 *color_bg = (Uint8*) &color_bgi;
      SDL_LockSurface(tex);
      SDL_LockSurface(color);

	  bool norecolor=(id.r==1.0f&&id.g==1.0f&&id.b==1.0f);
      
      Uint8 *pixel_src, *pixel_dst;
      for (int y = 0; y < tex->h; y++) {
        pixel_src = ((Uint8*)tex->pixels) + (y * tex->pitch);
        pixel_dst = ((Uint8*)color->pixels) + (y * color->pitch);
        for (int x = 0; x < tex->w; x++, pixel_src+=4, pixel_dst+=4) {
          float alpha = pixel_src[3] * (1 / 255.0f);
          for (int c = 0; c < 3; c++) {
		  if(id.flag & TEXTURE_FULLID_FLAG_DO_RECOLOR)
			{
            float fg = color_fg[c] * (1 / 255.0f), bg = color_bg[c] * (1 / 255.0f), tex = pixel_src[c] * (1 / 255.0f);
            pixel_dst[c] = (Uint8)(((alpha * (tex * fg)) + ((1 - alpha) * bg)) * 255);
			}
		  else if(norecolor)
			{
			pixel_dst[c] = pixel_src[c];
			}
		  else
		  {
			//********************** OVERLAY ALPHA BG
				//do we need to use alpha or bg?
            float fg = color_fg[c] * (1 / 255.0f), bg = color_bg[c] * (1 / 255.0f), tex = pixel_src[c] * (1 / 255.0f);

			//overlay shading
			//************************** FIX RAMP SHADING
			if(tex<0.5f)
				{
				pixel_dst[c]=(Uint8)((2.0f*tex*fg)*255);
				}
			else
				{
				pixel_dst[c]=(Uint8)((1.0f - 2.0f*(1.0f-tex)*(1.0f-fg))*255);
				}
		  }
          }

		  if(!(id.flag & TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND))
			{
			pixel_dst[3]=255;
			}
		  else
		  {
		  if(id.br!=0||id.bb!=0||id.bg!=0)pixel_dst[3]=255;
		  else pixel_dst[3]=pixel_src[3];
		}
        }
      }
      
      SDL_UnlockSurface(color);
      SDL_UnlockSurface(tex);
	  
	if(use_viewport_zoom)
		{
		SDL_Surface *disp = SDL_Resize(color, viewport_zoom_factor*tex->w/128, viewport_zoom_factor*tex->h/128);
		tile_cache[id] = disp;
		return disp;
		}

	SDL_Surface *disp = (id.flag & TEXTURE_FULLID_FLAG_CONVERT)?
		//SDL_Resize(color, dispx_z, dispy_z):// Convert to display format; deletes color
		/*
    double try_x = dispx, try_y = dispy;
    try_x = screen->w / w;
    try_y = MIN(try_x / dispx * dispy, screen->h / h);
    try_x = MIN(try_x, try_y / dispy * dispx);
    dispx_z = (int)(MAX(1,try_x)); dispy_z = (int)(MAX(try_y,1));
    cout << "Resizing font to " << dispx_z << "x" << dispy_z << endl;
		*/
		SDL_Resize(color, dispx_z*tex->w/dispx, dispy_z*tex->h/dispy):// Convert to display format; deletes color
		color;// color is not deleted, but we don't want it to be.
	// Insert and return
	tile_cache[id] = disp;

	return disp;
    }
  }
  
  virtual bool init_video(int w, int h) {
    // Get ourselves a 2D SDL window
    Uint32 flags = init.display.flag.has_flag(INIT_DISPLAY_FLAG_2DHW) ? SDL_HWSURFACE : SDL_SWSURFACE;
    flags |= init.display.flag.has_flag(INIT_DISPLAY_FLAG_2DASYNC) ? SDL_ASYNCBLIT : 0;

    // Set it up for windowed or fullscreen, depending.
    if (enabler.is_fullscreen()) { 
      flags |= SDL_FULLSCREEN;
    } else {
      if (!init.display.flag.has_flag(INIT_DISPLAY_FLAG_NOT_RESIZABLE))
        flags |= SDL_RESIZABLE;
    }

    // (Re)create the window
    screen = SDL_SetVideoMode(w, h, 32, flags);
    if (screen == NULL) cout << "INIT FAILED!" << endl;

    return screen != NULL;
  }
  
public:
  list<pair<SDL_Surface*,SDL_Rect> > ttfs_to_render;

  void update_anchor_tile(int x,int y) {
    // Figure out where to blit
    SDL_Rect dst;
    dst.x = dispx_z * x + origin_x;
    dst.y = dispy_z * y + origin_y;

	int32_t ltp=0;
	if(init.display.flag.has_flag(INIT_DISPLAY_FLAG_USE_GRAPHICS))
		{
		int32_t atp=gps.screentexpos_anchored[x * gps.dimy + y];
		if(atp!=0)
			{
			if(!(gps.screentexpos_flag[x * gps.dimy + y] & SCREENTEXPOS_FLAG_ANCHOR_SUBORDINATE))
				{
				dst.x+=gps.screentexpos_anchored_x[x * gps.dimy + y];
				dst.y+=gps.screentexpos_anchored_y[x * gps.dimy + y];

				texture_fullid background_tex;
					background_tex.texpos=atp;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					background_tex.flag|=TEXTURE_FULLID_FLAG_CONVERT;
					if(gps.screentexpos_flag[x * gps.dimy + y] & SCREENTEXPOS_FLAG_ANCHOR_USE_SCREEN_COLOR)
						{
						int32_t ind=(x*gps.dimy+y)*8;
						background_tex.r=gps.screen[ind+1]/255.0f;
						background_tex.g=gps.screen[ind+2]/255.0f;
						background_tex.b=gps.screen[ind+3]/255.0f;
						background_tex.br=gps.screen[ind+4]/255.0f;
						background_tex.bg=gps.screen[ind+5]/255.0f;
						background_tex.bb=gps.screen[ind+6]/255.0f;
						background_tex.flag|=TEXTURE_FULLID_FLAG_DO_RECOLOR;
						}
					else
						{
						background_tex.r=1.0f;
						background_tex.g=1.0f;
						background_tex.b=1.0f;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						}
					SDL_Surface *tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			}
		}
	}

  void update_top_anchor_tile(int x,int y) {
    // Figure out where to blit
    SDL_Rect dst;
    dst.x = dispx_z * x + origin_x;
    dst.y = dispy_z * y + origin_y;

	int32_t ltp=0;
	if(init.display.flag.has_flag(INIT_DISPLAY_FLAG_USE_GRAPHICS))
		{
		int32_t atp=gps.screentexpos_top_anchored[x * gps.dimy + y];
		if(atp!=0)
			{
			if(!(gps.screentexpos_top_flag[x * gps.dimy + y] & SCREENTEXPOS_FLAG_ANCHOR_SUBORDINATE))
				{
				dst.x+=gps.screentexpos_top_anchored_x[x * gps.dimy + y];
				dst.y+=gps.screentexpos_top_anchored_y[x * gps.dimy + y];

				texture_fullid background_tex;
					background_tex.texpos=atp;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					background_tex.flag|=TEXTURE_FULLID_FLAG_CONVERT;
					if(gps.screentexpos_top_flag[x * gps.dimy + y] & SCREENTEXPOS_FLAG_ANCHOR_USE_SCREEN_COLOR)
						{
						int32_t ind=(x*gps.dimy+y)*8;
						background_tex.r=gps.screen_top[ind+1]/255.0f;
						background_tex.g=gps.screen_top[ind+2]/255.0f;
						background_tex.b=gps.screen_top[ind+3]/255.0f;
						background_tex.br=gps.screen_top[ind+4]/255.0f;
						background_tex.bg=gps.screen_top[ind+5]/255.0f;
						background_tex.bb=gps.screen_top[ind+6]/255.0f;
						background_tex.flag|=TEXTURE_FULLID_FLAG_DO_RECOLOR;
						}
					else
						{
						background_tex.r=1.0f;
						background_tex.g=1.0f;
						background_tex.b=1.0f;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						}
					SDL_Surface *tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			}
		}
	}

  void update_tile(int x, int y) {
    // Figure out where to blit
    SDL_Rect dst;
    dst.x = dispx_z * x + origin_x;
    dst.y = dispy_z * y + origin_y;

	int32_t ltp=0;
	if(init.display.flag.has_flag(INIT_DISPLAY_FLAG_USE_GRAPHICS))
		{
		ltp=gps.screentexpos_lower[x * gps.dimy + y];
		if(ltp!=0)
			{
			texture_fullid background_tex;
				background_tex.texpos=ltp;
				background_tex.r=1.0f;
				background_tex.g=1.0f;
				background_tex.b=1.0f;
				background_tex.br=0;
				background_tex.bg=0;
				background_tex.bb=0;
				background_tex.flag=0;
				background_tex.flag|=TEXTURE_FULLID_FLAG_CONVERT;
				SDL_Surface *tex = tile_cache_lookup(background_tex);
				if(tex!=NULL)
					{
					SDL_SetAlpha(tex, 0, 0);//this appears to stop the ghosts from appearing! (first 0 defaults to SDL_SRCALPHA)
					SDL_BlitSurface(tex, NULL, screen, &dst);
					}
			}

		//lower strip of black squares
		if(y==gps.dimy-1)
			{
			SDL_Rect ldst;
			ldst.x = dispx_z * x + origin_x;
			ldst.y = dispy_z * (y+1) + origin_y;

			texture_fullid background_tex;
				if(enabler.flag & ENABLERFLAG_BASIC_TEXT)
					{
					background_tex.texpos=init.font.basic_font_texpos[' '];
					}
				else background_tex.texpos=enabler.is_fullscreen() ?
								init.font.large_font_texpos[' '] :
								init.font.small_font_texpos[' '];
				background_tex.r=1.0f;
				background_tex.g=1.0f;
				background_tex.b=1.0f;
				background_tex.br=0;
				background_tex.bg=0;
				background_tex.bb=0;
				background_tex.flag=0;
				background_tex.flag|=TEXTURE_FULLID_FLAG_CONVERT;
				SDL_Surface *tex = tile_cache_lookup(background_tex);
				if(tex!=NULL)
					{
					SDL_SetAlpha(tex, 0, 0);//this appears to stop the ghosts from appearing! (first 0 defaults to SDL_SRCALPHA)
					SDL_BlitSurface(tex, NULL, screen, &ldst);
					}
			}
		//right strip of black squares
		if(x==gps.dimx-1)
			{
			SDL_Rect ldst;
			ldst.x = dispx_z * (x+1) + origin_x;
			ldst.y = dispy_z * y + origin_y;

			texture_fullid background_tex;
				if(enabler.flag & ENABLERFLAG_BASIC_TEXT)
					{
					background_tex.texpos=init.font.basic_font_texpos[' '];
					}
				else
					{
					background_tex.texpos=enabler.is_fullscreen() ?
									init.font.large_font_texpos[' '] :
									init.font.small_font_texpos[' '];
					}
				background_tex.r=1.0f;
				background_tex.g=1.0f;
				background_tex.b=1.0f;
				background_tex.br=0;
				background_tex.bg=0;
				background_tex.bb=0;
				background_tex.flag=0;
				background_tex.flag|=TEXTURE_FULLID_FLAG_CONVERT;
				SDL_Surface *tex = tile_cache_lookup(background_tex);
				if(tex!=NULL)
					{
					SDL_SetAlpha(tex, 0, 0);//this appears to stop the ghosts from appearing! (first 0 defaults to SDL_SRCALPHA)
					SDL_BlitSurface(tex, NULL, screen, &ldst);
					}

			//lower right corner
			if(y==gps.dimy-1)
				{
				SDL_Rect ldst;
				ldst.x = dispx_z * (x+1) + origin_x;
				ldst.y = dispy_z * (y+1) + origin_y;

				texture_fullid background_tex;
					if(enabler.flag & ENABLERFLAG_BASIC_TEXT)
						{
						background_tex.texpos=init.font.basic_font_texpos[' '];
						}
					else
						{
						background_tex.texpos=enabler.is_fullscreen() ?
										init.font.large_font_texpos[' '] :
										init.font.small_font_texpos[' '];
						}
					background_tex.r=1.0f;
					background_tex.g=1.0f;
					background_tex.b=1.0f;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=0;
					background_tex.flag|=TEXTURE_FULLID_FLAG_CONVERT;
					SDL_Surface *tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, 0, 0);//this appears to stop the ghosts from appearing! (first 0 defaults to SDL_SRCALPHA)
						SDL_BlitSurface(tex, NULL, screen, &ldst);
						}
				}
			}
		}

	const int tile = x * gps.dimy + y;
	const unsigned char *s = gps.screen + tile*8;
	if(s[0]==0&&gps.screentexpos[x * gps.dimy + y]==0)return;

	// Read tiles from gps, create cached texture
	Either<texture_fullid,int32_t/*texture_ttfid*/> id = screen_to_texid(x, y);
	SDL_Surface *tex;
	if (id.isL) {      // Ordinary tile, cached here
		id.left.flag=TEXTURE_FULLID_FLAG_DO_RECOLOR;
		id.left.flag|=TEXTURE_FULLID_FLAG_CONVERT;
		if(ltp!=0&&s[4]==0&&s[5]==0&&s[6]==0)id.left.flag|=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
		tex = tile_cache_lookup(id.left);
		// And blit.
		if(tex!=NULL)
		{
		SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
		SDL_BlitSurface(tex, NULL, screen, &dst);
		}
		
	} /*else {  // TTF, cached in ttf_manager so no point in also caching here
		tex = ttf_manager.get_texture(id.right);
		// Blit later
		ttfs_to_render.push_back(make_pair(tex, dst));
	}
	*/
}

  void update_top_tile(int x, int y) {
    // Figure out where to blit
    SDL_Rect dst;
    dst.x = dispx_z * x + origin_x;
    dst.y = dispy_z * y + origin_y;

	int32_t ltp=0;
	if(init.display.flag.has_flag(INIT_DISPLAY_FLAG_USE_GRAPHICS))
		{
		ltp=gps.screentexpos_top_lower[x * gps.dimy + y];
		if(ltp!=0)
			{
			texture_fullid background_tex;
				background_tex.texpos=ltp;
				background_tex.r=1.0f;
				background_tex.g=1.0f;
				background_tex.b=1.0f;
				background_tex.br=0;
				background_tex.bg=0;
				background_tex.bb=0;
				background_tex.flag=0;
				background_tex.flag|=TEXTURE_FULLID_FLAG_CONVERT;
				SDL_Surface *tex = tile_cache_lookup(background_tex);
				if(tex!=NULL)
					{
					SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
					SDL_BlitSurface(tex, NULL, screen, &dst);
					}
			}
		}

	const int tile = x * gps.dimy + y;
	const unsigned char *s = gps.screen_top + tile*8;
	if(s[0]==0&&gps.screentexpos_top[x * gps.dimy + y]==0)return;

	// Read tiles from gps, create cached texture
	Either<texture_fullid,int32_t/*texture_ttfid*/> id = screen_top_to_texid(x, y);
	SDL_Surface *tex;
	if (id.isL) {      // Ordinary tile, cached here
		id.left.flag=TEXTURE_FULLID_FLAG_DO_RECOLOR;
		id.left.flag|=TEXTURE_FULLID_FLAG_CONVERT;
		if(/*ltp!=0&&*/s[4]==0&&s[5]==0&&s[6]==0)id.left.flag|=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
		tex = tile_cache_lookup(id.left);
		// And blit.
		if(tex!=NULL)
		{
		SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
		SDL_BlitSurface(tex, NULL, screen, &dst);
		}
	} /*else {  // TTF, cached in ttf_manager so no point in also caching here
		tex = ttf_manager.get_texture(id.right);
		// Blit later
		ttfs_to_render.push_back(make_pair(tex, dst));
	}
	*/
}

	void update_map_port_tile(graphic_map_portst *vp,int32_t x,int32_t y)
		{
		// Figure out where to blit
		SDL_Rect dst;
		//********************************** TILE WIDTH
		//dst.x = dispx_z * x + origin_x;
		//dst.y = dispy_z * y + origin_y;
			//*************************** TEXTURE SIZE DEPENDENCE
			dst.x = 16 * x + origin_x + vp->top_left_corner_x;
			dst.y = 16 * y + origin_y + vp->top_left_corner_y;
		/*
		//*************************** TEXTURE SIZE DEPENDENCE
		SDL_Rect dst_nw;
			dst_nw.x = 16 * x + origin_x + vp->top_left_corner_x;
			dst_nw.y = 16 * y + origin_y + vp->top_left_corner_y;
		//*************************** TEXTURE SIZE DEPENDENCE
		SDL_Rect dst_ne;
			dst_ne.x = 16 * x + origin_x + vp->top_left_corner_x+8;
			dst_ne.y = 16 * y + origin_y + vp->top_left_corner_y;
		//*************************** TEXTURE SIZE DEPENDENCE
		SDL_Rect dst_sw;
			dst_sw.x = 16 * x + origin_x + vp->top_left_corner_x;
			dst_sw.y = 16 * y + origin_y + vp->top_left_corner_y+8;
		//*************************** TEXTURE SIZE DEPENDENCE
		SDL_Rect dst_se;
			dst_se.x = 16 * x + origin_x + vp->top_left_corner_x+8;
			dst_se.y = 16 * y + origin_y + vp->top_left_corner_y+8;
		*/
		// Read tiles from gps, create cached texture
		//Either<texture_fullid,texture_ttfid> id = screen_to_texid(x, y);
		SDL_Surface *tex;
    /*if (id.isL)*/ {      // Ordinary tile, cached here
		if(init.display.flag.has_flag(INIT_DISPLAY_FLAG_USE_GRAPHICS))
			{
			if(vp->screentexpos_base[x + y * vp->dim_x]!=0)
				{
				int32_t tp=vp->screentexpos_base[x + y * vp->dim_x];

				texture_fullid background_tex;
					background_tex.texpos=tp;
					background_tex.r=1.0f;
					background_tex.g=1.0f;
					background_tex.b=1.0f;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, 0, 0);//this appears to stop the ghosts from appearing! (first 0 defaults to SDL_SRCALPHA)
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			int32_t ei;
			for(ei=7;ei>=0;--ei)
				{
				if(vp->screentexpos_edge2[ei][x + y * vp->dim_x]!=0)
					{
					int32_t tp=vp->screentexpos_edge2[ei][x + y * vp->dim_x];

					texture_fullid background_tex;
						background_tex.texpos=tp;
						background_tex.r=1.0f;
						background_tex.g=1.0f;
						background_tex.b=1.0f;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				}	
			for(ei=7;ei>=0;--ei)
				{
				if(vp->screentexpos_edge[ei][x + y * vp->dim_x]!=0)
					{
					int32_t tp=vp->screentexpos_edge[ei][x + y * vp->dim_x];

					texture_fullid background_tex;
						background_tex.texpos=tp;
						background_tex.r=1.0f;
						background_tex.g=1.0f;
						background_tex.b=1.0f;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				}
			if(vp->screentexpos_detail_to_nw[x + y * vp->dim_x]!=0)
				{
				int32_t tp=vp->screentexpos_detail_to_nw[x + y * vp->dim_x];

				texture_fullid background_tex;
					background_tex.texpos=tp;
					background_tex.r=1.0f;
					background_tex.g=1.0f;
					background_tex.b=1.0f;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_detail_to_n[x + y * vp->dim_x]!=0)
				{
				int32_t tp=vp->screentexpos_detail_to_n[x + y * vp->dim_x];

				texture_fullid background_tex;
					background_tex.texpos=tp;
					background_tex.r=1.0f;
					background_tex.g=1.0f;
					background_tex.b=1.0f;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_detail_to_ne[x + y * vp->dim_x]!=0)
				{
				int32_t tp=vp->screentexpos_detail_to_ne[x + y * vp->dim_x];

				texture_fullid background_tex;
					background_tex.texpos=tp;
					background_tex.r=1.0f;
					background_tex.g=1.0f;
					background_tex.b=1.0f;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_detail_to_w[x + y * vp->dim_x]!=0)
				{
				int32_t tp=vp->screentexpos_detail_to_w[x + y * vp->dim_x];

				texture_fullid background_tex;
					background_tex.texpos=tp;
					background_tex.r=1.0f;
					background_tex.g=1.0f;
					background_tex.b=1.0f;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_detail[x + y * vp->dim_x]!=0)
				{
				int32_t tp=vp->screentexpos_detail[x + y * vp->dim_x];

				texture_fullid background_tex;
					background_tex.texpos=tp;
					background_tex.r=1.0f;
					background_tex.g=1.0f;
					background_tex.b=1.0f;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_detail_to_e[x + y * vp->dim_x]!=0)
				{
				int32_t tp=vp->screentexpos_detail_to_e[x + y * vp->dim_x];

				texture_fullid background_tex;
					background_tex.texpos=tp;
					background_tex.r=1.0f;
					background_tex.g=1.0f;
					background_tex.b=1.0f;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_detail_to_sw[x + y * vp->dim_x]!=0)
				{
				int32_t tp=vp->screentexpos_detail_to_sw[x + y * vp->dim_x];

				texture_fullid background_tex;
					background_tex.texpos=tp;
					background_tex.r=1.0f;
					background_tex.g=1.0f;
					background_tex.b=1.0f;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_detail_to_s[x + y * vp->dim_x]!=0)
				{
				int32_t tp=vp->screentexpos_detail_to_s[x + y * vp->dim_x];

				texture_fullid background_tex;
					background_tex.texpos=tp;
					background_tex.r=1.0f;
					background_tex.g=1.0f;
					background_tex.b=1.0f;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_detail_to_se[x + y * vp->dim_x]!=0)
				{
				int32_t tp=vp->screentexpos_detail_to_se[x + y * vp->dim_x];

				texture_fullid background_tex;
					background_tex.texpos=tp;
					background_tex.r=1.0f;
					background_tex.g=1.0f;
					background_tex.b=1.0f;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_tunnel[x + y * vp->dim_x]!=0)
				{
				int32_t tp=vp->screentexpos_tunnel[x + y * vp->dim_x];

				texture_fullid background_tex;
					background_tex.texpos=tp;
					background_tex.r=1.0f;
					background_tex.g=1.0f;
					background_tex.b=1.0f;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			//***************************** RIVER OCEAN LAKE
				//here?
			if(vp->screentexpos_river[x + y * vp->dim_x]!=0)
				{
				int32_t tp=vp->screentexpos_river[x + y * vp->dim_x];

				texture_fullid background_tex;
					background_tex.texpos=tp;
					background_tex.r=1.0f;
					background_tex.g=1.0f;
					background_tex.b=1.0f;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_road[x + y * vp->dim_x]!=0)
				{
				int32_t tp=vp->screentexpos_road[x + y * vp->dim_x];

				texture_fullid background_tex;
					background_tex.texpos=tp;
					background_tex.r=1.0f;
					background_tex.g=1.0f;
					background_tex.b=1.0f;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_site[x + y * vp->dim_x]!=0)
				{
				int32_t tp=vp->screentexpos_site[x + y * vp->dim_x];

				texture_fullid background_tex;
					background_tex.texpos=tp;
					background_tex.r=1.0f;
					background_tex.g=1.0f;
					background_tex.b=1.0f;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}

			if(vp->screentexpos_interface[x + y * vp->dim_x]!=0)
				{
				int32_t tp=vp->screentexpos_interface[x + y * vp->dim_x];

				texture_fullid background_tex;
					background_tex.texpos=tp;
					background_tex.r=1.0f;
					background_tex.g=1.0f;
					background_tex.b=1.0f;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}

			if(x>=0&&y>=0&&dispx_z>0&&dispy_z>0)//just in case
				{
				//************************** TEXTURE SIZE DEPENDENCE
					//in particular, the dy having a +2 is to get rid of some tearing on the border, need to figure that one out
				
				int32_t tx=16,ty=16;
				int32_t sx=x*tx/dispx_z;
				int32_t sy=y*ty/dispy_z;
				int32_t dx=tx/dispx_z+1;
				int32_t dy=ty/dispy_z+2;
				sx-=1;sy-=1;dx+=2;dy+=2;

				int32_t gx,gy;
				for(gx=sx;gx<sx+dx;++gx)
					{
					for(gy=sy;gy<sy+dy;++gy)
						{
						if(gx>=0&&gx<gps.dimx&&gy>=0&&gy<gps.dimy)
							{
							gps.screentexpos_refresh_buffer[gx * gps.dimy + gy]=gps.refresh_buffer_val;

							if(gps.screentexpos_anchored[gx * gps.dimy + gy]!=0)
								{
								uint32_t tf=gps.screentexpos_flag[gx * gps.dimy + gy];
								if(tf & SCREENTEXPOS_FLAG_ANCHOR_SUBORDINATE)
									{
									int32_t off_x=((tf & SCREENTEXPOS_FLAG_ANCHOR_X_COORD)>>SCREENTEXPOS_FLAG_ANCHOR_X_COORD_SHIFT);
									int32_t off_y=((tf & SCREENTEXPOS_FLAG_ANCHOR_Y_COORD)>>SCREENTEXPOS_FLAG_ANCHOR_Y_COORD_SHIFT);
									if(gx-off_x>=0&&gx-off_x<gps.dimx&&gy-off_y>=0&&gy-off_y<gps.dimy)
										{
										gps.screentexpos_refresh_buffer[(gx-off_x) * gps.dimy + gy-off_y]=gps.refresh_buffer_val;
										}
									}
								}
							if(gps.screentexpos_top_anchored[gx * gps.dimy + gy]!=0)
								{
								uint32_t tf=gps.screentexpos_top_flag[gx * gps.dimy + gy];
								if(tf & SCREENTEXPOS_FLAG_ANCHOR_SUBORDINATE)
									{
									int32_t off_x=((tf & SCREENTEXPOS_FLAG_ANCHOR_X_COORD)>>SCREENTEXPOS_FLAG_ANCHOR_X_COORD_SHIFT);
									int32_t off_y=((tf & SCREENTEXPOS_FLAG_ANCHOR_Y_COORD)>>SCREENTEXPOS_FLAG_ANCHOR_Y_COORD_SHIFT);
									if(gx-off_x>=0&&gx-off_x<gps.dimx&&gy-off_y>=0&&gy-off_y<gps.dimy)
										{
										gps.screentexpos_refresh_buffer[(gx-off_x) * gps.dimy + gy-off_y]=gps.refresh_buffer_val;
										}
									}
								}
							}
						}
					}
				}
			}

      //tex = tile_cache_lookup(id.left);
      // And blit.
      //if(tex!=NULL)SDL_BlitSurface(tex, NULL, screen, &dst);
    }/*else {  // TTF, cached in ttf_manager so no point in also caching here
      tex = ttf_manager.get_texture(id.right);
      // Blit later
      ttfs_to_render.push_back(make_pair(tex, dst));
    }*/
		}

	void update_viewport_tile(graphic_viewportst *vp,int32_t x,int32_t y)
		{
		// Figure out where to blit
		SDL_Rect dst;
		//********************************** TILE WIDTH
		//dst.x = dispx_z * x + origin_x;
		//dst.y = dispy_z * y + origin_y;
			//*************************** TEXTURE SIZE DEPENDENCE
			dst.x=32*x+origin_x;
			dst.y=32*y+origin_y;

#ifndef FULL_RELEASE_VERSION
if(cinematic_mode)
	{
	dst.x+=cinematic_shift_x+32;
	dst.y+=cinematic_shift_y+32;
	}
#endif

			if(viewport_zoom_factor!=128)
				{
				use_viewport_zoom=true;

#ifndef FULL_RELEASE_VERSION
if(cinematic_mode)
	{
				dst.x=(viewport_zoom_factor*(32*x+cinematic_shift_x+32))/128+origin_x;
				dst.y=(viewport_zoom_factor*(32*y+cinematic_shift_y+32))/128+origin_y;
	}
else
	{
#endif
				dst.x=(viewport_zoom_factor*32*x)/128+origin_x;
				dst.y=(viewport_zoom_factor*32*y)/128+origin_y;
#ifndef FULL_RELEASE_VERSION
	}
#endif
				}

		// Read tiles from gps, create cached texture
		//Either<texture_fullid,texture_ttfid> id = screen_to_texid(x, y);
		SDL_Surface *tex;
    /*if (id.isL)*/ {      // Ordinary tile, cached here
		if(init.display.flag.has_flag(INIT_DISPLAY_FLAG_USE_GRAPHICS))
			{
			uint64_t rf=vp->screentexpos_ramp_flag[x * vp->dim_y + y];
			uint64_t ff=vp->screentexpos_floor_flag[x * vp->dim_y + y];
			uint32_t spfl=vp->screentexpos_spatter_flag[x * vp->dim_y + y];
			int32_t fire_frame=-1;
			if(spfl & VIEWPORT_SPATTER_FLAG_FIRE_BITS)
				{
				uint32_t fire_bit=(spfl & VIEWPORT_SPATTER_FLAG_FIRE_BITS);
				if(fire_bit==VIEWPORT_SPATTER_FLAG_FIRE_FRAME_1)fire_frame=0;
				if(fire_bit==VIEWPORT_SPATTER_FLAG_FIRE_FRAME_2)fire_frame=1;
				if(fire_bit==VIEWPORT_SPATTER_FLAG_FIRE_FRAME_3)fire_frame=2;
				if(fire_bit==VIEWPORT_SPATTER_FLAG_FIRE_FRAME_4)fire_frame=3;
				}

			uint64_t spectex=(ff & VIEWPORT_FLOOR_FLAG_SPECIAL_TEXTURE);
			if(spectex!=0)
				{
				int32_t tp=0;
				switch(spectex)
					{
					case VIEWPORT_FLOOR_FLAG_SOIL_BACKGROUND_1:tp=gps.dirt_floor_texpos[0][4];break;
					case VIEWPORT_FLOOR_FLAG_SOIL_BACKGROUND_2:tp=gps.dirt_floor_texpos[1][4];break;
					case VIEWPORT_FLOOR_FLAG_SOIL_BACKGROUND_3:tp=gps.dirt_floor_texpos[2][4];break;
					case VIEWPORT_FLOOR_FLAG_SOIL_BACKGROUND_4:tp=gps.dirt_floor_texpos[3][4];break;
					}
				if(tp!=0)
					{
					texture_fullid background_tex;
						background_tex.texpos=tp;
						background_tex.r=1.0f;
						background_tex.g=1.0f;
						background_tex.b=1.0f;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							//***************************** BROKEN TRANSPARENCY
							//SDL_SetAlpha(tex, 0, 0);
								if(tp==gps.black_background_texpos[0])SDL_SetAlpha(tex, 0, 0);
								else SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				}
			if(vp->screentexpos_background[x * vp->dim_y + y]!=0)
				{
				int32_t tp=vp->screentexpos_background[x * vp->dim_y + y];

				texture_fullid background_tex;
					background_tex.texpos=tp;
					background_tex.r=1.0f;
					background_tex.g=1.0f;
					background_tex.b=1.0f;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						//***************************** BROKEN TRANSPARENCY
						//SDL_SetAlpha(tex, 0, 0);
							if(tp==gps.black_background_texpos[0]&&spectex==0)SDL_SetAlpha(tex, 0, 0);
							else SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(rf!=0)
				{
				if(rf & VIEWPORT_RAMP_FLAG_SHOW_DOWN_ARROW)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.texpos_ramp_down_arrow;
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				else
					{
					//************************** RAMP CASES
					if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_E)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_W));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_E));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_W));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_W)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_E));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_W)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_E));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_E)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NW));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_W)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NE));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_W)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_E)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SW));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_S));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_W))
						{
						texture_fullid background_tex;
							background_tex.texpos=gps.ramp_shadow_on_ramp_inside_corner_nw;
							background_tex.r=1;
							background_tex.g=1;
							background_tex.b=1;
							background_tex.br=0;
							background_tex.bg=0;
							background_tex.bb=0;
							background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							tex = tile_cache_lookup(background_tex);
							if(tex!=NULL)
								{
								SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
								SDL_BlitSurface(tex, NULL, screen, &dst);
								}
						}
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_E))
						{
						texture_fullid background_tex;
							background_tex.texpos=gps.ramp_shadow_on_ramp_inside_corner_ne;
							background_tex.r=1;
							background_tex.g=1;
							background_tex.b=1;
							background_tex.br=0;
							background_tex.bg=0;
							background_tex.bb=0;
							background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							tex = tile_cache_lookup(background_tex);
							if(tex!=NULL)
								{
								SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
								SDL_BlitSurface(tex, NULL, screen, &dst);
								}
						}
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_W))
						{
						texture_fullid background_tex;
							background_tex.texpos=gps.ramp_shadow_on_ramp_inside_corner_sw;
							background_tex.r=1;
							background_tex.g=1;
							background_tex.b=1;
							background_tex.br=0;
							background_tex.bg=0;
							background_tex.bb=0;
							background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							tex = tile_cache_lookup(background_tex);
							if(tex!=NULL)
								{
								SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
								SDL_BlitSurface(tex, NULL, screen, &dst);
								}
						}
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_E))
						{
						texture_fullid background_tex;
							background_tex.texpos=gps.ramp_shadow_on_ramp_inside_corner_se;
							background_tex.r=1;
							background_tex.g=1;
							background_tex.b=1;
							background_tex.br=0;
							background_tex.bg=0;
							background_tex.bb=0;
							background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							tex = tile_cache_lookup(background_tex);
							if(tex!=NULL)
								{
								SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
								SDL_BlitSurface(tex, NULL, screen, &dst);
								}
						}
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_W)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_E));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_E)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SW));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NE));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_W)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NE)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NE));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_W)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SW));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_E)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NW));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_E)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SW));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_S)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NW));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_W)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NE));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_N)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE));
					else if(rf & VIEWPORT_RAMP_FLAG_WALL_N)
						{
						//******************* Z RAMP SHADOWS
						if(!(rf & VIEWPORT_RAMP_FLAG_S_IS_OPEN_AIR))
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.ramp_shadow_on_ramp_n;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
								tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						}
					else if(rf & VIEWPORT_RAMP_FLAG_WALL_S)
						{
						//******************* Z RAMP SHADOWS
						if(!(rf & VIEWPORT_RAMP_FLAG_N_IS_OPEN_AIR))
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.ramp_shadow_on_ramp_s;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
								tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						}
					else if(rf & VIEWPORT_RAMP_FLAG_WALL_W)
						{
						//******************* Z RAMP SHADOWS
						if(!(rf & VIEWPORT_RAMP_FLAG_E_IS_OPEN_AIR))
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.ramp_shadow_on_ramp_w;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
								tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						}
					else if(rf & VIEWPORT_RAMP_FLAG_WALL_E)
						{
						//******************* Z RAMP SHADOWS
						if(!(rf & VIEWPORT_RAMP_FLAG_W_IS_OPEN_AIR))
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.ramp_shadow_on_ramp_e;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
								tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						}
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NE)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NE)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SW));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_NE)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NE)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SW)&&
						(rf & VIEWPORT_RAMP_FLAG_WALL_SE));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NW)&&
							(rf & VIEWPORT_RAMP_FLAG_WALL_NE))
						{
						//******************* Z RAMP SHADOWS
						if(!(rf & VIEWPORT_RAMP_FLAG_S_IS_OPEN_AIR))
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.ramp_shadow_on_ramp_n;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
								tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						}
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_SW)&&
							(rf & VIEWPORT_RAMP_FLAG_WALL_SE))
						{
						//******************* Z RAMP SHADOWS
						if(!(rf & VIEWPORT_RAMP_FLAG_N_IS_OPEN_AIR))
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.ramp_shadow_on_ramp_s;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
								tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						}
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NW)&&
							(rf & VIEWPORT_RAMP_FLAG_WALL_SW))
						{
						//******************* Z RAMP SHADOWS
						if(!(rf & VIEWPORT_RAMP_FLAG_E_IS_OPEN_AIR))
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.ramp_shadow_on_ramp_w;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
								tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						}
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NE)&&
							(rf & VIEWPORT_RAMP_FLAG_WALL_SE))
						{
						//******************* Z RAMP SHADOWS
						if(!(rf & VIEWPORT_RAMP_FLAG_W_IS_OPEN_AIR))
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.ramp_shadow_on_ramp_e;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
								tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						}
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NW)&&
							(rf & VIEWPORT_RAMP_FLAG_WALL_SE));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NE)&&
							(rf & VIEWPORT_RAMP_FLAG_WALL_SW));
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NW))
						{
						//******************* Z RAMP SHADOWS
						if(!(rf & VIEWPORT_RAMP_FLAG_S_IS_OPEN_AIR))
							{
							{texture_fullid background_tex;
								if(rf & VIEWPORT_RAMP_FLAG_S_IS_DARK_CORNER)background_tex.texpos=gps.ramp_shadow_on_ramp_corner_se_s_tri_heavy;
								else background_tex.texpos=gps.ramp_shadow_on_ramp_corner_se_s_tri_light;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
								tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}}
							}
						//******************* Z RAMP SHADOWS
						if(!(rf & VIEWPORT_RAMP_FLAG_E_IS_OPEN_AIR))
							{
							{texture_fullid background_tex;
								if(rf & VIEWPORT_RAMP_FLAG_E_IS_DARK_CORNER)background_tex.texpos=gps.ramp_shadow_on_ramp_corner_se_e_tri_heavy;
								else background_tex.texpos=gps.ramp_shadow_on_ramp_corner_se_e_tri_light;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
								tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}}
							}
						}
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_SW))
						{
						//******************* Z RAMP SHADOWS
						if(!(rf & VIEWPORT_RAMP_FLAG_N_IS_OPEN_AIR))
							{
							{texture_fullid background_tex;
								if(rf & VIEWPORT_RAMP_FLAG_N_IS_DARK_CORNER)background_tex.texpos=gps.ramp_shadow_on_ramp_corner_ne_n_tri_heavy;
								else background_tex.texpos=gps.ramp_shadow_on_ramp_corner_ne_n_tri_light;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
								tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}}
							}
						//******************* Z RAMP SHADOWS
						if(!(rf & VIEWPORT_RAMP_FLAG_E_IS_OPEN_AIR))
							{
							{texture_fullid background_tex;
								if(rf & VIEWPORT_RAMP_FLAG_E_IS_DARK_CORNER)background_tex.texpos=gps.ramp_shadow_on_ramp_corner_ne_e_tri_heavy;
								else background_tex.texpos=gps.ramp_shadow_on_ramp_corner_ne_e_tri_light;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
								tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}}
							}
						}
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_SE))
						{
						//******************* Z RAMP SHADOWS
						if(!(rf & VIEWPORT_RAMP_FLAG_N_IS_OPEN_AIR))
							{
							{texture_fullid background_tex;
								if(rf & VIEWPORT_RAMP_FLAG_N_IS_DARK_CORNER)background_tex.texpos=gps.ramp_shadow_on_ramp_corner_nw_n_tri_heavy;
								else background_tex.texpos=gps.ramp_shadow_on_ramp_corner_nw_n_tri_light;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
								tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}}
							}
						//******************* Z RAMP SHADOWS
						if(!(rf & VIEWPORT_RAMP_FLAG_W_IS_OPEN_AIR))
							{
							{texture_fullid background_tex;
								if(rf & VIEWPORT_RAMP_FLAG_W_IS_DARK_CORNER)background_tex.texpos=gps.ramp_shadow_on_ramp_corner_nw_w_tri_heavy;
								else background_tex.texpos=gps.ramp_shadow_on_ramp_corner_nw_w_tri_light;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
								tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}}
							}
						}
					else if((rf & VIEWPORT_RAMP_FLAG_WALL_NE))
						{
						//******************* Z RAMP SHADOWS
						if(!(rf & VIEWPORT_RAMP_FLAG_S_IS_OPEN_AIR))
							{
							{texture_fullid background_tex;
								if(rf & VIEWPORT_RAMP_FLAG_S_IS_DARK_CORNER)background_tex.texpos=gps.ramp_shadow_on_ramp_corner_sw_s_tri_heavy;
								else background_tex.texpos=gps.ramp_shadow_on_ramp_corner_sw_s_tri_light;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
								tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}}
							}
						//******************* Z RAMP SHADOWS
						if(!(rf & VIEWPORT_RAMP_FLAG_W_IS_OPEN_AIR))
							{
							{texture_fullid background_tex;
								if(rf & VIEWPORT_RAMP_FLAG_W_IS_DARK_CORNER)background_tex.texpos=gps.ramp_shadow_on_ramp_corner_sw_w_tri_heavy;
								else background_tex.texpos=gps.ramp_shadow_on_ramp_corner_sw_w_tri_light;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
								tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}}
							}
						}

					if(rf & VIEWPORT_RAMP_FLAG_SHOW_UP_ARROW)
						{
						texture_fullid background_tex;
							background_tex.texpos=gps.texpos_ramp_up_arrow;
							background_tex.r=1;
							background_tex.g=1;
							background_tex.b=1;
							background_tex.br=0;
							background_tex.bg=0;
							background_tex.bb=0;
							background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							tex = tile_cache_lookup(background_tex);
							if(tex!=NULL)
								{
								SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
								SDL_BlitSurface(tex, NULL, screen, &dst);
								}
						}
					}
				}
			if(ff!=0)
				{
				if(ff & VIEWPORT_FLOOR_FLAG_S_EDGING)
					{
					Edging type=(ff & VIEWPORT_FLOOR_FLAG_S_EDGING)>>VIEWPORT_FLOOR_FLAG_S_EDGING_SHIFT;

					texture_fullid background_tex;
						if(type==EDGING_STONE)background_tex.texpos=gps.stone_floor_texpos[0][1];
						else if(type==EDGING_SOIL)background_tex.texpos=gps.dirt_floor_texpos[0][1];
						else if(type==EDGING_SOIL_SAND)background_tex.texpos=gps.texpos_floor_sand[0][1];
						else if(type==EDGING_SOIL_SAND_YELLOW)background_tex.texpos=gps.texpos_floor_sand_yellow[0][1];
						else if(type==EDGING_SOIL_SAND_WHITE)background_tex.texpos=gps.texpos_floor_sand_white[0][1];
						else if(type==EDGING_SOIL_SAND_BLACK)background_tex.texpos=gps.texpos_floor_sand_black[0][1];
						else if(type==EDGING_SOIL_SAND_RED)background_tex.texpos=gps.texpos_floor_sand_red[0][1];
						else if(type>=EDGING_CUSTOM_GRASS_1&&
								type<EDGING_CUSTOM_GRASS_32)
							{
							type-=EDGING_CUSTOM_GRASS_1;
							background_tex.texpos=gps.custom_grass_edge_texpos[type][1];
							}
						else background_tex.texpos=gps.grass_texpos[0][1];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(ff & VIEWPORT_FLOOR_FLAG_W_EDGING)
					{
					Edging type=(ff & VIEWPORT_FLOOR_FLAG_W_EDGING)>>VIEWPORT_FLOOR_FLAG_W_EDGING_SHIFT;

					texture_fullid background_tex;
						if(type==EDGING_STONE)background_tex.texpos=gps.stone_floor_texpos[0][5];
						else if(type==EDGING_SOIL)background_tex.texpos=gps.dirt_floor_texpos[0][5];
						else if(type==EDGING_SOIL_SAND)background_tex.texpos=gps.texpos_floor_sand[0][5];
						else if(type==EDGING_SOIL_SAND_YELLOW)background_tex.texpos=gps.texpos_floor_sand_yellow[0][5];
						else if(type==EDGING_SOIL_SAND_WHITE)background_tex.texpos=gps.texpos_floor_sand_white[0][5];
						else if(type==EDGING_SOIL_SAND_BLACK)background_tex.texpos=gps.texpos_floor_sand_black[0][5];
						else if(type==EDGING_SOIL_SAND_RED)background_tex.texpos=gps.texpos_floor_sand_red[0][5];
						else if(type>=EDGING_CUSTOM_GRASS_1&&
								type<EDGING_CUSTOM_GRASS_32)
							{
							type-=EDGING_CUSTOM_GRASS_1;
							background_tex.texpos=gps.custom_grass_edge_texpos[type][5];
							}
						else background_tex.texpos=gps.grass_texpos[0][5];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(ff & VIEWPORT_FLOOR_FLAG_E_EDGING)
					{
					Edging type=(ff & VIEWPORT_FLOOR_FLAG_E_EDGING)>>VIEWPORT_FLOOR_FLAG_E_EDGING_SHIFT;

					texture_fullid background_tex;
						if(type==EDGING_STONE)background_tex.texpos=gps.stone_floor_texpos[0][3];
						else if(type==EDGING_SOIL)background_tex.texpos=gps.dirt_floor_texpos[0][3];
						else if(type==EDGING_SOIL_SAND)background_tex.texpos=gps.texpos_floor_sand[0][3];
						else if(type==EDGING_SOIL_SAND_YELLOW)background_tex.texpos=gps.texpos_floor_sand_yellow[0][3];
						else if(type==EDGING_SOIL_SAND_WHITE)background_tex.texpos=gps.texpos_floor_sand_white[0][3];
						else if(type==EDGING_SOIL_SAND_BLACK)background_tex.texpos=gps.texpos_floor_sand_black[0][3];
						else if(type==EDGING_SOIL_SAND_RED)background_tex.texpos=gps.texpos_floor_sand_red[0][3];
						else if(type>=EDGING_CUSTOM_GRASS_1&&
								type<EDGING_CUSTOM_GRASS_32)
							{
							type-=EDGING_CUSTOM_GRASS_1;
							background_tex.texpos=gps.custom_grass_edge_texpos[type][3];
							}
						else background_tex.texpos=gps.grass_texpos[0][3];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(ff & VIEWPORT_FLOOR_FLAG_N_EDGING)
					{
					Edging type=(ff & VIEWPORT_FLOOR_FLAG_N_EDGING)>>VIEWPORT_FLOOR_FLAG_N_EDGING_SHIFT;

					texture_fullid background_tex;
						if(type==EDGING_STONE)background_tex.texpos=gps.stone_floor_texpos[0][7];
						else if(type==EDGING_SOIL)background_tex.texpos=gps.dirt_floor_texpos[0][7];
						else if(type==EDGING_SOIL_SAND)background_tex.texpos=gps.texpos_floor_sand[0][7];
						else if(type==EDGING_SOIL_SAND_YELLOW)background_tex.texpos=gps.texpos_floor_sand_yellow[0][7];
						else if(type==EDGING_SOIL_SAND_WHITE)background_tex.texpos=gps.texpos_floor_sand_white[0][7];
						else if(type==EDGING_SOIL_SAND_BLACK)background_tex.texpos=gps.texpos_floor_sand_black[0][7];
						else if(type==EDGING_SOIL_SAND_RED)background_tex.texpos=gps.texpos_floor_sand_red[0][7];
						else if(type>=EDGING_CUSTOM_GRASS_1&&
								type<EDGING_CUSTOM_GRASS_32)
							{
							type-=EDGING_CUSTOM_GRASS_1;
							background_tex.texpos=gps.custom_grass_edge_texpos[type][7];
							}
						else background_tex.texpos=gps.grass_texpos[0][7];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if((ff & VIEWPORT_FLOOR_FLAG_S_EDGING)&&
					(ff & VIEWPORT_FLOOR_FLAG_W_EDGING))
					{
					Edging type_s=(ff & VIEWPORT_FLOOR_FLAG_S_EDGING)>>VIEWPORT_FLOOR_FLAG_S_EDGING_SHIFT;
					Edging type_w=(ff & VIEWPORT_FLOOR_FLAG_W_EDGING)>>VIEWPORT_FLOOR_FLAG_W_EDGING_SHIFT;

					texture_fullid background_tex;
						background_tex.texpos=0;
						if(type_s==EDGING_STONE&&
							type_w==EDGING_STONE)background_tex.texpos=gps.stone_floor_texpos[0][2];
						else if(type_s==EDGING_SOIL&&
							type_w==EDGING_SOIL)background_tex.texpos=gps.dirt_floor_texpos[0][2];
						else if(type_s==EDGING_SOIL_SAND&&
								type_w==EDGING_SOIL_SAND)background_tex.texpos=gps.texpos_floor_sand[0][2];
						else if(type_s==EDGING_SOIL_SAND_YELLOW&&
								type_w==EDGING_SOIL_SAND_YELLOW)background_tex.texpos=gps.texpos_floor_sand_yellow[0][2];
						else if(type_s==EDGING_SOIL_SAND_WHITE&&
								type_w==EDGING_SOIL_SAND_WHITE)background_tex.texpos=gps.texpos_floor_sand_white[0][2];
						else if(type_s==EDGING_SOIL_SAND_BLACK&&
								type_w==EDGING_SOIL_SAND_BLACK)background_tex.texpos=gps.texpos_floor_sand_black[0][2];
						else if(type_s==EDGING_SOIL_SAND_RED&&
								type_w==EDGING_SOIL_SAND_RED)background_tex.texpos=gps.texpos_floor_sand_red[0][2];
						else if(type_s==EDGING_GRASS&&
							type_w==EDGING_GRASS)background_tex.texpos=gps.grass_texpos[0][2];
						else if((type_s>=EDGING_CUSTOM_GRASS_1&&
								type_s<EDGING_CUSTOM_GRASS_32)&&
								(type_w>=EDGING_CUSTOM_GRASS_1&&
								type_w<EDGING_CUSTOM_GRASS_32)&&
								(type_s-EDGING_CUSTOM_GRASS_1==
								type_w-EDGING_CUSTOM_GRASS_1))
							{
							type_s-=EDGING_CUSTOM_GRASS_1;
							background_tex.texpos=gps.custom_grass_edge_texpos[type_s][2];
							}
						if(background_tex.texpos!=0)
							{
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							  tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
					}
				if((ff & VIEWPORT_FLOOR_FLAG_S_EDGING)&&
					(ff & VIEWPORT_FLOOR_FLAG_E_EDGING))
					{
					Edging type_s=(ff & VIEWPORT_FLOOR_FLAG_S_EDGING)>>VIEWPORT_FLOOR_FLAG_S_EDGING_SHIFT;
					Edging type_e=(ff & VIEWPORT_FLOOR_FLAG_E_EDGING)>>VIEWPORT_FLOOR_FLAG_E_EDGING_SHIFT;

					texture_fullid background_tex;
						background_tex.texpos=0;
						if(type_s==EDGING_STONE&&
							type_e==EDGING_STONE)background_tex.texpos=gps.stone_floor_texpos[0][0];
						else if(type_s==EDGING_SOIL&&
							type_e==EDGING_SOIL)background_tex.texpos=gps.dirt_floor_texpos[0][0];
						else if(type_s==EDGING_SOIL_SAND&&
								type_e==EDGING_SOIL_SAND)background_tex.texpos=gps.texpos_floor_sand[0][0];
						else if(type_s==EDGING_SOIL_SAND_YELLOW&&
								type_e==EDGING_SOIL_SAND_YELLOW)background_tex.texpos=gps.texpos_floor_sand_yellow[0][0];
						else if(type_s==EDGING_SOIL_SAND_WHITE&&
								type_e==EDGING_SOIL_SAND_WHITE)background_tex.texpos=gps.texpos_floor_sand_white[0][0];
						else if(type_s==EDGING_SOIL_SAND_BLACK&&
								type_e==EDGING_SOIL_SAND_BLACK)background_tex.texpos=gps.texpos_floor_sand_black[0][0];
						else if(type_s==EDGING_SOIL_SAND_RED&&
								type_e==EDGING_SOIL_SAND_RED)background_tex.texpos=gps.texpos_floor_sand_red[0][0];
						else if(type_s==EDGING_GRASS&&
							type_e==EDGING_GRASS)background_tex.texpos=gps.grass_texpos[0][0];
						else if((type_s>=EDGING_CUSTOM_GRASS_1&&
								type_s<EDGING_CUSTOM_GRASS_32)&&
								(type_e>=EDGING_CUSTOM_GRASS_1&&
								type_e<EDGING_CUSTOM_GRASS_32)&&
								(type_s-EDGING_CUSTOM_GRASS_1==
								type_e-EDGING_CUSTOM_GRASS_1))
							{
							type_s-=EDGING_CUSTOM_GRASS_1;
							background_tex.texpos=gps.custom_grass_edge_texpos[type_s][0];
							}
						if(background_tex.texpos!=0)
							{
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							  tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
					}
				if((ff & VIEWPORT_FLOOR_FLAG_N_EDGING)&&
					(ff & VIEWPORT_FLOOR_FLAG_W_EDGING))
					{
					Edging type_n=(ff & VIEWPORT_FLOOR_FLAG_N_EDGING)>>VIEWPORT_FLOOR_FLAG_N_EDGING_SHIFT;
					Edging type_w=(ff & VIEWPORT_FLOOR_FLAG_W_EDGING)>>VIEWPORT_FLOOR_FLAG_W_EDGING_SHIFT;

					texture_fullid background_tex;
						background_tex.texpos=0;
						if(type_n==EDGING_STONE&&
							type_w==EDGING_STONE)background_tex.texpos=gps.stone_floor_texpos[0][8];
						else if(type_n==EDGING_SOIL&&
							type_w==EDGING_SOIL)background_tex.texpos=gps.dirt_floor_texpos[0][8];
						else if(type_n==EDGING_SOIL_SAND&&
								type_w==EDGING_SOIL_SAND)background_tex.texpos=gps.texpos_floor_sand[0][8];
						else if(type_n==EDGING_SOIL_SAND_YELLOW&&
								type_w==EDGING_SOIL_SAND_YELLOW)background_tex.texpos=gps.texpos_floor_sand_yellow[0][8];
						else if(type_n==EDGING_SOIL_SAND_WHITE&&
								type_w==EDGING_SOIL_SAND_WHITE)background_tex.texpos=gps.texpos_floor_sand_white[0][8];
						else if(type_n==EDGING_SOIL_SAND_BLACK&&
								type_w==EDGING_SOIL_SAND_BLACK)background_tex.texpos=gps.texpos_floor_sand_black[0][8];
						else if(type_n==EDGING_SOIL_SAND_RED&&
								type_w==EDGING_SOIL_SAND_RED)background_tex.texpos=gps.texpos_floor_sand_red[0][8];
						else if(type_n==EDGING_GRASS&&
							type_w==EDGING_GRASS)background_tex.texpos=gps.grass_texpos[0][8];
						else if((type_n>=EDGING_CUSTOM_GRASS_1&&
								type_n<EDGING_CUSTOM_GRASS_32)&&
								(type_w>=EDGING_CUSTOM_GRASS_1&&
								type_w<EDGING_CUSTOM_GRASS_32)&&
								(type_n-EDGING_CUSTOM_GRASS_1==
								type_w-EDGING_CUSTOM_GRASS_1))
							{
							type_n-=EDGING_CUSTOM_GRASS_1;
							background_tex.texpos=gps.custom_grass_edge_texpos[type_n][8];
							}
						if(background_tex.texpos!=0)
							{
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							  tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
					}
				if((ff & VIEWPORT_FLOOR_FLAG_N_EDGING)&&
					(ff & VIEWPORT_FLOOR_FLAG_E_EDGING))
					{
					Edging type_n=(ff & VIEWPORT_FLOOR_FLAG_N_EDGING)>>VIEWPORT_FLOOR_FLAG_N_EDGING_SHIFT;
					Edging type_e=(ff & VIEWPORT_FLOOR_FLAG_E_EDGING)>>VIEWPORT_FLOOR_FLAG_E_EDGING_SHIFT;

					texture_fullid background_tex;
						background_tex.texpos=0;
						if(type_n==EDGING_STONE&&
							type_e==EDGING_STONE)background_tex.texpos=gps.stone_floor_texpos[0][6];
						else if(type_n==EDGING_SOIL&&
							type_e==EDGING_SOIL)background_tex.texpos=gps.dirt_floor_texpos[0][6];
						else if(type_n==EDGING_SOIL_SAND&&
								type_e==EDGING_SOIL_SAND)background_tex.texpos=gps.texpos_floor_sand[0][6];
						else if(type_n==EDGING_SOIL_SAND_YELLOW&&
								type_e==EDGING_SOIL_SAND_YELLOW)background_tex.texpos=gps.texpos_floor_sand_yellow[0][6];
						else if(type_n==EDGING_SOIL_SAND_WHITE&&
								type_e==EDGING_SOIL_SAND_WHITE)background_tex.texpos=gps.texpos_floor_sand_white[0][6];
						else if(type_n==EDGING_SOIL_SAND_BLACK&&
								type_e==EDGING_SOIL_SAND_BLACK)background_tex.texpos=gps.texpos_floor_sand_black[0][6];
						else if(type_n==EDGING_SOIL_SAND_RED&&
								type_e==EDGING_SOIL_SAND_RED)background_tex.texpos=gps.texpos_floor_sand_red[0][6];
						else if(type_n==EDGING_GRASS&&
							type_e==EDGING_GRASS)background_tex.texpos=gps.grass_texpos[0][6];
						else if((type_n>=EDGING_CUSTOM_GRASS_1&&
								type_n<EDGING_CUSTOM_GRASS_32)&&
								(type_e>=EDGING_CUSTOM_GRASS_1&&
								type_e<EDGING_CUSTOM_GRASS_32)&&
								(type_n-EDGING_CUSTOM_GRASS_1==
								type_e-EDGING_CUSTOM_GRASS_1))
							{
							type_n-=EDGING_CUSTOM_GRASS_1;
							background_tex.texpos=gps.custom_grass_edge_texpos[type_n][6];
							}
						if(background_tex.texpos!=0)
							{
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							  tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
					}
				}
			if(vp->screentexpos_spatter[x * vp->dim_y + y]!=0)
				{
				texture_fullid background_tex;
					background_tex.texpos=vp->screentexpos_spatter[x * vp->dim_y + y];
					background_tex.r=1;
					background_tex.g=1;
					background_tex.b=1;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_liquid_flag[x * vp->dim_y + y]!=0)
				{
				uint32_t lf=vp->screentexpos_liquid_flag[x * vp->dim_y + y];
				uint32_t center_type=(lf & VIEWPORT_LIQUID_FLAG_CENTER_TYPE)>>VIEWPORT_LIQUID_FLAG_CENTER_SHIFT;
				uint32_t center_level=(lf & VIEWPORT_LIQUID_FLAG_CENTER_LEVEL)>>VIEWPORT_LIQUID_FLAG_CENTER_LEVEL_SHIFT;
				if(center_type>0&&
					center_level>0)
					{
					texture_fullid background_tex;
						switch(center_type)
							{
							case VIEWPORT_LIQUID_TYPE_WATER:
								if(vp->flag & GRAPHIC_VIEWPORT_FLAG_SHOW_LIQUID_NUMBERS)background_tex.texpos=gps.underwater_label_texpos[center_level];
								else background_tex.texpos=gps.underwater_texpos[center_level];
								break;
							case VIEWPORT_LIQUID_TYPE_MAGMA:
								if(vp->flag & GRAPHIC_VIEWPORT_FLAG_SHOW_LIQUID_NUMBERS)background_tex.texpos=gps.undermagma_label_texpos[center_level];
								else background_tex.texpos=gps.undermagma_texpos[center_level];
								break;
							}
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				else
					{
					uint32_t n_type=(lf & VIEWPORT_LIQUID_FLAG_N_EDGE_TYPE)>>VIEWPORT_LIQUID_FLAG_N_EDGE_SHIFT;
					uint32_t s_type=(lf & VIEWPORT_LIQUID_FLAG_S_EDGE_TYPE)>>VIEWPORT_LIQUID_FLAG_S_EDGE_SHIFT;
					uint32_t w_type=(lf & VIEWPORT_LIQUID_FLAG_W_EDGE_TYPE)>>VIEWPORT_LIQUID_FLAG_W_EDGE_SHIFT;
					uint32_t e_type=(lf & VIEWPORT_LIQUID_FLAG_E_EDGE_TYPE)>>VIEWPORT_LIQUID_FLAG_E_EDGE_SHIFT;
					if(s_type!=0)
						{
						texture_fullid background_tex;
							switch(s_type)
								{
								case VIEWPORT_LIQUID_TYPE_WATER:background_tex.texpos=gps.underwater_edge_texpos[1][0];break;
								case VIEWPORT_LIQUID_TYPE_MAGMA:background_tex.texpos=gps.undermagma_edge_texpos[1][0];break;
								}
							background_tex.r=1;
							background_tex.g=1;
							background_tex.b=1;
							background_tex.br=0;
							background_tex.bg=0;
							background_tex.bb=0;
							background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						  tex = tile_cache_lookup(background_tex);
							if(tex!=NULL)
								{
								SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
								SDL_BlitSurface(tex, NULL, screen, &dst);
								}
						}
					if(w_type!=0)
						{
						texture_fullid background_tex;
							switch(w_type)
								{
								case VIEWPORT_LIQUID_TYPE_WATER:background_tex.texpos=gps.underwater_edge_texpos[2][1];break;
								case VIEWPORT_LIQUID_TYPE_MAGMA:background_tex.texpos=gps.undermagma_edge_texpos[2][1];break;
								}
							background_tex.r=1;
							background_tex.g=1;
							background_tex.b=1;
							background_tex.br=0;
							background_tex.bg=0;
							background_tex.bb=0;
							background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						  tex = tile_cache_lookup(background_tex);
							if(tex!=NULL)
								{
								SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
								SDL_BlitSurface(tex, NULL, screen, &dst);
								}
						}
					if(e_type!=0)
						{
						texture_fullid background_tex;
							switch(e_type)
								{
								case VIEWPORT_LIQUID_TYPE_WATER:background_tex.texpos=gps.underwater_edge_texpos[0][1];break;
								case VIEWPORT_LIQUID_TYPE_MAGMA:background_tex.texpos=gps.undermagma_edge_texpos[0][1];break;
								}
							background_tex.r=1;
							background_tex.g=1;
							background_tex.b=1;
							background_tex.br=0;
							background_tex.bg=0;
							background_tex.bb=0;
							background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						  tex = tile_cache_lookup(background_tex);
							if(tex!=NULL)
								{
								SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
								SDL_BlitSurface(tex, NULL, screen, &dst);
								}
						}
					if(n_type!=0)
						{
						texture_fullid background_tex;
							switch(n_type)
								{
								case VIEWPORT_LIQUID_TYPE_WATER:background_tex.texpos=gps.underwater_edge_texpos[1][2];break;
								case VIEWPORT_LIQUID_TYPE_MAGMA:background_tex.texpos=gps.undermagma_edge_texpos[1][2];break;
								}
							background_tex.r=1;
							background_tex.g=1;
							background_tex.b=1;
							background_tex.br=0;
							background_tex.bg=0;
							background_tex.bb=0;
							background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						  tex = tile_cache_lookup(background_tex);
							if(tex!=NULL)
								{
								SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
								SDL_BlitSurface(tex, NULL, screen, &dst);
								}
						}
					if(s_type!=0&&w_type!=0&&s_type==w_type)
						{
						texture_fullid background_tex;
							background_tex.texpos=0;
							switch(s_type)
								{
								case VIEWPORT_LIQUID_TYPE_WATER:background_tex.texpos=gps.underwater_edge_texpos[2][0];break;
								case VIEWPORT_LIQUID_TYPE_MAGMA:background_tex.texpos=gps.undermagma_edge_texpos[2][0];break;
								}
							if(background_tex.texpos!=0)
								{
									background_tex.r=1;
									background_tex.g=1;
									background_tex.b=1;
									background_tex.br=0;
									background_tex.bg=0;
									background_tex.bb=0;
									background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
								  tex = tile_cache_lookup(background_tex);
									if(tex!=NULL)
										{
										SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
										SDL_BlitSurface(tex, NULL, screen, &dst);
										}
								}
						}
					if(s_type!=0&&e_type!=0&&s_type==e_type)
						{
						uint32_t type_s=(ff & VIEWPORT_FLOOR_FLAG_S_EDGING);
						uint32_t type_e=(ff & VIEWPORT_FLOOR_FLAG_E_EDGING);

						texture_fullid background_tex;
							background_tex.texpos=0;
							switch(s_type)
								{
								case VIEWPORT_LIQUID_TYPE_WATER:background_tex.texpos=gps.underwater_edge_texpos[0][0];break;
								case VIEWPORT_LIQUID_TYPE_MAGMA:background_tex.texpos=gps.undermagma_edge_texpos[0][0];break;
								}
							if(background_tex.texpos!=0)
								{
									background_tex.r=1;
									background_tex.g=1;
									background_tex.b=1;
									background_tex.br=0;
									background_tex.bg=0;
									background_tex.bb=0;
									background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
								  tex = tile_cache_lookup(background_tex);
									if(tex!=NULL)
										{
										SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
										SDL_BlitSurface(tex, NULL, screen, &dst);
										}
								}
						}
					if(n_type!=0&&w_type!=0&&n_type==w_type)
						{
						uint32_t type_n=(ff & VIEWPORT_FLOOR_FLAG_N_EDGING);
						uint32_t type_w=(ff & VIEWPORT_FLOOR_FLAG_W_EDGING);

						texture_fullid background_tex;
							background_tex.texpos=0;
							switch(n_type)
								{
								case VIEWPORT_LIQUID_TYPE_WATER:background_tex.texpos=gps.underwater_edge_texpos[2][2];break;
								case VIEWPORT_LIQUID_TYPE_MAGMA:background_tex.texpos=gps.undermagma_edge_texpos[2][2];break;
								}
							if(background_tex.texpos!=0)
								{
									background_tex.r=1;
									background_tex.g=1;
									background_tex.b=1;
									background_tex.br=0;
									background_tex.bg=0;
									background_tex.bb=0;
									background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
								  tex = tile_cache_lookup(background_tex);
									if(tex!=NULL)
										{
										SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
										SDL_BlitSurface(tex, NULL, screen, &dst);
										}
								}
						}
					if(n_type!=0&&e_type!=0&&n_type==e_type)
						{
						uint32_t type_n=(ff & VIEWPORT_FLOOR_FLAG_N_EDGING);
						uint32_t type_e=(ff & VIEWPORT_FLOOR_FLAG_E_EDGING);

						texture_fullid background_tex;
							background_tex.texpos=0;
							switch(n_type)
								{
								case VIEWPORT_LIQUID_TYPE_WATER:background_tex.texpos=gps.underwater_edge_texpos[0][2];break;
								case VIEWPORT_LIQUID_TYPE_MAGMA:background_tex.texpos=gps.undermagma_edge_texpos[0][2];break;
								}
							if(background_tex.texpos!=0)
								{
									background_tex.r=1;
									background_tex.g=1;
									background_tex.b=1;
									background_tex.br=0;
									background_tex.bg=0;
									background_tex.bb=0;
									background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
								  tex = tile_cache_lookup(background_tex);
									if(tex!=NULL)
										{
										SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
										SDL_BlitSurface(tex, NULL, screen, &dst);
										}
								}
						}
					}
				}
			if(vp->screentexpos_shadow_flag[x * vp->dim_y + y]!=0)
				{
				uint32_t sf=vp->screentexpos_shadow_flag[x * vp->dim_y + y];
				if(sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_N)
					{
					if(sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_NW)
						{
						if(sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_NE)
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.wall_shadow_straight_n;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							  tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						else
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.wall_shadow_near_n_open_ne;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							  tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						}
					else
						{
						if(sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_NE)
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.wall_shadow_near_n_open_nw;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							  tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						else
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.wall_shadow_end_wall_n;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							  tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						}
					}
				else
					{
					if((sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_NW)&&
						!(sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_W))
						{
						texture_fullid background_tex;
							background_tex.texpos=gps.wall_shadow_corner_nw;
							background_tex.r=1;
							background_tex.g=1;
							background_tex.b=1;
							background_tex.br=0;
							background_tex.bg=0;
							background_tex.bb=0;
							background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							tex = tile_cache_lookup(background_tex);
							if(tex!=NULL)
								{
								SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
								SDL_BlitSurface(tex, NULL, screen, &dst);
								}
						}
					if((sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_NE)&&
						!(sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_E))
						{
						texture_fullid background_tex;
							background_tex.texpos=gps.wall_shadow_corner_ne;
							background_tex.r=1;
							background_tex.g=1;
							background_tex.b=1;
							background_tex.br=0;
							background_tex.bg=0;
							background_tex.bb=0;
							background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							tex = tile_cache_lookup(background_tex);
							if(tex!=NULL)
								{
								SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
								SDL_BlitSurface(tex, NULL, screen, &dst);
								}
						}
					}
				if(sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_S)
					{
					if(sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_SW)
						{
						if(sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_SE)
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.wall_shadow_straight_s;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							  tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						else
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.wall_shadow_near_s_open_se;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							  tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						}
					else
						{
						if(sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_SE)
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.wall_shadow_near_s_open_sw;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							  tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						else
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.wall_shadow_end_wall_s;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							  tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						}
					}
				else
					{
					if((sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_SW)&&
						!(sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_W))
						{
						texture_fullid background_tex;
							background_tex.texpos=gps.wall_shadow_corner_sw;
							background_tex.r=1;
							background_tex.g=1;
							background_tex.b=1;
							background_tex.br=0;
							background_tex.bg=0;
							background_tex.bb=0;
							background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							tex = tile_cache_lookup(background_tex);
							if(tex!=NULL)
								{
								SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
								SDL_BlitSurface(tex, NULL, screen, &dst);
								}
						}
					if((sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_SE)&&
						!(sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_E))
						{
						texture_fullid background_tex;
							background_tex.texpos=gps.wall_shadow_corner_se;
							background_tex.r=1;
							background_tex.g=1;
							background_tex.b=1;
							background_tex.br=0;
							background_tex.bg=0;
							background_tex.bb=0;
							background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							tex = tile_cache_lookup(background_tex);
							if(tex!=NULL)
								{
								SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
								SDL_BlitSurface(tex, NULL, screen, &dst);
								}
						}
					}
				if(sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_W)
					{
					if(sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_SW)
						{
						if(sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_NW)
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.wall_shadow_straight_w;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							  tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						else
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.wall_shadow_near_w_open_nw;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							  tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						}
					else
						{
						if(sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_NW)
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.wall_shadow_near_w_open_sw;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							  tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						else
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.wall_shadow_end_wall_w;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							  tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						}
					}
				if(sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_E)
					{
					if(sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_SE)
						{
						if(sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_NE)
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.wall_shadow_straight_e;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							  tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						else
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.wall_shadow_near_e_open_ne;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							  tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						}
					else
						{
						if(sf & VIEWPORT_SHADOW_FLAG_SHADOW_WALL_TO_NE)
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.wall_shadow_near_e_open_se;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							  tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						else
							{
							texture_fullid background_tex;
								background_tex.texpos=gps.wall_shadow_end_wall_e;
								background_tex.r=1;
								background_tex.g=1;
								background_tex.b=1;
								background_tex.br=0;
								background_tex.bg=0;
								background_tex.bb=0;
								background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
							  tex = tile_cache_lookup(background_tex);
								if(tex!=NULL)
									{
									SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
									SDL_BlitSurface(tex, NULL, screen, &dst);
									}
							}
						}
					}
				if(sf & VIEWPORT_RAMP_SHADOW_ON_FLOOR_NW_OF_CORNER_SE)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.ramp_shadow_on_floor_nw_of_corner_se;
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(sf & VIEWPORT_RAMP_SHADOW_ON_FLOOR_N_OF_CORNER_SE)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.ramp_shadow_on_floor_n_of_corner_se;
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(sf & VIEWPORT_RAMP_SHADOW_ON_FLOOR_N_OF_S)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.ramp_shadow_on_floor_n_of_s;
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(sf & VIEWPORT_RAMP_SHADOW_ON_FLOOR_N_OF_CORNER_SW)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.ramp_shadow_on_floor_n_of_corner_sw;
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(sf & VIEWPORT_RAMP_SHADOW_ON_FLOOR_NE_OF_CORNER_SW)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.ramp_shadow_on_floor_ne_of_corner_sw;
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(sf & VIEWPORT_RAMP_SHADOW_ON_FLOOR_W_OF_CORNER_SE)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.ramp_shadow_on_floor_w_of_corner_se;
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(sf & VIEWPORT_RAMP_SHADOW_ON_FLOOR_E_OF_CORNER_SW)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.ramp_shadow_on_floor_e_of_corner_sw;
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(sf & VIEWPORT_RAMP_SHADOW_ON_FLOOR_W_OF_E)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.ramp_shadow_on_floor_w_of_e;
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(sf & VIEWPORT_RAMP_SHADOW_ON_FLOOR_E_OF_W)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.ramp_shadow_on_floor_e_of_w;
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(sf & VIEWPORT_RAMP_SHADOW_ON_FLOOR_W_OF_CORNER_NE)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.ramp_shadow_on_floor_w_of_corner_ne;
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(sf & VIEWPORT_RAMP_SHADOW_ON_FLOOR_E_OF_CORNER_NW)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.ramp_shadow_on_floor_e_of_corner_nw;
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(sf & VIEWPORT_RAMP_SHADOW_ON_FLOOR_SW_OF_CORNER_NE)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.ramp_shadow_on_floor_sw_of_corner_ne;
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(sf & VIEWPORT_RAMP_SHADOW_ON_FLOOR_S_OF_CORNER_NE)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.ramp_shadow_on_floor_s_of_corner_ne;
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(sf & VIEWPORT_RAMP_SHADOW_ON_FLOOR_S_OF_N)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.ramp_shadow_on_floor_s_of_n;
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(sf & VIEWPORT_RAMP_SHADOW_ON_FLOOR_S_OF_CORNER_NW)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.ramp_shadow_on_floor_s_of_corner_nw;
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(sf & VIEWPORT_RAMP_SHADOW_ON_FLOOR_SE_OF_CORNER_NW)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.ramp_shadow_on_floor_se_of_corner_nw;
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				}
			if(ff!=0)
				{
				//************************** INDIV TREE special floor texpos
					/*
					pmd_tree_texture_infost *tti=NULL;
					vegst *vegp=veg.grab_map_veg_event(mx,my,mz);
					if(vegp!=NULL)
						{
						if(vegp->gloss>=0&&vegp->gloss<plant.plant.size())
							{
							plant_material_definitionst *pmd=plant.plant[vegp->gloss];
							tti=pmd->tree_texture_info;
							}
						}
					*/
					/*
				if(ff & VIEWPORT_FLOOR_FLAG_TRUNK_CORE_NW)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.texpos_tree_core_trunk[0][0];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(ff & VIEWPORT_FLOOR_FLAG_TRUNK_CORE_N)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.texpos_tree_core_trunk[1][0];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(ff & VIEWPORT_FLOOR_FLAG_TRUNK_CORE_NE)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.texpos_tree_core_trunk[2][0];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(ff & VIEWPORT_FLOOR_FLAG_TRUNK_CORE_W)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.texpos_tree_core_trunk[0][1];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
  						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
						tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(ff & VIEWPORT_FLOOR_FLAG_TRUNK_CORE)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.texpos_tree_core_trunk[1][1];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(ff & VIEWPORT_FLOOR_FLAG_TRUNK_CORE_E)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.texpos_tree_core_trunk[2][1];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(ff & VIEWPORT_FLOOR_FLAG_TRUNK_CORE_SW)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.texpos_tree_core_trunk[0][2];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(ff & VIEWPORT_FLOOR_FLAG_TRUNK_CORE_S)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.texpos_tree_core_trunk[1][2];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(ff & VIEWPORT_FLOOR_FLAG_TRUNK_CORE_SE)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.texpos_tree_core_trunk[2][2];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
					*/
				}
			if(vp->screentexpos_background_two[x * vp->dim_y + y]!=0)
				{
				texture_fullid background_tex;
					background_tex.texpos=vp->screentexpos_background_two[x * vp->dim_y + y];
					background_tex.r=1.0f;
					background_tex.g=1.0f;
					background_tex.b=1.0f;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_building_one[x * vp->dim_y + y]!=0)
				{
				texture_fullid background_tex;
					background_tex.texpos=vp->screentexpos_building_one[x * vp->dim_y + y];
					background_tex.r=1;
					background_tex.g=1;
					background_tex.b=1;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_item[x * vp->dim_y + y]!=0)
				{
				texture_fullid background_tex;
					background_tex.texpos=vp->screentexpos_item[x * vp->dim_y + y];
					background_tex.r=1;
					background_tex.g=1;
					background_tex.b=1;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_vehicle[x * vp->dim_y + y]!=0)
				{
				texture_fullid background_tex;
					background_tex.texpos=vp->screentexpos_vehicle[x * vp->dim_y + y];
					background_tex.r=1;
					background_tex.g=1;
					background_tex.b=1;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_vermin[x * vp->dim_y + y]!=0)
				{
				texture_fullid background_tex;
					background_tex.texpos=vp->screentexpos_vermin[x * vp->dim_y + y];
					background_tex.r=1;
					background_tex.g=1;
					background_tex.b=1;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_right_creature[x * vp->dim_y + y]!=0)
				{
				texture_fullid background_tex;
					background_tex.texpos=vp->screentexpos_right_creature[x * vp->dim_y + y];
					background_tex.r=1;
					background_tex.g=1;
					background_tex.b=1;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos[x * vp->dim_y + y]!=0)
				{
				texture_fullid background_tex;
					background_tex.texpos=vp->screentexpos[x * vp->dim_y + y];
					background_tex.r=1;
					background_tex.g=1;
					background_tex.b=1;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_left_creature[x * vp->dim_y + y]!=0)
				{
				texture_fullid background_tex;
					background_tex.texpos=vp->screentexpos_left_creature[x * vp->dim_y + y];
					background_tex.r=1;
					background_tex.g=1;
					background_tex.b=1;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_building_two[x * vp->dim_y + y]!=0)
				{
				texture_fullid background_tex;
					background_tex.texpos=vp->screentexpos_building_two[x * vp->dim_y + y];
					background_tex.r=1;
					background_tex.g=1;
					background_tex.b=1;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(fire_frame!=-1)
				{
				texture_fullid background_tex;
					background_tex.texpos=gps.texpos_fire[fire_frame];
					background_tex.r=1;
					background_tex.g=1;
					background_tex.b=1;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_projectile[x * vp->dim_y + y]!=0)
				{
				texture_fullid background_tex;
					background_tex.texpos=vp->screentexpos_projectile[x * vp->dim_y + y];
					background_tex.r=1;
					background_tex.g=1;
					background_tex.b=1;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_high_flow[x * vp->dim_y + y]!=0)
				{
				texture_fullid background_tex;
					background_tex.texpos=vp->screentexpos_high_flow[x * vp->dim_y + y];
					background_tex.r=1;
					background_tex.g=1;
					background_tex.b=1;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_top_shadow[x * vp->dim_y + y]!=0)
				{
				//************************** INDIV TREE top shadow texpos
				/*
				if(ff & VIEWPORT_FLOOR_FLAG_TRUNK_SHADOW_TO_SE)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.texpos_tree_core_shadow[0][0];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(ff & VIEWPORT_FLOOR_FLAG_TRUNK_SHADOW_TO_S)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.texpos_tree_core_shadow[1][0];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(ff & VIEWPORT_FLOOR_FLAG_TRUNK_SHADOW_TO_SW)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.texpos_tree_core_shadow[2][0];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(ff & VIEWPORT_FLOOR_FLAG_TRUNK_SHADOW_TO_E)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.texpos_tree_core_shadow[0][1];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(ff & VIEWPORT_FLOOR_FLAG_TRUNK_SHADOW_TO_W)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.texpos_tree_core_shadow[2][1];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(ff & VIEWPORT_FLOOR_FLAG_TRUNK_SHADOW_TO_NE)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.texpos_tree_core_shadow[0][2];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(ff & VIEWPORT_FLOOR_FLAG_TRUNK_SHADOW_TO_N)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.texpos_tree_core_shadow[1][2];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				if(ff & VIEWPORT_FLOOR_FLAG_TRUNK_SHADOW_TO_NW)
					{
					texture_fullid background_tex;
						background_tex.texpos=gps.texpos_tree_core_shadow[2][2];
						background_tex.r=1;
						background_tex.g=1;
						background_tex.b=1;
						background_tex.br=0;
						background_tex.bg=0;
						background_tex.bb=0;
						background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
					  tex = tile_cache_lookup(background_tex);
						if(tex!=NULL)
							{
							SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
							SDL_BlitSurface(tex, NULL, screen, &dst);
							}
					}
				*/
				}
			if(vp->screentexpos_signpost[x * vp->dim_y + y]!=0)
				{
				texture_fullid background_tex;
					background_tex.texpos=vp->screentexpos_signpost[x * vp->dim_y + y];
					background_tex.r=1;
					background_tex.g=1;
					background_tex.b=1;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_upright_creature[x * vp->dim_y + y]!=0)
				{
				texture_fullid background_tex;
					background_tex.texpos=vp->screentexpos_upright_creature[x * vp->dim_y + y];
					background_tex.r=1;
					background_tex.g=1;
					background_tex.b=1;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_up_creature[x * vp->dim_y + y]!=0)
				{
				texture_fullid background_tex;
					background_tex.texpos=vp->screentexpos_up_creature[x * vp->dim_y + y];
					background_tex.r=1;
					background_tex.g=1;
					background_tex.b=1;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_upleft_creature[x * vp->dim_y + y]!=0)
				{
				texture_fullid background_tex;
					background_tex.texpos=vp->screentexpos_upleft_creature[x * vp->dim_y + y];
					background_tex.r=1;
					background_tex.g=1;
					background_tex.b=1;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_designation[x * vp->dim_y + y]!=0)
				{
				texture_fullid background_tex;
					background_tex.texpos=vp->screentexpos_designation[x * vp->dim_y + y];
					background_tex.r=1;
					background_tex.g=1;
					background_tex.b=1;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}
			if(vp->screentexpos_interface[x * vp->dim_y + y]!=0)
				{
				texture_fullid background_tex;
					background_tex.texpos=vp->screentexpos_interface[x * vp->dim_y + y];
					background_tex.r=1;
					background_tex.g=1;
					background_tex.b=1;
					background_tex.br=0;
					background_tex.bg=0;
					background_tex.bb=0;
					background_tex.flag=TEXTURE_FULLID_FLAG_TRANSPARENT_BACKGROUND;
				  tex = tile_cache_lookup(background_tex);
					if(tex!=NULL)
						{
						SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
						SDL_BlitSurface(tex, NULL, screen, &dst);
						}
				}

			if(x>=0&&y>=0&&dispx_z>0&&dispy_z>0)//just in case
				{
				//************************** TEXTURE SIZE DEPENDENCE
					//in particular, the dy having a +2 is to get rid of some tearing on the border, need to figure that one out
				
				int32_t tx=(viewport_zoom_factor*32)/128,ty=(viewport_zoom_factor*32)/128;
				int32_t sx=x*tx/dispx_z;
				int32_t sy=y*ty/dispy_z;
				int32_t dx=tx/dispx_z+1;
				int32_t dy=ty/dispy_z+2;
				sx-=1;sy-=1;dx+=2;dy+=2;

				int32_t gx,gy;
				for(gx=sx;gx<sx+dx;++gx)
					{
					for(gy=sy;gy<sy+dy;++gy)
						{
						if(gx>=0&&gx<gps.dimx&&gy>=0&&gy<gps.dimy)
							{
							gps.screentexpos_refresh_buffer[gx * gps.dimy + gy]=gps.refresh_buffer_val;

							if(gps.screentexpos_anchored[gx * gps.dimy + gy]!=0)
								{
								uint32_t tf=gps.screentexpos_flag[gx * gps.dimy + gy];
								if(tf & SCREENTEXPOS_FLAG_ANCHOR_SUBORDINATE)
									{
									int32_t off_x=((tf & SCREENTEXPOS_FLAG_ANCHOR_X_COORD)>>SCREENTEXPOS_FLAG_ANCHOR_X_COORD_SHIFT);
									int32_t off_y=((tf & SCREENTEXPOS_FLAG_ANCHOR_Y_COORD)>>SCREENTEXPOS_FLAG_ANCHOR_Y_COORD_SHIFT);
									if(gx-off_x>=0&&gx-off_x<gps.dimx&&gy-off_y>=0&&gy-off_y<gps.dimy)
										{
										gps.screentexpos_refresh_buffer[(gx-off_x) * gps.dimy + gy-off_y]=gps.refresh_buffer_val;
										}
									}
								}
							if(gps.screentexpos_top_anchored[gx * gps.dimy + gy]!=0)
								{
								uint32_t tf=gps.screentexpos_top_flag[gx * gps.dimy + gy];
								if(tf & SCREENTEXPOS_FLAG_ANCHOR_SUBORDINATE)
									{
									int32_t off_x=((tf & SCREENTEXPOS_FLAG_ANCHOR_X_COORD)>>SCREENTEXPOS_FLAG_ANCHOR_X_COORD_SHIFT);
									int32_t off_y=((tf & SCREENTEXPOS_FLAG_ANCHOR_Y_COORD)>>SCREENTEXPOS_FLAG_ANCHOR_Y_COORD_SHIFT);
									if(gx-off_x>=0&&gx-off_x<gps.dimx&&gy-off_y>=0&&gy-off_y<gps.dimy)
										{
										gps.screentexpos_refresh_buffer[(gx-off_x) * gps.dimy + gy-off_y]=gps.refresh_buffer_val;
										}
									}
								}
							}
						}
					}
				}
			}

      //tex = tile_cache_lookup(id.left);
      // And blit.
      //if(tex!=NULL)SDL_BlitSurface(tex, NULL, screen, &dst);
    }/*else {  // TTF, cached in ttf_manager so no point in also caching here
      tex = ttf_manager.get_texture(id.right);
      // Blit later
      ttfs_to_render.push_back(make_pair(tex, dst));
    }*/
		use_viewport_zoom=false;
		}

void do_blank_screen_fill()
{
		SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
}

  void update_all() {

	if(gps.display_background)
		{
		SDL_Rect dst;
		dst.x = /*dispx_z * x +*/ origin_x + (screen->w)/2-1920/2;
		dst.y = /*dispx_z * x +*/ origin_y + (screen->h)/2-1080/2;
		SDL_Surface *tex=enabler.textures.get_texture_data(gps.tex_pos[TEXTURE_TITLE_BACKGROUND]);
		SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
		SDL_BlitSurface(tex, NULL, screen, &dst);
		}
	if(gps.display_title)
		{
		SDL_Rect dst;
		dst.x = /*dispx_z * x +*/ origin_x + (screen->w)/2-907/2;
		dst.y = /*dispx_z * x +*/ origin_y;
		if(screen->h>=800)dst.y+=(screen->h-800)/3;
		SDL_Surface *tex=enabler.textures.get_texture_data(gps.tex_pos[TEXTURE_TITLE]);
		SDL_SetAlpha(tex, SDL_SRCALPHA, 0);
		SDL_BlitSurface(tex, NULL, screen, &dst);
		}

    for (int x = 0; x < gps.dimx; x++)
      for (int y = 0; y < gps.dimy; y++)
        update_tile(x, y);
    for (int x = 0; x < gps.dimx; x++)
      for (int y = 0; y < gps.dimy; y++)
        update_anchor_tile(x, y);

	if(gps.top_in_use)
		{
		for (int x = 0; x < gps.dimx; x++)
		  for (int y = 0; y < gps.dimy; y++)
			update_top_tile(x, y);
		for (int x = 0; x < gps.dimx; x++)
		  for (int y = 0; y < gps.dimy; y++)
			update_top_anchor_tile(x, y);
		}
  }

	void clean_tile_cache()
		{
		for (map<texture_fullid, SDL_Surface*>::iterator it = tile_cache.begin();
				it != tile_cache.end();
				++it)
			{
			SDL_FreeSurface(it->second);
			}
		tile_cache.clear();
		}

	void update_full_map_port(graphic_map_portst *vp)
		{
		int32_t x,y;
		for(y=0;y<vp->dim_y;++y)
			{
			for(x=0;x<vp->dim_x;++x)
				{
				update_map_port_tile(vp,x,y);
				}
			}
		}

	void update_full_viewport(graphic_viewportst *vp)
		{
		int32_t x,y;
		for(y=0;y<vp->dim_y;++y)
			{
			for(x=0;x<vp->dim_x;++x)
				{
				update_viewport_tile(vp,x,y);
				}
			}
		}

  virtual void render() {
    // Render the TTFs, which we left for last
    for (auto it = ttfs_to_render.begin(); it != ttfs_to_render.end(); ++it) {
      SDL_BlitSurface(it->first, NULL, screen, &it->second);
    }
    ttfs_to_render.clear();
    // And flip out.
    SDL_Flip(screen);
  }

  virtual ~renderer_2d_base() {
	for (auto it = tile_cache.cbegin(); it != tile_cache.cend(); ++it)
		SDL_FreeSurface(it->second);
	for (auto it = ttfs_to_render.cbegin(); it != ttfs_to_render.cend(); ++it)
		SDL_FreeSurface(it->first);
  }

  void grid_resize(int w, int h) {
    dimx = w; dimy = h;
    // Only reallocate the grid if it actually changes
    if (init.display.grid_x != dimx || init.display.grid_y != dimy)
      gps_allocate(dimx, dimy, screen->w, screen->h,dispx_z,dispy_z);

    // But always force a full display cycle
    gps.force_full_display_count = 1;
    enabler.flag |= ENABLERFLAG_RENDER;    
  }

  renderer_2d_base() {
    zoom_steps = forced_steps = 0;
	use_viewport_zoom=false;
	viewport_zoom_factor=192;
  }
  
  int zoom_steps, forced_steps;
  int natural_w, natural_h;

  void compute_forced_zoom() {
    forced_steps = 0;

    const int dispx = (enabler.flag & ENABLERFLAG_BASIC_TEXT) ?
		init.font.basic_font_dispx :
		(enabler.is_fullscreen() ?
		  init.font.large_font_dispx :
		  init.font.small_font_dispx);
    const int dispy = (enabler.flag & ENABLERFLAG_BASIC_TEXT) ?
		init.font.basic_font_dispy :
		(enabler.is_fullscreen() ?
		  init.font.large_font_dispy :
		  init.font.small_font_dispy);

	if(init.display.flag.has_flag(INIT_DISPLAY_FLAG_INTERFACE_SCALING_TO_DESIRED_HEIGHT_WIDTH))
		{
	    pair<int,int> zoomed=compute_zoom();

		while(zoomed.first>init.display.interface_scaling_desired_width&&
			zoomed.second>init.display.interface_scaling_desired_height)
			{
			forced_steps--;
			zoomed = compute_zoom();
			}
		//needs to err on side of allowing menus to fit (for default setting)
		if((zoomed.first<init.display.interface_scaling_desired_width||
			zoomed.second<init.display.interface_scaling_desired_height)&&
			forced_steps<0)
			{
			forced_steps++;
			zoomed = compute_zoom();
			}
		}
	else
		{
		if(dispx<dispy)
			{
			int32_t desired_dim=natural_w*100/init.display.interface_scaling_percentage;

			pair<int,int> zoomed=compute_zoom();

			while(zoomed.first>desired_dim)
				{
				forced_steps--;
				zoomed = compute_zoom();
				}
			//needs to err on side of allowing menus to fit (for default setting)
			if(zoomed.first<desired_dim&&forced_steps<0)
				{
				forced_steps++;
				zoomed = compute_zoom();
				}
			}
		else
			{
			int32_t desired_dim=natural_h*100/init.display.interface_scaling_percentage;

			pair<int,int> zoomed=compute_zoom();

			while(zoomed.second>desired_dim)
				{
				forced_steps--;
				zoomed = compute_zoom();
				}
			//needs to err on side of allowing menus to fit (for default setting)
			if(zoomed.second<desired_dim&&forced_steps<0)
				{
				forced_steps++;
				zoomed = compute_zoom();
				}
			}
		}

    pair<int,int> zoomed = compute_zoom();
    while (zoomed.first < MIN_GRID_X || zoomed.second < MIN_GRID_Y) {
      forced_steps++;
      zoomed = compute_zoom();
    }
    while (zoomed.first > MAX_GRID_X || zoomed.second > MAX_GRID_Y) {
      forced_steps--;
      zoomed = compute_zoom();
    }
  }

  pair<int,int> compute_zoom(bool clamp = false) {
	if(enabler.flag & ENABLERFLAG_BASIC_TEXT)

    const int dispx = (enabler.flag & ENABLERFLAG_BASIC_TEXT) ?
		init.font.basic_font_dispx :
		(enabler.is_fullscreen() ?
		  init.font.large_font_dispx :
		  init.font.small_font_dispx);
    const int dispy = (enabler.flag & ENABLERFLAG_BASIC_TEXT) ?
		init.font.basic_font_dispy :
		(enabler.is_fullscreen() ?
		  init.font.large_font_dispy :
		  init.font.small_font_dispy);
    int w, h;
    if (dispx < dispy) {
      w = natural_w + zoom_steps + forced_steps;
      h = (int)(double(natural_h) * (double(w) / double(natural_w)));
    } else {
      h = natural_h + zoom_steps + forced_steps;
      w = (int)(double(natural_w) * (double(h) / double(natural_h)));
    }
    if (clamp) {
      w = MIN(MAX(w, MIN_GRID_X), MAX_GRID_X);
      h = MIN(MAX(h, MIN_GRID_Y), MAX_GRID_Y);
    }
    return make_pair(w,h);
  }

  
  void resize(int w, int h) {

	  if(w<MINIMUM_WINDOW_WIDTH)w=MINIMUM_WINDOW_WIDTH;
	  if(h<MINIMUM_WINDOW_HEIGHT)h=MINIMUM_WINDOW_HEIGHT;

    // We've gotten resized.. first step is to reinitialize video
    cout << "New window size: " << w << "x" << h << endl;
    init_video(w, h);
    dispx = (enabler.flag & ENABLERFLAG_BASIC_TEXT) ?
		 init.font.basic_font_dispx :
		(enabler.is_fullscreen() ?
		  init.font.large_font_dispx :
		  init.font.small_font_dispx);
    dispy = (enabler.flag & ENABLERFLAG_BASIC_TEXT) ?
		 init.font.basic_font_dispy :
		(enabler.is_fullscreen() ?
		  init.font.large_font_dispy :
		  init.font.small_font_dispy);
    cout << "Font size: " << dispx << "x" << dispy << endl;

    // If grid size is currently overridden, we don't change it
    if (enabler.overridden_grid_sizes.size() == 0) {
      // (Re)calculate grid-size
		//******************** WIDE SCREENS
      dimx = MIN(MAX(w / dispx, MIN_GRID_X), MAX_GRID_X);
      dimy = MIN(MAX(h / dispy, MIN_GRID_Y), MAX_GRID_Y);
      cout << "Resizing grid to " << dimx << "x" << dimy << endl;
      grid_resize(dimx, dimy);
    }
    // Calculate zoomed tile size
    natural_w = MAX(w / dispx,1);
    natural_h = MAX(h / dispy,1);
    compute_forced_zoom();
    reshape(compute_zoom(true));
    cout << endl;
  }

  void reshape(pair<int,int> max_grid) {
    int w = max_grid.first,
      h = max_grid.second;
    // Compute the largest tile size that will fit this grid into the window, roughly maintaining aspect ratio
    double try_x = dispx, try_y = dispy;
    try_x = screen->w / w;
    try_y = MIN(try_x / dispx * dispy, screen->h / h);
    try_x = MIN(try_x, try_y / dispy * dispx);
    dispx_z = (int)(MAX(1,try_x)); dispy_z = (int)(MAX(try_y,1));
    cout << "Resizing font to " << dispx_z << "x" << dispy_z << endl;
    // Remove now-obsolete tile catalog
    for (map<texture_fullid, SDL_Surface*>::iterator it = tile_cache.begin();
         it != tile_cache.end();
         ++it)
      SDL_FreeSurface(it->second);
    tile_cache.clear();
    // Recompute grid based on the new tile size
	//*********************************** SCREEN SIZE 1
		//where does the mouse wheel stuff enter into this?  still need to be able to zoom out the 32x32 stuff too
	//************************** WIDE SCREENS
    w = CLAMP(screen->w / dispx_z, MIN_GRID_X, MAX_GRID_X);
    h = CLAMP(screen->h / dispy_z, MIN_GRID_Y, MAX_GRID_Y);
    // Reset grid size
#ifdef DEBUG
    cout << "Resizing grid to " << w << "x" << h << endl;
#endif
    gps_allocate(w,h,screen->w,screen->h,dispx_z,dispy_z);

    // Force redisplay
    gps.force_full_display_count = 1;
    // Calculate viewport origin, for centering
    origin_x = (screen->w - dispx_z * w) / 2;
    origin_y = (screen->h - dispy_z * h) / 2;

    // Reset TTF rendering
    //ttf_manager.init(dispy_z, dispx_z);
  }

private:
  
  void set_fullscreen() {
    if (enabler.is_fullscreen()) {
      init.display.actual_windowed_width = screen->w;
      init.display.actual_windowed_height = screen->h;
      resize(init.display.actual_fullscreen_width,
             init.display.actual_fullscreen_height);
    } else {
      resize(init.display.actual_windowed_width, init.display.actual_windowed_height);
    }
  }

	void get_current_interface_tile_dims(int32_t &cur_tx,int32_t &cur_ty)
		{
		cur_tx=dispx_z;
		cur_ty=dispy_z;
		}

  bool get_precise_mouse_coords(int &px, int &py, int &x, int &y) {
    int mouse_x, mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    mouse_x -= origin_x; mouse_y -= origin_y;
    if (mouse_x < 0 || mouse_x >= dispx_z*dimx ||
        mouse_y < 0 || mouse_y >= dispy_z*dimy)
      return false;
    px = mouse_x;
    py = mouse_y;
    x = mouse_x / dispx_z;
    y = mouse_y / dispy_z;
    return true;
  }

  void zoom(zoom_commands cmd) {
    pair<int,int> before = compute_zoom(true);
    int before_steps = zoom_steps;
    switch (cmd) {
    case zoom_in:    zoom_steps -= init.input.zoom_speed; break;
    case zoom_out:   zoom_steps += init.input.zoom_speed; break;
    case zoom_reset:
      zoom_steps = 0;
    case zoom_resetgrid:
      compute_forced_zoom();
      break;
    }
    pair<int,int> after = compute_zoom(true);
    if (after == before && (cmd == zoom_in || cmd == zoom_out))
      zoom_steps = before_steps;
    else
      reshape(after);
  }
  
};

class renderer_2d : public renderer_2d_base {
public:
  renderer_2d() {
    // Disable key repeat
    SDL_EnableKeyRepeat(0, 0);
    // Set window title/icon.
    SDL_WM_SetCaption(GAME_TITLE_STRING, NULL);
    SDL_Surface *icon = IMG_Load("data/art/icon.png");
    if (icon != NULL) {
      SDL_WM_SetIcon(icon, NULL);
      // The icon's surface doesn't get used past this point.
      SDL_FreeSurface(icon); 
    }
    
    // Find the current desktop resolution if fullscreen resolution is auto
	init.display.actual_windowed_width = init.display.desired_windowed_width;
	init.display.actual_windowed_height = init.display.desired_windowed_height;
	init.display.actual_fullscreen_width = init.display.desired_fullscreen_width;
	init.display.actual_fullscreen_height = init.display.desired_fullscreen_height;
    if (init.display.desired_fullscreen_width  == 0 ||
        init.display.desired_fullscreen_height == 0) {
      const struct SDL_VideoInfo *info = SDL_GetVideoInfo();
      init.display.actual_fullscreen_width = info->current_w;
      init.display.actual_fullscreen_height = info->current_h;
    }

	//verify full screen value against available modes
	SDL_PixelFormat fmt;
		fmt.palette = NULL;
		fmt.BitsPerPixel = 32;
		fmt.BytesPerPixel = 4;
		fmt.Rloss = fmt.Gloss = fmt.Bloss = fmt.Aloss = 0;
	#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		fmt.Rshift = 24; fmt.Gshift = 16; fmt.Bshift = 8; fmt.Ashift = 0;
	#else
		fmt.Rshift = 0; fmt.Gshift = 8; fmt.Bshift = 16; fmt.Ashift = 24;
	#endif
		fmt.Rmask = 255 << fmt.Rshift;
		fmt.Gmask = 255 << fmt.Gshift;
		fmt.Bmask = 255 << fmt.Bshift;
		fmt.Amask = 255 << fmt.Ashift;
		fmt.colorkey = 0;
		fmt.alpha = 255;
	Uint32 flags = (SDL_SWSURFACE|SDL_FULLSCREEN);

	bool good=false;
	int32_t backup_fullscreen_width=0;
	int32_t backup_fullscreen_height=0;
	SDL_Rect **modes=SDL_ListModes(&fmt,flags);
	if(modes==NULL);
	else if(modes==(SDL_Rect **)-1);
	else
		{
		int32_t i=0;
		while(modes[i])
			{
			if(modes[i]->w>=MINIMUM_WINDOW_WIDTH&&modes[i]->h>=MINIMUM_WINDOW_HEIGHT)
				{
				if(backup_fullscreen_width==0)
					{
					backup_fullscreen_width=modes[i]->w;
					backup_fullscreen_height=modes[i]->h;
					}
				if(init.display.actual_fullscreen_width==modes[i]->w&&
					init.display.actual_fullscreen_height==modes[i]->h)
					{
					good=true;
					break;
					}
				}

			++i;
			}
		}
	if(!good&&backup_fullscreen_width!=0)
		{
		init.display.actual_fullscreen_width=backup_fullscreen_width;
		init.display.actual_fullscreen_height=backup_fullscreen_height;
		}

    // Initialize our window
    bool worked = init_video(enabler.is_fullscreen() ?
                             init.display.actual_fullscreen_width :
                             init.display.actual_windowed_width,
                             enabler.is_fullscreen() ?
                             init.display.actual_fullscreen_height :
                             init.display.actual_windowed_height);

    // Fallback to windowed mode if fullscreen fails
    if (!worked && enabler.is_fullscreen()) {
      enabler.fullscreen = false;
      report_error("SDL initialization failure, trying windowed mode", SDL_GetError());
      worked = init_video(init.display.actual_windowed_width,
                          init.display.actual_windowed_height);
    }
    // Quit if windowed fails
    if (!worked) {
      report_error("SDL initialization failure", SDL_GetError());
      exit(EXIT_FAILURE);
    }
  }
};

class renderer_offscreen : public renderer_2d_base {
  virtual bool init_video(int, int);
public:
  virtual ~renderer_offscreen();
  renderer_offscreen(int, int);
  void update_all(int, int);
  void save_to_file(const string &file);
};
