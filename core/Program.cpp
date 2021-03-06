#include "Program.h"

#include <llvm/IR/LLVMContext.h>
#include "llvm/Support/raw_ostream.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/IR/Constants.h"

#include "../type/Type.h"

#include <fstream>
#include <exception>
#include <algorithm>
#include <regex>
#include <iostream>

Program::Program() : typeHandler(TypeHandler(this)) {
}

std::string Program::getAnonStructName() {
	std::string name = "anonymous_struct" + std::to_string(anonStructCount);
	anonStructCount++;
	return name;
}

StructType* Program::getStruct(const llvm::StructType* strct) const {
	std::string structName = TypeHandler::getStructName(strct->getName().str());

	for (const auto& structElem : structs) {
		if (structElem->name.compare(structName) == 0) {
			return structElem.get();
		}
	}

	if (unnamedStructs.find(strct) != unnamedStructs.end()) {
		return unnamedStructs.find(strct)->second.get();
	}

	return nullptr;
}

StructType* Program::getStruct(const std::string& name) const {
	for (const auto& structElem : structs) {
		if (structElem->name.compare(name) == 0) {
			return structElem.get();
		}
	}

	for (const auto& mapElem : unnamedStructs) {
		if (mapElem.second->name.compare(name) == 0) {
			return mapElem.second.get();
		}
	}

	return nullptr;
}

RefExpr* Program::getGlobalVar(const llvm::Value* val) {
	if (const llvm::GlobalVariable* GV = llvm::dyn_cast_or_null<llvm::GlobalVariable>(val)) {
		if (globalRefs.count(GV)) {
			return globalRefs.find(GV)->second.get();
		}
	}

	return nullptr;
}

void Program::createNewUnnamedStruct(const llvm::StructType *strct) {
	if (unnamedStructs.find(strct) != unnamedStructs.end()) {
		return;
	}

	auto structExpr = std::make_unique<StructType>(getAnonStructName());

	for (llvm::Type* type : strct->elements()) {
		structExpr->addItem(getType(type), getStructVarName());
	}

	unnamedStructs[strct] = std::move(structExpr);
}

Type* Program::getType(const llvm::Type* type) {
	auto result = typeHandler.getType(type);
    assert(result);
    return result;
}

Func* Program::getFunction(const llvm::Function* f) {
	auto it = functions.find(f);
	if (it == functions.end()) {
		return nullptr;
	}

	return it->second.get();
}

RefExpr* Program::getGlobalRef(const llvm::GlobalVariable* gvar) {
	return globalRefs[gvar].get();
}

void Program::addFunction(const llvm::Function* llvmFunc, std::unique_ptr<Func> func) {
	functions[llvmFunc] = std::move(func);
}

void Program::addStruct(std::unique_ptr<StructType> strct) {
	structs.push_back(std::move(strct));
}

const std::set<std::string>& Program::getGlobalVarNames() const {
	return globalVarNames;
}

std::string Program::getStructVarName() {
	std::string varName = "structVar";
	varName += std::to_string(structVarCount);
	structVarCount++;

	return varName;
}

bool Program::isFunctionDeclared(const llvm::Function* llvmFunc) const {
    return functions.find(llvmFunc) != functions.end();
}

Expr* Program::addOwnership(std::unique_ptr<Expr> expr) {
    Expr* result = expr.get();
    ownership.push_back(std::move(expr));
    return result;
}

Expr* Program::getExpr(const llvm::Value* value) {
    auto it = exprMap.find(value);
    if (it == exprMap.end()) {
        return nullptr;
    }

    return it->second;
}

void Program::addExpr(const llvm::Value* value, Expr* expr) {
    exprMap[value] = expr;
}

bool Program::isPassCompleted(PassType pass) const {
    return passes.find(pass) != passes.end();
}

void Program::addPass(PassType pass) {
    passes.insert(pass);
}

std::string Program::getUnionName() {
    ++unionCounter;
    return "u" + std::to_string(unionCounter);
}

UnionType* Program::addUnion(const std::vector<Type*>& subtypes) {
    auto ut = std::make_unique<UnionType>(getUnionName());

    size_t index = 0;
    for (auto* type : subtypes) {
        ut->addItem(type, "ty" + std::to_string(index));
        ++index;
    }

    auto* result = ut.get();
    unions.push_back(std::move(ut));
    return result;
}
