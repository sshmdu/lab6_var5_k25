#include <coroutine>
#include <deque>
#include <exception>
#include <format>
#include <iostream>

constexpr std::size_t window_size = 10;
constexpr std::size_t equal_run = 3;

class SlidingWindow {
public:
    void add(double value) {
        values.push_back(value);
        sum += value;
        if (values.size() > window_size) {
            sum -= values.front();
            values.pop_front();
        }
    }
    double average() const { return sum / static_cast<double>(values.size()); }

    bool last_equal(std::size_t count) const {
        if (values.size() < count) return false;
        for (std::size_t i = values.size() - count + 1; i < values.size(); ++i) {
            if (values[i] != values.back()) return false;
        }
        return true;
    }
private:
    std::deque<double> values;
    double sum = 0.0;
};

class PauseController {
public:
    explicit PauseController(const SlidingWindow& source) : window(source) {}
    bool await_ready() const noexcept { return !window.last_equal(equal_run); }
    void await_suspend(std::coroutine_handle<>) const noexcept {}
    void await_resume() const noexcept {}
private:
    const SlidingWindow& window;
};

class Task {
public:
    struct promise_type {
        std::exception_ptr exception;
        Task get_return_object() {
            return Task{ std::coroutine_handle<promise_type>::from_promise(*this) };
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { exception = std::current_exception(); }
    };
    explicit Task(std::coroutine_handle<promise_type> source) : handle(source) {}
    Task(Task&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task& operator=(Task&&) = delete;
    ~Task() {
        if (handle) handle.destroy();
    }
    bool done() const { return handle.done(); }
    void resume() {
        handle.resume();
        if (handle.promise().exception) {
            std::rethrow_exception(handle.promise().exception);
        }
    }
private:
    std::coroutine_handle<promise_type> handle;
};

Task average_coroutine(std::istream& in, std::ostream& out) {
    SlidingWindow window;
    PauseController pause(window);
    double value = 0;
    while (in >> value) {
        window.add(value);
        out << std::format("Average of last {}: {:.10g}\n", window_size,
            window.average());
        co_await pause;
    }
    if (!in.eof()) {
        std::cerr << "Read error: a number was expected.\n";
    }
}

int main() {
    try {
        Task average_task = average_coroutine(std::cin, std::cout);
        while (!average_task.done()) {
            average_task.resume();
            if (!average_task.done()) {
                std::cout << "[Coroutine suspended: last three values are equal. "
                    "Resuming...]\n";
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}