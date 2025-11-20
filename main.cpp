#include <iostream>
#include "InplaceAnyCopy.h"
using namespace std;

class Animale {
  public:
  virtual void verso()=0;
  void versoConst() const { printf("Const verso\n"); }
};
class Cane: public Animale {
  public:
  Cane(std::string name): name(name) {
    // printf("Created new Cane at %p\n", this);
  };
  ~Cane() {
    // printf("Destroy cane at %p\n", this);
  }
  void verso() { printf("Bau!"); }
  std::string name;
};
class Gatto: public Animale {
  public:
  Gatto() {
    // printf("Created new Gatto at %p\n", this);
  }
  Gatto(const Gatto &source): value(source.value) {
    // printf("Copy constructor gatto from %p to %p\n",&source, this);
  }
  Gatto(Gatto &&source): value(source.value) {
    // printf("Copy constructor gatto from %p to %p\n",&source, this);
  }
  ~Gatto() {
    // printf("Destroy Gatto at %p\n", this);
  }
  void verso() { printf("Miao!"); }
  uint64_t value;
};
class Giraffa: public Animale {
  void verso() { printf("Luuungo!"); }
  uint8_t data[200];
};
using GenericAnimale=InplaceAnyCopy<Animale, 64>;
GenericAnimale getCane(std::string nome) {
  return Cane{nome};
}
int main()
{
  InplaceAnyCopy<Animale, 64> bestia;
  // bestia.emplace<Cane>("Fido");
  // std::cout<<"Allocated class uses "<<bestia.allocated_size()<<"\n";
  //bestia.emplace<Gatto>();
  bestia=getCane("Fido");
  std::cout<<"Allocated class uses "<<bestia.allocated_size()<<"\n";
  InplaceAnyCopy<Animale, 64> bestia2;
  bestia2=std::move(bestia);
  //bestia2=bestia;
  std::cout<<"Bestia uses "<<bestia.allocated_size()<<"\n";
  std::cout<<"Bestia2 uses "<<bestia2.allocated_size()<<"\n";
  if(auto b2=bestia2.data()) {
    b2->verso();
  }
  const auto &bestia3(bestia2);
  bestia3->versoConst();
  return 0;
}
