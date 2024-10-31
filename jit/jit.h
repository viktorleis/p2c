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
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>

namespace jit {

// Simple JIT engine based on the KaleidoscopeJIT.
// https://www.llvm.org/docs/tutorial/BuildingAJIT1.html
class Jit {
private:
  std::unique_ptr<llvm::orc::ExecutionSession> ES;

  llvm::DataLayout DL;
  llvm::orc::MangleAndInterner Mangle;

  llvm::orc::RTDyldObjectLinkingLayer ObjectLayer;
  llvm::orc::IRCompileLayer CompileLayer;

  llvm::orc::JITDylib& JD;

public:
  Jit(std::unique_ptr<llvm::orc::ExecutionSession> ES,
      llvm::orc::JITTargetMachineBuilder JTMB,
      llvm::DataLayout DL)
      : ES(std::move(ES)),
        DL(std::move(DL)),
        Mangle(*this->ES, this->DL),
        ObjectLayer(*this->ES,
                    []() { return std::make_unique<llvm::SectionMemoryManager>(); }),
        CompileLayer(*this->ES,
                     ObjectLayer,
                     std::make_unique<llvm::orc::ConcurrentIRCompiler>(std::move(JTMB))),
        JD(this->ES->createBareJITDylib("main")) {
    // https://www.llvm.org/docs/ORCv2.html#how-to-add-process-and-library-symbols-to-jitdylibs
    JD.addGenerator(
         llvm::cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
             DL.getGlobalPrefix())));
    llvm::cantFail(JD.define(llvm::orc::absoluteSymbols(
        {{Mangle("libc_puts"),
          {llvm::orc::ExecutorAddr::fromPtr(&puts), llvm::JITSymbolFlags::Exported}}})));
  }

  ~Jit() {
    if (auto Err = ES->endSession())
      ES->reportError(std::move(Err));
  }

  static std::unique_ptr<Jit> Create() {
    auto EPC = llvm::cantFail(llvm::orc::SelfExecutorProcessControl::Create());
    auto ES = std::make_unique<llvm::orc::ExecutionSession>(std::move(EPC));

    llvm::orc::JITTargetMachineBuilder JTMB(
        ES->getExecutorProcessControl().getTargetTriple());

    auto DL = llvm::cantFail(JTMB.getDefaultDataLayoutForTarget());

    auto jit =  std::make_unique<Jit>(std::move(ES), std::move(JTMB), std::move(DL));
  	auto libcxx = llvm::cantFail(llvm::orc::DynamicLibrarySearchGenerator::Load("/usr/local/lib/libc++.so.1.0", '\0'));
    auto libcxxabi = llvm::cantFail(llvm::orc::DynamicLibrarySearchGenerator::Load("/usr/local/lib/libc++abi.so", '\0'));
    jit->JD.addGenerator(std::move(libcxx));
    jit->JD.addGenerator(std::move(libcxxabi));
	return jit;
  }

  llvm::Expected<llvm::orc::ResourceTrackerSP> addModule(llvm::orc::ThreadSafeModule TSM) {
    auto RT = JD.createResourceTracker();
    if (auto E = CompileLayer.add(RT, std::move(TSM))) {
      return E;
    }
    return RT;
  }

  llvm::Expected<llvm::orc::ExecutorSymbolDef> lookup(llvm::StringRef Name) {
      auto lookupName = Mangle(Name.str());
      std::cout << "looking with " << (*lookupName).str() << std::endl;
    return ES->lookup({&JD}, lookupName);
  }
};

}  // namespace jit

#endif