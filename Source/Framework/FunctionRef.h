#pragma once
#include <memory>
#include <type_traits>
#include <utility>

// A non-owning reference to a callable, for callbacks that run during the call: no allocation, not storable.
template <typename Sig> class FunctionRef;
template <typename R, typename... Args> class FunctionRef<R(Args...)>
{
public:
	template <typename F>
		requires(!std::is_same_v<std::remove_cvref_t<F>, FunctionRef> && std::is_invocable_r_v<R, F&, Args...>)
	FunctionRef(F&& f) : object(const_cast<void*>(static_cast<const void*>(std::addressof(f)))) {
		call = [](void* o, Args... args) -> R { return (*static_cast<std::remove_reference_t<F>*>(o))(std::forward<Args>(args)...); };
	}
	R operator()(Args... args) const { return call(object, std::forward<Args>(args)...); }

private:
	void* object = nullptr;
	R (*call)(void*, Args...) = nullptr;
};
