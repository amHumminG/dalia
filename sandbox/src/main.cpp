#include "Sandbox.h"

#if defined(DALIA_PROFILE_ENABLE) && defined(TRACY_ENABLE)
#include <tracy/Tracy.hpp>
#include <new>
#include <cstdlib>

void* operator new(std::size_t count) {
	if (count == 0) ++count;
	if (void* ptr = std::malloc(count)) {
		TracyAlloc(ptr, count);
		return ptr;
	}
	throw std::bad_alloc{};
}

void* operator new[](std::size_t count) {
	if (count == 0) ++count;
	if (void* ptr = std::malloc(count)) {
		TracyAlloc(ptr, count);
		return ptr;
	}
	throw std::bad_alloc{};
}

void operator delete(void* ptr) noexcept {
	if (ptr) {
		TracyFree(ptr);
		std::free(ptr);
	}
}

void operator delete(void* ptr, std::size_t) noexcept {
	if (ptr) {
		TracyFree(ptr);
		std::free(ptr);
	}
}

void operator delete[](void* ptr) noexcept {
	if (ptr) {
		TracyFree(ptr);
		std::free(ptr);
	}
}

void operator delete[](void* ptr, std::size_t) noexcept {
	if (ptr) {
		TracyFree(ptr);
		std::free(ptr);
	}
}
#endif

int main() {

	Sandbox sandbox;
	sandbox.Run();

    return 0;
}