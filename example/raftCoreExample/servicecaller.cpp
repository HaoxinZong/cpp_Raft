#include"util.h"
#include<iostream>
#include"clerk.h"


int main()
{
    Clerk client;
    client.Init("test.conf");
    auto start = std::chrono::high_resolution_clock::now(); 
    int count = 500;
    int tmp = count;
    while(tmp --){
        
        client.Put("x",std::to_string(tmp));
        std::string get1 = client.Get(std::to_string(tmp));
        std::cout<<"ServiceCaller:: Get return from kvserver , The Value is  " << get1 <<std::endl;
    }
    // int tmp2 =count;
    // while(tmp--){
    //     client.Append("x",std::to_string(tmp2));
    //     std::string get1 = client.Get(std::to_string(tmp));
    //     std::cout<<"2----ServiceCaller:: Get return from kvserver , The Value is  " << get1 <<std::endl;
    // }
  
    return 0;
}