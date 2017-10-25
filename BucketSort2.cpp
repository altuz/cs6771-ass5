#include "BucketSort.h"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <stack>
#include <thread>
#include <iostream>

unsigned int getDigit(unsigned int value, unsigned int positionFromLeft)
{
    int posFromRight = 1;
    {
        unsigned int v = value;
        while (v /= 10)
            ++posFromRight;
    }
    posFromRight -= positionFromLeft - 1;
    while (--posFromRight)
        value /= 10;
    value %= 10;
    return value > 0 ? value : -value;
}

void BucketSort::sort(unsigned int numCores) {
    numCores = 8;
    // have queue of buckets
    // all threads will work on one bucket at a time
    // might change to stack
    std::stack<Bucket> bucket_queue;
    bucket_queue.emplace(this->numbersToSort);
    // one mutex for each sub bucket, an invariant no matter what bucket we are inspecting
    std::vector<unsigned int> sorted;
    std::mutex smutex;
    std::array<std::mutex, 10> mutexes;
    std::mutex cmutex;
    std::vector<unsigned int> numbers
    unsigned int count;
    bool done = true;
    std::vector<std::thread> thread_list;
    std::vector<Bucket> sub_buckets;

    


    this->numbersToSort = std::move(sorted);
}
