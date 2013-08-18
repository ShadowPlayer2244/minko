/*
Copyright (c) 2013 Aerys

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include "minko/Common.hpp"

#include "minko/component/AbstractLight.hpp"

namespace minko
{
	namespace component
	{
		class AmbientLight :
			public AbstractLight
		{
		public:
			typedef std::shared_ptr<AmbientLight>	Ptr;

		private:
			static uint _counter;

			float	_ambient;

		public:
			inline static
			Ptr
			create(float ambient = .2f)
			{
				auto al = std::shared_ptr<AmbientLight>(new AmbientLight(ambient));

				al->initialize();

				return al;
			}

			~AmbientLight()
			{
			}

			inline
			float
			ambient()
			{
				return _ambient;
			}

			inline
			void
			ambient(float ambient)
			{
				_ambient = ambient;
				data()->set("ambient", ambient);
			}

		protected:
			AmbientLight(float ambient);
		};
	}
}
