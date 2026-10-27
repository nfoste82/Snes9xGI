// Synthetic failure-class fixture; measures real visual projection/solve, not a
// claim about actual large-room or complete end-to-end performance.
#define main visible_room_regression_main
#include "../tests/dungeon_visible_face_runtime_test.cpp"
#undef main
#include <chrono>
int main(){
 auto fixture=std::make_unique<Fixture>(0,0,0,0);
 const auto frame=fixture->frame;
 std::cout<<"{\"synthetic\":true,\"placements\":8192,\"samples\":[";
 for(int mode=0;mode<2;mode++){
  auto cache=std::make_unique<RemasterVisibleFaceTemplateCache>();
  for(int i=0;i<101;i++){
   fixture->frame=frame;
   if(!mode)cache->modelValid=false;
   const auto a=std::chrono::steady_clock::now();fixture->run(cache.get());const auto b=std::chrono::steady_clock::now();
   std::cout<<(mode || i?",":"")<<"{\"reuse\":"<<mode<<",\"sample\":"<<i<<",\"ms\":"<<std::chrono::duration<double,std::milli>(b-a).count()<<",\"base\":"<<fixture->baseAt(fixture->first)<<",\"builds\":"<<cache->modelBuilds<<",\"hits\":"<<cache->modelHits<<"}";
  }
 }
 std::cout<<"]}\n";
}
