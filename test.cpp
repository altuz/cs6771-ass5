#include <list>
#include <cmath>
#include <iostream>
#include <numeric>
#include <algorithm>
#include <vector>
#include <random>
int main() {
    unsigned int i = 0 % 10;
    std::cout << i << "\n";
    std::cout << std::pow(10, 0) << "\n";
    std::vector<int> l(10);
    std::iota(l.begin(), l.end(), -4);
    std::shuffle(l.begin(), l.end(), std::mt19937{std::random_device{}()});
    for(auto i : l) {
        std::cout << i << "\n";
    }
}
