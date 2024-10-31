#include <iostream>

#include <llvm/ExecutionEngine/Orc/LLJIT.h>

#include "ccompiler.h"
#include "jit.h"

struct P2CJit {
    static void execute(const std::string& gen_code){
    	llvm::DebugFlag = true;
        std::string finalized_code =
            "#include <functional>\n"
			"#include <tuple>\n"
			"#include <algorithm>\n"
			"#include <iostream>\n"
			"#include <string>\n"
			"#include <unordered_map>\n"
			"#include <vector>\n"
			"#include <cassert>\n"
            "#include \"io.hpp\"\n"
            "#include \"types.hpp\"\n"
			"#include \"tpch.hpp\"\n"
			"using namespace std;\n"
			"using namespace p2c;\n"

    		"void*   __dso_handle = (void*) &__dso_handle;\n"
            "void std::__libcpp_verbose_abort(char const* format, ...) noexcept {std::abort();}"
            "void jitted(){\n"
    	"TPCH db(\"data-generator/output/\");\n"
        //+ gen_code
    		//
    		//"vector<tuple<int32_t, char, double, double>> vector14;\n"
    		//"unordered_map<tuple<char>, tuple<int32_t, double, double>> aggHT10;\n"
            //"int32_t sum = 0;\n"
            //"for (size_t i=0; i!=db.nation.tupleCount;i++){\n"
            //"sum+=db.nation.n_nationkey[i];\n"
            //"}\n"
            //"std::cout << sum << std::endl\n;"
        + "\n}";
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

