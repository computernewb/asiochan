#pragma once

#include <cassert>
#include <exception>
#include <functional>
#include <optional>
#include <stdexcept>
#include <utility>

#include "asiochan/asio.hpp"
#include "asiochan/sendable.hpp"

namespace asiochan
{
    enum class awaitable_promise_errc
    {
        broken_promise = 1,
    };

    [[nodiscard]] inline auto make_error_code(awaitable_promise_errc const errc) noexcept -> system::error_code
    {
        class awaitable_promise_category final : public system::error_category
        {
          public:
            [[nodiscard]] auto name() const noexcept -> char const* override
            {
                return "awaitable promise";
            }

            [[nodiscard]] auto message(int const errc) const -> std::string override
            {
                switch (static_cast<awaitable_promise_errc>(errc))
                {
                case awaitable_promise_errc::broken_promise:
                    return "broken promise";
                default:
                    return "unknown";
                }
            }
        };

        static constinit auto category = awaitable_promise_category{};
        return system::error_code{static_cast<int>(errc), category};
    }
}  // namespace asiochan

template <>
struct asiochan::system::is_error_code_enum<asiochan::awaitable_promise_errc>
  : std::true_type
{
};

namespace asiochan
{
    template <typename T>
    class awaitable_promise;

    template <sendable_value T>
    class awaitable_promise<T>
    {
      public:
        awaitable_promise() noexcept = default;

        void set_value(T const& value)
        {
            assert(valid());
            auto executor = asio::get_associated_executor(*impl_);
            asio::post(
                std::move(executor),
                std::bind_front(std::move(*impl_), nullptr, value));
            impl_.reset();
        }

        void set_value(T&& value)
        {
            assert(valid());
            auto executor = asio::get_associated_executor(*impl_);
            asio::post(
                std::move(executor),
                std::bind_front(std::move(*impl_), nullptr, std::move(value)));
            impl_.reset();
        }

        void set_exception(std::exception_ptr error)
        {
            assert(valid());
            auto executor = asio::get_associated_executor(*impl_);
            asio::post(
                std::move(executor),
                std::bind_front(std::move(*impl_), std::move(error), T{}));
            impl_.reset();
        }

        void set_error_code(system::error_code const error)
        {
            set_exception(std::make_exception_ptr(system::system_error{error}));
        }

        void reset()
        {
            if (valid())
            {
                set_error_code(awaitable_promise_errc::broken_promise);
            }
        }

        [[nodiscard]] auto valid() const noexcept -> bool
        {
            return not impl_.empty();
        }

        [[nodiscard]] auto get_awaitable() -> asio::awaitable<T>
        {
            assert(not valid());
            return asio::async_initiate<decltype(asio::use_awaitable), handler_sig>(
                [this](auto&& resumeCb) mutable {
                    impl_.emplace(std::move(resumeCb));
                },
                asio::use_awaitable);
        }

      private:
        using handler_sig = void(std::exception_ptr error, T value);
        using impl_type = std::optional<
            boost::asio::detail::awaitable_handler<asio::any_io_executor, std::exception_ptr, T>>;

        impl_type impl_;
    };

    template <>
    class awaitable_promise<void>
    {
      public:
        awaitable_promise() noexcept = default;

        void set_value()
        {
            assert(valid());
            auto executor = asio::get_associated_executor(*impl_);
            asio::post(
                std::move(executor),
                std::bind_front(std::move(*impl_), nullptr));
            impl_.reset();
        }

        void set_exception(std::exception_ptr error)
        {
            assert(valid());
            auto executor = asio::get_associated_executor(*impl_);
            asio::post(
                std::move(executor),
                std::bind_front(std::move(*impl_), std::move(error)));
            impl_.reset();
        }

        void set_error_code(system::error_code const error)
        {
            set_exception(std::make_exception_ptr(system::system_error{error}));
        }

        void reset()
        {
            if (valid())
            {
                set_error_code(awaitable_promise_errc::broken_promise);
            }
        }

        [[nodiscard]] auto valid() const noexcept -> bool
        {
            return static_cast<bool>(impl_);
        }

        [[nodiscard]] auto get_awaitable() -> asio::awaitable<void>
        {
            assert(not valid());
            return asio::async_initiate<decltype(asio::use_awaitable), handler_sig>(
                [this](auto&& resumeCb) mutable {
                    impl_.emplace(std::move(resumeCb));
                },
                asio::use_awaitable);
        }

      private:
        using handler_sig = void(std::exception_ptr error);
        using impl_type = std::optional<
            boost::asio::detail::awaitable_handler<asio::any_io_executor, std::exception_ptr>>;

        impl_type impl_;
    };
}  // namespace asiochan