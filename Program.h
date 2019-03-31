#pragma once

#include <vector>

#include <llvm/Support/SourceMgr.h>
#include <llvm/IR/Module.h>
#include "llvm/ADT/DenseMap.h"

#include "Func.h"
#include "Expr/Expr.h"
#include "Type/TypeHandler.h"

/**
 * @brief The Program class represents the whole parsed LLVM program.
 */
class Program {
friend class TypeHandler;
private:
    llvm::LLVMContext context;
    llvm::SMDiagnostic error;
    std::unique_ptr<llvm::Module> module;

    TypeHandler typeHandler;

    std::vector<std::unique_ptr<Func>> functions; // vector of parsed functions
    std::vector<std::unique_ptr<Func>> declarations; // vector of function declarations
    std::vector<std::unique_ptr<Struct>> structs; // vector of parsed structs
    std::vector<std::unique_ptr<GlobalValue>> globalVars; // vector of parsed global variables
    llvm::DenseMap<const llvm::GlobalVariable*, std::unique_ptr<RefExpr>> globalRefs; //map containing references to global variables
    llvm::DenseMap<const llvm::StructType*, std::unique_ptr<Struct>> unnamedStructs; // map containing unnamed structs

    unsigned structVarCount = 0;
    unsigned gvarCount = 0;
    unsigned anonStructCount = 0;

    bool hasVarArg; //program uses "stdarg.h"

    /**
     * @brief getVarName Creates a new name for a variable in form of string containing "var" + structVarCount.
     * @return String containing a new variable name.
     */
    std::string getStructVarName();

    /**
     * @brief getAnonStructName Creates new name for anonymous struct.
     * @return New name for anonymous struct
     */
    std::string getAnonStructName();

    /**
     * @brief getValue Return string containing value used for global variable initialization.
     * @param val llvm Constant used for initialization
     * @return Init value
     */
    std::string getValue(const llvm::Constant* val) const;

    /**
     * @brief unsetAllInit Resets the init flag for every global variable.
     * Used for repeated calling of print and saveFile.
     */
    void unsetAllInit();

    /**
     * @brief saveStruct Saves parsed Struct into the file. If Struct contains other Struct, then the other is saved first.
     * @param strct Struct for saving
     * @param file Opened file for saving the struct.
     */
    void saveStruct(Struct* strct, std::ofstream& file);

    /**
     * @brief printStruct Prints parsed Struct. If Struct contains other Struct, then the other is printed first.
     * @param strct Struct for printing
     */
    void printStruct(Struct* strct);

    /**
     * @brief parseProgram Parses the whole program (structs, functions and global variables0.
     */
    void parseProgram();

    /**
     * @brief parseStructs Parses structures into Struct expression.
     */
    void parseStructs();

    /**
     * @brief parseFunctions Parses functions into corresponding expressions.
     */
    void parseFunctions();

    /**
     * @brief parseGlobalVars Parses global variables.
     */
    void parseGlobalVars();

public:
    std::string fileName;
    bool stackIgnored; //instruction stacksave was ignored

    /**
     * @brief Program Constructor of a Program class, parses given file into a llvm::Module.
     * @param file Path to a file for parsing.
     */
    Program(const std::string& file);

    /**
     * @brief print Prints the translated program in the llvm::outs() stream.
     */
    void print();

    /**
     * @brief saveFile Saves the translated program to the file with given name.
     * @param fileName Name of the file.
     */
    void saveFile(const std::string& fileName);

    /**
     * @brief getStruct Returns pointer to the Struct corresponding to the given LLVM StructType.
     * @param strct LLVM StructType
     * @return Pointer to Struct expression if the struct is found, nullptr otherwise
     */
    Struct* getStruct(const llvm::StructType* strct) const;

    /**
     * @brief getStruct Returns pointer to the Struct with the given name.
     * @param name Name of the struct
     * @return Pointer to Struct expression if the struct is found, nullptr otherwise
     */
    Struct* getStruct(const std::string& name) const;

    /**
     * @brief getGlobalVar Returns corresponding refference to GlobalValue expression.
     * @param val llvm global variable
     * @return RefExpr expression or nullptr
     */
    const RefExpr* getGlobalVar(const llvm::Value* val) const;

    /**
     * @brief addDeclaration Adds new declaration of given function.
     * @param func LLVM Function
     */
    void addDeclaration(llvm::Function* func);

    /**
     * @brief createNewUnnamedStruct Adds new unnamed struct to the unnamedStructs map.
     * @param strct Unnamed struct
     */
    void createNewUnnamedStruct(const llvm::StructType* strct);

    /**
     * @brief getType Transforms llvm::Type into corresponding Type object
     * @param type llvm::Type for transformation
     * @return unique_ptr to corresponding Type object
     */
    std::unique_ptr<Type> getType(const llvm::Type* type, bool voidType = false);
};
