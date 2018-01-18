#include <iostream>
#include <chrono>
#include <thread>
#include <assert.h>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include "thread_pool/Future.h"
#include "thread_pool/System.h"

using namespace me::eax::thread_pool;

// TODO: while loops
// TODO: channels / queues
// TODO: sockets, http - see https://raw.githubusercontent.com/YasserAsmi/lev/master/include/lev.h
// TODO: timers
// TODO: job stealing
// TODO: tests, code coverage
// TODO: benchmarks

auto main() -> int {
    auto system = std::make_unique<System>();

    auto f = system->spawn([]() -> int {
        std::cout << "system.spawn - OK" << std::endl;
        return 123;
    });
    
    // make sure all objects live long enough
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    f->then([](int x) -> std::string {
        std::cout << "then(): x = " << x << std::endl;
        return std::string("It works!");
    })->onSuccess([](std::string str) {
        std::cout << "onSuccess called, str = " << str << std::endl;
    })->then([](std::string str) -> int {
        std::cout << "then(): str = " << str << std::endl;
        return 0;
    });

    system->spawn([]() -> int {
        std::cout << "This procedure is about to throw an exception" << std::endl;
        throw std::runtime_error("test exception");
    })->onFailure([](const std::exception& err) {
        std::cout << "onFailure was successfully called, err.what() = " << err.what() << std::endl;
    })->onSuccess([](int) -> int {
        throw std::runtime_error("This onSuccess should have never been called!");
    });


    // TODO: test Promise class!

    system->wait();

    return 0;
}
