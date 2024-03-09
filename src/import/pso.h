#include <stdexcept>
#include <cmath>
#include <limits>
#include <functional>
#include <iostream>
#include <ctime>
#include <iomanip>
#include <random>
#include <vector>
#include <chrono>
#include <algorithm>
#include <execution>
#include <mutex>
#include <shared_mutex>
#include <map>
#include <string_view>
#include <array>
#include <iterator>


namespace pso {

	namespace {
		inline std::mutex out_mtx;
	}

    using value = double;

    struct RNG {
        std::random_device rd;
        std::default_random_engine rng{rd()};
        std::uniform_real_distribution<value> d;

		RNG(unsigned _seed = std::chrono::system_clock::now().time_since_epoch().count() + std::hash<std::thread::id>{}(std::this_thread::get_id())) {
            rng.seed(_seed);
        }

        value gen() {
            return d(rng);
        }

        value gen(value min, value max) {
            range(min, max);
            return gen();
        }

        void range(value min, value max) {
            d.param(std::uniform_real_distribution<value>::param_type(min, max));
        }

    };

    struct Particle {
        std::vector<value> x, v, p_best;
    };

    struct Dimension {
        value lower;
        value upper;
    };

    struct Bound {
        std::vector<Dimension> x;
        std::vector<Dimension> v;
    };


    template<class Objective>
    class ParticleSwarmOptimization
    {
    public:

        std::shared_mutex mtx;
        Objective func;
		RNG rand;

        Bound bound;

        std::vector<value> g_best;
        std::vector<Particle> particles;

        double c1 = 1.5 / 2, c2 = 1.5 / 2;
        double w_max = 0.8, w_min = 0.4;
        int T = 200;
        std::size_t particles_num = 10;
		int precision = 6;

        void set_bound(const Bound& _bound) {
            bound = _bound;
        }

		void set_iterations(int iters) {
			T = iters;
		}

        void set_particle_num(std::size_t num) {
            particles_num = num;
            particles.resize(particles_num);
        }

        void init_particles() {
            auto len = bound.x.size();
            g_best.resize(len + 1);
            std::fill(g_best.begin(), g_best.end(), 0);
            g_best.back() = std::numeric_limits<double>::min();

            for (auto& el : particles) {
                el.x.reserve(len);
                el.v.reserve(len);
                el.p_best.resize(len + 1);
                std::fill(el.p_best.begin(), el.p_best.end(), 0);
                el.p_best.back() = std::numeric_limits<double>::min();
				for (std::size_t i = 0; i < len; i++) {
					el.x.emplace_back(rand.gen(bound.x[i].lower, bound.x[i].upper));

					std::stringstream ss;
					ss << std::fixed << std::setprecision(precision) << el.x[i];
					ss >> el.x[i];

					el.v.emplace_back(rand.gen(bound.v[i].lower, bound.v[i].upper));
				}
            }
        }

        void run() {

			for (auto i = 0; i < T; i++) {
//std::for_each(std::execution::par, particles.begin(), particles.end(), [&](auto& el) {
#pragma omp parallel for num_threads(12)
			for (auto n = 0; n < particles.size(); n++) {
				auto& el = particles[n];
				thread_local RNG rand;

					// update the optimal values and positions for each individual
					if (auto f_val = func(el.x); el.p_best.back() < f_val) {
						std::copy(el.x.begin(), el.x.end(), el.p_best.begin());
						el.p_best.back() = f_val;
					}

					// update global optimal position and value
					{
						std::unique_lock lock(mtx);
						if (g_best.back() <= el.p_best.back()) {
							std::copy(el.p_best.begin(), el.p_best.end(), g_best.begin());
						}
					}

					// dynamically calculating inertia weights
					auto w = w_max - (w_max - w_min) * i / T;

					for (auto k = 0; k < el.v.size(); k++) {
						auto r1 = rand.gen(0, 1), r2 = rand.gen(0, 1);
						// update speed, as the location needs to be updated with probability judgment later on
						{
							std::shared_lock lock(mtx);
							el.v[k] = w * el.v[k] + c1 * r1 * (el.p_best[k] - el.x[k]) + c2 * r2 * (g_best[k] - el.x[k]);
						}
						el.x[k] += el.v[k];

						// boundary condition processing
						if (el.v[k] < bound.v[k].lower || el.v[k] > bound.v[k].upper) {
							el.v[k] = bound.v[k].lower + rand.gen(0, 1) * (bound.v[k].upper - bound.v[k].lower);
						}
						if (el.x[k] < bound.x[k].lower || el.x[k] > bound.x[k].upper) {
							el.x[k] = bound.x[k].lower + rand.gen(0, 1) * (bound.x[k].upper - bound.x[k].lower);
						}

						std::stringstream ss;
						ss << std::fixed << std::setprecision(precision) << el.x[k];
						ss >> el.x[k];

					}
				}
//);
			}


            std::cout << "iterations:" << T << " \n param:[ ";
            for (auto i = 0; i < g_best.size() - 1; i++) {
                std::cout << g_best[i] << " ";
            }
            std::cout << "] value ֵ:" << g_best.back();
        }
    };
}


#include<iostream>
#include<vector>
#include<algorithm>
#include<iterator>
#include<type_traits>
#include<ios>
#include<iomanip>
#include<assert.h>
#include<string_view>
#include<random>
#include<chrono>
#include<thread>
#include<array>
#include<numeric>
#include<cmath>

namespace Base {
	// primary template
	template<class T>
	struct extract_type {
		using value_type = T;
	};

	// specialization for template instantiated types
	template<template<class> class T, class U>
	struct extract_type<T<U>> {
		using value_type = typename extract_type<U>::value_type;
	};

	// helper alias
	template<class T>
	using extract_type_t = typename extract_type<T>::value_type;

	// decide whether T is a container type - i define this as anything that has a well formed begin iterator type.
	// we return true/false to determing if T is a container type.
	// we use the type conversion ability of nullptr to std::nullptr_t or void* (prefers std::nullptr_t overload if it exists).
	// use SFINAE to conditionally enable the std::nullptr_t overload.
	// these types might not have a default constructor, so return a pointer to it.
	// base case returns void* which we decay to void to represent not a container.
	template<class T>
	void* _iter_elem(void*) { return nullptr; };

	template<class T>
	typename std::iterator_traits<decltype(std::begin(*(T*)nullptr))>::value_type* _iter_elem(std::nullptr_t) { return nullptr; };

	// this is just a convenience wrapper to make the above user friendly
	template<class T>
	struct container_stuff {
		using elem_t = std::remove_pointer_t<decltype(_iter_elem<T>(nullptr))>;
		static inline constexpr bool is_container = !std::is_same_v<elem_t, void>;
	};

	// and our old dimension counting logic (now uses std::nullptr_t SFINAE logic)
	template<class T>
	constexpr std::size_t _dimensions(void*) { return 0; }

	template<class T, std::enable_if_t<container_stuff<T>::is_container, int> = 0>
	constexpr std::size_t _dimensions(std::nullptr_t) { return 1 + _dimensions<typename container_stuff<T>::elem_t>(nullptr); }

	// and our nice little alias
	template<class T>
	inline constexpr std::size_t dimensions_v = _dimensions<T>(nullptr);

	template<class T, typename std::enable_if_t<std::is_arithmetic_v<T>>* = nullptr>
	using require_num_type = T;

	template<class T>
	class Mat : public std::conditional_t<std::is_arithmetic_v<extract_type_t<T>>, std::vector<T>, void> {
	public:
		using std::vector<T>::vector;
		using inner_value_type = extract_type_t<T>;

		template<class Pred>
		Mat& map(Pred&& pred) {
			map_(*this, pred);
			return *this;
		}

		void clip(inner_value_type lb, inner_value_type ub) {
			map([&](auto& p) { p = std::clamp(p, lb, ub); });
		}

		void clip(const Mat<T>& v1, const Mat<T>& v2) {
			each_([&](auto& p, auto& lb, auto& ub) {
				p = std::clamp(p, lb, ub);
				}, *this, v1, v2);
		}

		auto sum() {
			inner_value_type s = 0.;
			map([&](auto& p) { s += p; });
			return s;
		}

		template<class...Ts, class Pred>
		auto each(Pred&& pred, Ts& ...ts) {
			each_(pred, *this, ts...);
			return *this;
		}

		auto test(Mat<T>& v) {
			double out = 0;
			each_([&](auto v1, auto v2) {
				out += v1 + v2;
				}, *this, v);
			return out;
		}

#define Mat_OP(OP)																											\
		template<class K>																									\
		friend Mat<T> operator OP (require_num_type<K> v, const Mat<T>& _d){												\
			auto d = _d;																									\
			std::for_each(d.begin(), d.end(),[&](auto & p){ p = v OP p;});													\
			return d;																										\
		}																													\
																															\
		template<class K>																									\
		friend Mat<T> operator OP(const Mat<T>& _d, require_num_type<K> v){													\
			auto d = _d;																									\
			std::for_each(d.begin(), d.end(),[&](auto & p){ p = p OP v;});													\
			return d;																										\
		}																													\
																															\
		friend Mat<T> operator OP(const Mat<T>& d1, const Mat<T>& d2){														\
			auto d = d1;																									\
			std::for_each(d.begin(), d.end(),[&, n = 0](auto & p) mutable { p OP##= d2[n++];});								\
			return d;																										\
		}																													\
																															\
		friend Mat<Mat<T>> operator OP(const Mat<T>& d1, const Mat<Mat<T>>& d2){											\
			auto d = d2;																									\
			std::for_each(d.begin(), d.end(),[&](auto & p) mutable { p = d1 OP p;});										\
			return d;																										\
		}																													\
																															\
		friend Mat<Mat<T>> operator OP(const Mat<Mat<T>>& d1, const Mat<T>& d2){											\
			auto d = d1;																									\
			std::for_each(d.begin(), d.end(),[&](auto & p) mutable { p OP##= d2;});											\
			return d;																										\
		}

		Mat_OP(+)
		Mat_OP(-)
		Mat_OP(*)
		Mat_OP(/)
#undef Mat_OP

#define Mat_OP(OP)																											\
		friend Mat<T>& operator OP(Mat<T>& d1, const Mat<T>& d2){															\
			std::for_each(d1.begin(), d1.end(),[&, n = 0](auto & p) mutable { p OP d2[n++];});								\
			return d1;																										\
		}																													\
																															\
		template<class K>																									\
		friend Mat<T>& operator OP(Mat<T>& _d, require_num_type<K> v){														\
			std::for_each(_d.begin(), _d.end(),[&](auto & p){ p OP v;});													\
			return _d;																										\
		}																													\
																															\
		template<class K>																									\
		friend require_num_type<K>& operator OP(require_num_type<K>& v, const Mat<T>& _d){									\
			std::for_each(_d.begin(), _d.end(),[&](auto & p){ v OP p;});													\
			return v;																										\
		}

		Mat_OP(+=)
		Mat_OP(-=)
		Mat_OP(*=)
		Mat_OP(/=)
#undef Mat_OP

	private:
		template<class...Ts, class Pred>
		auto each_(Pred&& pred, Ts& ...ts) {
			for (auto i = 0; i < std::min({ ts.size()... }); i++) {
				if constexpr ((std::is_arithmetic_v<typename Ts::value_type> && ...))
					pred(ts[i]...);
				else
					each_(pred, ts[i]...);
			}
		}

		template<class K, class Pred>
		static void map_(Mat<K>& v, Pred&& pred) {
			std::for_each(v.begin(), v.end(), [&](auto& p) { pred(p); });
		}

		template<class K, class Pred>
		static void map_(Mat<Mat<K>>& v, Pred&& pred) {
			std::for_each(v.begin(), v.end(), [&](auto& p) { map_(p, pred); });
		}

	};

	template<class T>
	inline std::ostream& operator<<(std::ostream& out, Mat<require_num_type<T>>& v) {
		out << "[";
		std::for_each(v.begin(), v.end(), [&](auto& p) { out << p << ","; });
		out << "\b],";
		return out;
	}

	namespace Mat_print { int i = 1; }

	template<class T>
	inline std::ostream& operator<<(std::ostream& out, Mat<Mat<T>>& v) {
		Mat_print::i++;
		out << "[";
		out << std::setfill(' ');
		std::for_each(v.begin(), v.end() - 1, [&](auto& p) {
			out << p << "\n" << std::right << std::setw(Mat_print::i);
			});
		out << v.back();
		out << "\b]";
		Mat_print::i--;
		return out;
	}

	void UNIT_TEST() {
		assert((
			Mat<double>{ 1, 2, 3 } + Mat<double>{ 4, 5, 6 } == Mat<double>{ 5, 7, 9 }
		));
		assert((
			Mat<double>{ 1, 2, 3 } - Mat<double>{ 4, 5, 6 } == Mat<double>{ -3, -3, -3 }
		));
		assert((
			Mat<double>{ 1, 2, 3 } * 2 == Mat<double>{ 2, 4, 6 }
		));
		assert((
			3 * Mat<double>{ 1, 2, 3 } == Mat<double>{ 3, 6, 9 }
		));
		assert((
			Mat<double>{ 1, 2, 3 } / 4 == Mat<double>{ .25, .5, .75 }
		));
		assert((
			3 / Mat<double>{ 1, 2, 3 } == Mat<double>{ 3, 1.5, 1 }
		));
		assert((
			Mat<Mat<double>>{{ 1, 2, 3 }, { 4, 5, 6 }} * 2 == Mat<Mat<double>>{{2, 4, 6}, { 8, 10, 12 }}
		));
		assert((
			Mat<Mat<double>>{{ 1, 2, 3 }, { 4, 5, 6 }} - Mat<double>{ 1, 2, 3 } == Mat<Mat<double>>{{ 0, 0, 0 }, { 3, 3, 3 }}
		));
		assert((
			Mat<double>{ 1, 2, 3 } - Mat<Mat<double>>{{ 1, 2, 3 }, { 4, 5, 6 }} == Mat<Mat<double>>{{ 0, 0, 0 }, { -3, -3, -3 }}
		));
	}

	using value = double;
	struct RNG {
		std::random_device rd;
		std::default_random_engine rng{ rd() };
		std::uniform_real_distribution<value> d;

		RNG(unsigned _seed = std::chrono::system_clock::now().time_since_epoch().count() + std::hash<std::thread::id>{}(std::this_thread::get_id())) {
			rng.seed(_seed);
		}

		value gen() {
			return d(rng);
		}

		value gen_normal(value mu, value sigma) {
			static std::normal_distribution<value> d_n;
			d_n.param(std::normal_distribution<value>::param_type(mu, sigma));
			return d_n(rng);
		}

		template<class T = int>
		T gen(T min, T max) {
			static std::conditional_t<std::is_integral_v<T>, std::uniform_int_distribution<T>, std::uniform_real_distribution<value>> d_i;
			d_i.param(decltype(d_i)::param_type(min, max));
			return d_i(rng);
		}

	};
}

namespace APSO {

	using namespace std::literals::string_view_literals;
	using namespace Base;

	template<class Objective>
	class AdaptiveParticleSwarmOptimization
	{
	private:
		Objective fitness;
		int D, P, G;
		double w, w_max, w_min, c1, c2, k;
		std::string_view Previous_State;

		std::array<std::array<std::string_view, 7>, 4> rule_base{
			std::array<std::string_view, 7>{"S3", "S2", "S2", "S1", "S1", "S1", "S4"},
			std::array<std::string_view, 7>{"S3", "S2", "S2", "S2", "S1", "S1", "S4"},
			std::array<std::string_view, 7>{"S3", "S3", "S2", "S2", "S1", "S4", "S4"},
			std::array<std::string_view, 7>{"S3", "S3", "S2", "S1", "S1", "S4", "S4"}
		};

		// rule_base.columns = ["S3", "S3&S2", "S2", "S2&S1", "S1", "S1&S4", "S4"]
		// rule_base.index = ["S1", "S2", "S3". "S4"]

		int rule_column(const std::string_view& s) {
			static constexpr auto rules = std::array{ "S3"sv, "S3&S2"sv, "S2"sv, "S2&S1"sv, "S1"sv, "S1&S4"sv, "S4"sv };
			if ("S3" == s)
				return 0;
			else if ("S3&S2" == s)
				return 1;
			else if ("S2" == s)
				return 2;
			else if ("S2&S1" == s)
				return 3;
			else if ("S1" == s)
				return 4;
			else if ("S1&S4" == s)
				return 5;
			else if ("S4" == s)
				return 6;
		}

		int rule_index(const std::string_view& s) {
			static constexpr auto rules = std::array{ "S1"sv, "S2"sv, "S3"sv, "S4"sv };
			if ("S1" == s)
				return 0;
			else if ("S2" == s)
				return 1;
			else if ("S3" == s)
				return 2;
			else if ("S4" == s)
				return 3;
		}

		//static constexpr auto rule_columns = std::array{"S3", "S3&S2", "S2", "S2&S1", "S1", "S1&S4", "S4"};

		Mat<Mat<double>> ub, lb, v_max;
		Mat<Mat<double>> X, V;

		Mat<double> F;

		Mat<Mat<double>> pbest_X;
		Mat<double> pbest_F;

		Mat<double> gbest_X;
		double gbest_F;

		Mat<double> loss_curve;

		inline static thread_local RNG rand;

		template <typename T, typename... Args>
		inline void InitClass(T& t, Args... args)
		{
			t.~T();
			new (&t) T(args...);
		}

	public:
		AdaptiveParticleSwarmOptimization(int _D = 30, int _P = 20, int _G = 500, int _ub = 1, int _lb = 0, double _w_max = .9,
			double _w_min = .4, double _c1 = 2.0, double _c2 = 2.0, double _k = 0.2) {
			D = _D, P = _P, G = _G;
			InitClass(ub, P, Mat<double>(D, _ub));
			InitClass(lb, P, Mat<double>(D, _lb));
			//ub = _ub, lb = _lb;
			w_max = _w_max, w_min = _w_min;
			w = w_max;
			c1 = _c1, c2 = _c2, k = _k;
			v_max = k * (ub - lb);
			Previous_State = "S1";
			InitClass(pbest_X, P, Mat<double>(D, 0.));
			InitClass(pbest_F, P, 0.);
			InitClass(gbest_X, D, 0.);
			gbest_F = 0.;
			InitClass(loss_curve, G, 0.);
		}

		void opt() {
			//initialize
			InitClass(X, P, Mat<double>(D, 0));

			X.each([&](auto& x, auto& _lb, auto& _ub) {x = rand.gen(_lb, _ub); }, lb, ub);
			InitClass(V, P, Mat<double>(D, 0.));
			InitClass(F, P, 0.);
			//iterate
			for (int g = 0; g < G; g++) {
				//fitness calculation
				std::transform(X.begin(), X.end(), F.begin(), [&](auto& p)
					{
						return fitness(p);
					});

				//update the best solution
				std::for_each(F.begin(), F.end(), [&, n = 0](auto f) mutable
					{
						auto& Xn = pbest_X[n];
						auto& Fn = pbest_F[n];
						if (Fn < f) {
							Xn = X[n];
							Fn = f;
						}
						++n;
					});


				if (auto extra_F = std::max_element(F.begin(), F.end()); gbest_F < *extra_F) {
					auto idx = std::distance(F.begin(), extra_F);
					gbest_X = X[idx];
					gbest_F = *extra_F;
				}

				//convergence curve
				loss_curve[g] = gbest_F;

				//Iterative state estimation
				EvolutionaryStateEstimation(g);

				//update
				Mat<Mat<double>> r1, r2;
				r1 = r2 = decltype(r1)(P, decltype(r1)::value_type(D, 0.));
				for (auto r : std::array{ &r1, &r2 }) {
					r->map([&](auto& p) { p = rand.gen(0., 1.); });
				}
				//update V
				V = w * V + c1 * r1 * (pbest_X - X) + c2 * r2 * (gbest_X - X);
				//boundary treatment
				V.clip(0 - v_max, v_max);
				//update X
				X += V;
				//boundary treatment
				X.clip(lb, ub);
			}
			//std::cout << "loss_curve:" << loss_curve << "\n";
			std::cout << "gbest_X:" << gbest_X << "\ngbest_F:" << gbest_F;
		}

		void EvolutionaryStateEstimation(int g) {
			//step 1
			Mat<double> d(P, 0.);
			for (auto i = 0; i < P; i++) {
				auto f1_ = (X[i] - X).map([](auto& p) { p = std::pow(p, 2); });
				Mat<double> f1, f2;
				std::transform(f1_.begin(), f1_.end(), std::back_inserter(f1), [](auto& p)
					{
						return p.sum();
					});
				std::transform(f1.begin(), f1.end(), std::back_inserter(f2), [](auto& p)
					{
						return std::sqrt(p);
					});
				auto f3 = f2.sum();
				d[i] = f3 / (P - 1);
			}
			//step 2
			auto idx = std::distance(F.begin(), std::max_element(F.begin(), F.end()));
			auto [dmin, dmax] = [](const auto& pair) {
				return std::make_pair(*pair.first, *pair.second);
				}(std::minmax_element(d.begin(), d.end()));
				auto dg = d[idx];

				//step 3
				auto f = (dg - dmin) / (dmax - dmin);
				if (dmax == dmin)
					f = 0.;
				//std::cout << "dg:" << dg << " dmin:" << dmin << " dmax:" << dmax << " f:" << f << "\n";

				//step 4
				//Case (a)-Exploration
				double uS1 = 0, uS2 = 0, uS3 = 0, uS4 = 0;
				std::string_view Current_State, Final_State;
				if (0.0 <= f && f <= 0.4)
					uS1 = 0.0;
				else if (0.4 < f && f <= 0.6)
					uS1 = 5 * f - 2;
				else if (0.6 < f && f <= 0.7)
					uS1 = 1.0;
				else if (0.7 < f && f <= 0.8)
					uS1 = -10 * f + 8;
				else if (0.8 < f && f <= 1.0)
					uS1 = 0.0;
				//Case (b)-Exploitation
				if (0.0 <= f && f <= 0.2)
					uS2 = 0;
				else if (0.2 < f && f <= 0.3)
					uS2 = 10 * f - 2;
				else if (0.3 < f && f <= 0.4)
					uS2 = 1.0;
				else if (0.4 < f && f <= 0.6)
					uS2 = -5 * f + 3;
				else if (0.6 < f && f <= 1.0)
					uS2 = 0.0;
				//Case (c)-Convergence
				if (0.0 <= f && f <= 0.1)
					uS3 = 1.0;
				else if (0.1 < f && f <= 0.3)
					uS3 = -5 * f + 1.5;
				else if (0.3 < f && f <= 1.0)
					uS3 = 0.0;
				//Case (d)-Jumping Out
				if (0.0 <= f && f <= 0.7)
					uS4 = 0.0;
				else if (0.7 < f && f <= 0.9)
					uS4 = 5 * f - 3.5;
				else if (0.9 < f && f <= 1.0)
					uS4 = 1.0;

				//===============================================================================
				//        S3   S3&S2    S2   S2&S1    S1   S1&S4    S4     ->  Current state
				// S1     S3     S2     S2     S1     S1     S1     S4
				// S2     S3     S2     S2     S2     S1     S1     S4
				// S3     S3     S3     S2     S2     S1     S4     S4
				// S4     S3     S3     S2     S1     S1     S4     S4
				// |
				// -> Previous state
				//===============================================================================

				//std::cout << "uS1:" << uS1 << " uS2:" << uS2 << " uS3:" << uS3 << " uS4:" << uS4 << "\n";
				if (uS3 != 0) {
					Current_State = "S3";
					if (uS2 != 0) {
						Current_State = "S3&S2";
					}
				}
				else if (uS2 != 0) {
					Current_State = "S2";
					if (uS1 != 0) {
						Current_State = "S2&S1";
					}
				}
				else if (uS1 != 0) {
					Current_State = "S1";
					if (uS4 != 0) {
						Current_State = "S1&S4";
					}
				}
				else if (uS4 != 0) {
					Current_State = "S4";
				}

				//std::cout << "Current_State:" << Current_State << " Previous_State:" << Previous_State << "\n";
				Final_State = rule_base[rule_index(Previous_State)][rule_column(Current_State)];
				Previous_State = Final_State;

				//step 5
				double delta[2]{};
				std::generate(std::begin(delta), std::end(delta), [&]() {return rand.gen(0.05, 0.1); });

				if (Final_State == "S1") { //Exploration
					c1 += delta[0];
					c2 -= delta[1];
				}
				else if (Final_State == "S2") { //Exploitation
					c1 += 0.5 * delta[0];
					c2 -= 0.5 * delta[1];
				}
				else if (Final_State == "S3") { //Converrgence
					c1 += 0.5 * delta[0];
					c2 += 0.5 * delta[1];
					ElitistLearningStrategy(g);
				}
				else if (Final_State == "S4") { //Jumping Out
					c1 -= delta[0];
					c2 += delta[1];
				}

				c1 = std::clamp(c1, 1.5, 2.5);
				c2 = std::clamp(c2, 1.5, 2.5);
				if (auto c = c1 + c2; !(3.0 <= c && c <= 4.0)) {
					c1 = 4.0 * c1 / c;
					c2 = 4.0 * c2 / c;
				}

				//step 6
				w = 1 / (1 + 1.5 * std::exp(-2.6 * f));
				w = std::clamp(w, w_min, w_max);
		}

		void ElitistLearningStrategy(int g) {
			//return;
			auto P0 = gbest_X;
			auto d = rand.gen(0, D - 1);

			auto mu = 0.;
			auto sigma = 1 - 0.9 * g / G;
			P0[d] = P0[d] + ub[0][d] - lb[0][d] * rand.gen_normal(mu, sigma * sigma);

			P0.clip(lb[0], ub[0]);
			auto v = fitness(P0);

			if (v > gbest_F) {
				gbest_X = P0;
				gbest_F = v;
			}
			else if (auto idx = std::distance(F.begin(), std::min_element(F.begin(), F.end())); v > F[idx]) {
				X[idx] = P0;
				F[idx] = v;
			}

		}
	};
}

namespace CE {
	using namespace Base;

	template<class F>
	class CrossEntropyMethod {
	public:
		F func;
		std::size_t d;
		std::size_t maxits, N, Ne;
		bool reverse;
		Mat<Mat<double>> v_lim;
		double init_coef;
		RNG rand;

		CrossEntropyMethod(std::size_t _d, std::size_t _maxits = 2, std::size_t _N = 100, std::size_t _Ne = 10,
			const Mat<Mat<double>>& _v_lim = {}, bool argmin = true) :
			d(_d), maxits(_maxits), N(_N), Ne(_Ne), v_lim(_v_lim), reverse(argmin), init_coef(1)
		{

		}

		double noise(auto x) {
#define PI std::acos(-1)
			double ex = -(x + 100) / (10 * PI);
			return std::exp(ex);
#undef PI
		}

		void opt() {
			Mat<double> mu(d, 0), sigma(d, init_coef);
			for (auto t = 0; t < maxits; ++t) {
				Mat<Mat<double>> x(N, Mat<double>(d, 0.));
				Mat<double> s(N, 0.);
				std::transform(x.begin(), x.end(), s.begin(), [&](auto& k)
					{
						k.each([&, n = 0](auto& _k, auto& _mu, auto& _sigma) mutable
							{
								_k = std::clamp(rand.gen_normal(_mu, _sigma), v_lim[0][n], v_lim[1][n]);
								n++;
							}, mu, sigma);
						return func(k);
					});

				Mat<std::size_t> seq(N);
				std::iota(seq.begin(), seq.end(), 0);
				std::sort(seq.begin(), seq.end(), [&](auto a, auto b) {
					return !reverse ? std::less{}(s[a], s[b]) : std::greater{}(s[a], s[b]);
					});

				{
					Mat<double> mean(d, 0.);
					for (auto i = 0; i < Ne; i++) {
						mean += x[seq[i]];
					}
					mean /= Ne;
					mu = mean;
				}

				{
					Mat<double> stddif(d, 0.);
					for (auto i = 0; i < Ne; i++) {
						stddif += (mu - x[seq[i]]).each([](auto& k) { k = std::pow(k, 2); });
					}
					stddif /= Ne;
					stddif.each([](auto& k) { k = std::sqrt(k); });
					stddif += noise(t);
					sigma = stddif;
				}

				std::cout << "iteration " << t << " mu: " << mu << " sigma: " << sigma << "\n";
			}

			std::cout << "parm:" << mu << " value:" << func(mu);
		}

		void opt_uniform() {
			Mat<double> max_x(d, 0.), min_x(d, 0.);
			min_x.each([&](auto& k, auto& lower) {k = lower; }, v_lim[0]);
			max_x.each([&](auto& k, auto& upper) {k = upper; }, v_lim[1]);

			for (auto t = 0; t < maxits; ++t) {
				Mat<Mat<double>> x(N, Mat<double>(d, 0.));
				Mat<double> s(N, 0.);
				std::transform(x.begin(), x.end(), s.begin(), [&](auto& k)
					{
						k.each([&, n = 0](auto& _k, auto& _min_x, auto& _max_x) mutable
							{
								_k = std::clamp(rand.gen<double>(_min_x, _max_x), v_lim[0][n], v_lim[1][n]);
								n++;
							}, min_x, max_x);
						return func(k);
					});

				Mat<std::size_t> seq(N);
				std::iota(seq.begin(), seq.end(), 0);
				std::sort(seq.begin(), seq.end(), [&](auto a, auto b) {
					return !reverse ? std::less{}(s[a], s[b]) : std::greater{}(s[a], s[b]);
					});


				Mat<double> min_select = x[seq.front()], max_select = x[seq.front()];
				for (auto i = 0; i < Ne; i++) {
					x[seq[i]].each([&](auto& _x, auto& _min_sel, auto& _max_sel) {
						if (_min_sel > _x)
							_min_sel = _x;
						if (_max_sel < _x)
							_max_sel = _x;
						}, min_select, max_select);
				}

				min_x.each([&](auto& _min_x, auto& _max_x, auto& _min_sel, auto& _max_sel) {
					if (_min_x < _min_sel)
						_min_x = _min_sel;
					if (_max_x > _max_sel)
						_max_x = _max_sel;
					}, max_x, min_select, max_select);

				auto avg_x = (min_x + max_x) / 2;
				std::cout << "iteration " << t << " avg: " << avg_x << "\n";
			}

			auto avg_x = ((min_x + max_x) / 2);
			std::cout << "parm:" << avg_x << " value:" << func(avg_x);
		}

	};
}

