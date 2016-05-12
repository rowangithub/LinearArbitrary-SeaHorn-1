#include "seahorn/Analysis/ApiAnalysisPass.hh"

/**
* Identifies functions that call a specific API function
*/
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/raw_ostream.h"
#include "avy/AvyDebug.h"
#include "llvm/ADT/SCCIterator.h"
#include "boost/range/algorithm/reverse.hpp"

namespace seahorn
{
  using namespace llvm;

  ApiCallInfo* ApiAnalysisPass::analyzeFunction(Function& F, ApiCallInfo *init_state)
  {
    for (auto analyzedfunc : m_apiAnalysis)
    {
      for (auto af : analyzedfunc.m_funcs)
      {
        if (af->getName() == F.getName())
        {
          bool done = analyzedfunc.m_progress == m_apilist.size();
          if (done) return init_state;;
        }
      }
    }

    // First, get the basic blocks in topological order
    std::vector<const BasicBlock*> sortedBBlocks;
    RevTopoSort(F,sortedBBlocks);
    boost::reverse(sortedBBlocks);

    // initialize the API list with no APIs found for this BB
    ApiCallList apilist;
    for (std::string API : m_apilist)
    {
      apilist.push_back(API);
    }

    ApiCallInfo *aci=NULL;
    if (init_state != NULL)
    {
      aci = init_state;
    }
    else
    {
      aci = new ApiCallInfo();
    }

    aci->m_funcs.push_back(&F);

    std::string targetapi = m_apilist[aci->m_progress];

    // for each of the sorted BBs,
    for (const BasicBlock *bb : sortedBBlocks)
    {
      for (BasicBlock::const_iterator bi = bb->begin(); bi != bb->end(); bi++)
      {
        const Instruction *I = &*bi;
        if (const CallInst *CI = dyn_cast<CallInst> (I))
        {
          CallSite CS (const_cast<CallInst*> (CI));
          Function *cf = CS.getCalledFunction();

          // this function contains an API function call of interest

          if (cf)
          {
            // This is a call to the API of interest
            if (cf->getName().str() == m_apilist[aci->m_progress])
            {
              // Found a call to the target, now record that and increment
              // progress
              //outs() << "Found target API: " << m_apilist[aci->m_progress] << " in " << F.getName() << "\n";

              aci->m_finalapilist.push_back(m_apilist[aci->m_progress]);
              aci->m_funcs.push_back(cf);
              if (0==aci->m_progress)
              {
                aci->m_startFunc = &F;
              }
              ++aci->m_progress;
            }
            else
            {
              if (!cf->empty())
              {
                //outs() << "In Function "<< F.getName()<< " calling outgoing Function "
                //     << cf->getName() << " looking for " << m_apilist[aci->m_progress] << "\n";

                aci = analyzeFunction(*cf, aci);

                //outs() << "Back in Function "<< F.getName() << " looking for " << m_apilist[aci->m_progress] << "\n";
              }
            }
          }
        }
      } // for each insn

      // are we done?
      if (aci->m_progress >= m_apilist.size())
      {
        break;
      }
    }

    //outs() << "Returning\n";

    return aci;

  }

  void ApiAnalysisPass::printFinalAnalysis() const
  {
    // for each function, propagate the analysis
    for (auto& analysis : m_apiAnalysis)
    {
      if (analysis.m_progress >= m_apilist.size())
      {
        outs() << "FINAL RESULTS:\n Required APIs called in required order starting at "
        << analysis.m_startFunc->getName() << "\nSequence of calls:\n";
        for (auto path : analysis.m_funcs)
        {
          outs() << "\t* " << path->getName() << "\n";
        }
      }
    }
  }

  void ApiAnalysisPass::parseApiString(std::string apistring)
  {
    std::istringstream ss(apistring);
    std::string api;
    while(std::getline(ss, api, ','))
    {
      m_apilist.push_back(api);
    }
  }

  // The body of the pass
  bool ApiAnalysisPass::runOnModule (Module &M)
  {

    // sort funcs in topo order
    std::vector<Function*> sortedFuncs;
    CallGraph &CG = getAnalysis<CallGraphWrapperPass> ().getCallGraph();

    for (auto it = scc_begin (&CG); !it.isAtEnd (); ++it)
    {
      auto &scc = *it;
      for (CallGraphNode *cgn : scc)
      {
        Function *f = cgn->getFunction();
        if (!f) continue;
        sortedFuncs.push_back(f);
      }
    }

    // This call generates API call information for each
    for (Function *F : sortedFuncs)
    {
      ApiCallInfo *aci = analyzeFunction(*F,NULL);
      m_apiAnalysis.push_back(aci);

      // if (aci->m_progress == m_apilist.size())
      // {
      //   outs() << "Completed analysis of " << F->getName() << " with success.\n\n";
      // }
      // else
      // {
      //   outs() << "Completed analysis of " << F->getName() << " withOUT success.\n\n";
      // }
    }

    printFinalAnalysis();

    return false;
  }

  void ApiAnalysisPass::getAnalysisUsage (AnalysisUsage &AU) const {
    AU.setPreservesAll ();
    AU.addRequired<CallGraphWrapperPass> ();
    AU.addPreserved<CallGraphWrapperPass> ();
  }

  char ApiAnalysisPass::ID = 0;

  llvm::Pass *createApiAnalysisPass(std::string &config) {
    return new ApiAnalysisPass(config);
  }

  llvm::Pass *createApiAnalysisPass() {
    return new ApiAnalysisPass();
  }
}   // namespace seahorn

static llvm::RegisterPass<seahorn::ApiAnalysisPass>
X("call-api", "Determine if a given API is called",false, false);
