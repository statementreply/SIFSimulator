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
			for (;;) {
				uint32_t u = static_cast<uint32_t>(g());
				if (u != 0) {
					return u <= param._p32;
				}
			};
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
				static double _initFlag = _initTables();
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
			return _evalInternal(g) * param._stddev + param._mean;
		}

		template<class Generator>
		double _evalInternal(Generator & g) const {
			static_assert(g.min() == 0 && g.max() >= std::numeric_limits<uint32_t>::max(),
				"UniformRandomBitGenerator for FastRandom shall generate at least 32 bits per call");
			for (;;) {
				constexpr double Y_SCALE = 1. / (std::numeric_limits<uint32_t>::max() + 1.);
				constexpr double X_SCALE = 2. / (std::numeric_limits<uint32_t>::max() + 1.);
				uint32_t r = static_cast<uint32_t>(g());
				uint32_t index = r & ZIGNOR_INDEX_MASK;
				uint32_t uvalue = r & ZIGNOR_VALUE_MASK | ZIGNOR_VALUE_OFFSET;
				int32_t value = reinterpret_cast<const int32_t &>(uvalue);
				if (static_cast<uint32_t>(abs(value)) < ZIGNOR_THRES(index)) {
					return value * ZIGNOR_SCALE(index);
				}
				if (index > 0) {
					uint32_t r2 = static_cast<uint32_t>(g());
					double x = value * ZIGNOR_SCALE(index);
					double y = ZIGNOR_Y(index) + (ZIGNOR_Y(index + 1) - ZIGNOR_Y(index))
						* ((r2 + 0.5) * Y_SCALE);
					if (y < exp(-0.5 * x * x)) {
						return x;
					}
				} else {
					for (;;) {
						constexpr static double REV_TAIL_X = 1. / ZIGNOR_TAIL_X;
						uint32_t r2 = static_cast<uint32_t>(g());
						uint32_t r3 = static_cast<uint32_t>(g());
						int32_t ir3 = reinterpret_cast<const int32_t &>(r3);
						double py = -log((r2 + 0.5) * Y_SCALE);
						double dx = -log(fabs(ir3 + 0.5) * X_SCALE) * REV_TAIL_X;
						if (py > 0.5 * dx * dx) {
							return ir3 >= 0 ? dx + ZIGNOR_TAIL_X : -(dx + ZIGNOR_TAIL_X);
						}
					}
				}
			}
		}

		param_type _param;

	private:
		constexpr static int ZIGNOR_LOG2_N = 7;
		constexpr static uint32_t ZIGNOR_N = UINT32_C(1) << ZIGNOR_LOG2_N;
		constexpr static uint32_t ZIGNOR_INDEX_MASK = ZIGNOR_N - 1;
		constexpr static uint32_t ZIGNOR_VALUE_MASK = ~ZIGNOR_INDEX_MASK;
		constexpr static uint32_t ZIGNOR_VALUE_OFFSET = UINT32_C(1) << (ZIGNOR_LOG2_N - 1);
		constexpr static double ZIGNOR_TAIL_X = 3.4426198558966520937;
		constexpr static double ZIGNOR_PSEUDO_X = 3.71308624674036290314;
		constexpr static double ZIGNOR_AREA = 0.00991256303533646107072;

		static double & ZIGNOR_Y(size_t index) {
			static double ZIGNOR_Y_DATA[ZIGNOR_N + 1];
			return ZIGNOR_Y_DATA[index];
		}
		static double & ZIGNOR_SCALE(size_t index) {
			static double ZIGNOR_SCALE_DATA[ZIGNOR_N];
			return ZIGNOR_SCALE_DATA[index];
		}
		static uint32_t & ZIGNOR_THRES(size_t index) {
			static uint32_t ZIGNOR_THRES_DATA[ZIGNOR_N];
			return ZIGNOR_THRES_DATA[index];
		}

		static double _initTables() {
			constexpr double QUANT = (std::numeric_limits<uint32_t>::max() + 1.) * 0.5;
			constexpr double SCALE = 1. / QUANT;
			double y = 0;
			double x = ZIGNOR_PSEUDO_X;
			ZIGNOR_Y(0) = 0;
			for (size_t i = 0; i < ZIGNOR_N; i++) {
				double oldx = x;
				y += ZIGNOR_AREA / oldx;
				ZIGNOR_Y(i + 1) = y;
				ZIGNOR_SCALE(i) = oldx * SCALE;
				if (i + 1 < ZIGNOR_N) {
					x = std::sqrt(-2. * std::log(y));
					ZIGNOR_THRES(i) = static_cast<uint32_t>(std::lrint(x / oldx * QUANT));
				} else {
					ZIGNOR_THRES(i) = 0;
				}
			}
			return y;
		}
	};
}
