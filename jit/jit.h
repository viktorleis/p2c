#ifndef JIT_H
#define JIT_H

#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/ExecutorProcessControl.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/Orc/Shared/ExecutorAddress.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/LLVMContext.h>

namespace jit {
using llvm::cantFail;
using llvm::DataLayout;
using llvm::Expected;
using llvm::JITEvaluatedSymbol;
using llvm::JITSymbolFlags;
using llvm::SectionMemoryManager;
using llvm::StringRef;

using llvm::orc::ConcurrentIRCompiler;
// using llvm::orc::DynamicLibrarySearchGenerator;
using llvm::orc::ExecutionSession;
using llvm::orc::ExecutorAddr;
using llvm::orc::ExecutorSymbolDef;
using llvm::orc::IRCompileLayer;
using llvm::orc::JITDylib;
using llvm::orc::JITTargetMachineBuilder;
using llvm::orc::MangleAndInterner;
using llvm::orc::ResourceTrackerSP;
using llvm::orc::RTDyldObjectLinkingLayer;
using llvm::orc::SelfExecutorProcessControl;
using llvm::orc::ThreadSafeModule;

// Simple JIT engine based on the KaleidoscopeJIT.
// https://www.llvm.org/docs/tutorial/BuildingAJIT1.html
class Jit {
private:
  std::unique_ptr<ExecutionSession> ES;

  DataLayout DL;
  MangleAndInterner Mangle;

  RTDyldObjectLinkingLayer ObjectLayer;
  IRCompileLayer CompileLayer;

  JITDylib& JD;

public:
  Jit(std::unique_ptr<ExecutionSession> ES,
      JITTargetMachineBuilder JTMB,
      DataLayout DL)
      : ES(std::move(ES)),
        DL(std::move(DL)),
        Mangle(*this->ES, this->DL),
        ObjectLayer(*this->ES,
                    []() { return std::make_unique<SectionMemoryManager>(); }),
        CompileLayer(*this->ES,
                     ObjectLayer,
                     std::make_unique<ConcurrentIRCompiler>(std::move(JTMB))),
        JD(this->ES->createBareJITDylib("main")) {
    // https://www.llvm.org/docs/ORCv2.html#how-to-add-process-and-library-symbols-to-jitdylibs
    // JD.addGenerator(
    //     cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(
    //         DL.getGlobalPrefix())));
    cantFail(JD.define(llvm::orc::absoluteSymbols(
        {{Mangle("libc_puts"),
          {ExecutorAddr::fromPtr(&puts), JITSymbolFlags::Exported}}})));
  }

  ~Jit() {
    if (auto Err = ES->endSession())
      ES->reportError(std::move(Err));
  }

  static std::unique_ptr<Jit> Create() {
    auto EPC = cantFail(SelfExecutorProcessControl::Create());
    auto ES = std::make_unique<ExecutionSession>(std::move(EPC));

    JITTargetMachineBuilder JTMB(
        ES->getExecutorProcessControl().getTargetTriple());

    auto DL = cantFail(JTMB.getDefaultDataLayoutForTarget());

    return std::make_unique<Jit>(std::move(ES), std::move(JTMB), std::move(DL));
  }

  Expected<ResourceTrackerSP> addModule(ThreadSafeModule TSM) {
    auto RT = JD.createResourceTracker();
    if (auto E = CompileLayer.add(RT, std::move(TSM))) {
      return E;
    }
    return RT;
  }

  Expected<ExecutorSymbolDef> lookup(StringRef Name) {
    return ES->lookup({&JD}, Mangle(Name.str()));
  }
};

}  // namespace jit

#endif