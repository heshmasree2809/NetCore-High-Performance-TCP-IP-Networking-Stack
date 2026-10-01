#include "test_harness.hpp"
#include "core/event_loop.hpp"
#include <sys/eventfd.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <atomic>
using namespace netcore;

NETCORE_TEST(EventLoopTest, WakeupAndStop) {
    EventLoop loop(16);
    std::atomic<bool> threadStarted{false};
    std::thread th([&]() 
    {
        threadStarted = true;
        loop.run();
    });

    while (!threadStarted.load()) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    loop.wakeup();
    loop.stop();

    if (th.joinable()) {
        th.join();
    }

    ASSERT_FALSE(loop.isRunning());
}

NETCORE_TEST(EventLoopTest, EventNotification) {
    EventLoop loop(16);
    int efd = eventfd(0, EFD_NONBLOCK);
    ASSERT_TRUE(efd >= 0);

    std::atomic<bool> eventTriggered{false};
    uint64_t val = 1;

    bool reg = loop.registerSocket(efd, EPOLLIN, [&](int fd, uint32_t events) {
        if (events & EPOLLIN) {
            uint64_t readVal = 0;
            read(fd, &readVal, sizeof(readVal));
            eventTriggered = true;
            loop.stop();
        }
    });
    ASSERT_TRUE(reg);

    std::thread th([&]() {
        loop.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    write(efd, &val, sizeof(val));

    if (th.joinable()) {
        th.join();
    }

    ASSERT_TRUE(eventTriggered.load());
    loop.unregisterSocket(efd);
    close(efd);
}
