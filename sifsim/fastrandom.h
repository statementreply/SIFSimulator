#pragma once

#include <cstdint>
#include <cmath>
#include <limits>


namespace FastRandom {
	// Faster bernoulli distribution
	// Requires 32+ bit UniformRandomBitGenerator
	class bernoulli_distribution {
	public:
		typedef bool result_type;

		struct param_type {
			typedef bernoulli_distribution distribution_type;

			explicit param_type(double p = 0.5) {
				_p32 = static_cast<uint32_t>(std::llrint(
					p * std::numeric_limits<uint32_t>::max()));
			}

			bool operator ==(const param_type & b) const {
				return _p32 == b._p32;
			}

			bool operator !=(const param_type & b) const {
				return !(*this == b);
			}

			double p() const {
				return static_cast<double>(_p32) / std::numeric_limits<uint32_t>::max();
			}

			uint32_t _p32;
		};

		explicit bernoulli_distribution(double p = 0.5)
			: _param(p) {
		}

		explicit bernoulli_distribution(const param_type & param)
			: _param(param) {
		}

		double p() const {
			return _param.p();
		}

		param_type param() const {
			return _param;
		}

		void param(const param_type & param) {
			_param = param;
		}

		result_type(min)() const {
			return false;
		}

		result_type(max)() const {
			return true;
		}

		void reset() {}

		template<class Generator>
		result_type operator()(Generator & g) const {
			return _eval(g, _param);
		}

		template<class Generator>
		result_type operator()(Generator & g, const param_type & param) const {
			return _eval(g, param);
		}

	private:
		template<class Generator>
		result_type _eval(Generator & g, const param_type & param) const {
			static_assert(g.min() == 0 && g.max() >= std::numeric_limits<uint32_t>::max(),
				"UniformRandomBitGenerator for FastRandom shall generate at least 32 bits per call");
			uint32_t u;
			do {
				u = static_cast<uint32_t>(g());
			} while (u == 0);
			return u <= _param._p32;
		}

		param_type _param;
	};


	// Faster normal distribution (Ziggurat algorithm)
	// Requires 32+ bit UniformRandomBitGenerator
	template <class RealType = double>
	class normal_distribution {
	public:
		typedef RealType result_type;

		struct param_type {
			typedef normal_distribution<RealType> distribution_type;

			explicit param_type(RealType mean = 0., RealType stddev = 1.) {
				_mean = mean;
				_stddev = stddev;
			}

			bool operator ==(const param_type & b) const {
				return _mean == b._mean
					&& _stddev == b._stddev;
			}

			bool operator !=(const param_type & b) const {
				return !(*this == b);
			}

			RealType mean() const {
				return _mean;
			}

			RealType stddev() const {
				return _stddev;
			}

			RealType _mean;
			RealType _stddev;
		};

		explicit normal_distribution(RealType mean = 0., RealType stddev = 1.)
			: _param(mean, stddev) {
		}

		explicit normal_distribution(const param_type & param)
			: _param(param) {
		}

		RealType mean() const {
			return _param.mean();
		}

		RealType stddev() const {
			return _param.stddev();
		}

		param_type param() const {
			return _param;
		}

		void param(const param_type & param) {
			_param = param;
		}

		result_type(min)() const {
			return -INFINITY;
		}

		result_type(max)() const {
			return INFINITY;
		}

		void reset() {}

		template<class Generator>
		result_type operator()(Generator & g) const {
			return _eval(g, _param);
		}

		template<class Generator>
		result_type operator()(Generator & g, const param_type & param) const {
			return _eval(g, param);
		}

	private:
		template<class Generator>
		result_type _eval(Generator & g, const param_type & param) const {
			static_assert(g.min() == 0 && g.max >= std::numeric_limits<uint32_t>::max(),
				"UniformRandomBitGenerator for FastRandom shall generate at least 32 bits per call");
			return NAN;
		}

		param_type _param;

	private:
		constexpr static int ZIGNOR_LOG2_N = 128;
		constexpr static size_t ZIGNOR_N = size_t(1) << ZIGNOR_LOG2_N;
		constexpr static double ZIGNOR_TAIL_X = 3.442619855899;
		constexpr static double ZIGNOR_AREA = 9.91256303526217e-3;
		static double ZIGNOR_SCALE[ZIGNOR_N];
		static uint32_t ZIGNOR_THERS[ZIGNOR_N];

		static double _initFlag = _initTables();
		static bool _initTables() {
			constexpr double QUANT = UINT32_C(1) << (31 - ZIGNOR_LOG2_N);
			double x = ZIGNOR_TAIL_X;
			double y = std::exp(-0.5 * x * x);
			ZIGNOR_SCALE[0] = ZIGNOR_AREA / y;
			for (size_t i = 1; i < ZIGNOR_N; i++) {
				double oldx = x;
				ZIGNOR_SCALE[i] = oldx;
				y += ZIGNOR_AREA / oldx;
				if (i + 1 < ZIGNOR_N) {
					x = std::sqrt(-2. * std::log(y));
					ZIGNOR_THERS[i] = (x / oldx * QUANT);
				} else {
					ZIGNOR_THRES[i] = 0;
				}
			}
			return y;
		}
	};
}
