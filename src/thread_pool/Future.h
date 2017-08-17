#pragma once

// forward declaration for Promise.h
namespace me::eax::thread_pool {

template <class T> class Future;

}

#include <type_traits>
#include "System.h"
#include "Promise.h"

namespace me::eax::thread_pool {

template<class T>
class Future {

friend class Promise<T>;

public:
    Future(Future const &) = delete;
    void operator=(Future const &) = delete;

    Future(System& system):
        _system(system)
        , _value_ptr(nullptr)
        , _exception_ptr(nullptr) {
    }

    ~Future() {
        if(_value_ptr)
            delete _value_ptr;
        if(_exception_ptr)
            delete _exception_ptr;
    }

    template<
        class L,
        class R = typename std::result_of<L(T)>::type
    >
    auto then(L lambda) -> std::shared_ptr< Future< R > > {
        auto p = std::make_shared< Promise< R > >(_system);
        System* systemPtr = &_system;

        _add_or_call_on_success_callback([systemPtr, p, lambda](T& arg) {
            systemPtr->_schedule([p, lambda, arg]() {
                try {
                    R result = lambda(arg);
                    try {
                        p->success(result);
                    } catch(const std::exception& err) {
                        // Prevents calling p->failure() after p->success(), hiding the real exception
                        std::terminate();
                    }
                } catch(const std::exception& err) {
                    p->failure(err);
                }
            });
        });

        return p->future();
    }

    auto onSuccess(std::function<void(T)> lambda) -> Future<T>* {
        System* systemPtr = &_system;
        _add_or_call_on_success_callback([systemPtr, lambda](T& arg) {
            systemPtr->_schedule([lambda, arg]() {
                try {
                    lambda(arg);
                } catch(const std::exception& err) {
                    /*
                     * It's not clear what to do in case when onSuccess handler throws an exception.
                     * To terminate a program seems to be safer than continue to work with a probably
                     * corrupted state of a program.
                     */
                    std::cerr << "Future.onSuccess handler throwed an exception" << std::endl;
                    std::terminate();
                }
            });
        });
        return this;
    }

    auto onFailure(std::function<void(const std::exception&)> lambda) -> Future<T>* {
        System* systemPtr = &_system;
        _add_or_call_on_failure_callback([systemPtr, lambda](const std::exception& err) {
            systemPtr->_schedule([lambda, err]() {
                try {
                    lambda(err);
                } catch(const std::exception& err) {
                    /*
                     * It's not clear what to do in case when onFailure handler throws an exception.
                     * To terminate a program seems to be safer than continue to work with a probably
                     * corrupted state of a program.
                     */
                    std::cerr << "Future.onFailure handler throwed an exception" << std::endl;
                    std::terminate();
                }
            });
        });
        return this;
    }

private:
    System& _system;
    std::mutex _mutex;
    std::vector<std::function<void(T&)>> _on_success_vec;
    std::vector<std::function<void(const std::exception&)>> _on_failure_vec;
    T* _value_ptr;
    std::exception* _exception_ptr;

    auto _success(const T& result) -> Future<T>& {
        std::lock_guard<std::mutex> lock(_mutex);

        if(_value_ptr != nullptr)
            throw std::runtime_error("Future._success was called twice");

        if(_exception_ptr != nullptr)
            throw std::runtime_error("Future._success was called after Future._failure");

        _value_ptr = new T(result);

        for(const auto& callback : _on_success_vec)
            callback(*_value_ptr);

        return *this;
    }

    auto _failure(const std::exception& err) -> Future<T>& {
        std::lock_guard<std::mutex> lock(_mutex);

        if(_exception_ptr != nullptr)
            throw std::runtime_error("Future._failure was called twice");

        if(_value_ptr != nullptr)
            throw std::runtime_error("Future._failure was called after Future._success");

        _exception_ptr = new std::exception(err); // TODO FIXME - doesn't copy .what() message

        for(const auto& callback : _on_failure_vec)
            callback(*_exception_ptr);

        return *this;
    }

    auto _add_or_call_on_success_callback(std::function<void(T&)> callback) -> Future<T>& {
        std::lock_guard<std::mutex> lock(_mutex);
        if(_value_ptr != nullptr) {
            // Future is successfuly finished
            callback(*_value_ptr);
        } else if(_exception_ptr == nullptr) {
            // Future is not yet finished
            _on_success_vec.emplace_back(callback);
        }

        return *this;
    }

    auto _add_or_call_on_failure_callback(std::function<void(const std::exception&)> callback) -> Future<T>& {
        std::lock_guard<std::mutex> lock(_mutex);
        if(_exception_ptr != nullptr) {
            // Future is finished with an exception
            callback(*_exception_ptr);
        } else if(_value_ptr == nullptr) {
            // Future is not yet finished
            _on_failure_vec.emplace_back(callback);
        }

        return *this;
    }
};

} // namespace
