// Read-only completion of the USA dungeon's incremental collision builder.
// The game spreads this work across the scroll; geometry needs it before the
// first destination pixel. Never write emulated RAM or reuse source collision.
// Attribute rules adapted from snesrev/zelda3 dungeon.c (MIT; see
// tools/ALttP/zelda3-MIT-LICENSE.txt, copyright 2022 snesrev, 2021 elzo_d).
#ifndef REMASTER_DUNGEON_TRANSITION_ATTRIBUTES_H
#define REMASTER_DUNGEON_TRANSITION_ATTRIBUTES_H
#include <array>
#include <cstdint>

inline bool S9xRemasterCompleteDungeonAttributes(const uint8_t *ram,
	std::array<uint8_t,8192> &out)
{
	if(!ram)return false;
	auto word=[&](int a){return int(ram[a]|ram[a+1]<<8);};
	bool valid=true;
	auto put=[&](int bg,int j,int v){
		if(j<0 || bg*4096+j>=8191){valid=false;return;}
		out[bg*4096+j]=uint8_t(v);out[bg*4096+j+1]=uint8_t(v>>8);
	};
	auto both=[&](int j,int v){put(0,j,v);put(1,j,v);};
	auto table=[&](int a,int i){if(i<0 || i>=64){valid=false;return 0;}return word(a+i);};
	for(int i=0;i<8192;i++) {
		int tile=word(0x2000+i*2),a=ram[0xfe00+(tile&1023)];
		if(a>=0x10 && a<0x1c)a|=tile>>14;
		out[i]=uint8_t(a);
	}
	// All counters are byte offsets into bounded word tables. Reject corrupt or
	// unsupported inventories rather than publishing a plausible partial room.
	auto count=[&](int a,int max=64){int n=word(a);if((n&1) || n>max){valid=false;return 0;}return n;};
	for(int i=0,n=count(0x432,16);i<n;i+=2){int j=table(0x6a0,i);put(0,j,0x3b3b);put(0,j+64,0x3b3b);}
	int i=0,t=0x3030;
	const int ends[]={0x438,0x47e,0x482,0x4a2,0x4a4,0x43a,0x480,0x484,0x4a6,0x4a8};
	for(int kind=0;kind<10;kind++) {
		if(kind==5)t=(t&0x707)|0x3434;
		int end=count(ends[kind],8);if(end<i){valid=false;break;}
		for(;i<end;i+=2,t+=0x101){int j=table(0x6b0,i);
			if(kind==0){put(0,j+129,0);put(0,j+1,0x2626);put(0,j+65,t);}
			else if(kind==1 || kind==2 || kind==6 || kind==7){int a=(kind==1 || kind==6)?0x5e5e:0x5f5f;put(0,j+1,a);put(0,j+129,a);put(0,j+193,a);put(0,j+65,t);}
			else if(kind==3 || kind==8){put(0,j+1,0x3838);put(0,j+129,0);put(0,j+193,0);put(0,j+65,t);}
			else if(kind==4 || kind==9){put(0,j+1,0);put(0,j+65,0);put(0,j+129,t);put(0,j+193,0x3939);}
			else {put(0,j+129,t);put(0,j+193,0x2626);}
		}
	}
	auto stair=[&](int j,int a,bool second){
		put(second?1:0,j,2);put(second?0:1,j+192,2);
		put(second?1:0,j+2,0x200);put(second?0:1,j+194,0x200);
		put(second?1:0,j+64,1);put(second?0:1,j+128,1);
		put(second?1:0,j+66,0x100);put(second?0:1,j+130,0x100);
		both(j+65,a);both(j+129,a);
	};
	i=0;int end=count(0x43c,32),a=0x1f1f;
	if(!end){end=count(0x43e,32);a=0x1e1e;if(!end){end=count(0x440,32);a=0x1d1d;}}
	for(;i<end;i+=2)stair(table(0x6b8,i),a,false);
	end=count(0x448,32);if(end && end<i)valid=false;
	for(;i<end;i+=2){int j=table(0x6b8,i);both(j,0xa03);both(j+2,0x30a);put(0,j+64,0x803);put(0,j+66,0x308);}
	i=0;end=count(0x442,32);
	for(;i<end;i+=2){int j=table(0x6b8,i);put(0,j,3);put(0,j+2,0x300);put(1,j,0xa03);put(1,j+2,0x30a);put(0,j+64,0x808);put(0,j+66,0x808);}
	end=count(0x444,32);if(end && end<i)valid=false;
	for(;i<end;i+=2){int j=table(0x6b8,i);put(0,j,3);put(0,j+2,0x300);put(1,j,0xa03);put(1,j+2,0x30a);}
	i=0;t=0x7070;end=count(0x42c);
	for(;i<end;i+=2,t+=0x101)if((table(0x500,i)&0xf0)!=0x30){int j=(table(0x540,i)&0x3fff)>>1;put(0,j,t);put(0,j+64,t);}
	end=count(0x42e);if(end && end<i)valid=false;
	for(t=0xc0c0;i<end;i+=2,t=(t&0xefef)+0x101){int j=(table(0x540,i)&0x3fff)>>1;put(0,j,t);put(0,j+64,t);}
	i=0;t=0x5858;
	auto specialTag=[](int tag){return tag==0x27 || tag==0x3c || tag==0x3e || (tag>=0x29 && tag<0x33);};
	if(!specialTag(ram[0xae]) && !specialTag(ram[0xaf])) {
		end=count(0x496,12);
		for(;i<end;i+=2,t+=0x101){int k=table(0x6e0,i);if(!k)continue;int j=(k&0x7fff)>>1;put(0,j,t);put(0,j+64,t);if(k&0x8000){put(0,j+66,t);put(0,j+128,t);put(0,j+130,t);}}
		end=count(0x498,12);if(end && end<i)valid=false;
		for(;i<end;i+=2,t+=0x101){int j=(table(0x6e0,i)&0x7fff)>>1;put(0,j,t);put(0,j+64,t);}
	}
	i=0;end=count(0x49a,32);a=0x3f3f;
	if(!end){end=count(0x49c,32);a=0x3e3e;if(!end){end=count(0x49e,32);a=0x3d3d;}}
	for(;i<end;i+=2)stair(table(0x6ec,i),a,true);
	for(i=0,end=count(0x4ae,32);i<end;i+=2){int j=table(0x6ec,i);put(1,j+192,0xa03);put(1,j+194,0x30a);put(0,j+192,3);put(0,j+194,0x300);put(0,j+128,0x808);put(0,j+130,0x808);}
	static const uint16_t doorAttrs[]={0x8080,0x8484,0,0x101,0x8484,0x8e8e,0,0,0x8888,0x8e8e,0x8080,0x8080,0x8282,0x8080,0x8080,0x8080,0x8080,0x8080,0x8080,0x8080,0x8282,0x8e8e,0x8080,0x8282,0x8080,0x8080,0x8080,0x8282,0x8282,0x8080,0x8080,0x8080,0x8484,0x8484,0x8686,0x8888,0x8686,0x8686,0x8080,0x8080};
	for(int k=0;k<16;k++) {
		int address=word(0x19a0+k*2);if(!address)continue;
		int type=word(0x1980+k*2)&0xfe,dir=word(0x19c0+k*2)&3;
		if(type/2>=40){valid=false;continue;}
		bool alpha=type==0 || type==6 || type==18 || type==10 || type==14;
		bool beta=type==12 || type==16 || type==4 || type==2 || type==8;
		if(type==48)continue;
		if(!alpha && !beta) {
			int bit=(type>=64 || type==24 || type==68)?k:k&7;
			beta=type==64 || type==70 || (word(0x68c)&(0x8000>>bit));
			if(!beta){int j=address>>1;put(0,j+65,(0xf0+k)*0x101);put(0,j+129,(0xf0+k)*0x101);continue;}
			// Open ordinary locked/shutter doors use alpha, high types beta.
			if(type<64){alpha=true;beta=false;}
		}
		if(alpha && type>=32 && type<=38)continue;
		int attr=doorAttrs[type/2],j=address>>1;
		bool exit=false;for(int e=0;e<4;e++)exit|=(beta && dir==1?address&0x1fff:address)==word(0x19e2+e*2);
		if((dir==0 && alpha && exit) || (dir==1 && (exit || (beta?(type==12 || type==16 || type==4):(type==10 || type==14)))))attr=0x8e8e;
		if(dir==0){j&=~0x7c0;for(int y=0;y<(beta?10:8);y++)put(0,j+1+y*64,!beta && y==7?0:attr);}
		else if(dir==1){for(int y=1;y<=(beta?8:5);y++)put(0,j+1+y*64,attr);}
		else if(beta){if(dir==2)j&=~31;else j++;for(int y=1;y<=2;y++)for(int x=0;x<8;x+=2)put(0,j+x+y*64,attr+0x101);}
		else {if(dir==2)j&=~31;for(int y=1;y<=2;y++){put(0,j+(dir==2?0:2)+y*64,attr+0x101);put(0,j+(dir==2?2:4)+y*64,attr+0x101);put(0,j+(dir==2?4:0)+y*64,(attr+0x101)&(dir==2?0xff:0xff00));}}
	}
	for(int kind=0;kind<2;kind++)for(i=0,end=count(kind?0x450:0x44e,16);i<end;i+=2){
		int j=table(kind?0x6d0:0x6c0,i);if(j<0 || j>=4031){valid=false;continue;}
		int bg=(out[j]&0xf0)==0x80?0:1,at=bg*4096+j;
		int v=(out[at]|out[at+1]<<8)|(kind?0x2020:0x1010);put(bg,j,v);put(bg,j+64,v);
	}
	if(word(0x4b0))valid=false; // Unimplemented original ChangeDoorToSwitch.
	if(word(0xc172))for(auto &v:out)if((v&~1)==0x66)v^=1;
	return valid;
}
#endif
