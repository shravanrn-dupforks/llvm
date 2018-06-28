//===-- WebAssemblyRegNumbering.cpp - Register Numbering ------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file implements a pass which corrects array indexes into pointer arrays
/// when using a non default HostTriple.
///
//===----------------------------------------------------------------------===//

#include "WebAssembly.h"
#include "llvm/Pass.h"
#include "llvm/IR/InstVisitor.h"

using namespace llvm;

#define DEBUG_TYPE "wasm-pointer-array-index-expand"

namespace {

class WebAssemblyPointerArrayIndexExpand : public FunctionPass,
                               public InstVisitor<WebAssemblyPointerArrayIndexExpand> {

  StringRef getPassName() const override {
    return "WebAssembly Pointer Array Index Expand";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    FunctionPass::getAnalysisUsage(AU);
  }

  bool runOnFunction(Function &F) override;

public:
  static char ID;
  WebAssemblyPointerArrayIndexExpand() : FunctionPass(ID) {}

  void visitGetElementPtrInst(GetElementPtrInst &I);
};

}

char WebAssemblyPointerArrayIndexExpand::ID = 0;
INITIALIZE_PASS(WebAssemblyPointerArrayIndexExpand, DEBUG_TYPE,
                "WebAssembly correct array indexes into pointer arrays",
                false, false)

FunctionPass *llvm::createWebAssemblyPointerArrayIndexExpand() {
  return new WebAssemblyPointerArrayIndexExpand();
}

void WebAssemblyPointerArrayIndexExpand::visitGetElementPtrInst(GetElementPtrInst &Inst) {
  if (auto *pointerType = dyn_cast<PointerType>(Inst.getPointerOperandType())) {
    // Inst.dump();
    auto baseType = pointerType->getElementType();
    ArrayType *arrayType = dyn_cast<ArrayType>(baseType);
    if (baseType->isPointerTy() || (arrayType != nullptr && arrayType->getElementType()->isPointerTy())) {
      for (unsigned I = 1, E = Inst.getNumIndices() + 1; I != E; ++I) {
        auto value = Inst.getOperand(I);
        Value* newVal;
        if (auto *valueConst = dyn_cast<ConstantInt>(value)) {
          auto intVal = valueConst->getSExtValue();
          if (intVal == 0) {
            continue;
          }
          newVal = ConstantInt::get(value->getType(), intVal * 2, false /* isSigned */);
        } else {
          auto multiply = ConstantInt::get(value->getType(), 2, false /* isSigned */);
          newVal = BinaryOperator::Create(Instruction::BinaryOps::Mul, value, multiply, value->getName(), &Inst);
          // newVal->dump();
        }
        Inst.setOperand(I, newVal);
        Inst.setIsInBounds(false);
      }
      // Inst.dump();
    } else if (baseType->isStructTy()) {
      bool indexingIntoPointerArray = false;
      // Inst.dump();
      for (unsigned I = 1, E = Inst.getNumIndices() + 1; I != E; ++I) {
        auto value = Inst.getOperand(I);
        if (indexingIntoPointerArray) {
          Value* newVal;
          if (auto *valueConst = dyn_cast<ConstantInt>(value)) {
            auto intVal = valueConst->getSExtValue();
            newVal = ConstantInt::get(value->getType(), intVal * 2, false /* isSigned */);
          } else {
            auto multiply = ConstantInt::get(value->getType(), 2, false /* isSigned */);
            newVal = BinaryOperator::Create(Instruction::BinaryOps::Mul, value, multiply, value->getName(), &Inst);
            // newVal->dump();
          }
          Inst.setOperand(I, newVal);
          indexingIntoPointerArray = false;
        }
        SmallVector<Value *, 8> IdxVec(Inst.idx_begin(),
                                   Inst.idx_begin() + I);
        auto indexType = GetElementPtrInst::getIndexedType(baseType, IdxVec);
        ArrayType *indexArrayType = dyn_cast<ArrayType>(indexType);
        if (indexArrayType != nullptr && indexArrayType->getElementType()->isPointerTy()) {
          indexingIntoPointerArray = true;
        }
      }
    }

    auto operandType = Inst.getOperand(1)->getType();
    if(operandType == Type::getInt64Ty(operandType->getContext())) {
      auto targetType = Type::getInt32Ty(operandType->getContext());
      for (unsigned I = 1, E = Inst.getNumIndices() + 1; I != E; ++I) {
        auto value = Inst.getOperand(I);
        Value* newVal;
        if (auto *valueConst = dyn_cast<ConstantInt>(value)) {
          auto intVal = valueConst->getSExtValue();
          newVal = ConstantInt::get(targetType, intVal, false /* isSigned */);
        } else {
          newVal = new TruncInst(value, targetType, value->getName(), &Inst);
          // newVal->dump();
        }
        Inst.setOperand(I, newVal);
      }
    }
  }
}

bool WebAssemblyPointerArrayIndexExpand::runOnFunction(Function &F) {
  visit(F);
  return true;
}
