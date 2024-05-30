#include "cppruntime.hpp"
#include <iostream>

#define DEBUG_MSG(msg) std::cout << "[DEBUG] " << msg << std::endl

#if defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(__BCPLUSPLUS__) || defined(__MWERKS__)
#if defined(NV_LIB_STATIC)
#define NVLLVM_EXPORT
#elif defined(NVLLVM_LIB)
#define NVLLVM_EXPORT __declspec(dllexport)
#else
#define NVLLVM_EXPORT __declspec(dllimport)
#endif
#else
#define NVLLVM_EXPORT
#endif

#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0602
#endif


#include <llvm/InitializePasses.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>

#include <llvm/Support/Host.h>
#include <llvm/Support/raw_ostream.h>

#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"

#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/FileSystemOptions.h>
#include <clang/Basic/LangOptions.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Lex/HeaderSearch.h>
#include <clang/Lex/HeaderSearchOptions.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Parse/ParseAST.h>
#include <clang/Sema/Sema.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/ASTConsumer.h>



# define BEGIN_REGION(name) \
    auto name##_start = std::chrono::high_resolution_clock::now();


# define END_REGION(name) \
    std::cout << "Exiting region " << #name << "("<< std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - name##_start).count() << "mcs)" << std::endl;



struct ModuleState
{
    llvm::LLVMContext context;

    llvm::EngineBuilder* builder;
    llvm::ExecutionEngine* executionEngine;
    std::unique_ptr<llvm::Module> module;
    llvm::ModulePassManager modulePassManager;
    std::string filename;

    void update()
    {
        BEGIN_REGION(CREATING_INSTANCE)
        clang::CompilerInstance compilerInstance;
        auto& compilerInvocation = compilerInstance.getInvocation();
        END_REGION(CREATING_INSTANCE)




        BEGIN_REGION(DIAGNOSTIC_OBJECTS_CREATION)
        // Диагностика работы Clang
        clang::IntrusiveRefCntPtr<clang::DiagnosticOptions> DiagOpts = new clang::DiagnosticOptions;
        clang::TextDiagnosticPrinter *textDiagPrinter = new clang::TextDiagnosticPrinter(llvm::errs(), &*DiagOpts);

        clang::IntrusiveRefCntPtr<clang::DiagnosticIDs> pDiagIDs;
        clang::DiagnosticsEngine *pDiagnosticsEngine = new clang::DiagnosticsEngine(pDiagIDs, &*DiagOpts, textDiagPrinter);
        END_REGION(DIAGNOSTIC_OBJECTS_CREATION)


        BEGIN_REGION(CREATING_PARAMS_FOR_INVOCATION)
        // Target platform
        std::string triplet="-triple=" + llvm::sys::getDefaultTargetTriple();
        std::vector<const char*> itemcstrs;
        itemcstrs.push_back(triplet.c_str());
        itemcstrs.push_back(filename.c_str());
        END_REGION(CREATING_PARAMS_FOR_INVOCATION)


        BEGIN_REGION(CREATING_COMPILER_INVOCATION)
        clang::CompilerInvocation::CreateFromArgs(compilerInvocation, llvm::ArrayRef<const char *>(itemcstrs.data(), itemcstrs.size()), *pDiagnosticsEngine);
        END_REGION(CREATING_COMPILER_INVOCATION)




        BEGIN_REGION(CREATING_COMPILER_INSTANCE)

        auto* languageOptions = compilerInvocation.getLangOpts();
        auto& preprocessorOptions = compilerInvocation.getPreprocessorOpts();
        auto& targetOptions = compilerInvocation.getTargetOpts();
        auto& frontEndOptions = compilerInvocation.getFrontendOpts();
        auto& headerSearchOptions = compilerInvocation.getHeaderSearchOpts();
        auto& codeGenOptions = compilerInvocation.getCodeGenOpts();

        frontEndOptions.ShowStats = false;
        headerSearchOptions.Verbose = false;
        targetOptions.Triple = llvm::sys::getDefaultTargetTriple();
        compilerInstance.createDiagnostics(textDiagPrinter, false);


        std::unique_ptr<clang::CodeGenAction> action = std::make_unique<clang::EmitLLVMOnlyAction>(&context);

        //Compilation HERE
        if(!compilerInstance.ExecuteAction(*action))
        {
            throw std::runtime_error("Compilation error");
        }
        END_REGION(CREATING_COMPILER_INSTANCE)


        BEGIN_REGION(CREATING_LLVM_MODULE)
        // Runtime LLVM Module
        module = action->takeModule();
        if(!module)
            throw std::runtime_error("Cannot retrieve IR module.");

        // Оптимизация IR
        llvm::PassBuilder passBuilder;
        llvm::LoopAnalysisManager loopAnalysisManager;
        llvm::FunctionAnalysisManager functionAnalysisManager;
        llvm::CGSCCAnalysisManager cGSCCAnalysisManager;
        llvm::ModuleAnalysisManager moduleAnalysisManager;

        passBuilder.registerModuleAnalyses(moduleAnalysisManager);
        passBuilder.registerCGSCCAnalyses(cGSCCAnalysisManager);
        passBuilder.registerFunctionAnalyses(functionAnalysisManager);
        passBuilder.registerLoopAnalyses(loopAnalysisManager);
        passBuilder.crossRegisterProxies(loopAnalysisManager, functionAnalysisManager, cGSCCAnalysisManager, moduleAnalysisManager);

        modulePassManager = passBuilder.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3);
        modulePassManager.run(*module, moduleAnalysisManager);
        END_REGION(CREATING_LLVM_MODULE)

        //enumerate all functions
//        for (auto& func : *module) {
//            DEBUG_MSG("Found function: " << func.getName().str());
//        }

        BEGIN_REGION(CREATING_EXECUTION_ENGINE)
        builder= new llvm::EngineBuilder(std::move(module));
        builder->setMCJITMemoryManager(std::make_unique<llvm::SectionMemoryManager>());
        builder->setOptLevel(llvm::CodeGenOpt::Level::Aggressive);

        std::string createErrorMsg;

        builder->setEngineKind(llvm::EngineKind::JIT);
        builder->setVerifyModules(true);
        builder->setErrorStr(&createErrorMsg);

        std::string triple = llvm::sys::getDefaultTargetTriple();
//        DEBUG_MSG("Using target triple: " << triple);
        executionEngine = builder->create();

        if(!executionEngine)
        {
            throw std::runtime_error("Cannot create execution engine.'" + createErrorMsg + "'");
        }
        END_REGION(CREATING_EXECUTION_ENGINE)

    }
};


namespace vkbase::cppruntime
{
    std::map<std::string, Module> modules;

    bool LLVMInitialized = false;
    void initLLVM()
    {
        if(LLVMInitialized)
            return;

        // Run the global LLVM pass initialization functions.
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();

        auto& Registry = *llvm::PassRegistry::getPassRegistry();

        llvm::initializeCore(Registry);
        llvm::initializeScalarOpts(Registry);
        llvm::initializeVectorization(Registry);
        llvm::initializeIPO(Registry);
        llvm::initializeAnalysis(Registry);
        llvm::initializeTransformUtils(Registry);
        llvm::initializeInstCombine(Registry);

        llvm::initializeTarget(Registry);


        LLVMInitialized = true;
    }


    Module::Module(const char *filename)
    {
        moduleState=new ModuleState;
        ModuleState& state=*reinterpret_cast<ModuleState*>(moduleState);
        state.filename=filename;
        initLLVM();
        state.update();
    }

    Module::~Module()
    {
        delete reinterpret_cast<ModuleState*>(moduleState);
    }

    void *Module::getFunction(const char *funcname)
    {
        BEGIN_REGION(GETTING_FUNCTION)
        ModuleState& state=*reinterpret_cast<ModuleState*>(moduleState);
        void* resFunc= reinterpret_cast<void*>(state.executionEngine->getFunctionAddress(funcname));
        END_REGION(GETTING_FUNCTION)
        return resFunc;
    }

    void Module::update()
    {
        ModuleState& state=*reinterpret_cast<ModuleState*>(moduleState);
        state.update();
    }
}