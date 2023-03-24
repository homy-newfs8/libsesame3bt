#pragma once

namespace libsesame3bt {

template <typename T>
class api_wrapper {
 public:
	using fun = void (*)(T*);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
	api_wrapper(fun init_fn, fun free_fn) : _init_fn(init_fn), _free_fn(free_fn) { _init_fn(&wrapped); }
#pragma GCC diagnostic pop
	virtual ~api_wrapper() { _free_fn(&wrapped); }
	T* operator&() { return &wrapped; }
	const T* operator&() const { return &wrapped; }
	T& operator()() { return wrapped; }
	const T& operator()() const { return wrapped; }
	void reset() {
		_free_fn(&wrapped);
		_init_fn(&wrapped);
	}

 private:
	T wrapped;
	fun _init_fn;
	fun _free_fn;
};

}  // namespace libsesame3bt
