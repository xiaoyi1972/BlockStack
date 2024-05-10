#pragma once

#include<stdlib.h>
#include<atomic>
#include<cstring>
#include<array>
#include<tuple>
#include<type_traits>
#include<utility>
#include<cmath>
#include<variant>
#include<semaphore>

//#include<semaphore>

#ifdef _MSC_VER // for MSVC
#define forceinline __forceinline
#elif defined __GNUC__ // for gcc on Linux/Apple OS X
#define forceinline __inline__ __attribute__((always_inline))
#else
#define forceinline
#endif

namespace {

	template <class Enum>
	constexpr std::underlying_type_t<Enum> enum_type_value(Enum e) noexcept {
		return static_cast<std::underlying_type_t<Enum>>(e);
	}

	template<template<class...Ts> class Container, class T>
	using type_all_type = Container<T, T*, const T*>;

	template<class T>
	using no_const_pointer_t = std::add_pointer_t<std::remove_cv_t<std::remove_pointer_t<T>>>;

	template<class T>
	struct type_all_t : public type_all_type<std::variant, T> {
		using base_type = type_all_type<std::variant, T>;
		using base_type::base_type;
		using base_type::operator=;
		using type = T;

		inline static struct {
			template<class U>
			auto operator()(U&& arg) -> std::conditional_t<std::is_const_v<std::remove_reference_t<U>>, const type&, type&> {
				if constexpr (std::is_pointer_v<std::decay_t<U>>)
					return *const_cast<no_const_pointer_t<std::decay_t<U>>>(arg);
				else
					return arg;
			};
		} visitor;

		operator T&() {
			return std::visit(visitor, *this);
		}

		operator const T&() const {
			return std::visit(visitor, *this);
		}

		T& operator()() {
			return std::visit(visitor, *this);
		}

		const T& operator()() const {
			return std::visit(visitor, *this);
		}

		T& operator*() {
			return &std::visit(visitor, *this);
		}

		const T& operator*() const {
			return std::visit(visitor, *this);
		}

		T* operator->() {
			return &std::visit(visitor, *this);
		}

		const T* operator->() const {
			return &std::visit(visitor, *this);
		}
	};
}

namespace std {
	template<class T>
	struct std::variant_size<type_all_t<T>> : std::variant_size<typename type_all_t<T>::base_type> {

	};

	template<std::size_t I, class T>
	struct std::variant_alternative<I, type_all_t<T>> : std::variant_alternative<I, typename type_all_t<T>::base_type> {

	};
}


#ifdef USE_SCOPE_WITH_vector_stream_buffer 
namespace {
#define self * this
	using namespace std;

	class vector_stream_buffer : public streambuf {
	private:
		friend class vector_output_stream;
	private:
		string buffer;
	public:
		vector_stream_buffer(size_t size);
	protected:
		virtual streamsize xsputn(const char_type* s, streamsize count) override;
		virtual int_type overflow(int_type character) override;
	};

	class vector_output_stream : protected vector_stream_buffer, public ostream {
	private:
		using string_type = add_pointer_t<const ostream::char_type>;
	public:
		vector_output_stream(size_t size);
	public:
		string str();
	public:
		template <typename T>
		vector_output_stream& operator<<(T&& value);
	public:
		operator string_type() const noexcept;
	};

	vector_stream_buffer::vector_stream_buffer(size_t size) {
		buffer.reserve(size);
	}

	streamsize vector_stream_buffer::xsputn(const char_type* s, streamsize count) {
		return buffer.append(s, count), count;
	}

	vector_stream_buffer::int_type vector_stream_buffer::overflow(int_type character) {
		return 1;
	}

	vector_output_stream::vector_output_stream(size_t size) : vector_stream_buffer(size), ostream(this) {

	}

	string vector_output_stream::str() {
		return buffer;
	}

	template <typename T>
	vector_output_stream& vector_output_stream::operator<<(T&& value) {
		using base_reference = add_lvalue_reference_t<ostream>;
		return static_cast<base_reference>(self) << forward<T>(value), self;
	}

	vector_output_stream::operator string_type() const noexcept {
		return buffer.c_str();
	}

#undef self
}
#endif

namespace {
	template <auto ... Is> 
	struct List {};

	template<auto Cmp, class List_t>
	struct extremum;

	template<auto Cmp, auto I0>
	struct extremum<Cmp, List<I0>> {
		template<bool reverse>
		static constexpr auto select = reverse ^ Cmp(I0, I0) ? I0 : I0;
		static constexpr auto value = std::pair{ select<false>, select<true> };
	};

	template<auto Cmp, auto I0, auto ...Is>
	struct extremum<Cmp, List<I0, Is...>> {
		using rest = extremum<Cmp, List<Is...>>;
		template<bool reverse>
		static constexpr auto select = reverse ^ Cmp(I0, rest::template select<reverse>) ? I0 : rest::template select<reverse>;
		static constexpr auto value = std::pair{ select<false>, select<true> };
	};

	template<auto Cmp, auto...Val>
	static constexpr auto extremum_v = extremum<Cmp, List<Val...>>::value;
}

#ifdef USE_SCOPE_WITH_mergesort
namespace mergesort{
	template <auto ... Is> struct array {};

	template <typename ArrayT, int INDEX>
	struct get;

	template <auto I0, auto ... Is, int INDEX>
	struct get<array<I0, Is...>, INDEX>
	{
		static constexpr auto value = get<array<Is...>, INDEX - 1>::value;
	};

	template <auto I0, auto ... Is>
	struct get<array<I0, Is...>, 0>
	{
		static constexpr auto value = I0;
	};

	template <typename ArrayT, int INDEX>
	constexpr inline auto get_v = get<ArrayT, INDEX>::value;

	// prepend
	template <auto I0, typename ArrayT>
	struct prepend;

	template <auto I0, auto ... Is>
	struct prepend<I0, array<Is...>>
	{
		using type = array<I0, Is...>;
	};

	template <auto I0, typename ArrayT>
	using prepend_t = typename prepend<I0, ArrayT>::type;

	// take
	template <typename ArrayT, int N>
	struct take;

	template <auto I0, auto ... Is, int N>
	struct take<array<I0, Is...>, N>
	{
		using type = prepend_t<I0, typename take<array<Is...>, N - 1>::type>;
	};

	template <auto I0, auto ... Is>
	struct take<array<I0, Is...>, 1>
	{
		using type = array<I0>;
	};

	template <typename ArrayT, int N>
	using take_t = typename take<ArrayT, N>::type;

	// drop
	template <typename ArrayT, int N>
	struct drop;

	template <auto I0, auto ... Is, int N>
	struct drop<array<I0, Is...>, N>
	{
		using type = typename drop<array<Is...>, N - 1>::type;
	};

	template <auto I0, auto ... Is>
	struct drop<array<I0, Is...>, 1>
	{
		using type = array<Is...>;
	};

	template <typename ArrayT, int N>
	using drop_t = typename drop<ArrayT, N>::type;

	// merge
	template <typename Array1, typename Array2>
	struct merge;

	template <auto I0, auto ... Is, auto J0, auto ... Js>
	struct merge<array<I0, Is...>, array<J0, Js...>>
	{
		using type = prepend_t<
			(I0 <= J0 ? I0 : J0),
			typename merge<
			std::conditional_t<I0 <= J0, array<Is...>, array<I0, Is...>>,
			std::conditional_t<I0 <= J0, array<J0, Js...>, array<Js...>>
			>::type
		>;
	};

	template <auto ... Is>
	struct merge<array<Is...>, array<>>
	{
		using type = array<Is...>;
	};

	template <auto ... Js>
	struct merge<array<>, array<Js...>>
	{
		using type = array<Js...>;
	};

	template <>
	struct merge<array<>, array<>>
	{
		using type = array<>;
	};

	template <typename Array1, typename Array2>
	using merge_t = typename merge<Array1, Array2>::type;

	/*
	 * sort
	 */
	template <typename ArrayT>
	struct sort;

	template <>
	struct sort<array<>>
	{
		using type = array<>;
	};

	template <auto I>
	struct sort<array<I>>
	{
		using type = array<I>;
	};

	template <auto ... Is>
	struct sort<array<Is...>>
	{
	private:
		static const int length = sizeof...(Is);
	public:
		using type = merge_t<
			typename sort<take_t<array<Is...>, length / 2>>::type,
			typename sort<drop_t<array<Is...>, length / 2>>::type
		>;
	};

	template <typename ArrayT>
	using sort_t = typename sort<ArrayT>::type;
}
#endif

namespace {
	template <class T>
	struct function_traits : public function_traits<decltype(&std::remove_reference_t<T>::operator())> {};
	template <class R, class C, class... Ts>
	struct function_traits<R(C::*)(Ts...) const>: public function_traits<R(*)(Ts...)> {};
	template <class R, class C, class... Ts>
	struct function_traits<R(C::*)(Ts...)> : public function_traits<R(*)(Ts...)> {};
	template <class R, class... Ts>
	struct function_traits<R(*)(Ts...)> : public function_traits<R(Ts...)> {};
	template <class R, class... Ts> struct function_traits<R(Ts...)> {
		using result_type = R;
		using arg_tuple = std::tuple<Ts...>;
		using ptr = R(*)(Ts...);
		static constexpr auto arity = sizeof...(Ts);
	};

	template<class T>
	struct self_args_func {
		template<class R, class...Ts>
		struct functor_from_tuple { using type = R(*)(Ts...); };

		template<class R, class...Ts>
		struct functor_from_tuple<R, std::tuple<Ts...>> : functor_from_tuple <R, Ts...> {};

		template<class F>
		struct class_member_type;

		template<class F, class U>
		struct class_member_type<U F::*> {
			using object_t = F;
			using object_pointer_t = F*;
			using member_t = U;
		};

		template<class... input_t>
		using tuple_cat_t = decltype(std::tuple_cat(std::declval<input_t>()...));

		using func_t = function_traits<T>;
		using Self = std::tuple<T>;

		using caller_from_this = std::conditional_t<std::is_member_function_pointer_v<T>, 
			std::tuple<typename class_member_type<T>::object_pointer_t>, std::tuple<>>;

		using Concat = tuple_cat_t<Self, caller_from_this, typename func_t::arg_tuple>;
		using merged_type = functor_from_tuple<typename func_t::result_type, Concat>::type;
	};

	template<auto K>
	using self_args_func_t = self_args_func<decltype(K)>::merged_type;

	template<class T, class...Ts>
	void initial_place_with(T& obj, Ts&&...args) {
		std::destroy_at(&obj);
		new (&obj) T(std::forward<Ts>(args)...);
	}

#ifdef USE_SCOPE_WITH_restore_with_noassign
	template<class T>
	struct restore_with_noassign {
		restore_with_noassign(T& obj) :
			origin(obj), save(origin) {}
		~restore_with_noassign() {
			initial_place_with(origin, std::move(save));
		}
		T& origin;
		T save;
	};
#endif

	using voidfun = void(*)();

	template<class F>
	auto lambda_to_void_function(F&& lambda) -> voidfun {
		static thread_local typename std::decay_t<F> lambda_copy = std::forward<F>(lambda);
		if constexpr (std::is_same_v<voidfun, std::decay_t<F>>)
			return lambda;
		initial_place_with(lambda_copy, lambda);
		return []() {
			return (*std::launder(&lambda_copy))();
		};
	}

	template<class F>
	auto lambda_to_pointer(F&& lambda) -> typename function_traits<std::remove_reference_t<F>>::ptr {
		static thread_local typename std::decay_t<F> lambda_copy = std::forward<F>(lambda);
		if constexpr (std::is_same_v<typename function_traits<std::remove_reference_t<F>>::ptr, std::decay_t<F>>)
			return lambda;
		initial_place_with(lambda_copy, lambda);
		return []<class...Args>(Args...args) {
			return (*std::launder(&lambda_copy))(args...);
		};
	}

	template<auto K, class F>
	auto lambda_to_pointer_from(F&& lambda) -> self_args_func_t<K> {
		static thread_local typename std::decay_t<F> lambda_copy = std::forward<F>(lambda);
		if constexpr (std::is_same_v<self_args_func_t<K>, std::decay_t<F>>)
			return lambda;
		initial_place_with(lambda_copy, lambda);
		return []<class...Args>(Args...args) {
			return (*std::launder(&lambda_copy))(args...);
		};
	}
}

namespace {
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

	template <class T, T... As, T... Bs>
	constexpr std::integer_sequence<T, As..., Bs...> operator+(std::integer_sequence < T, As...>,
		std::integer_sequence < T, Bs...>)
	{
		return {};
	}

	template <class F, class T, T Val>
	constexpr auto filter_single(std::integer_sequence<T, Val>, F predicate) {
		if constexpr (predicate(Val)) {
			return std::integer_sequence<T, Val>{};
		}
		else {
			return std::integer_sequence<T>{};
		}
	}

	template <class F, class T, T... Vals>
	constexpr auto filter(std::integer_sequence<T, Vals...>, F predicate) {
		return (filter_single(std::integer_sequence<T, Vals>{}, predicate) + ...);
	}


	template <class Pred, class Type, Type... I>
	constexpr auto filter_integer_sequence(std::integer_sequence<Type, I...> sequence, Pred p) {
		return filter(sequence, p);
	}

	/*
	template <auto Pred, class Type, Type... I>
	constexpr auto filter_integer_sequence(std::integer_sequence<Type, I...>) {
		return typename filter_integer_sequence_impl<Pred, Type, I...>::Result();
	}

	template <class Pred, class Type, Type... I>
	constexpr auto filter_integer_sequence(std::integer_sequence<Type, I...> sequence, Pred) {
		return filter_integer_sequence<(Pred*)nullptr>(sequence);
	}
	*/

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

	template<class T, class...Ts>
	constexpr auto any_is_same_v = (std::is_same_v<T, Ts> || ...);
}

#include<bit>
/*template<class T>
T BitCount(T n) {
	n = n - ((n >> 1) & 0x55555555);
	n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
	n = (n + (n >> 4)) & 0x0f0f0f0f;
	n = n + (n >> 8);
	n = n + (n >> 16);
	return n & 0x3f;
}*/

/*template<class T, class ... Ts>
T BitCount(T first, Ts ... rest) {
	return std::popcount(first) + std::popcount(rest...);
}*/

auto BitCount(auto...v) {
	return (std::popcount(static_cast<unsigned>(v)) + ...);
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

inline std::string operator""_S(const char8_t* str, std::size_t) {
	return reinterpret_cast<const char*>(str);
}

inline char const* operator""_C(const char8_t* str, std::size_t) {
	return reinterpret_cast<const char*>(str);
}

template<class T, std::enable_if_t<std::is_trivially_copyable_v<T>, int> = 0>
class trivail_vector {
	private:
		T* ptr = nullptr;
		std::size_t capacity = 0, count = 0;

		forceinline auto grow_capcity(std::size_t capacity) {
			return capacity < 8 ? 8 : capacity << 1;
		}

		bool reallocate(std::size_t oldSize, std::size_t newSize) {
			if (newSize == 0) {
				std::free(ptr);
				return false;
			}
			decltype(ptr) prev = nullptr;
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
			auto old_capacity = capacity;
			capacity = grow_capcity(old_capacity);
			grow_array(old_capacity, capacity);
		}

		void copy(T* dest, const T* src) {
			if constexpr (std::is_trivial_v<T>) {
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
		difference_type operator-(const Iterator& n) const { return m_ptr - n.m_ptr; }
		Iterator& operator+=(int n) { m_ptr += n; return *this; }
		Iterator& operator-=(int n) { m_ptr -= n; return *this; }
		friend Iterator operator- (const Iterator& a, int n) { Iterator temp = a; return temp -= n; };
		friend Iterator operator+ (const Iterator& a, int n) { Iterator temp = a; return temp += n; };
		friend Iterator operator+ (int n, const Iterator& a) { Iterator temp = a; return temp += n; };
		friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_ptr == b.m_ptr; };
		friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_ptr != b.m_ptr; };
		friend bool operator< (const Iterator& a, const Iterator& b) { return a.m_ptr < b.m_ptr; };
		friend bool operator> (const Iterator& a, const Iterator& b) { return a.m_ptr > b.m_ptr; };
		friend bool operator>= (const Iterator& a, const Iterator& b) { return !(a.m_ptr < b.m_ptr); };
		friend bool operator<= (const Iterator& a, const Iterator& b) { return !(a.m_ptr > b.m_ptr); };
	};

public:
		trivail_vector() {
		}

		~trivail_vector() {
			std::free(ptr);
		}

		trivail_vector(std::initializer_list<T> list) {
			count = list.size();
			reserve(count);
			std::memcpy(ptr, std::data(list), type_size * count);
		}

		trivail_vector(std::size_t _count) {
			reserve(_count);
			count = _count;
			std::memset(ptr, 0, type_size * count);
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
				memcpy(ptr, p.ptr, p.size() * p.type_size);
			}
			return *this;
		}

		T* data() {
			return ptr;
		}

		void reserve(std::size_t _capacity) {
			if (ptr != nullptr) 
				free(ptr);
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

		void push_back(const T& value) {
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

/*struct semaphore_lock {
	std::binary_semaphore& sem;
	semaphore_lock(std::binary_semaphore& _sem) :sem(_sem)
	{
		sem.acquire();
	}

	~semaphore_lock() {
		sem.release();
	}
};*/

#include <atomic>
#include <vector>
#include <iostream>
#include <optional>

template<typename T>
class lock_free_stack
{
private:
	struct node
	{
		node(T const& data_) :
			data(data_), next(nullptr)
		{}

		T data;
		node* next;
	};

	std::atomic<unsigned> threads_in_pop; 
	std::atomic<node*> to_be_deleted;

	static void delete_nodes(node* nodes)
	{
		while (nodes)
		{
			node* next = nodes->next;
			delete nodes;
			nodes = next;
		}
	}

	void chain_pending_nodes(node* nodes)
	{
		node* last = nodes;
		while (node* const next = last->next)  
		{
			last = next;
		}
		chain_pending_nodes(nodes, last);
	}

	void chain_pending_nodes(node* first, node* last)
	{
		last->next = to_be_deleted;
		while (!to_be_deleted.compare_exchange_weak(
			last->next, first));
	}

	void chain_pending_node(node* n)
	{
		chain_pending_nodes(n, n);
	}

	void try_reclaim(node* old_head)
	{
		if (threads_in_pop == 1)
		{
			node* nodes_to_delete = to_be_deleted.exchange(nullptr);
			if (!--threads_in_pop)
			{
				delete_nodes(nodes_to_delete);
			}
			else if (nodes_to_delete)
			{
				chain_pending_nodes(nodes_to_delete);
			}
			delete old_head;
		}
		else
		{
			chain_pending_node(old_head);
			--threads_in_pop;
		}
	}

	std::atomic<node*> head;
	std::atomic<std::size_t> m_size;

public:

	lock_free_stack() :head(nullptr), m_size(0) {};
	~lock_free_stack() {
		while (head)
			pop();
	}

	std::size_t size() {
		return m_size.load();
	}

	bool empty() {
		return head == nullptr;
	}

	void push(T const& data)
	{
		node* const new_node = new node(data);
		new_node->next = head.load();
		while (!head.compare_exchange_weak(new_node->next, new_node));
		m_size.fetch_add(1);
	}

	std::optional<T> pop()
	{
		++threads_in_pop;
		node* old_head = head.load();
		while (old_head &&
			!head.compare_exchange_weak(old_head, old_head->next));
		std::optional<T> res(std::nullopt);
		if (old_head)
		{
			m_size.fetch_sub(1);
			res = old_head->data;
			try_reclaim(old_head);
		}
		return res;
	}

	void swap(lock_free_stack& other) {
		threads_in_pop = other.threads_in_pop.exchange(threads_in_pop);
		to_be_deleted = other.to_be_deleted.exchange(to_be_deleted);
		head = other.head.exchange(head);
		m_size = other.m_size.exchange(m_size);
	}
};
