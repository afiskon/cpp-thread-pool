#pragma once

#include <memory>
#include "System.h"

namespace me::eax::thread_pool {

template<class T>
class Promise {
public:
    Promise(System& system):
      _future(std::make_shared< Future<T> >(system)) {
    }

    auto success(const T& result) -> Promise<T>& {
        _future->_success(result);
        return *this;
    }

    auto failure(const std::exception& err) -> Promise<T>& {
        _future->_failure(err);
        return *this;
    }

    auto future() const -> std::shared_ptr< Future<T> > {
        return _future;
    }

private:
    std::shared_ptr< Future<T> > _future;
};

} // namespace
