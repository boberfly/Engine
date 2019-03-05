#pragma once

#include "core/types.h"
#include "core/linear_allocator.h"
#include "core/string.h"

extern "C" {
struct fppTag;
}

namespace Graphics
{
	class ShaderPreprocessor
	{
	public:
		ShaderPreprocessor();
		~ShaderPreprocessor();

		void AddInclude(const char* includePath);
		void AddDefine(const char* define, const char* value);
		bool Preprocess(const char* inputFile, const char* inputData);
		const Core::String& GetOutput() const { return output_; }
		const Core::Vector<const char*> GetDependencies() const { return dependencies_; }

	private:
		static void cbError(void* userData, char* format, va_list varArgs);
		static char* cbInput(char* buffer, int size, void* userData);
		static void cbOutput(int inChar, void* userData);
		static void cbDependency(char* dependency, void* userData);

		Core::Vector<fppTag> tags_;
		char* inputData_;
		i32 inputOffset_;
		i32 inputSize_;
		Core::LinearAllocator allocator_;
		Core::String output_;
		Core::Vector<const char*> dependencies_;
	};

} // namespace Graphics
