#ifndef DRAGAZO_FATE_H
#define DRAGAZO_FATE_H

#include <utility>
#include <type_traits>

// fate (function at the end) is a wrapper for any any function-like object that takes no args.
// at the end of the fate object's lifetime, the function is invoked once.
template<typename T>
class fate
{
private: // -- data -- //

	// this buffer is used to store the function object, if any.
	// at all times this shall either hold a constructed function object or nothing at all.
	// i.e. if the buffer doesn't contain a function object it need not be destroyed.
	alignas(T) char func_buf[sizeof(T)];

	// marks if func_buf currently holds a (constructed) function object.
	bool has_func;

public: // -- ctor / dtor / asgn -- //
	
	// creates a fate object that is not associated with a function object (empty)
	constexpr fate() noexcept : has_func(false) {}

	// creates a fate object for the given function-like object - the argument will be forwarded to the T constructor.
	template<typename J>
	explicit fate(J &&arg) : has_func(false)
	{
		// construct the function object
		new(&func_buf) T(std::forward<J>(arg));
		// only mark as having a func if that succeeded (so we don't call garbage on destruction)
		has_func = true;
	}

	// calls the stored function (if any)
	inline ~fate()
	{
		// if we have a function
		if (has_func)
		{
			// attempt to call it
			try { (*(T*)&func_buf)(); }
			catch (...) {}

			// then destroy it
			release();
		}
	}

	fate(const fate&) = delete;
	
	template<std::enable_if_t<std::is_nothrow_move_constructible_v<T>, int> = 0>
	fate(fate &&other) noexcept
	{
		// if other has a function
		if (other.has_func)
		{
			// move other's data
			new(&func_buf) T(std::move(*(T*)&other.func_buf));
			has_func = true;

			// empty other
			other.release();
		}
		// otherwise mark ourselves as empty
		else has_func = false;
	}

	fate &operator=(const fate&) = delete;
	fate &operator=(fate&&) = delete;

public: // -- utilities -- //

	// returns true iff this fate object is still associated with a function object
	inline explicit operator bool() const noexcept { return has_func; }
	
	// returns true iff this fate object is not associated with a function object
	inline constexpr bool empty() const noexcept { return !has_func; }

	// abandons the function (will no longer be executed at the end of fate's lifetime)
	inline void release() noexcept
	{
		// only do this if we have a function
		if (has_func)
		{
			// mark that we don't have a function object and call its destructor
			has_func = false;
			(*(T*)&func_buf).~T();
		}
	}
};

// creates a fate object from the given function-like object.
// effectively just a means of template class type deduction without needing C++17.
template<typename T>
auto make_fate(T &&arg) { return fate<std::decay_t<T>>{std::forward<T>(arg)}; }

#endif
