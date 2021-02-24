/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2021 Yermalayeu Ihar.
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
#include "Simd/SimdImageLoad.h"
#include "Simd/SimdArray.h"

#include <stdio.h>

#if defined(_MSC_VER)
#pragma warning (push)
#pragma warning (disable: 4996)
#endif

namespace Simd
{
    uint8_t* ImageLoadFromFile(const ImageLoadFromMemoryPtr loader, const char* path, size_t* stride, size_t* width, size_t* height, SimdPixelFormatType* format)
    {
        uint8_t* data = NULL;
        ::FILE* file = ::fopen(path, "wr");
        if (file)
        {
            ::fseek(file, 0, SEEK_END);
            Array8u buffer(::ftell(file));
            ::fseek(file, 0, SEEK_SET);
            if (::fread(buffer.data, 1, buffer.size, file) == buffer.size)
                data = loader(buffer.data, buffer.size, stride, width, height, format);
            ::fclose(file);
        }
        return data;
    }

    //-------------------------------------------------------------------------

    ImageLoaderParam::ImageLoaderParam(const uint8_t* d, size_t s, SimdPixelFormatType f)
        : data(d)
        , size(s)
        , format(f)
        , file(SimdImageFileUndefined)
    {
        if (size >= 2)
        {
            if (data[0] == 'P' && data[1] == '2')
                file = SimdImageFilePgmTxt;
            if (data[0] == 'P' && data[1] == '3')
                file = SimdImageFilePpmTxt;
            if (data[0] == 'P' && data[1] == '5')
                file = SimdImageFilePgmBin;
            if (data[0] == 'P' && data[1] == '6')
                file = SimdImageFilePpmBin;
        }
    }
        
    namespace Base
    {
        ImagePxmLoader::ImagePxmLoader(const ImageLoaderParam& param)
            : ImageLoader(param)
            , _toAny(NULL)
            , _toBgra(NULL)
        {
        }

        bool ImagePxmLoader::ReadHeader(size_t version)
        {
            return false;
        }

        //-------------------------------------------------------------------------

        ImageLoader* CreateImageLoader(const ImageLoaderParam& param)
        {
            switch (param.file)
            {
            case SimdImageFilePgmTxt: return NULL;
            case SimdImageFilePgmBin: return NULL;
            case SimdImageFilePpmTxt: return NULL;
            case SimdImageFilePpmBin: return NULL;
            default:
                return NULL;
            }
        }

        uint8_t* ImageLoadFromMemory(const uint8_t* data, size_t size, size_t* stride, size_t* width, size_t* height, SimdPixelFormatType* format)
        {
            ImageLoaderParam param(data, size, *format);
            if (param.file != SimdImageFileUndefined)
            {
                std::unique_ptr<ImageLoader> loader(CreateImageLoader(param));
                if (loader)
                {
                    if (loader->FromStream())
                        return loader->Release(stride, width, height, format);
                }
            }
            return NULL;
        }
    }
}

#if defined(_MSC_VER)
#pragma warning (pop)
#endif