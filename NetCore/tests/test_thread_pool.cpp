#include "test_harness.hpp"
#include "core/thread_pool.hpp"
#include <atomic>
#include <vector>

using namespace netcore;

NETCORE_TEST(ThreadPoolTest, ExecuteTasks) {
    ThreadPool pool(4);
    pool.start();

    const int kTasks = 1000;
    std::atomic<int> counter{0};

    for (int i = 0; i < kTasks; ++i) {
        pool.execute([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    pool.shutdown();
    ASSERT_EQ(counter.load(), kTasks);
    ASSERT_TRUE(pool.getTasksCompleted() >= kTasks);
}

NETCORE_TEST(ThreadPoolTest, SubmitFuture) {
    ThreadPool pool(2);
    pool.start();

    auto fut1 = pool.submit([](int a, int b) { return a + b; }, 10, 20);
    auto fut2 = pool.submit([](const std::string& s) { return s.size(); }, "netcore");

    ASSERT_EQ(fut1.get(), 30);
    ASSERT_EQ(fut2.get(), 7u);

    pool.shutdown();
}
