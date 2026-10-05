// Presentation-only, supersampled floor/wall diagram. No geometry is modified.
#ifndef _REMASTER_ROOM_DEBUG_VIEW_H_
#define _REMASTER_ROOM_DEBUG_VIEW_H_
#include "frame.h"
#include "held_lamp.h"
#include <map>
#include <tuple>

inline uint32_t S9xRemasterRoomFloorColor(int id, uint16_t room)
{
	// A hue per support identity, including disconnected floors at equal height.
	uint32_t h = (uint32_t(id) + uint32_t(room) * 16381u) * 2654435761u;
	const int hue = h % 1536, sector = hue / 256, t = hue % 256;
	const uint8_t a = 230, b = 65, up = b + (a-b)*t/255, down = a-(a-b)*t/255;
	const uint8_t rgb[6][3] = {{a,up,b},{down,a,b},{b,a,up},{b,down,a},{up,b,a},{a,b,down}};
	return (rgb[sector][0]<<16) | (rgb[sector][1]<<8) | rgb[sector][2];
}

inline std::vector<uint8_t> S9xRemasterDrawRoomDebugView(const RemasterFrame &frame,
	const uint8_t *rgba, int scale = 4)
{
	const int w = frame.width, h = frame.height, outW = w*scale;
	std::vector<uint8_t> out(size_t(outW)*h*scale*4);
	struct Tile { int x,y; RemasterRoomPixelDiagnostic d; };
	std::map<std::tuple<int,int,int,int>,Tile> tiles;
	auto diagnostic = [&](size_t i) -> const RemasterRoomPixelDiagnostic * {
		if(i>=frame.mainPixels.size()) return nullptr;
		if(S9xRemasterHeldLampHudPixel(frame,i)) return nullptr;
		const bool sub = !frame.mainPixels[i].instanceId && i<frame.subPixels.size();
		const auto &pixels = sub ? frame.subPixels : frame.mainPixels;
		const auto &ds = sub ? frame.subRoomDiagnostics : frame.mainRoomDiagnostics;
		if(i>=ds.size() || !pixels[i].instanceId || pixels[i].instanceId>frame.tileInstances.size()) return nullptr;
		const auto &instance=frame.tileInstances[pixels[i].instanceId-1];
		if(instance.source!=RemasterSourceType::Background || instance.sourceIndex>1) return nullptr;
		return &ds[i];
	};
	for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
		const size_t i=size_t(y)*w+x;
		const auto *d=diagnostic(i);
		bool wall=false;
		if(d) {
			wall=d->planes[0].wall || d->planes[1].wall;
			if(wall) {
				const bool sub=!frame.mainPixels[i].instanceId;
				const auto &p=sub?frame.subPixels[i]:frame.mainPixels[i];
				const auto &instance=frame.tileInstances[p.instanceId-1];
				int tx=p.tilePixel%8,ty=p.tilePixel/8;
				if(instance.hFlip)tx=7-tx;
				if(instance.vFlip)ty=7-ty;
				const auto &identity=d->planes[0];
				tiles.emplace(std::make_tuple(x-tx,y-ty,identity.room,identity.background),Tile{x-tx,y-ty,*d});
			}
		}
		for(int sy=0;sy<scale;sy++)for(int sx=0;sx<scale;sx++) {
			const size_t at=(size_t(y*scale+sy)*outW+x*scale+sx)*4;
			uint32_t color=0; bool tint=false;
			if(d) {
				const int plane=d->visiblePlane<2 ? d->visiblePlane : (sx+sy<scale ? 0:1);
				const auto &v=d->planes[plane];
				if(wall) { color=0x555866; tint=true; }
				else if(v.floorId>=0) {
					color=S9xRemasterRoomFloorColor(v.floorId,v.room);tint=true;
					// Keep the identified floor color even when its elevation failed.
					if((v.heightConflict || v.floorBase==255) && ((x+y)&7)==0)color=0xff5577;
				}
				else {color=((x+y)&3)?0x77304a:0xff4080;tint=true;}
			}
			for(int c=0;c<3;c++)out[at+c]=tint ? (rgba[i*4+c]+3*((color>>(16-8*c))&255))/4:rgba[i*4+c];
			out[at+3]=255;
		}
	}
	for(const auto &entry:tiles) {
		const Tile &tile=entry.second;
		const auto &a=tile.d.planes[0], &b=tile.d.planes[1];
		const auto &v=tile.d.visiblePlane<2 ? tile.d.planes[tile.d.visiblePlane] : (a.wall?a:b);
		const bool ambiguous=tile.d.visiblePlane>=2 && a.wall && b.wall &&
			(a.lowerFloorDistance!=b.lowerFloorDistance || a.lowerFloorId!=b.lowerFloorId);
		const int distance=ambiguous ? -1:v.lowerFloorDistance;
		auto dot=[&](int x,int y,uint32_t color) {
			x+=tile.x*scale;y+=tile.y*scale;
			if(x<0 || y<0 || x>=outW || y>=h*scale)return;
			const auto *d=diagnostic(size_t(y/scale)*w+x/scale);
			if(!d || (!d->planes[0].wall && !d->planes[1].wall) ||
				d->planes[0].cell!=a.cell || d->planes[0].room!=a.room || d->planes[0].background!=a.background)return;
			const size_t at=(size_t(y)*outW+x)*4;
			out[at]=color>>16;out[at+1]=color>>8;out[at+2]=color;
		};
		auto line=[&](int x,int y,int xx,int yy,uint32_t color) {
			const int n=std::max(abs(xx-x),abs(yy-y));
			for(int j=0;j<=n;j++) dot(x+(xx-x)*j/std::max(1,n),y+(yy-y)*j/std::max(1,n),color);
		};
		// Place arrows beside, rather than beneath, the distance label.
		static const int dx[]={0,0,-1,1,-1,1,-1,1},dy[]={-1,1,0,0,-1,-1,1,1};
		const int cx=2*scale,cy=4*scale,r=2*scale;
		// Height conflicts and ambiguous structural paths are different failures.
		// Preserve any known distance number; mark the tile edge independently.
		if(v.heightConflict)line(scale,scale,7*scale-1,scale,0xff5577);
		if(v.axisUnknown || !v.uphillDirections) {
			line(cx-2,cy-2,cx+2,cy+2,0xffb040);
			line(cx-2,cy+2,cx+2,cy-2,0xffb040);
		}
		for(int k=0;k<8;k++)if(v.uphillDirections&(1<<k)) {
			for(int sign:{-1,1}) {
				const int ex=cx+sign*dx[k]*r,ey=cy+sign*dy[k]*r;
				const uint32_t color=sign>0?0x40ffff:0xffb040;
				line(cx,cy,ex,ey,color);
				const int bx=ex-sign*dx[k]*3,by=ey-sign*dy[k]*3;
				line(ex,ey,bx-dy[k]*2,by+dx[k]*2,color);
				line(ex,ey,bx+dy[k]*2,by-dx[k]*2,color);
			}
		}
		// 3x5 glyphs, doubled: legible at normal window scale without hiding arrows.
		static const uint16_t glyphs[]={0x7b6f,0x2492,0x73e7,0x73cf,0x5bc9,0x79cf,0x79ef,0x7249,0x7bef,0x7bcf,0x7282,0x2482};
		std::string text=distance==-2?"!":distance<0?"?":std::to_string(distance);
		const int gs=std::max(1,scale/2),start=8*scale-int(text.size())*4*gs;
		for(size_t j=0;j<text.size();j++) {
			const int glyph=text[j]=='?'?10:text[j]=='!'?11:text[j]-'0';
			for(int gy=0;gy<5;gy++)for(int gx=0;gx<3;gx++)if(glyphs[glyph]&(1<<(14-gy*3-gx)))
				for(int oy=0;oy<gs;oy++)for(int ox=0;ox<gs;ox++) {
					int x=start+int(j)*4*gs+gx*gs+ox,y=cy-5*gs/2+gy*gs+oy;
					for(int yy=-1;yy<=1;yy++)for(int xx=-1;xx<=1;xx++)dot(x+xx,y+yy,0x111118);
				}
			for(int gy=0;gy<5;gy++)for(int gx=0;gx<3;gx++)if(glyphs[glyph]&(1<<(14-gy*3-gx)))
				for(int oy=0;oy<gs;oy++)for(int ox=0;ox<gs;ox++)dot(start+int(j)*4*gs+gx*gs+ox,cy-5*gs/2+gy*gs+oy,distance==-2?0xff5577:0xffffff);
		}
	}
	return out;
}
#endif
