#include "Writer.h"
#include "../parser/cfunc.h"

#include <unordered_set>

void Writer::writeProgram(const Program& program) {
    includes(program);
    wr.line("");
    structDeclarations(program);
    wr.line("");
    anonymousStructDeclarations(program);
    wr.line("");
    typedefs(program);
    wr.line("");
    structDefinitions(program);
    wr.line("");
    functionDeclarations(program);
    wr.line("");
    globalVarDefinitions(program);
    wr.line("");
    functionDefinitions(program);
    wr.line("");
}

void Writer::includes(const Program& program) {
    if (!useIncludes)
        return;

    wr.comment("includes");
    if (program.hasVarArg)
        wr.include("stdarg.h");

    if (program.hasStdLib)
        wr.include("stdlib.h");

    if (program.hasString)
        wr.include("string.h");

    if (program.hasStdio)
        wr.include("stdio.h");

    if (program.hasPthread)
        wr.include("pthread.h");
}

void Writer::structDeclarations(const Program& program) {
    wr.comment("struct declarations");
    const auto& structs = program.structs;

    for (const auto& strct : structs) {
        wr.declareStruct(strct->name);
    }
}

void Writer::structDefinition(const Program& program, const Struct* strct, std::unordered_set<const Struct*>& printed) {
    for (const auto& item : strct->items) {
        auto type = item.first.get();
        if (auto AT = llvm::dyn_cast_or_null<ArrayType>(type)) {
            if (AT->isStructArray) {
                structDefinition(program, program.getStruct(AT->structName), printed);
            }
        }

        if (auto PT = llvm::dyn_cast_or_null<PointerType>(type)) {
            if (PT->isStructPointer && PT->isArrayPointer) {
                structDefinition(program, program.getStruct(PT->structName), printed);
            }
        }

        if (auto ST = llvm::dyn_cast_or_null<StructType>(type)) {
            structDefinition(program, program.getStruct(ST->name), printed);
        }
    }

    auto it = printed.find(strct);
    if (it != printed.end()) {
        return;
    }

    printed.insert(it, strct);

    wr.startStruct(strct->name);

    for (const auto& pair : strct->items) {
        const auto& ty = pair.first;
        const auto& name = pair.second;
        wr.indent(1);
        wr.structItem(ty->toString(), ty->surroundName(name));
    }

    wr.endStruct();
}

void Writer::structDefinitions(const Program& program) {
    wr.comment("struct definitions");
    std::unordered_set<const Struct*> printed;

    for (const auto& strct : program.structs) {
        structDefinition(program, strct.get(), printed);
    }

    for (const auto& pair : program.unnamedStructs) {
        const auto& strct = pair.second;
        structDefinition(program, strct.get(), printed);
    }
}

void Writer::typedefs(const Program& program) {
    wr.comment("type definitions");
    const auto& defs = program.typeHandler.sortedTypeDefs;

    for (const auto& def : defs) {
        wr.defineType(def->type, def->name, def->typeEnd);
    }
}

void Writer::anonymousStructDeclarations(const Program& program) {
    wr.comment("anonymous struct declarations");
    const auto& structs = program.unnamedStructs;

    for (const auto& elem : structs) {
        wr.declareStruct(elem.second->name);
    }
}

void Writer::globalVars(const Program& program) {
    wr.comment("global variable declarations");
    for (const auto& gvar : program.globalVars) {
        if (useIncludes && (gvar->valueName == "stdin" || gvar->valueName == "stdout" || gvar->valueName == "stderr")) {
            continue;
        }

        wr.declareVar(gvar->getType()->toString(), gvar->getType()->surroundName(gvar->valueName));
    }
}

void Writer::globalVarDefinitions(const Program& program) {
    wr.comment("global variable definitions");
    for (const auto& gvar : program.globalVars) {
        if (useIncludes && (gvar->valueName == "stdin" || gvar->valueName == "stdout" || gvar->valueName == "stderr")) {
            continue;
        }

        wr.raw(gvar->getType()->toString());
        wr.raw(" ");
        wr.raw(gvar->getType()->surroundName(gvar->valueName));
        if (gvar->value) {
            wr.raw(" = ");
            gvar->value->accept(ew);
        }
        wr.line(";");
    }
}

void Writer::functionHead(const Func* func) {
    auto PT = llvm::dyn_cast_or_null<PointerType>(func->returnType.get());
    bool arrayPtr = (PT && PT->isArrayPointer);
    if (arrayPtr) {
        wr.startArrayFunction(func->returnType->toString(), PT->levels, func->name);
    } else {
        wr.startFunction(func->returnType->toString(), func->name);
    }

    auto last = func->parameters.cend() - 1;
    wr.startFunctionParams();
    for (auto it = func->parameters.cbegin(); it != func->parameters.cend(); ++it) {
        const auto& param = *it;
        wr.raw(param->getType()->toString());
        wr.raw(" ");
        param->accept(ew);

        if (it != last)
            wr.nextFunctionParam();
    }

    if (func->isVarArg) {
        if (!func->parameters.empty()) {
            wr.nextFunctionParam();
        }
        wr.functionVarArgs();
    }
    wr.endFunctionParams();

    if (arrayPtr) {
        wr.raw(")");
        wr.raw(PT->sizes);
    }
}

void Writer::functionDeclarations(const Program& program) {
    wr.comment("function declarations");
    std::vector<Func*> declarations;

    for (const auto& decl : program.functions) {
        auto& func = decl.second;
        if (!isFunctionPrinted(func.get())) {
            continue;
        }

        declarations.push_back(func.get());
    }

    std::sort(declarations.begin(), declarations.end(), [](const Func* a, const Func* b){ return a->name <= b->name;});

    for (const auto* func : declarations) {
        functionHead(func);
        wr.endFunctionDecl();
    }
}

bool Writer::isFunctionPrinted(const Func* func) const {
    if (isCFunc(func->name) || func->name == "va_start" || func->name == "va_end" || func->name == "va_copy" || isCMath(func->name)) {
        return false;
    }

    if (useIncludes) {
        if (isStdLibFunc(func->name) || isStringFunc(func->name) || isStdioFunc(func->name) || isPthreadFunc(func->name)) {
            return false;
        }
    } else {
        if ((func->name == "memcpy" || func->name == "memset" || func->name == "memmove") && func->parameters.size() > 3) {
            return false;
        }
    }

    return true;
}

void Writer::writeBlock(const Block* block) {
    wr.indent(1);
    wr.startBlock(block->blockName);

    for (const auto& expr : block->expressions) {
        wr.indent(1);
        expr->accept(ew);
        if (!llvm::isa<IfExpr>(expr) && !llvm::isa<SwitchExpr>(expr)) {
            wr.line(";");
        }
    }
}

void Writer::functionDefinitions(const Program& program) {
    std::vector<const Func*> functions;

    for (const auto& pair : program.functions) {
        const auto* func = pair.second.get();
        if (func->isDeclaration) {
            continue;
        }

        if (!isFunctionPrinted(func)) {
            continue;
        }

        functions.push_back(func);
    }

    std::sort(functions.begin(), functions.end(), [](const Func* a, const Func* b){ return a->name <= b->name;});

    for (const auto* func : functions) {

        functionHead(func);
        wr.startFunctionBody();

        // start with phi variables
        // TODO: prepend phi variables of function to the first block instead of this hack
        for (const auto& var : func->phiVariables) {
            wr.indent(1);
            wr.declareVar(var->getType()->toString(), var->getType()->surroundName(var->valueName));
        }

        assert(func->entry);
        writeBlock(func->entry);

        for (const auto& blockEntry : func->blockMap) {
            const auto* block = blockEntry.second.get();

            if (block == func->entry)
                continue;

            if (!block->doInline) {
                writeBlock(block);
            }
        }

        wr.endFunctionBody();
        wr.line("");
    }
}
