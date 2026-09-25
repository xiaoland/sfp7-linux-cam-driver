#include "actual-functions.inc"
#include <cassert>
#include <iostream>
#include <limits>
int main()
{
 unsigned tests=0;
 IPAContext c{};c.configuration.agc={2,2462,1,16,.15};
 auto same=[](const IPAFrameContext&a,const IPAFrameContext&b){return a.sensor.exposure==b.sensor.exposure&&a.sensor.gain==b.sensor.gain;};
 for(double m:{0.,-1.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}) {
  candidate::Agc a;IPAFrameContext f{{32,1}},before=f;a.updateExposure(c,f,m);assert(same(f,before)&&a.startup_&&a.startupComputations_==0);tests++;
 }
 {candidate::Agc a;IPAFrameContext f{{32,1}},before=f;SwIspStats stats;ControlList metadata;a.process(c,0,f,&stats,metadata);assert(same(f,before)&&a.startupComputations_==0);tests++;}
 {candidate::Agc a;IPAFrameContext f{{632,16}},before=f;a.updateExposure(c,f,2.3);assert(same(f,before)&&!a.startup_&&a.startupComputations_==1);tests++;}
 {candidate::Agc a;IPAFrameContext f{{100,1}};a.updateExposure(c,f,2.299);assert(a.startup_&&f.sensor.exposure==108);tests++;}
 {candidate::Agc a;IPAFrameContext f{{100,1}};a.updateExposure(c,f,1);assert(a.startup_&&f.sensor.exposure==200);tests++;}
 {candidate::Agc a;baseline::Agc old;IPAFrameContext f{{632,16}},copy=f;a.updateExposure(c,f,1);old.updateExposure(c,copy,1);assert(f.sensor.exposure==1264&&copy.sensor.exposure==758);tests++;}
 {candidate::Agc a;IPAFrameContext f{{632,16}},g=f;a.updateExposure(c,f,1);a.updateExposure(c,g,1);assert(same(f,g)&&f.sensor.exposure==1264&&a.startupComputations_==2);tests++;}
 {candidate::Agc a;IPAFrameContext f{{1264,16}};a.updateExposure(c,f,1);assert(f.sensor.exposure==2462&&f.sensor.gain==16&&!a.startup_);tests++;}
 {candidate::Agc a;IPAFrameContext f{{2462,4}};a.updateExposure(c,f,1);assert(f.sensor.exposure==2462&&f.sensor.gain==8);tests++;}
 {IPAContext front{};front.configuration.agc={1,2070,1./16,127./16,126./1600};candidate::Agc a;IPAFrameContext f{{2070,1./16}};a.updateExposure(front,f,1);assert(std::abs(f.sensor.gain-(1./16+126./1600))<1e-12);tests++;}
 {candidate::Agc a;IPAFrameContext f{{2462,16}},before=f;a.updateExposure(c,f,1);assert(same(f,before)&&!a.startup_);tests++;}
 {candidate::Agc a;
  for(unsigned i=1;i<=96;i++){IPAFrameContext f{{32,1}};a.updateExposure(c,f,1);assert(f.sensor.exposure==64&&a.startupComputations_==i&&a.startup_==(i<96));}
  IPAFrameContext f{{32,1}};a.updateExposure(c,f,1);assert(f.sensor.exposure==35&&!a.startup_&&a.startupComputations_==96);tests++;
 }
 {candidate::Agc a;
  for(unsigned i=0;i<300;i++){IPAFrameContext f{{32,1}};a.updateExposure(c,f,0);assert(f.sensor.exposure==32&&a.startupComputations_==0);}
  for(unsigned i=1;i<=96;i++){IPAFrameContext empty{{32,1}},valid=empty;a.updateExposure(c,empty,0);a.updateExposure(c,valid,1);assert(a.startupComputations_==i);}
  assert(!a.startup_);tests++;
 }
 {candidate::Agc a;IPAFrameContext f{{632,16}},before=f;a.updateExposure(c,f,2.3);assert(!a.startup_);a.configure(c,IPAConfigInfo{});assert(a.startup_&&a.startupComputations_==0&&same(f,before));a.updateExposure(c,f,1);assert(a.startup_&&f.sensor.exposure==1264);tests++;}
 {baseline::Agc old;candidate::Agc a;old.startup_=false;a.startup_=false;for(double m:{1.,2.,2.5,3.,5.})for(int e:{2,32,2000,2462})for(double g:{1.,2.,16.}){IPAFrameContext f{{e,g}},copy=f;old.updateExposure(c,f,m);a.updateExposure(c,copy,m);assert(same(f,copy));tests++;}}
 std::cout<<"PASS "<<tests<<" actual configure/process/update cases; deficit scaling, threshold exit, 96 valid calls and 300 empty calls included\n";
}
