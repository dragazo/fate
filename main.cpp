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

int main()
{
	{
		auto _fate = make_fate(foo);
		std::cerr << "fate size: " << sizeof(_fate) << '\n';
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
