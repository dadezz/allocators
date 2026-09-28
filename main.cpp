#include <iostream>
#include <vector>
#include <list>
#include <string>
#include "output_alloc.hpp"

struct dummyStruct {
    int* p;
    bool a;
    int z;
    size_t u;
    std::string g;
};


int main() {
    std::cout <<"vectors \n";
    std::vector<int> v_std;
    std::vector<int, OutputAllocator<int>> v_out;
    
    std::cout <<"lists \n";
    std::list<int> l_std;
    std::list<int, OutputAllocator<int>> l_out;

    std::cout <<"dummy structs \n";
    std::list<dummyStruct> ds_std;
    std::list<dummyStruct, OutputAllocator<dummyStruct>> ds_out;

    for(int i = 0; i < 5;) {
        std::cout << "\nPush_back " << ++i << std::endl;

        std::cout <<"vectors \n";
        v_std.push_back(i);
        v_out.push_back(i);

        std::cout <<"lists \n";
        l_std.emplace_back(i);
        l_out.emplace_back(i);

        std::cout <<"dummyStructs \n";
        ds_std.emplace_back(dummyStruct{});
        ds_out.emplace_back(dummyStruct{});
    }

    std::cout<<"\nEOF" <<std::endl;
    return 0;
}