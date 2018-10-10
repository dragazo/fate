#include <iostream>
#include <string>
#include <exception>

#include "fate.h"

// pretend synchronized resource for an example of usage
struct resource
{
	void lock() {/* locks a hidden mutex - must be done before we're allowed to do anything to the object */}
	void unlock() {/* unlocks the mutex */}

	void mutate_something() {/* mutates the resource - must be locked prior */}
	// ... other methods that require it to be locked
} some_external_resource;

// gets our pretend resource
resource &get_external_resource() { return some_external_resource; }

// -----------------------------

void foo()
{
	std::cerr << "foo\n";
}

// type used to test fate contract edge conditions - what if the stored function calls the fate object explicitly
struct fate_breaker
{
	fate<fate_breaker> *fate_obj;
	void operator()()
	{
		std::cerr << "fate breaker :3\n";
		(*fate_obj)();
	}
};

fate<void(*)()> *fate_breaker_func_val;
void fate_breaker_func()
{
	std::cerr << "fate breaker 2 :3\n";
	(*fate_breaker_func_val)();
}

int main()
{

	{
		std::cerr << "here\n";
		fate<fate_breaker> breaker;
		breaker = make_fate(fate_breaker{&breaker});
	}

	std::cerr << "\n\n";

	{
		std::cerr << "here2\n";
		fate<void(*)()> _loc_fate = make_fate(fate_breaker_func);
		fate_breaker_func_val = &_loc_fate;
	}

	std::cerr << "\n\n";

	{
		struct t_t
		{
			t_t() {}
			t_t(const t_t&) {}
			t_t(t_t&&) {  }
			void operator()() { std::cerr << "thingy\n"; }
		} thingy;

		

		//fate _fate_1(foo);
		//fate _fate_3([] { foo(); });

		//fate _fate_2 = foo;
		//fate _fate_4 = [] {};

		auto _fate = make_fate(foo);
		std::cerr << "fate size: " << sizeof(_fate) << '\n';

		auto _fate2 = make_fate(thingy);
		auto _fate3 = std::move(_fate2);
		std::cerr << "nothrow move ctor: " << std::is_nothrow_move_constructible_v<decltype(thingy)> << '\n';
		std::cerr << "fate size: " << sizeof(_fate2) << '\n';

		auto _fate4 = make_fate([] { std::cerr << std::isnan(5.6) << '\n'; });
	}

	std::cerr << '\n';
	
	// contrived example for freeing dynamic resources
	try
	{
		// allocate a dynamic array
		double *arr = new double[16];
		// create a fate object to ensure it gets deleted
		auto _fate = make_fate([=]() { delete[] arr; std::cerr << "array freed by fate\n"; });
		std::cerr << "fate size: " << sizeof(_fate) << '\n';

		// ...

		// uhoh, an error (more likely from a function invocation)
		throw std::runtime_error("something bad");

		// ...
	}
	catch (const std::runtime_error &ex)
	{
		std::cerr << "ERROR: " << ex.what() << '\n';
	}

	std::cerr << '\n';
	
	// more practical example
	try
	{
		// get a resource
		resource &dat = get_external_resource();
		// lock it
		dat.lock();
		// create a fate object to ensure it gets unlocked at some point
		auto _unlocker = make_fate([&]() { dat.unlock(); std::cerr << "resource unlocked by fate\n"; });
		std::cerr << "fate size: " << sizeof(_unlocker) << '\n';

		// ... do mutatey stuff now that it's locked

		// uhoh, an error (more likely from a function invocation)
		throw std::runtime_error("something bad");

		// ...
	}
	catch (const std::runtime_error &ex)
	{
		std::cerr << "ERROR: " << ex.what() << '\n';
	}
	
	std::cin.get();
	return 0;
}
