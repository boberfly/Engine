#include "catch.hpp"

#include "core/debug.h"
#include "core/file.h"
#include "core/random.h"
#include "core/timer.h"
#include "core/vector.h"
#include "core/os.h"
#include "job/manager.h"
#include "plugin/manager.h"
#include "resource/manager.h"

#include "graphics/converters/shader_parser.h"
#include "graphics/converters/shader_preprocessor.h"

#include <cstring>

namespace
{
	class PathResolver : public Core::IFilePathResolver
	{
	public:
		PathResolver(const char* resolvePath)
		    : resolvePath_(resolvePath)
		{
		}

		virtual ~PathResolver() {}

		bool ResolvePath(const char* inPath, char* outPath, i32 maxOutPath) override
		{
			const char* paths[] = {resolvePath_};

			char intPath[Core::MAX_PATH_LENGTH];
			for(i32 i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
			{
				memset(intPath, 0, sizeof(intPath));
				Core::FileAppendPath(intPath, sizeof(intPath), paths[i]);
				Core::FileAppendPath(intPath, sizeof(intPath), inPath);
				if(Core::FileExists(intPath))
				{
					strcpy_s(outPath, maxOutPath, intPath);
					return true;
				}
			}

			return false;
		}

		bool OriginalPath(const char* inPath, char* outPath, i32 maxOutPath) override
		{
			const char* paths[] = {resolvePath_};

			for(i32 i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
			{
				if(strstr(inPath, paths[i]) == inPath)
				{
					auto len = strlen(paths[i]);
					strcpy_s(outPath, maxOutPath, inPath + len + 1);
					return true;
				}
			}
			return false;
		}

		const char* resolvePath_ = nullptr;
	};

	class ShaderParserCallbacks : public Graphics::IShaderParserCallbacks
	{
	public:
		ShaderParserCallbacks(const char* logFile)
		    : logFile_()
		{
			if(logFile)
				logFile_ = Core::File(logFile, Core::FileFlags::DEFAULT_WRITE);
		}

		void OnError(Graphics::ErrorType errorType, const char* fileName, i32 lineNumber, i32 lineOffset,
		    const char* line, const char* str) override
		{
			auto outLine = Core::String().Printf(
			    "%s(%i-%i): error: %u: %s\n", fileName, lineNumber, lineOffset, (u32)errorType, str);

			outLine.Appendf("> %s\n> ", line);
			for(i32 idx = 1; idx < lineOffset; ++idx)
				outLine.Appendf(" ");
			outLine.Appendf("^\n");

			Core::Log(" %s", outLine.c_str());
			if(logFile_)
				logFile_.Write(outLine.data(), outLine.size());
		}

		Core::File logFile_;
	};

	bool compareFiles(const char* a, const char* b)
	{
		Core::File fileA(a, Core::FileFlags::READ);
		Core::File fileB(b, Core::FileFlags::READ);

		if(fileA.Size() != fileB.Size())
			return false;

		Core::Vector<char> dataA;
		Core::Vector<char> dataB;
		dataA.resize((i32)fileA.Size());
		dataB.resize((i32)fileB.Size());
		fileA.Read(dataA.data(), dataA.size());
		fileB.Read(dataB.data(), dataB.size());

		return memcmp(dataA.data(), dataB.data(), dataA.size()) == 0;
	}
}

TEST_CASE("graphics-tests-shader-preprocessor")
{
	const char* testPath = "../../../../res/shader_tests";
	PathResolver pathResolver(testPath);
	const char* testShader = "00-basic.esf";

	Core::File shaderFile(testShader, Core::FileFlags::READ, &pathResolver);
	if(shaderFile)
	{
		Core::Log("Preprocessing %s...\n", testShader);

		Core::String shaderCode;
		shaderCode.resize((i32)shaderFile.Size());
		shaderFile.Read(shaderCode.data(), shaderCode.size());

		{
			Graphics::ShaderPreprocessor shaderPreprocessor;
			shaderPreprocessor.AddInclude(testPath);
			shaderPreprocessor.AddInclude("../../../../res");
			shaderPreprocessor.AddInclude("../../../../res/shaders");
			shaderPreprocessor.AddInclude("../../../../res/shader_tests");
			CHECK(shaderPreprocessor.Preprocess(testShader, shaderCode.c_str()));
		}
	}
}

TEST_CASE("graphics-tests-shader-parser")
{
	const char* testPath = "../../../../res/shader_tests/parser";
	PathResolver pathResolver(testPath);

	// Gather all esf files in "res/shader_tests/parser"
	Core::Vector<Core::FileInfo> fileInfos;
	i32 numFiles = Core::FileFindInPath(pathResolver.resolvePath_, "esf", nullptr, 0);
	fileInfos.resize(numFiles);
	Core::FileFindInPath(pathResolver.resolvePath_, "esf", fileInfos.data(), fileInfos.size());

#if 1
	fileInfos.clear();
	Core::FileInfo testFileInfo;
	strcpy_s(testFileInfo.fileName_, sizeof(testFileInfo.fileName_), "technique-01.esf");
	fileInfos.push_back(testFileInfo);
#endif

	// Create temporary log path.
	auto logPath = Core::String().Printf("%s/logs/tmp", testPath);
	Core::FileCreateDir(logPath.c_str());

	for(const auto& fileInfo : fileInfos)
	{
		Core::File shaderFile(fileInfo.fileName_, Core::FileFlags::READ, &pathResolver);
		if(shaderFile)
		{
			Core::Log("Parsing %s...\n", fileInfo.fileName_);

			auto compareFileName = Core::String().Printf("%s/logs/%s.log", testPath, fileInfo.fileName_);
			auto logFileName = Core::String().Printf("%s/logs/tmp/%s.log", testPath, fileInfo.fileName_);

			// If log exists, remove it.
			if(Core::FileExists(logFileName.c_str()))
			{
				Core::FileRemove(logFileName.c_str());
			}

			Core::String shaderCode;
			shaderCode.resize((i32)shaderFile.Size());
			shaderFile.Read(shaderCode.data(), shaderCode.size());

			{
				Graphics::ShaderParser shaderParser;
				ShaderParserCallbacks callbacks(logFileName.c_str());
				shaderParser.Parse(fileInfo.fileName_, shaderCode.c_str(), &callbacks);
			}

			CHECK(compareFiles(compareFileName.c_str(), logFileName.c_str()));
		}
	}
}

TEST_CASE("graphics-tests-shader-basic")
{
	const char* testPath = "../../../../res/shader_tests";
	PathResolver pathResolver(testPath);
	const char* shaderName = "00-basic.esf";

	Core::File shaderFile(shaderName, Core::FileFlags::READ, &pathResolver);
	if(shaderFile)
	{
		Core::String shaderCode;
		shaderCode.resize((i32)shaderFile.Size());
		shaderFile.Read(shaderCode.data(), shaderCode.size());

		Core::Log("Preprocessing %s...\n", shaderName);
		Graphics::ShaderPreprocessor shaderPreprocessor;
		shaderPreprocessor.AddInclude(testPath);
		shaderPreprocessor.AddInclude("../../../../res");
		shaderPreprocessor.AddInclude("../../../../res/shaders");
		shaderPreprocessor.AddInclude("../../../../res/shader_tests");

		CHECK(shaderPreprocessor.Preprocess(shaderName, shaderCode.c_str()));
		Core::Log("Parsing %s...\n", shaderName);

		{
			Graphics::ShaderParser shaderParser;
			ShaderParserCallbacks callbacks(nullptr);
			auto* nodeShaderFile = shaderParser.Parse(shaderName, shaderPreprocessor.GetOutput().c_str(), &callbacks);

			using namespace Graphics;

			class ASTLogger : public AST::IVisitor
			{
			public:
				void Log(const char* msg, ...)
				{
					for(i32 ind = 0; ind < indent_; ++ind)
						Core::Log("    ");
					va_list args;
					va_start(args, msg);
					Core::LogV(msg, args);
					va_end(args);
					Core::Log("\n");
				}
				bool VisitEnter(AST::NodeShaderFile* node) override
				{
					Log("ShaderFile (%s) {", node->name_.c_str());
					++indent_;
					return true;
				}
				void VisitExit(AST::NodeShaderFile* node) override
				{
					--indent_;
					Log("}");
				}
				bool VisitEnter(AST::NodeAttribute* node) override
				{
					Log("Attribute (%s) {", node->name_.c_str());
					++indent_;
					return true;
				}
				void VisitExit(AST::NodeAttribute* node) override
				{
					--indent_;
					Log("}");
				}
				bool VisitEnter(AST::NodeStorageClass* node) override
				{
					Log("StorageClass (%s) {", node->name_.c_str());
					++indent_;
					return true;
				}
				void VisitExit(AST::NodeStorageClass* node) override
				{
					--indent_;
					Log("}");
				}
				bool VisitEnter(AST::NodeModifier* node) override
				{
					Log("Modifier (%s) {", node->name_.c_str());
					++indent_;
					return true;
				}
				void VisitExit(AST::NodeModifier* node) override
				{
					--indent_;
					Log("}");
				}
				bool VisitEnter(AST::NodeType* node) override
				{
					Log("Type (%s) {", node->name_.c_str());
					++indent_;
					return true;
				}
				void VisitExit(AST::NodeType* node) override
				{
					--indent_;
					Log("}");
				}
				bool VisitEnter(AST::NodeTypeIdent* node) override
				{
					Log("TypeIdent (%s<>) {", node->baseType_->name_.c_str());
					Log("}");
					return false;
				}
				void VisitExit(AST::NodeTypeIdent* node) override {}
				bool VisitEnter(AST::NodeStruct* node) override
				{
					Log("Struct (%s) {", node->name_.c_str());
					++indent_;
					return true;
				}
				void VisitExit(AST::NodeStruct* node) override
				{
					--indent_;
					Log("}");
				}
				bool VisitEnter(AST::NodeDeclaration* node) override
				{
					Log("Declaration (%s) {", node->name_.c_str());
					++indent_;
					return true;
				}
				void VisitExit(AST::NodeDeclaration* node) override
				{
					--indent_;
					Log("}");
				}
				bool VisitEnter(AST::NodeValue* node) override
				{
					Log("Value (%s) { %s", node->name_.c_str(), node->data_.c_str());
					++indent_;
					return true;
				}
				void VisitExit(AST::NodeValue* node) override
				{
					--indent_;
					Log("}");
				}
				bool VisitEnter(AST::NodeValues* node) override
				{
					Log("Values (%s) {", node->name_.c_str());
					++indent_;
					return true;
				}
				void VisitExit(AST::NodeValues* node) override
				{
					--indent_;
					Log("}");
				}
				bool VisitEnter(AST::NodeMemberValue* node) override
				{
					Log("MemberValue (%s) { %s = ", node->name_.c_str(), node->member_.c_str());
					++indent_;
					return true;
				}
				void VisitExit(AST::NodeMemberValue* node) override
				{
					--indent_;
					Log("}");
				}


			private:
				i32 indent_ = 0;
			};
			ASTLogger astLogger;
			nodeShaderFile->Visit(&astLogger);
		}
	}
}