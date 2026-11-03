#ifndef REMASTER_TERRAIN_OFFSET_H
#define REMASTER_TERRAIN_OFFSET_H
#include <array>
#include <cmath>

// World-oriented samples: artwork H/V transforms do not transform terrain.
// Height is additive relief on top of the existing scalar placement base.
struct RemasterTerrainOffset {
	bool hasHeight = false;
	std::array<float,64> height{};
	std::array<float,3> normal{{0,0,1}};
	bool hasPixelNormals = false;
	std::array<float,192> pixelNormals{};
	std::array<float,3> normalAt(unsigned pixel) const {
		if(!hasPixelNormals)return normal;
		return {{pixelNormals[pixel*3],pixelNormals[pixel*3+1],pixelNormals[pixel*3+2]}};
	}
	bool operator==(const RemasterTerrainOffset &b) const { return hasHeight==b.hasHeight && height==b.height && hasPixelNormals==b.hasPixelNormals && (hasPixelNormals?pixelNormals==b.pixelNormals:normal==b.normal); }
	bool operator!=(const RemasterTerrainOffset &b) const { return !(*this==b); }
	bool identity() const {
		for(float h:height)if(hasHeight && h!=0)return false;
		for(unsigned k=0;k<64;k++)if(normalAt(k)!=std::array<float,3>{{0,0,1}})return false;
		return true;
	}
};
inline bool S9xRemasterValidTerrainOffset(const RemasterTerrainOffset &v) {
	double length=0;for(float n:v.normal){if(!std::isfinite(n))return false;length+=double(n)*n;}
	if(std::abs(length-1)>0.002 || v.normal[2]<-0.999f)return false;
	if(v.hasPixelNormals)for(unsigned k=0;k<64;k++) {
		double squared=0;for(float n:v.normalAt(k)){if(!std::isfinite(n))return false;squared+=double(n)*n;}
		if(std::abs(squared-1)>0.002 || v.pixelNormals[k*3+2]<-0.999f)return false;
	}
	for(float h:v.height)if(!std::isfinite(h) || std::abs(h)>32767)return false;
	return true;
}
// Shortest-arc rotation +Z -> support normal, with no additional twist.
// R = I + [v]x + [v]x²/(1+nz), v = cross(+Z,n).
inline void S9xRemasterRotateTerrainNormal(const std::array<float,3> &n,float &x,float &y,float &z) {
	const float vx=-n[1],vy=n[0],a=vy*z,b=-vx*z,c=vx*y-vy*x;
	const float k=1/(1+n[2]);
	x+=a+vy*c*k;y+=b-vx*c*k;z+=c+(vx*b-vy*a)*k;
}
inline unsigned S9xRemasterTerrainPixel(unsigned canonical,bool h,bool v) {
	return (v?7-canonical/8:canonical/8)*8+(h?7-canonical%8:canonical%8);
}
#endif
