#include "lighting_field_cache.h"
#include "field_upload_cache.h"
#include "surface_mesh_cache.h"
#include <cassert>
#include <chrono>
#include <iostream>

// Independent original full per-pixel preparation, kept as a correctness oracle.
static RemasterLightingFieldCache reference(const RemasterFrame &f, bool scene)
{
	RemasterLightingFieldCache r;
	const size_t n=f.mainPixels.size();
	r.occlusion.assign(n*2,0);r.emission.assign(n*4,0);r.height.assign(n*2,0);
	r.participation.assign(n*2,255);r.opposite.assign(n,0);r.authoredNormals.assign(n,0);
	r.surface.assign(n*4,0);r.reflectance.assign(n*4,0);r.depths.assign(n,0);r.omnidirectional.assign(n,0);r.samples.resize(n);
	for(size_t i=0;i<n;i++) {
		const bool hud=S9xRemasterHeldLampHudPixel(f,i);
		if(hud){r.participation[i*2]=r.participation[i*2+1]=0;
			if(!scene || f.scenePixels[i].owner==f.mainPixels[i].owner ||
				(uint8_t(f.scenePixels[i].owner>>24)==uint8_t(RemasterSourceType::Background) && ((f.scenePixels[i].owner>>16)&255)==2))continue;}
		const auto &main=scene?f.scenePixels[i]:f.mainPixels[i];
		const auto &p=!scene && !main.instanceId && uint8_t(main.owner>>24)==uint8_t(RemasterSourceType::Backdrop)?f.subPixels[i]:main;
		if ((f.area.id==320 || f.area.id==321) &&
			f.profileRomSha256=="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb" &&
			!main.instanceId && uint8_t(main.owner>>24)==uint8_t(RemasterSourceType::Backdrop) &&
			!f.subPixels[i].instanceId) {
			r.height[i*2+1]=1;r.occlusion[i*2]=r.occlusion[i*2+1]=255;r.surface[i*4+3]=1;
			r.samples[i].known=r.samples[i].sheet=r.samples[i].fallbackGround=true;
			r.samples[i].coverage=1;r.samples[i].domain=uint64_t(3)<<48;
		}
		if(!p.instanceId || p.instanceId>f.tileInstances.size() || p.tilePixel>=64)continue;
		const auto &in=f.tileInstances[p.instanceId-1];
		const auto *original=S9xRemasterFrameMetadataForTile(f,in.tileId);
		const auto resolved=S9xRemasterResolveInstanceGeometry(original?*original:RemasterFrameAssetMetadata{},in);
		const auto *m=&resolved;
		const RemasterFrameMaterial *mat=nullptr;
		std::string name=in.material;
		if(m && m->hasMaterialSelectors && !m->materialSelectors[p.tilePixel].empty())name=m->materialSelectors[p.tilePixel];
		for(const auto &candidate:f.materials)if(candidate.name==name){mat=&candidate;break;}
		RemasterSurfaceMesh::classify(r.samples[i],in.source,in.sourceIndex,mat?mat->surfaceClass:RemasterSurfaceClass::Unclassified,in.assetGroup,p.instanceId,m->tileRole,in.geometryStatus!=RemasterOccurrenceGeometryStatus::Inherit);
		r.samples[i].wallBase=in.heightOffset/255.0f*f.lightingCoordinateScale;
		r.depths[i]=m?m->emissionDepth:0;r.omnidirectional[i]=m && m->emissionOmnidirectional?1:0;
		if(mat && mat->surfaceClass==RemasterSurfaceClass::UserInterface)r.participation[i*2]=r.participation[i*2+1]=0;
		else if(mat && !mat->receivesGi)r.participation[i*2+1]=0;
		if(mat && mat->hasDiffuseReflectance){std::copy(mat->diffuseReflectance.begin(),mat->diffuseReflectance.end(),r.reflectance.begin()+i*4);r.reflectance[i*4+3]=1;}
		const bool unresolved=S9xRemasterInstanceGeometryUnresolved(in);
		const bool fallback=!unresolved && in.source==RemasterSourceType::Background && (r.participation[i*2]!=0 || hud) && !m->hasHeight;
		if(fallback){r.samples[i].fallbackGround=true;r.samples[i].sheet=true;r.samples[i].solidWall=false;r.samples[i].domain=(uint64_t(3)<<48)|uint32_t(in.heightOffset);}
		r.occlusion[i*2]=unresolved?0:m->hasOcclusion?m->occlusion[p.tilePixel]:255;r.occlusion[i*2+1]=unresolved?0:255;
		r.opposite[i]=m && m->directLightingOppositeFacing?255:0;
		if(!hud && m && m->hasEmission)std::copy_n(m->emissionRgba.begin()+p.tilePixel*4,4,r.emission.begin()+i*4);
		if(!unresolved && (m->hasHeight||in.heightOffset||fallback)){const auto local=m->hasHeight?m->height[p.tilePixel]:0;r.height[i*2]=S9xRemasterSignedHeightField(in.heightOffset,local);r.height[i*2+1]=1;}
		r.surface[i*4]=r.height[i*2]*f.lightingCoordinateScale;
		if(fallback)r.surface[i*4+3]=1;
		if(m && m->hasNormals){r.authoredNormals[i]=1;float x=m->normalXyz[p.tilePixel*3]/127.5f-1,y=m->normalXyz[p.tilePixel*3+1]/127.5f-1,z=m->normalXyz[p.tilePixel*3+2]/127.5f-1;
			if(S9xRemasterContinuedDiagonalNormalMask(in,m)&(UINT64_C(1)<<p.tilePixel))x=y=z=1;
			S9xRemasterTransformNormalForTileInstance(in,x,y,z,m->normalIgnoreTileTransform);float length=std::sqrt(x*x+y*y+z*z);
			r.surface[i*4+1]=length>0.0001f?x/length:0;r.surface[i*4+2]=length>0.0001f?y/length:0;r.surface[i*4+3]=length>0.0001f?z/length:1;}
		r.samples[i].height=r.surface[i*4];r.samples[i].known=r.height[i*2+1]!=0 && (r.participation[i*2]!=0 || hud);r.samples[i].coverage=r.occlusion[i*2]/255.0f;
	}
	return r;
}
static void compare(const RemasterLightingFieldCache &a,const RemasterLightingFieldCache &b)
{
	assert(a.occlusion==b.occlusion && a.emission==b.emission && a.height==b.height && a.participation==b.participation);
	assert(a.opposite==b.opposite && a.authoredNormals==b.authoredNormals && a.surface==b.surface && a.reflectance==b.reflectance);
	assert(a.depths==b.depths && a.omnidirectional==b.omnidirectional && a.samples.size()==b.samples.size());
	for(size_t i=0;i<a.samples.size();i++){const auto &x=a.samples[i],&y=b.samples[i];assert(x.height==y.height && x.coverage==y.coverage && x.known==y.known && x.sheet==y.sheet && x.domain==y.domain && x.solidWall==y.solidWall && x.wallBase==y.wallBase && x.fallbackGround==y.fallbackGround);}
}
int main(int argc,char **argv)
{
	RemasterFieldUploadCache uploads;
	std::vector<uint8_t> data(64,0),gpu(64,255);size_t calls=0;
	auto upload=[&](size_t row,size_t rows){calls++;std::copy_n(data.begin()+row*8,rows*8,gpu.begin()+row*8);};
	assert(uploads.update(data.data(),8,8,upload));assert(gpu==data);
	assert(!uploads.update(data.data(),8,8,upload));assert(calls==1);
	data[9]=1;data[17]=2;assert(uploads.update(data.data(),8,8,upload));assert(calls==2 && gpu==data);
	data[63]=3;uploads.update(data.data(),8,8,upload);assert(gpu==data);
	// Three independent resource slots must catch up to their own older versions.
	std::array<RemasterFieldUploadCache,3> slots;
	std::array<std::vector<uint8_t>,3> slotBytes;
	for(auto &bytes:slotBytes)bytes.resize(data.size());
	for(int frame=0;frame<18;frame++) {
		data[(frame*7)%data.size()]++;
		const int slot=frame%3;
		slots[slot].update(data.data(),8,8,[&](size_t row,size_t rows){std::copy_n(data.begin()+row*8,rows*8,slotBytes[slot].begin()+row*8);});
		assert(slotBytes[slot]==data);
	}
	for(int arg=1;arg<argc;arg++) {
		auto f=std::make_unique<RemasterFrame>();assert(S9xReadRemasterFrame(argv[arg],*f));
		RemasterLightingFieldCache cache;
		RemasterSurfaceMesh::Cache meshCache;
		auto check=[&](bool scene=false){
			cache.update(*f,scene);auto expected=reference(*f,scene);
			// Renderer writes derived normals back into retained fields each frame.
			const auto &mesh=meshCache.update(f->width,f->height,cache.samples,f->lightingCoordinateScale/255.0f);
			const auto fullMesh=RemasterSurfaceMesh::build(f->width,f->height,expected.samples,f->lightingCoordinateScale/255.0f);
			assert(mesh.size()==fullMesh.size());
			for(size_t i=0;i<mesh.size();i++) {
				assert(std::memcmp(&mesh[i],&fullMesh[i],sizeof(mesh[i]))==0);
				if(!cache.authoredNormals[i])std::copy_n(mesh[i].normal,3,cache.surface.begin()+i*4+1);
				if(!expected.authoredNormals[i])std::copy_n(fullMesh[i].normal,3,expected.surface.begin()+i*4+1);
			}
			compare(cache,expected);
		};
		check();check();assert(cache.updatedPixels==0);
		if (std::getenv("REMASTER_FIELD_BENCHMARK")) {
			for(bool full:{true,false}) {
				std::vector<double> times;
				for(int k=0;k<190;k++) {const auto start=std::chrono::steady_clock::now();cache.update(*f,false,full);
					if(k>=10)times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}
				std::sort(times.begin(),times.end());std::cout<<"field preparation full="<<full<<" median_ms="<<times[90]<<" p95_ms="<<times[171]<<" updated="<<cache.updatedPixels<<'\n';
			}
		}
		if(!f->assetMetadata.empty()){auto &m=f->assetMetadata[0];m.hasEmission=true;m.emissionRgba.fill(100);m.emissionDepth+=2;check();assert(cache.updatedPixels==0);}
		for(size_t j=0;j<std::min(size_t(20),f->tileInstances.size());j++) {
			auto &in=f->tileInstances[j];in.heightOffset-=13;in.hFlip=!in.hFlip;in.normalYaw++;check();
			in.hasPlacementHeight=true;in.placementHeight.fill(21);check();
		}
		if(!f->assetMetadata.empty()){auto &m=f->assetMetadata[0];m.hasNormals=true;m.normalXyz.fill(220);m.hasOcclusion=true;m.occlusion.fill(127);check();}
		if(!f->materials.empty()){f->materials[0].receivesGi=!f->materials[0].receivesGi;check();}
		f->lightingCoordinateScale*=2;check();
		if(f->scenePixels.size()==f->mainPixels.size())check(true);
		for(auto &p:f->mainPixels){p.instanceId=0;p.owner=uint32_t(RemasterSourceType::Backdrop)<<24;}check();
		std::cout<<argv[arg]<<" mutation/full-field equivalence PASS\n";
	}
	std::cout<<"exact dirty-row uploads PASS\n";
}
