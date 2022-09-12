#pragma once
#include<stdlib.h>

template<class T>
struct Option {
	bool exist;
	T m_value;

	T& value() {return m_value;}

	bool has_value() const noexcept { return exist; }

	Option& operator=(const T& _value) {
		m_value = _value;
		exist = true;
		return *this;
	}

	Option& operator=(std::nullopt_t _value) {
		exist = false;
		return *this;
	}

	operator bool() const { return exist; }

	const T* operator->() const noexcept { return std::addressof(m_value); }

	T* operator->() noexcept { return std::addressof(m_value); }

	const T& operator*() const& noexcept { return m_value; }

	T& operator*() & noexcept { return m_value; }

	const T&& operator*() const&& noexcept { return std::move(m_value); }

	constexpr T&& operator*() && noexcept { return std::move(m_value); }
};

template<class T, class K>
struct Pair {
	T first;
	K second;
};


template<class T>
class Vec {
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
				std::memcpy(dest, src, type_size);
			}
			else {
				dest->operator = (*src);
			}
		}

public:
	static const auto type_size = sizeof T;

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
		Vec() {
			
		}

		~Vec() {
			free(ptr);
		}

		Vec(std::size_t _count) {
			reserve(_count);
			count = _count;
			std::memset(ptr, 0, type_size * count);
		}

		Vec(const Vec& p) {
			operator=(p);
		}

		Vec& operator=(const Vec& p) {
			if (this != &p) {
				if (capacity != p.size()) {
					free(ptr);
					reallocate(type_size * capacity, type_size * p.size());
				}
				capacity = p.count;
				count = p.count;
				std::memcpy(ptr,p.ptr,p.size() * p.type_size);
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
			std::memmove(ptr + 1, ptr, len * type_size);
			copy(ptr, &value);
			count++;
			return pos;
		}

		Iterator erase(Iterator pos) {
			auto ptr = pos.get();
			auto len = end().get() - ptr;
			std::memmove(ptr - 1, ptr, len * type_size);
			count--;
			return pos;
		}

		T& operator[](std::size_t index) {
			return ptr[index];
		}

		Iterator begin() { return Iterator(&ptr[0]); }
		Iterator end() { return Iterator(&ptr[count]); }
};