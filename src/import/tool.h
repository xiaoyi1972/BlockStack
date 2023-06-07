#pragma once
#include<stdlib.h>
#include<atomic>
#include<cstring>
#include<array>
#include<tuple>
#include<type_traits>
#include<utility>

template<std::size_t...I, class F>
constexpr auto with_sequence_impl(F&& func, std::index_sequence<I...>) {
	return func(std::integral_constant<std::size_t, I>{}...);
}

template<std::size_t N, class F>
constexpr auto with_sequence(F&& func) {
	return with_sequence_impl(std::forward<F>(func), std::make_index_sequence<N>{});
}

template <auto Pred, class Type, Type... I>
struct filter_integer_sequence_impl {
	template <class... T>
	static constexpr auto Unpack(std::tuple<T...>) {
		return std::integer_sequence<Type, T::value...>();
	}

	template <Type Val>
	using Keep = std::tuple<std::integral_constant<Type, Val>>;
	using Ignore = std::tuple<>;
	using Tuple = decltype(std::tuple_cat(std::conditional_t<(*Pred)(I), Keep<I>, Ignore>()...));
	using Result = decltype(Unpack(Tuple()));
};

template <auto Pred, class Type, Type... I>
constexpr auto filter_integer_sequence(std::integer_sequence<Type, I...>) {
	return typename filter_integer_sequence_impl<Pred, Type, I...>::Result();
}

template <class Pred, class Type, Type... I>
constexpr auto filter_integer_sequence(std::integer_sequence<Type, I...> sequence, Pred) {
	return filter_integer_sequence<(Pred*)nullptr>(sequence);
}

template<std::size_t N>
constexpr auto bit_seq_tuple(std::size_t v) {
	return with_sequence<N>([&](auto...c) {
		return std::make_tuple(1 & v >> c...);
		});
}

template<class... Args>
constexpr auto tuple_to_array(const std::tuple<Args...>& t) {
	constexpr auto N = sizeof...(Args);
	return with_sequence<N>([&](auto...c) {
		return std::array{ std::get<c.value>(t)... };
		});
}

template<class T>
T BitCount(T n) {
	n = n - ((n >> 1) & 0x55555555);
	n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
	n = (n + (n >> 4)) & 0x0f0f0f0f;
	n = n + (n >> 8);
	n = n + (n >> 16);
	return n & 0x3f;
}
template<class T, class ... Ts>
T BitCount(T first, Ts ... rest) {
	return BitCount(first) + BitCount(rest...);
}

/*class Spinlock
{
private:
	std::atomic_flag atomic_flag = ATOMIC_FLAG_INIT;

public:
	void lock()
	{
		for (;;)
		{
			if (!atomic_flag.test_and_set(std::memory_order_acquire))
			{
				break;
			}
			while (atomic_flag.test(std::memory_order_relaxed))
				;
		}
	}
	void unlock()
	{
		atomic_flag.clear(std::memory_order_release);
	}
};*/

template<char...num>
constexpr int operator"" _r()
{
	auto value = 0, n = 0;
	return ((value = num - '0' << n++ | value) , ...);
}

template<class T/*, std::enable_if_t<std::is_trivially_copyable_v<T>, int> = 0*/>
class trivail_vector {
	private:
		T* ptr{nullptr};
		std::size_t capacity{0}, count{0};

		auto grow_capcity(std::size_t capacity) {
			return capacity < 8 ? 8 : capacity * 2;
		}

		bool reallocate(std::size_t oldSize, std::size_t newSize) {
			if (newSize == 0) {
				free(ptr);
				return false;
			}
			decltype(ptr) prev{nullptr};
			if (ptr = static_cast<T*>(std::realloc(prev = ptr, newSize))) {
				return true;
			}
			else { 
				ptr = prev;
				return false;
			}
		}
     
		void free_array(std::size_t oldCount) {
			reallocate(type_size * oldCount, 0);
		}
	
		void grow_array(std::size_t oldCount, std::size_t newCount) {
			reallocate(type_size * oldCount, type_size * newCount);
		}

		void expand() {
				auto oldCapacity = capacity;
				capacity = grow_capcity(oldCapacity);
				grow_array(oldCapacity, capacity);
		}

		void copy(T* dest, const T* src) {
			if constexpr (std::is_trivial_v<T> == true) {
				memcpy(dest, src, type_size);
			}
			else {
				dest->operator = (*src);
			}
		}

public:
	static const auto type_size = sizeof (T);

	class Iterator
	{
	public:
		using iterator_category = std::forward_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = T;
		using pointer = T*;  // or also value_type*
		using reference = T&;  // or also value_type&

		pointer m_ptr;

		Iterator(pointer ptr) noexcept : m_ptr(ptr) {}
		reference operator[](int n) { return *(m_ptr + n); }
		reference operator*() const { return *m_ptr; }
		pointer operator->() { return m_ptr; }
		Iterator& operator++() { m_ptr++; return *this; }
		Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }
		Iterator& operator--() { m_ptr--; return *this; }
		Iterator operator--(int) { Iterator tmp = *this; --(*this); return tmp; }
		std::size_t operator-(const Iterator& n) const { return m_ptr - n.m_ptr; }
		Iterator& operator+=(int n) { m_ptr += n; return *this; }
		Iterator& operator-=(int n) { m_ptr -= n; return *this; }
		friend Iterator operator- (const Iterator& a, int n) { Iterator temp = a; return temp -= n; };
		friend Iterator operator+ (const Iterator& a, int n) { Iterator temp = a; return temp += n; };
		friend Iterator operator+ (int n, const Iterator& a) { Iterator temp = a; return temp += n; };
		friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_ptr == b.m_ptr; };
		friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_ptr != b.m_ptr; };
		friend bool operator< (const Iterator& a, const Iterator& b) { return a.m_ptr - b.m_ptr > 0; };
		friend bool operator> (const Iterator& a, const Iterator& b) { return a.m_ptr < b.m_ptr; };
		friend bool operator>= (const Iterator& a, const Iterator& b) { return !(a.m_ptr < b.m_ptr); };
		friend bool operator<= (const Iterator& a, const Iterator& b) { return !(a.m_ptr > b.m_ptr); };
	};

public:
		trivail_vector() {
		}

		~trivail_vector() {
			free(ptr);
		}

		trivail_vector(std::size_t _count) {
			reserve(_count);
			count = _count;
			memset(ptr, 0, type_size * count);
		}

		trivail_vector(const trivail_vector& p) {
			operator=(p);
		}

		trivail_vector& operator=(const trivail_vector& p) {
			if (this != &p) {
				if (capacity != p.size()) {
					free(ptr);
					reallocate(type_size * capacity, type_size * p.size());
				}
				capacity = p.count;
				count = p.count;
				memcpy(ptr,p.ptr,p.size() * p.type_size);
			}
			return *this;
		}

		T* data() {
			return ptr;
		}

		void reserve(std::size_t _capacity) {
			if (ptr != nullptr) free(ptr);
			else {
				reallocate(type_size * capacity, type_size * _capacity);
				capacity = _capacity;
			}
		}

		void resize(std::size_t _count) {
			count = _count;
		}

		auto size() const{
			return count;
		}

		auto capacitys() const {
			return capacity;
		}

		void push(const T& value) {
			if (capacity < count + 1) {
				expand();
			}
			copy(ptr + count, &value);
			count++;
		}

		template<class...Ts>
		void emplace_back(Ts&&...ts) {
			if (capacity < count + 1) {
				expand();
			}
			new (ptr + count) T(std::forward<Ts>(ts)...);
			count++;
		}

		T& front() { return ptr[0]; }
		T& back() { return ptr[count - 1]; }
		bool empty() {return count == 0;}
		void clear() { count = 0; }

		Iterator insert(Iterator pos, const T& value) {
			auto ptr = pos.get();
			if (capacity < count + 1) {
				expand();
			}
			auto len = end().get() - ptr;
			memmove(ptr + 1, ptr, len * type_size);
			copy(ptr, &value);
			count++;
			return pos;
		}

		Iterator erase(Iterator pos) {
			auto ptr = pos.get();
			auto len = end().get() - ptr;
			memmove(ptr - 1, ptr, len * type_size);
			count--;
			return pos;
		}

		T& operator[](std::size_t index) {
			return ptr[index];
		}

		Iterator begin() { return Iterator(&ptr[0]); }
		Iterator end() { return Iterator(&ptr[count]); }
};

class dynamic_bitset {
private:
	int section(int n) {
		return n >> 5;
	}

	int index(int n) {
		return n & 31;
	}

	trivail_vector<std::int32_t> bits;

public:
	dynamic_bitset(int n) noexcept : bits(std::ceil(n / 32.)) {
	}

	template<bool value>
	void set_value(int n) {
		if constexpr (value)
			bits[section(n)] |= 1 << index(n);
		else
			bits[section(n)] &= ~(1 << index(n));
	}

	bool has_value(int n) {
		return bits[section(n)] & (1 << index(n));
	}
};

namespace bitwise {
	inline int abs(int v) {
		return v * ((v > 0) - (v < 0));
	}

	inline int min(int x, int y) {
		return y ^ ((x ^ y) & -(x < y));
	}

	inline int max(int x, int y) {
		return x ^ ((x ^ y) & -(x < y));
	}
}