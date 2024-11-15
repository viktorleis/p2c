#include <iostream>

#include <llvm/ExecutionEngine/Orc/LLJIT.h>

#include "ccompiler.h"
#include "jit.h"

struct P2CJit {
    static void execute(const std::string& gen_code){
    	llvm::DebugFlag = true;
        std::string finalized_code =
			"#include <unordered_map>\n"
            "#include <iostream>\n"
    		"void*   __dso_handle = (void*) &__dso_handle;\n"
            "void std::__libcpp_verbose_abort(char const* format, ...) noexcept {std::abort();}"
            "void jitted(){\n"
    		"	std::unordered_map<int, int> map;"
            "	map.insert({2,  3});"
            "   std::cout << map[2] << std::endl;"
        	"\n}";
        auto R = cc::CCompiler().compile(finalized_code.c_str());
        // Abort if compilation failed.
        auto [C, M] = llvm::cantFail(std::move(R));
        M->print(llvm::errs(), nullptr);

        // -- JIT compiler the IR module.

        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();

    	auto JIT = jit::Jit::Create();
    	auto TSM = llvm::orc::ThreadSafeModule(std::move(M), std::move(C));
    	std::string mangledInit = "_Z6jittedv";


    	auto RT = JIT->addModule(std::move(TSM));
    	if (auto E = RT.takeError()) {
    		llvm::errs() << llvm::toString(std::move(E)) << '\n';
    		exit(1);
    	}

    	if (auto ADDR = JIT->lookup(mangledInit)) {
    		std::printf("JIT ADDR 0x%lx\n", (*ADDR).getAddress().getValue());
    		auto JIT_FN = (*ADDR).getAddress().toPtr<void()>();
    		JIT_FN();
    		std::cout << "returned from function" << std::endl;
    	}
    }
};

