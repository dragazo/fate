#ifndef DRAGAZO_FATE_H
#define DRAGAZO_FATE_H

#include <utility>
#include <type_traits>

// fate (function at the end) is a wrapper for any function-like object that takes no args.
// a fate object is bound to a function-like object and forms a contract with it to invoke it exactly one time (unless explicitly told not to).
// upon being invoked explicitly, the fate instance becomes "empty" and will no longer be attached to its function-like object.
// at the end of the fate object's lifetime, it will invoke its stored function if it still has one.
// this wrapper is not designed to be threadsafe.
// the type parameter T is the type of object to store (e.g. binding "void foo()" would require "T = void(*)()").
// i recommend using auto type deduction and the make_fate() helper function - especially for binding lambdas.
template<typename T>
class fate
{
private: // -- data -- //

	// this buffer is used to store the function object, if any.
	// at all times this shall either hold a constructed function object or nothing at all.
	// i.e. if the buffer doesn't contain a function object it need not (and must not) be destroyed.
	alignas(T) char func_buf[sizeof(T)];

	// marks if func_buf currently holds a (constructed) function object.
	bool has_func;

private: // -- helpers -- //

	// WARNING - assumes this object is currently empty.
	// WARNING - assumes other is not this instance.
	// attempts to transfer other's fate to this object via move semantics.
	// if T is nothrow move constructable:
	//     this is a nothrow operation.
	//     the function-like object is transfered via move constructor.
	// otherwise, if T has a copy constructor:
	//     this is a potentially-throwing operation with the strong exception guarantee.
	//     the function-like object is transfered via copy constructor.
	// otherwise:
	//     this is a potentially-throwing operation with the basic exception guarantee.
	//     the function-like object is transfered via move constructor.
	// on success, this object is guaranteed to have other's function and other is guaranteed to be empty.
	void transfer(fate &&other) noexcept(std::is_nothrow_move_constructible_v<T> || std::is_nothrow_copy_constructible_v<T>)
	{
		// if other has a function
		if (other.has_func)
		{
			try
			{
				// move or copy other's function, depending on if moving is safe
				new(&func_buf) T(std::move_if_noexcept(*(T*)&other.func_buf));
				// only mark as having a func if that succeeded (so we don't call garbage on destruction)
				has_func = true;
				// empty other (also only if the move/copy succeeded)
				other.release();
			}
			// if we got an error, rethrow it.
			// the compiler will likely issue a warning about this for noexcept(true),
			// but that's just because it's not being smart enough to know this can never happen in that case.
			catch (...) { throw; }
		}
	}

public: // -- ctor / dtor / asgn -- //
	
	// creates a fate object that is not associated with a function object (empty).
	// does not invoke the T constructor, so this still works for types that lack a default ctor.
	inline constexpr fate() noexcept : has_func(false) {}

	// creates a fate object for the given function-like object - the argument will be forwarded to the T constructor.
	// on success, a valid fate is made that binds the given function-like object.
	// on failure, the created fate instance is guaranteed to be empty and an exception is thrown.
	template<typename J>
	constexpr explicit fate(J &&arg) noexcept(noexcept((T)std::forward<J>(arg))) : has_func(false)
	{
		// construct the function object
		new(&func_buf) T(std::forward<J>(arg));
		// only mark as having a func if that succeeded (so we don't call garbage on destruction)
		has_func = true;
	}

	inline ~fate() { (*this)(); }

	fate(const fate&) = delete;
	fate &operator=(const fate&) = delete;

	// constructs a new fate object by transfering other's contract to the new instance
	inline constexpr fate(fate &&other) noexcept(noexcept(transfer(std::move(other)))) : has_func(false)
	{
		transfer(std::move(other));
	}
	// if this instance currently holds a function, it is triggered. after this, other's contract is transfered to this instance.
	// in the special case of self-assignment, does nothing.
	inline constexpr fate &operator=(fate &&other) noexcept(noexcept(transfer(std::move(other))))
	{
		if (this != &other)
		{
			(*this)();
			transfer(std::move(other));
		}
		return *this;
	}

public: // -- utilities -- //

	// triggers the fate object to call its stored function (if any).
	// if the function-like object throws an exception, it is caught and ignored.
	// the resulting fate object is guaranteed to be empty after this.
	constexpr void operator()() noexcept
	{
		// if we have a function
		if (has_func)
		{
			// mark that we're empty (so that if the function calls this function we don't call it multiple times)
			has_func = false;

			// attempt to call it
			try { (*(T*)&func_buf)(); }
			catch (...) {}

			// then destroy it
			(*(T*)&func_buf).~T();
		}
	}

	// returns true iff this fate object is still associated with a function object
	inline constexpr explicit operator bool() const noexcept { return has_func; }
	// returns true iff this fate object is not associated with a function object
	inline constexpr bool operator!() const noexcept { return !has_func; }

	// returns true iff this fate object is not associated with a function object
	inline constexpr bool empty() const noexcept { return !has_func; }

	// abandons the function (will no longer be executed at the end of fate's lifetime)
	constexpr void release() noexcept
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

// specialization for space-saving with function pointers
template<typename T>
class fate<T(*)()>
{
private: // -- data -- //

	// this holds a pointer to the function to call at the end of the fate object's lifetime.
	// at all times this shall either hold a valid function pointer or nullptr to signify no function (empty).
	T(*func)();

public: // -- ctor / dtor / asgn -- //

	// creates a fate object that is not associated with a function object (empty)
	inline constexpr fate() noexcept : func(nullptr) {}

	// creates a fate object for the given function
	inline constexpr explicit fate(T(*f)()) noexcept : func(f) {}

	inline ~fate() { (*this)(); }

	fate(const fate&) = delete;
	fate &operator=(const fate&) = delete;
	
	// constructs a new fate object by transfering other's contract to the new instance
	inline constexpr fate(fate &&other) noexcept : func(other.func)
	{
		other.func = nullptr;
	}
	// if this instance currently holds a function, it is triggered. after this, other's contract is transfered to this instance.
	// in the special case of self-assignment, does nothing.
	inline constexpr fate &operator=(fate &&other) noexcept
	{
		if (this != &other)
		{
			(*this)();
			func = other.func;
			other.func = nullptr;
		}
		return *this;
	}

public: // -- utilities -- //

	// triggers the fate object to call its stored function (if any).
	// if the function-like object throws an exception, it is caught and ignored.
	// the resulting fate object is guaranteed to be empty after this.
	constexpr void operator()() noexcept
	{
		// if we have a function
		if (func)
		{
			// mark that we're empty (so that if the function calls this function we don't call it multiple times)
			T(*_f)() = func;
			func = nullptr;

			// attempt to call it
			try { _f(); }
			catch (...) {}
		}
	}

	// returns true iff this fate object is still associated with a function object
	inline constexpr explicit operator bool() const noexcept { return func; }
	// returns true iff this fate object is not associated with a function object
	inline constexpr bool operator!() const noexcept { return !func; }

	// returns true iff this fate object is not associated with a function object
	inline constexpr bool empty() const noexcept { return !func; }

	// abandons the function (will no longer be executed at the end of fate's lifetime)
	inline constexpr void release() noexcept { func = nullptr; }
};

// -------------------------------------------------------------- //

// creates a fate object from the given function-like object.
// effectively just a means of template class type deduction without needing C++17.
template<typename T>
constexpr auto make_fate(T &&arg) { return fate<std::decay_t<T>>{std::forward<T>(arg)}; }

#endif
