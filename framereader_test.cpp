#include "framereader.hpp"
#include <iostream>
using namespace std;

int main() {
    FrameReader<100> f;
    namedWindow("Frame", WINDOW_NORMAL);
    while(waitKey(1) != 'e') {
        auto v = f.get();
//        cout << v.first.time_since_epoch().count() << endl;
        auto frame = v.second;
        imshow("Frame", frame);
//        this_thread::sleep_for(milliseconds(50));
    }
}