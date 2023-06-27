/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2023 Yermalayeu Ihar.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#include "Simd/SimdMemory.h"
#include "Simd/SimdStore.h"
#include "Simd/SimdExtract.h"
#include "Simd/SimdArray.h"
#include "Simd/SimdUnpack.h"
#include "Simd/SimdDescrInt.h"
#include "Simd/SimdDescrIntCommon.h"
#include "Simd/SimdCpu.h"
#include "Simd/SimdFloat16.h"

namespace Simd
{
#ifdef SIMD_SSE41_ENABLE    
    namespace Sse41
    {
        static void MinMax32f(const float* src, size_t size, float& min, float& max)
        {
            assert(size % 8 == 0);
            __m128 _min = _mm_set1_ps(FLT_MAX);
            __m128 _max = _mm_set1_ps(-FLT_MAX);
            size_t i = 0;
            for (; i < size; i += 4)
            {
                __m128 _src = _mm_loadu_ps(src + i);
                _min = _mm_min_ps(_src, _min);
                _max = _mm_max_ps(_src, _max);
            }
            MinVal32f(_min, min);
            MaxVal32f(_max, max);
        }

        //-------------------------------------------------------------------------------------------------

        static void MinMax16f(const uint16_t* src, size_t size, float& min, float& max)
        {
            assert(size % 8 == 0);
            __m128 _min = _mm_set1_ps(FLT_MAX);
            __m128 _max = _mm_set1_ps(-FLT_MAX);
            size_t i = 0;
            for (; i < size; i += 4)
            {
                __m128i f16 = _mm_loadl_epi64((__m128i*)(src + i));
                __m128 _src = Float16ToFloat32(UnpackU16<0>(f16));
                _min = _mm_min_ps(_src, _min);
                _max = _mm_max_ps(_src, _max);
            }
            MinVal32f(_min, min);
            MaxVal32f(_max, max);
        }

        //-------------------------------------------------------------------------------------------------

        static void UnpackNormA(size_t count, const uint8_t* const* src, float* dst, size_t stride)
        {
            for (size_t i = 0; i < count; ++i)
                _mm_storeu_si128((__m128i*)dst + i, _mm_loadu_si128((__m128i*)src[i]));
        }

        //-------------------------------------------------------------------------------------------------


        static void UnpackNormB(size_t count, const uint8_t* const* src, float* dst, size_t stride)
        {
            size_t count4 = AlignLo(count, 4), i = 0;
            for (; i < count4; i += 4, src += 4, dst += 4)
            {
                __m128 s0 = _mm_loadu_ps((float*)src[0]);
                __m128 s1 = _mm_loadu_ps((float*)src[1]);
                __m128 s2 = _mm_loadu_ps((float*)src[2]);
                __m128 s3 = _mm_loadu_ps((float*)src[3]);
                __m128 s00 = _mm_unpacklo_ps(s0, s2);
                __m128 s01 = _mm_unpacklo_ps(s1, s3);
                __m128 s10 = _mm_unpackhi_ps(s0, s2);
                __m128 s11 = _mm_unpackhi_ps(s1, s3);
                _mm_storeu_ps(dst + 0 * stride, _mm_unpacklo_ps(s00, s01));
                _mm_storeu_ps(dst + 1 * stride, _mm_unpackhi_ps(s00, s01));
                _mm_storeu_ps(dst + 2 * stride, _mm_unpacklo_ps(s10, s11));
                _mm_storeu_ps(dst + 3 * stride, _mm_unpackhi_ps(s10, s11));
            }
            for (; i < count; i++, src++, dst++)
            {
                dst[0 * stride] = ((float*)src)[0];
                dst[1 * stride] = ((float*)src)[1];
                dst[2 * stride] = ((float*)src)[2];
                dst[3 * stride] = ((float*)src)[3];
            }
        }

        //-------------------------------------------------------------------------------------------------

        DescrInt::DescrInt(size_t size, size_t depth)
            : Base::DescrInt(size, depth)
        {
            _minMax32f = MinMax32f;
            _minMax16f = MinMax16f;
            _encode32f = GetEncode32f(_depth);
            _encode16f = GetEncode16f(_depth);

            _decode32f = GetDecode32f(_depth);
            _decode16f = GetDecode16f(_depth);

            _cosineDistance = GetCosineDistance(_depth);
            _macroCosineDistancesDirect = GetMacroCosineDistancesDirect(_depth);
            _microMd = 2;
            _microNd = 4;

            _unpackNormA = UnpackNormA;
            _unpackNormB = UnpackNormB;
            _unpackDataA = GetUnpackData(_depth, false);
            _unpackDataB = GetUnpackData(_depth, true);
            _macroCosineDistancesUnpack = GetMacroCosineDistancesUnpack(_depth);
            _unpSize = _size * (_depth == 8 ? 2 : 1);
            _microMu = _depth == 8 ? 6 : 5;
            _microNu = 8;
        }

        void DescrInt::CosineDistancesMxNa(size_t M, size_t N, const uint8_t* const* A, const uint8_t* const* B, float* distances) const
        {
            if(_unpSize * _microNu > Base::AlgCacheL1() || N * 2 < _microNu || _depth < 5 || _depth == 8)
                CosineDistancesDirect(M, N, A, B, distances);
            else
                CosineDistancesUnpack(M, N, A, B, distances);
        }

        void DescrInt::CosineDistancesMxNp(size_t M, size_t N, const uint8_t* A, const uint8_t* B, float* distances) const
        {
            Array8ucp a(M);
            for (size_t i = 0; i < M; ++i) 
                a[i] = A + i * _encSize;
            Array8ucp b(N);
            for (size_t j = 0; j < N; ++j)
                b[j] = B + j * _encSize;
            CosineDistancesMxNa(M, N, a.data, b.data, distances);
        }

        void DescrInt::CosineDistancesDirect(size_t M, size_t N, const uint8_t* const* A, const uint8_t* const* B, float* distances) const
        {
            const size_t L2 = Base::AlgCacheL2();
            size_t mN = AlignLoAny(L2 / _encSize, _microNd);
            size_t mM = AlignLoAny(L2 / _encSize, _microMd);
            for (size_t i = 0; i < M; i += mM)
            {
                size_t dM = Simd::Min(M, i + mM) - i;
                for (size_t j = 0; j < N; j += mN)
                {
                    size_t dN = Simd::Min(N, j + mN) - j;
                    _macroCosineDistancesDirect(dM, dN, A + i, B + j, _size, distances + i * N + j, N);
                }
            }
        }

        void DescrInt::CosineDistancesUnpack(size_t M, size_t N, const uint8_t* const* A, const uint8_t* const* B, float* distances) const
        {
            size_t macroM = AlignLoAny(Base::AlgCacheL2() / _unpSize, _microMu);
            size_t macroN = AlignLoAny(Base::AlgCacheL3() / _unpSize, _microNu);
            Array8u dA(Min(macroM, M) * _unpSize);
            Array8u dB(Min(macroN, N) * _unpSize);
            Array32f nA(Min(macroM, M) * 4);
            Array32f nB(AlignHi(Min(macroN, N), _microNu) * 4);
            for (size_t i = 0; i < M; i += macroM)
            {
                size_t dM = Simd::Min(M, i + macroM) - i;
                _unpackNormA(dM, A + i, nA.data, 1);
                _unpackDataA(dM, A + i, _size, dA.data, _unpSize);
                for (size_t j = 0; j < N; j += macroN)
                {
                    size_t dN = Simd::Min(N, j + macroN) - j;
                    _unpackNormB(dN, B + j, nB.data, dN);
                    _unpackDataB(dN, B + j, _size, dB.data, 1);
                    _macroCosineDistancesUnpack(dM, dN, _size, dA.data, nA.data, dB.data, nB.data, distances + i * N + j, N);
                }
            }
        }

        //-------------------------------------------------------------------------------------------------

        void* DescrIntInit(size_t size, size_t depth)
        {
            if (!Base::DescrInt::Valid(size, depth))
                return NULL;
            return new Sse41::DescrInt(size, depth);
        }
    }
#endif
}
